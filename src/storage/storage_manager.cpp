#include "storage/storage_manager.h"
#include "storage/mysql_storage.h"
#include "storage/storage_factory.h"
#include "storage/storage_cache.h"
#include "storage/mysql_connection_pool.h"
#include "common/logging.h"
#include <algorithm>
#include <regex>

namespace kb {
namespace storage {

// 获取单例实例
StorageManager& StorageManager::getInstance() {
    static StorageManager instance;
    return instance;
}

// 构造函数
StorageManager::StorageManager() 
    : defaultStorage_(nullptr),
      useCaching_(false),
      useConnectionPool_(false),
      connectionPoolSize_(10) {
    
    LOG_INFO("存储管理器初始化");
    registerBuiltinStorageTypes();
}

// 析构函数
StorageManager::~StorageManager() {
    if (defaultStorage_) {
        closeDefaultStorage();
    }
    LOG_INFO("存储管理器销毁");
}

// 注册存储适配器
void StorageManager::registerStorageType(
    const std::string& name,
    std::function<StorageInterfacePtr(const std::string&)> factory) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!factory) {
        LOG_WARN("尝试注册空工厂函数: {}", name);
        return;
    }
    
    auto it = factories_.find(name);
    if (it != factories_.end()) {
        LOG_WARN("存储类型已存在，将被覆盖: {}", name);
    }
    
    factories_[name] = factory;
    LOG_INFO("已注册存储类型: {}", name);
}

// 创建存储适配器实例
StorageInterfacePtr StorageManager::createStorage(
    const std::string& type,
    const std::string& connectionString) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 使用存储工厂创建存储适配器
    auto storage = StorageFactory::getInstance().createStorage(type, connectionString);
    
    if (storage && useCaching_) {
        LOG_INFO("为存储适配器启用缓存");
    }
    
    if (storage && useConnectionPool_ && type == "mysql") {
        // 初始化MySQL连接池
        LOG_INFO("为MySQL存储适配器启用连接池，大小: {}", connectionPoolSize_);
        
        // 解析连接字符串
        MySQLConnectionInfo connectionInfo;
        auto mysqlStorage = std::dynamic_pointer_cast<MySQLStorage>(storage);
        if (mysqlStorage) {
            // 使用 MySQLStorage 对象获取连接信息
            connectionInfo.host = mysqlStorage->getHost();
            connectionInfo.port = mysqlStorage->getPort();
            connectionInfo.username = mysqlStorage->getUsername();
            connectionInfo.password = mysqlStorage->getPassword();
            connectionInfo.database = mysqlStorage->getDatabaseName();
            connectionInfo.charset = mysqlStorage->getCharset();
            
            // 初始化连接池
            if (!MySQLConnectionPool::getInstance().initialize(
                connectionInfo, connectionPoolSize_)) {
                LOG_ERROR("MySQL连接池初始化失败");
            }
        }
    }
    
    return storage;
}

// 获取已注册的存储类型列表
std::vector<std::string> StorageManager::getRegisteredStorageTypes() const {
    return StorageFactory::getInstance().getRegisteredTypes();
}

// 初始化默认存储适配器
bool StorageManager::initializeDefaultStorage(
    const std::string& type,
    const std::string& connectionString) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 先关闭现有的默认存储
    if (defaultStorage_) {
        if (!closeDefaultStorage()) {
            LOG_WARN("无法关闭现有的默认存储");
            // 继续初始化新的默认存储
        }
    }
    
    // 创建新的存储实例
    defaultStorage_ = createStorage(type, connectionString);
    if (!defaultStorage_) {
        LOG_ERROR("初始化默认存储失败: {}", type);
        return false;
    }
    
    LOG_INFO("默认存储初始化成功: {}", type);
    return true;
}

// 获取默认存储适配器
StorageInterfacePtr StorageManager::getDefaultStorage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return defaultStorage_;
}

// 关闭默认存储适配器
bool StorageManager::closeDefaultStorage() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!defaultStorage_) {
        LOG_WARN("没有默认存储可关闭");
        return false;
    }
    
    // 检查存储类型
    auto mysqlStorage = std::dynamic_pointer_cast<MySQLStorage>(defaultStorage_);
    if (mysqlStorage && useConnectionPool_) {
        // 关闭MySQL连接池
        MySQLConnectionPool::getInstance().shutdown();
        LOG_INFO("MySQL连接池已关闭");
    }
    
    // 使用正确的 close 方法
    if (!defaultStorage_->close()) {
        LOG_WARN("关闭默认存储连接失败");
        return false;
    }
    
    defaultStorage_.reset();
    LOG_INFO("默认存储已关闭");
    return true;
}

void StorageManager::enableCache(size_t maxSize, int ttlMinutes) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (useCaching_) {
        LOG_INFO("缓存已经启用，更新配置");
    } else {
        LOG_INFO("启用缓存");
    }
    
    useCaching_ = true;
    StorageCache::getInstance().initialize(
        maxSize, std::chrono::minutes(ttlMinutes));
}

void StorageManager::disableCache() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!useCaching_) {
        LOG_INFO("缓存已经禁用");
        return;
    }
    
    useCaching_ = false;
    StorageCache::getInstance().clear();
    LOG_INFO("缓存已禁用");
}

void StorageManager::clearCache() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!useCaching_) {
        LOG_INFO("缓存未启用，无需清理");
        return;
    }
    
    StorageCache::getInstance().clear();
    LOG_INFO("缓存已清理");
}

std::string StorageManager::getCacheStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!useCaching_) {
        return "缓存未启用";
    }
    
    return StorageCache::getInstance().getStats();
}

void StorageManager::setUseConnectionPool(bool usePool, size_t poolSize) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    bool wasUsingPool = useConnectionPool_;
    useConnectionPool_ = usePool;
    connectionPoolSize_ = poolSize;
    
    if (usePool) {
        LOG_INFO("启用连接池，大小: {}", poolSize);
        
        // 如果默认存储是MySQL且已经初始化，则初始化连接池
        if (defaultStorage_) {
            auto mysqlStorage = std::dynamic_pointer_cast<MySQLStorage>(defaultStorage_);
            if (mysqlStorage && !wasUsingPool) {
                // 获取连接信息
                MySQLConnectionInfo connectionInfo;
                
                // 直接从 MySQLStorage 对象获取连接信息
                connectionInfo.host = mysqlStorage->getHost();
                connectionInfo.port = mysqlStorage->getPort();
                connectionInfo.username = mysqlStorage->getUsername();
                connectionInfo.password = mysqlStorage->getPassword();
                connectionInfo.database = mysqlStorage->getDatabaseName();
                connectionInfo.charset = mysqlStorage->getCharset();
                
                if (!MySQLConnectionPool::getInstance().initialize(
                    connectionInfo, connectionPoolSize_)) {
                    LOG_ERROR("MySQL连接池初始化失败");
                }
            }
        }
    } else {
        if (wasUsingPool) {
            LOG_INFO("禁用连接池");
            // 关闭现有连接池
            MySQLConnectionPool::getInstance().shutdown();
        }
    }
}

void StorageManager::getConnectionPoolConfig(bool& outUsePool, size_t& outPoolSize) const {
    std::lock_guard<std::mutex> lock(mutex_);
    outUsePool = useConnectionPool_;
    outPoolSize = connectionPoolSize_;
}

bool StorageManager::checkConnectionPoolHealth() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!useConnectionPool_) {
        LOG_INFO("连接池未启用");
        return false;
    }
    
    return MySQLConnectionPool::getInstance().checkHealth();
}

// 注册所有内置存储适配器
void StorageManager::registerBuiltinStorageTypes() {
    // 使用存储工厂来注册内置类型
    auto& factory = StorageFactory::getInstance();
    
    // 获取已注册的类型
    auto types = factory.getRegisteredTypes();
    
    // 将所有已注册的类型复制到本地工厂
    for (const auto& type : types) {
        registerStorageType(type, [type](const std::string& connStr) -> StorageInterfacePtr {
            return StorageFactory::getInstance().createStorage(type, connStr);
        });
    }
    
    LOG_INFO("内置存储类型已注册完成");
}

} // namespace storage
} // namespace kb 