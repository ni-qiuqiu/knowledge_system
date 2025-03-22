/**
 * @file backup_manager.h
 * @brief 备份管理器
 */

#ifndef KB_BACKUP_MANAGER_H
#define KB_BACKUP_MANAGER_H

#include "storage/storage_interface.h"
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <mutex>
#include <functional>

namespace kb {
namespace storage {

/**
 * @brief 备份类型
 */
enum class BackupType {
    FULL,       ///< 全量备份
    INCREMENTAL ///< 增量备份
};

/**
 * @brief 备份状态
 */
enum class BackupStatus {
    PENDING,    ///< 等待中
    RUNNING,    ///< 运行中
    COMPLETED,  ///< 已完成
    FAILED      ///< 失败
};

/**
 * @brief 备份元数据
 */
struct BackupMetadata {
    std::string id;              ///< 备份ID
    std::string name;            ///< 备份名称
    std::string description;     ///< 备份描述
    std::string path;            ///< 备份路径
    BackupType type;             ///< 备份类型
    std::string baseBackupId;    ///< 基础备份ID（增量备份时使用）
    std::chrono::system_clock::time_point timestamp;  ///< 备份时间戳
    size_t sizeBytes;            ///< 备份大小（字节）
    BackupStatus status;         ///< 备份状态
    std::string errorMessage;    ///< 错误信息（如果有）
    
    /**
     * @brief 转换为JSON字符串
     * @return JSON字符串
     */
    std::string toJson() const;
    
    /**
     * @brief 从JSON字符串解析
     * @param json JSON字符串
     * @return 是否成功
     */
    bool fromJson(const std::string& json);
};

/**
 * @brief 备份进度回调
 */
using BackupProgressCallback = std::function<void(float progress, const std::string& message)>;

/**
 * @brief 备份管理器
 */
class BackupManager {
public:
    /**
     * @brief 获取单例实例
     * @return 备份管理器实例
     */
    static BackupManager& getInstance();
    
    /**
     * @brief 初始化备份管理器
     * @param backupDir 备份目录
     * @return 是否成功
     */
    bool initialize(const std::string& backupDir);
    
    /**
     * @brief 创建全量备份
     * @param storage 存储适配器
     * @param name 备份名称
     * @param description 备份描述
     * @param callback 进度回调
     * @return 备份ID，失败返回空字符串
     */
    std::string createFullBackup(
        StorageInterfacePtr storage,
        const std::string& name,
        const std::string& description = "",
        BackupProgressCallback callback = nullptr);
    
    /**
     * @brief 创建增量备份
     * @param storage 存储适配器
     * @param baseBackupId 基础备份ID
     * @param name 备份名称
     * @param description 备份描述
     * @param callback 进度回调
     * @return 备份ID，失败返回空字符串
     */
    std::string createIncrementalBackup(
        StorageInterfacePtr storage,
        const std::string& baseBackupId,
        const std::string& name,
        const std::string& description = "",
        BackupProgressCallback callback = nullptr);
    
    /**
     * @brief 从备份恢复
     * @param storage 存储适配器
     * @param backupId 备份ID
     * @param callback 进度回调
     * @return 是否成功
     */
    bool restoreFromBackup(
        StorageInterfacePtr storage,
        const std::string& backupId,
        BackupProgressCallback callback = nullptr);
    
    /**
     * @brief 获取备份列表
     * @return 备份元数据列表
     */
    std::vector<BackupMetadata> getBackupList() const;
    
    /**
     * @brief 获取备份详情
     * @param backupId 备份ID
     * @return 备份元数据
     */
    BackupMetadata getBackupDetails(const std::string& backupId) const;
    
    /**
     * @brief 删除备份
     * @param backupId 备份ID
     * @return 是否成功
     */
    bool deleteBackup(const std::string& backupId);
    
    /**
     * @brief 清理过期备份
     * @param maxBackups 保留的最大备份数
     * @param maxAgeInDays 保留的最大备份天数
     * @return 删除的备份数量
     */
    size_t cleanupOldBackups(size_t maxBackups = 10, int maxAgeInDays = 30);
    
    /**
     * @brief 验证备份
     * @param backupId 备份ID
     * @return 是否有效
     */
    bool validateBackup(const std::string& backupId) const;
    
    /**
     * @brief  设置备份目录
     * @param backupDir 备份目录
     * @return 是否成功
     */
    bool setBackupDirectory(const std::string& backupDir);
    
    /**
     * @brief 获取备份目录
     * @return 备份目录
     */
    std::string getBackupDirectory() const;
    
private:
    /**
     * @brief 构造函数
     */
    BackupManager();
    
    /**
     * @brief 加载备份元数据
     * @return 是否成功
     */
    bool loadBackupMetadata();
    
    /**
     * @brief 保存备份元数据
     * @return 是否成功
     */
    bool saveBackupMetadata() const;
    
    /**
     * @brief 生成备份ID
     * @return 备份ID
     */
    std::string generateBackupId() const;
    
    /**
     * @brief 创建备份目录
     * @param backupId 备份ID
     * @return 备份路径
     */
    std::string createBackupDirectory(const std::string& backupId) const;
    
    /**
     * @brief 内部创建全量备份
     * @param storage 存储适配器
     * @param backupId 备份ID
     * @param metadata 备份元数据
     * @param callback 进度回调
     * @return 是否成功
     */
    bool createFullBackupInternal(
        StorageInterfacePtr storage,
        const std::string& backupId,
        BackupMetadata& metadata,
        BackupProgressCallback callback);
    
    /**
     * @brief 内部创建增量备份
     * @param storage 存储适配器
     * @param backupId 备份ID
     * @param baseBackupId 基础备份ID
     * @param metadata 备份元数据
     * @param callback 进度回调
     * @return 是否成功
     */
    bool createIncrementalBackupInternal(
        StorageInterfacePtr storage,
        const std::string& backupId,
        const std::string& baseBackupId,
        BackupMetadata& metadata,
        BackupProgressCallback callback);
    
    /**
     * @brief 内部恢复备份
     * @param storage 存储适配器
     * @param backupId 备份ID
     * @param callback 进度回调
     * @return 是否成功
     */
    bool restoreBackupInternal(
        StorageInterfacePtr storage,
        const std::string& backupId,
        BackupProgressCallback callback);
    
    std::string backupDir_;  ///< 备份目录
    std::vector<BackupMetadata> backups_;  ///< 备份列表
    mutable std::mutex mutex_;  ///< 互斥锁
    bool initialized_;  ///< 是否已初始化
};

} // namespace storage
} // namespace kb

#endif // KB_BACKUP_MANAGER_H 