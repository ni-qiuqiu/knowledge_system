/**
 * @file storage_factory.cpp
 * @brief 存储适配器工厂实现
 */

#include "storage/storage_factory.h"
#include "common/logging.h"
#include <algorithm>

namespace kb {
namespace storage {

StorageFactory& StorageFactory::getInstance() {
    static StorageFactory instance;
    return instance;
}

StorageFactory::StorageFactory() {
    registerBuiltinTypes();
}

void StorageFactory::registerBuiltinTypes() {
    // 注册MySQL存储适配器
    registerStorageType("mysql", 
        [this](const std::string& connectionString) -> StorageInterfacePtr {
            return createMySQLStorage(connectionString);
        }
    );
    
    LOG_INFO("内置存储类型已注册");
}

StorageInterfacePtr StorageFactory::createStorage(
    const std::string& type,
    const std::string& connectionString) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = factories_.find(type);
    if (it == factories_.end()) {
        LOG_ERROR("未知的存储类型: {}", type);
        return nullptr;
    }
    
    try {
        auto storage = it->second(connectionString);
        if (!storage) {
            LOG_ERROR("创建存储适配器失败: {}", type);
            return nullptr;
        }
        
        LOG_INFO("成功创建存储适配器: {}", type);
        return storage;
    } catch (const std::exception& e) {
        LOG_ERROR("创建存储适配器异常: {}", std::string(e.what()));
        return nullptr;
    }
}

StorageInterfacePtr StorageFactory::createMySQLStorage(const std::string& connectionString) {
    auto storage = std::make_shared<MySQLStorage>();
    
    if (!storage->initialize(connectionString)) {
        LOG_ERROR("初始化MySQL存储连接失败");
        return nullptr;
    }
    
    if (!storage->createSchema()) {
        LOG_ERROR("创建或更新MySQL数据库架构失败");
        storage->close();
        return nullptr;
    }
    
    LOG_INFO("成功创建MySQL存储适配器");
    return storage;
}

void StorageFactory::registerStorageType(
    const std::string& type,
    std::function<StorageInterfacePtr(const std::string&)> factory) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!factory) {
        LOG_WARN("尝试注册空工厂函数: {}", type);
        return;
    }
    
    auto it = factories_.find(type);
    if (it != factories_.end()) {
        LOG_WARN("存储类型已存在，将被覆盖: {}", type);
    }
    
    factories_[type] = factory;
    LOG_INFO("已注册存储类型: {}", type);
}

std::vector<std::string> StorageFactory::getRegisteredTypes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> types;
    types.reserve(factories_.size());
    
    for (const auto& pair : factories_) {
        types.push_back(pair.first);
    }
    
    // 排序以确保结果一致
    std::sort(types.begin(), types.end());
    
    return types;
}

} // namespace storage
} // namespace kb 