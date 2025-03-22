#ifndef KB_STORAGE_MANAGER_H
#define KB_STORAGE_MANAGER_H

#include "storage/storage_interface.h"
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <vector>

namespace kb {
namespace storage {

/**
 * @brief 存储管理器类，管理不同的存储适配器
 */
class StorageManager {
public:
    /**
     * @brief 获取单例实例
     * @return 存储管理器实例
     */
    static StorageManager& getInstance();
    
    /**
     * @brief 注册存储类型
     * @param name 存储类型名称
     * @param factory 存储适配器工厂函数
     */
    void registerStorageType(
        const std::string& name,
        std::function<StorageInterfacePtr(const std::string&)> factory);
    
    /**
     * @brief 创建存储适配器实例
     * @param type 存储类型名称
     * @param connectionString 连接字符串
     * @return 存储适配器指针，未注册类型返回nullptr
     */
    StorageInterfacePtr createStorage(
        const std::string& type,
        const std::string& connectionString);
    
    /**
     * @brief 获取已注册的存储类型列表
     * @return 存储类型名称列表
     */
    std::vector<std::string> getRegisteredStorageTypes() const;
    
    /**
     * @brief 初始化默认存储适配器
     * @param type 存储类型名称
     * @param connectionString 连接字符串
     * @return 是否成功初始化
     */
    bool initializeDefaultStorage(
        const std::string& type,
        const std::string& connectionString);
    
    /**
     * @brief 获取默认存储适配器
     * @return 默认存储适配器指针，未初始化返回nullptr
     */
    StorageInterfacePtr getDefaultStorage() const;
    
    /**
     * @brief 关闭默认存储适配器
     * @return 是否成功关闭
     */
    bool closeDefaultStorage();
    
    /**
     * @brief 启用缓存
     * @param maxSize 最大缓存项数量
     * @param ttlMinutes 缓存生存时间(分钟)
     */
    void enableCache(size_t maxSize = 1000, int ttlMinutes = 10);
    
    /**
     * @brief 禁用缓存
     */
    void disableCache();
    
    /**
     * @brief 清空缓存
     */
    void clearCache();
    
    /**
     * @brief 获取缓存统计信息
     * @return 统计信息字符串
     */
    std::string getCacheStats() const;
    
    /**
     * @brief 设置是否使用连接池
     * @param usePool 是否使用连接池
     * @param poolSize 连接池大小
     */
    void setUseConnectionPool(bool usePool, size_t poolSize = 10);
    
    /**
     * @brief 获取连接池配置
     * @param outUsePool 输出是否使用连接池
     * @param outPoolSize 输出连接池大小
     */
    void getConnectionPoolConfig(bool& outUsePool, size_t& outPoolSize) const;
    
    /**
     * @brief 检查连接池健康状态
     * @return 连接池是否健康
     */
    bool checkConnectionPoolHealth() const;
    
private:
    /**
     * @brief 构造函数
     */
    StorageManager();
    
    /**
     * @brief 析构函数
     */
    ~StorageManager();
    
    /**
     * @brief 注册所有内置存储类型
     */
    void registerBuiltinStorageTypes();
    
    std::unordered_map<std::string, std::function<StorageInterfacePtr(const std::string&)>> factories_;
    StorageInterfacePtr defaultStorage_;
    mutable std::mutex mutex_;
    
    bool useCaching_;           ///< 是否使用缓存
    bool useConnectionPool_;    ///< 是否使用连接池
    size_t connectionPoolSize_; ///< 连接池大小
};

} // namespace storage
} // namespace kb

#endif // KB_STORAGE_MANAGER_H 