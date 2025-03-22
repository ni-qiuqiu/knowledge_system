/**
 * @file mysql_connection_pool.cpp
 * @brief MySQL连接池实现
 */

#include "storage/mysql_connection_pool.h"
#include "common/logging.h"
#include <mysql/mysql.h>
#include <thread>

namespace kb {
namespace storage {

MySQLConnection::MySQLConnection() 
    : mysql_(nullptr), lastUsedTime_(std::chrono::steady_clock::now()) {
    mysql_ = mysql_init(nullptr);
    if (!mysql_) {
        LOG_ERROR("无法初始化MySQL连接");
    }
}

MySQLConnection::~MySQLConnection() {
    disconnect();
}

bool MySQLConnection::connect(const MySQLConnectionInfo& info) {
    connectionInfo_ = info;
    
    if (!mysql_) {
        mysql_ = mysql_init(nullptr);
        if (!mysql_) {
            LOG_ERROR("无法初始化MySQL连接");
            return false;
        }
    }
    
    // 设置连接选项
    unsigned int timeout = 5;  // 5秒连接超时
    mysql_options(mysql_, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    
    // 设置自动重连
    bool reconnect = true;
    mysql_options(mysql_, MYSQL_OPT_RECONNECT, &reconnect);
    
    // 设置字符集
    mysql_options(mysql_, MYSQL_SET_CHARSET_NAME, info.charset.c_str());
    
    // 连接到数据库
    if (!mysql_real_connect(
            mysql_, 
            info.host.c_str(), 
            info.username.c_str(), 
            info.password.c_str(), 
            info.database.c_str(), 
            info.port, 
            nullptr,  // UNIX套接字
            0  // 客户端标志
        )) {
        LOG_ERROR("连接到MySQL服务器失败: {}", std::string(mysql_error(mysql_)));
        return false;
    }
    
    updateLastUsedTime();
    return true;
}

void MySQLConnection::disconnect() {
    if (mysql_) {
        mysql_close(mysql_);
        mysql_ = nullptr;
    }
}

bool MySQLConnection::isValid() {
    if (!mysql_) {
        return false;
    }
    
    // 使用ping检查连接是否有效
    if (mysql_ping(mysql_) != 0) {
        LOG_WARN("MySQL连接无效: {}", std::string(mysql_error(mysql_)));
        return false;
    }
    
    return true;
}

bool MySQLConnection::reset() {
    disconnect();
    mysql_ = mysql_init(nullptr);
    if (!mysql_) {
        LOG_ERROR("无法初始化MySQL连接");
        return false;
    }
    
    return connect(connectionInfo_);
}

// ====== MySQLConnectionPool 实现 ======

MySQLConnectionPool& MySQLConnectionPool::getInstance() {
    static MySQLConnectionPool instance;
    return instance;
}

MySQLConnectionPool::MySQLConnectionPool() 
    : maxSize_(20), 
      connectionTimeout_(std::chrono::milliseconds(5000)), 
      idleTimeout_(std::chrono::milliseconds(60000)),
      isInitialized_(false),
      isShuttingDown_(false) {
}

MySQLConnectionPool::~MySQLConnectionPool() {
    shutdown();
}

bool MySQLConnectionPool::initialize(
    const MySQLConnectionInfo& info,
    size_t initialSize,
    size_t maxSize,
    std::chrono::milliseconds connectionTimeout,
    std::chrono::milliseconds idleTimeout) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (isInitialized_) {
        LOG_WARN("MySQL连接池已经初始化");
        return true;
    }
    
    connectionInfo_ = info;
    maxSize_ = maxSize;
    connectionTimeout_ = connectionTimeout;
    idleTimeout_ = idleTimeout;
    
    // 初始化mysql库
    if (mysql_library_init(0, nullptr, nullptr)) {
        LOG_ERROR("无法初始化MySQL客户端库");
        return false;
    }
    
    // 创建初始连接
    for (size_t i = 0; i < initialSize; ++i) {
        auto conn = createConnection();
        if (conn) {
            idleConnections_.push(conn);
        } else {
            LOG_WARN("创建初始连接失败，继续初始化其余连接");
        }
    }
    
    isInitialized_ = true;
    LOG_INFO("MySQL连接池初始化成功，初始连接数: {}, 最大连接数: {}", idleConnections_.size(), maxSize_);
    
    // 启动后台线程清理空闲连接
    std::thread cleanupThread([this]() {
        while (!isShuttingDown_) {
            std::this_thread::sleep_for(std::chrono::minutes(1));
            if (!isShuttingDown_) {
                cleanupIdleConnections();
            }
        }
    });
    cleanupThread.detach();
    
    return true;
}

std::shared_ptr<MySQLConnection> MySQLConnectionPool::createConnection() {
    auto conn = std::make_shared<MySQLConnection>();
    if (!conn->connect(connectionInfo_)) {
        LOG_ERROR("无法创建新的MySQL连接");
        return nullptr;
    }
    
    LOG_DEBUG("创建了新的MySQL连接");
    return conn;
}

MYSQL* MySQLConnectionPool::getConnection() {
    std::unique_lock<std::mutex> lock(mutex_);
    
    if (!isInitialized_) {
        LOG_ERROR("MySQL连接池未初始化");
        return nullptr;
    }
    
    if (isShuttingDown_) {
        LOG_ERROR("MySQL连接池正在关闭");
        return nullptr;
    }
    
    // 等待可用连接
    auto waitUntil = std::chrono::steady_clock::now() + connectionTimeout_;
    
    while (idleConnections_.empty() && 
           (idleConnections_.size() + activeConnections_.size() >= maxSize_)) {
        // 等待直到有连接被释放或超时
        auto status = condition_.wait_until(lock, waitUntil);
        if (status == std::cv_status::timeout) {
            LOG_ERROR("获取MySQL连接超时");
            return nullptr;
        }
        
        // 如果连接池正在关闭，返回nullptr
        if (isShuttingDown_) {
            LOG_ERROR("获取连接期间MySQL连接池关闭");
            return nullptr;
        }
    }
    
    std::shared_ptr<MySQLConnection> conn;
    
    // 有空闲连接
    if (!idleConnections_.empty()) {
        conn = idleConnections_.front();
        idleConnections_.pop();
    } 
    // 创建新连接
    else if (idleConnections_.size() + activeConnections_.size() < maxSize_) {
        conn = createConnection();
        if (!conn) {
            LOG_ERROR("无法创建新的MySQL连接");
            return nullptr;
        }
    }
    
    // 检查连接是否有效，如果无效则重试
    if (!conn->isValid()) {
        LOG_WARN("MySQL连接无效，尝试重置");
        if (!conn->reset()) {
            LOG_ERROR("重置MySQL连接失败");
            return nullptr;
        }
    }
    
    // 更新最后使用时间
    conn->updateLastUsedTime();
    
    // 添加到活动连接列表
    activeConnections_.push_back(conn);
    
    LOG_DEBUG("获取了一个MySQL连接，当前活动连接: {}, 空闲连接: {}", activeConnections_.size(), idleConnections_.size());
    
    return conn->getHandle();
}

void MySQLConnectionPool::releaseConnection(MYSQL* mysql) {
    if (!mysql) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 查找连接
    auto it = std::find_if(activeConnections_.begin(), activeConnections_.end(),
        [mysql](const std::shared_ptr<MySQLConnection>& conn) {
            return conn->getHandle() == mysql;
        });
    
    if (it == activeConnections_.end()) {
        LOG_WARN("尝试释放不属于连接池的MySQL连接");
        return;
    }
    
    auto conn = *it;
    activeConnections_.erase(it);
    
    // 如果连接池正在关闭，不返回连接到空闲池
    if (isShuttingDown_) {
        LOG_DEBUG("连接池正在关闭，不重用连接");
        return;
    }
    
    // 更新最后使用时间
    conn->updateLastUsedTime();
    
    // 返回连接到空闲池
    idleConnections_.push(conn);
    
    LOG_DEBUG("释放了一个MySQL连接，当前活动连接: {}, 空闲连接: {}", activeConnections_.size(), idleConnections_.size());
    
    // 通知等待的线程
    condition_.notify_one();
}

void MySQLConnectionPool::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!isInitialized_ || isShuttingDown_) {
        return;
    }
    
    isShuttingDown_ = true;
    LOG_INFO("MySQL连接池开始关闭");
    
    // 清空空闲连接
    while (!idleConnections_.empty()) {
        idleConnections_.pop();
    }
    
    // 清空活动连接
    activeConnections_.clear();
    
    // 清理MySQL库
    mysql_library_end();
    
    isInitialized_ = false;
    isShuttingDown_ = false;
    LOG_INFO("MySQL连接池已完全关闭");
}

void MySQLConnectionPool::cleanupIdleConnections() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!isInitialized_ || isShuttingDown_) {
        return;
    }
    
    auto now = std::chrono::steady_clock::now();
    size_t originalSize = idleConnections_.size();
    std::queue<std::shared_ptr<MySQLConnection>> remainingConnections;
    
    // 保留至少一个空闲连接，即使它已经超时
    bool preservedOne = false;
    
    // 处理所有空闲连接
    while (!idleConnections_.empty()) {
        auto conn = idleConnections_.front();
        idleConnections_.pop();
        
        // 如果连接空闲时间超过阈值且我们已经保留了一个连接，关闭它
        if (now - conn->getLastUsedTime() > idleTimeout_ && preservedOne) {
            // 连接会在离开作用域时自动清理
        } else {
            // 保留这个连接
            remainingConnections.push(conn);
            preservedOne = true;
        }
    }
    
    // 恢复剩余的连接
    idleConnections_ = std::move(remainingConnections);
    
    size_t removedCount = originalSize - idleConnections_.size();
    if (removedCount > 0) {
        LOG_DEBUG("清理了 {} 个空闲MySQL连接", removedCount);
    }
}

bool MySQLConnectionPool::checkHealth() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!isInitialized_ || isShuttingDown_) {
        LOG_ERROR("MySQL连接池未初始化或正在关闭");
        return false;
    }
    
    // 检查是否有活动连接
    if (activeConnections_.empty() && idleConnections_.empty()) {
        LOG_WARN("MySQL连接池中没有连接");
        return false;
    }
    
    // 如果有空闲连接，检查一个连接是否有效
    if (!idleConnections_.empty()) {
        auto conn = idleConnections_.front();
        idleConnections_.pop();
        
        bool isValid = conn->isValid();
        idleConnections_.push(conn);
        
        if (!isValid) {
            LOG_WARN("MySQL连接池中的空闲连接无效");
            return false;
        }
    }
    
    LOG_INFO("MySQL连接池健康检查通过，当前活动连接: {}, 空闲连接: {}", activeConnections_.size(), idleConnections_.size());
    
    return true;
}

} // namespace storage
} // namespace kb