/**
 * @file mysql_connection_pool.h
 * @brief MySQL连接池实现
 */

#ifndef KB_MYSQL_CONNECTION_POOL_H
#define KB_MYSQL_CONNECTION_POOL_H

#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <chrono>
#include <atomic>

struct MYSQL;  // 前向声明，避免包含MySQL头文件

namespace kb {
namespace storage {

/**
 * @brief MySQL连接信息
 */
struct MySQLConnectionInfo {
    std::string host;       ///< 主机名
    int port;               ///< 端口号
    std::string username;   ///< 用户名
    std::string password;   ///< 密码
    std::string database;   ///< 数据库名
    std::string charset;    ///< 字符集
    
    /**
     * @brief 构造函数
     */
    MySQLConnectionInfo(
        const std::string& host = "localhost",
        int port = 3306,
        const std::string& username = "root",
        const std::string& password = "",
        const std::string& database = "",
        const std::string& charset = "utf8mb4"
    ) : host(host), port(port), username(username), 
        password(password), database(database), charset(charset) {}
};

/**
 * @brief MySQL连接包装器
 */
class MySQLConnection {
public:
    /**
     * @brief 构造函数
     */
    MySQLConnection();
    
    /**
     * @brief 析构函数
     */
    ~MySQLConnection();
    
    /**
     * @brief 连接到MySQL数据库
     * @param info 连接信息
     * @return 是否连接成功
     */
    bool connect(const MySQLConnectionInfo& info);
    
    /**
     * @brief 关闭连接
     */
    void disconnect();
    
    /**
     * @brief 获取MySQL连接句柄
     * @return MySQL连接句柄
     */
    MYSQL* getHandle() const { return mysql_; }
    
    /**
     * @brief 检查连接是否有效
     * @return 是否有效
     */
    bool isValid();
    
    /**
     * @brief 重置连接
     * @return 是否重置成功
     */
    bool reset();
    
    /**
     * @brief 获取最后一次使用的时间
     * @return 最后使用时间点
     */
    const std::chrono::steady_clock::time_point& getLastUsedTime() const {
        return lastUsedTime_;
    }
    
    /**
     * @brief 更新最后使用时间
     */
    void updateLastUsedTime() {
        lastUsedTime_ = std::chrono::steady_clock::now();
    }
    
private:
    MYSQL* mysql_;                              ///< MySQL连接句柄
    MySQLConnectionInfo connectionInfo_;        ///< 连接信息
    std::chrono::steady_clock::time_point lastUsedTime_;  ///< 最后使用时间
};

/**
 * @brief MySQL连接池
 */
class MySQLConnectionPool {
public:
    /**
     * @brief 获取单例实例
     * @return 连接池实例
     */
    static MySQLConnectionPool& getInstance();
    
    /**
     * @brief 初始化连接池
     * @param info 连接信息
     * @param initialSize 初始连接数
     * @param maxSize 最大连接数
     * @param connectionTimeout 连接超时时间(毫秒)
     * @param idleTimeout 空闲连接超时时间(毫秒)
     * @return 是否初始化成功
     */
    bool initialize(
        const MySQLConnectionInfo& info,
        size_t initialSize = 5,
        size_t maxSize = 20,
        std::chrono::milliseconds connectionTimeout = std::chrono::milliseconds(5000),
        std::chrono::milliseconds idleTimeout = std::chrono::milliseconds(60000)
    );
    
    /**
     * @brief 获取一个连接
     * @return 连接句柄
     */
    MYSQL* getConnection();
    
    /**
     * @brief 释放一个连接
     * @param mysql 连接句柄
     */
    void releaseConnection(MYSQL* mysql);
    
    /**
     * @brief 关闭连接池
     */
    void shutdown();
    
    /**
     * @brief 检查连接池健康状态
     * @return 是否健康
     */
    bool checkHealth();
    
private:
    /**
     * @brief 构造函数 (私有)
     */
    MySQLConnectionPool();
    
    /**
     * @brief 析构函数
     */
    ~MySQLConnectionPool();
    
    /**
     * @brief 创建新连接
     * @return 新连接
     */
    std::shared_ptr<MySQLConnection> createConnection();
    
    /**
     * @brief 清理空闲连接
     */
    void cleanupIdleConnections();
    
    MySQLConnectionInfo connectionInfo_;                  ///< 连接信息
    std::queue<std::shared_ptr<MySQLConnection>> idleConnections_;  ///< 空闲连接队列
    std::vector<std::shared_ptr<MySQLConnection>> activeConnections_;  ///< 活动连接列表
    std::mutex mutex_;                                   ///< 互斥锁
    std::condition_variable condition_;                  ///< 条件变量
    size_t maxSize_;                                     ///< 最大连接数
    std::chrono::milliseconds connectionTimeout_;        ///< 连接超时时间
    std::chrono::milliseconds idleTimeout_;              ///< 空闲连接超时时间
    std::atomic<bool> isInitialized_;                    ///< 是否已初始化
    std::atomic<bool> isShuttingDown_;                   ///< 是否正在关闭
};

} // namespace storage
} // namespace kb

#endif // KB_MYSQL_CONNECTION_POOL_H 