/**
 * @file storage_factory.h
 * @brief 存储适配器工厂
 */

#ifndef KB_STORAGE_FACTORY_H
#define KB_STORAGE_FACTORY_H

#include "storage/storage_interface.h"
#include "storage/mysql_storage.h"
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include <mutex>

namespace kb {
namespace storage {

/**
 * @brief 存储适配器工厂类
 */
class StorageFactory {
public:
    /**
     * @brief 获取单例实例
     * @return 工厂实例
     */
    static StorageFactory& getInstance();
    
    /**
     * @brief 根据配置创建存储适配器
     * @param type 存储类型
     * @param connectionString 连接字符串
     * @return 存储适配器指针
     */
    StorageInterfacePtr createStorage(
        const std::string& type,
        const std::string& connectionString);
    
    /**
     * @brief 创建MySQL存储适配器
     * @param connectionString 连接字符串
     * @return MySQL存储适配器指针
     */
    StorageInterfacePtr createMySQLStorage(const std::string& connectionString);
    
    /**
     * @brief 注册存储适配器类型
     * @param type 存储类型
     * @param factory 工厂函数
     */
    void registerStorageType(
        const std::string& type,
        std::function<StorageInterfacePtr(const std::string&)> factory);
    
    /**
     * @brief 获取所有已注册的存储类型
     * @return 存储类型列表
     */
    std::vector<std::string> getRegisteredTypes() const;
    
private:
    /**
     * @brief 构造函数
     */
    StorageFactory();
    
    /**
     * @brief 注册内置存储类型
     */
    void registerBuiltinTypes();
    
    std::unordered_map<std::string, std::function<StorageInterfacePtr(const std::string&)>> factories_;
    mutable std::mutex mutex_;
};

} // namespace storage
} // namespace kb

#endif // KB_STORAGE_FACTORY_H 