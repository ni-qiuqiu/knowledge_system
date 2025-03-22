/**
 * @file version_control.h
 * @brief 版本控制系统
 */

#ifndef KB_VERSION_CONTROL_H
#define KB_VERSION_CONTROL_H

#include "storage/storage_interface.h"
#include "knowledge/knowledge_graph.h"
#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <map>
#include <mutex>

namespace kb {
namespace storage {

/**
 * @brief 版本信息
 */
struct VersionInfo {
    std::string id;                                  ///< 版本ID
    std::string graphName;                           ///< 知识图谱名称
    std::string description;                         ///< 版本描述
    std::chrono::system_clock::time_point timestamp; ///< 版本时间戳
    std::string author;                              ///< 作者
    std::vector<std::string> addedEntities;          ///< 添加的实体ID
    std::vector<std::string> modifiedEntities;       ///< 修改的实体ID
    std::vector<std::string> deletedEntities;        ///< 删除的实体ID
    std::vector<std::string> addedRelations;         ///< 添加的关系ID
    std::vector<std::string> modifiedRelations;      ///< 修改的关系ID
    std::vector<std::string> deletedRelations;       ///< 删除的关系ID
    std::vector<std::string> addedTriples;           ///< 添加的三元组ID
    std::vector<std::string> modifiedTriples;        ///< 修改的三元组ID
    std::vector<std::string> deletedTriples;         ///< 删除的三元组ID
    
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
 * @brief 版本控制系统
 */
class VersionControl {
public:
    /**
     * @brief 获取单例实例
     * @return 版本控制系统实例
     */
    static VersionControl& getInstance();
    
    /**
     * @brief 初始化版本控制系统
     * @param storage 存储适配器
     * @param versionDir 版本目录
     * @return 是否成功
     */
    bool initialize(StorageInterfacePtr storage, const std::string& versionDir);
    
    /**
     * @brief 创建版本
     * @param graph 知识图谱
     * @param description 版本描述
     * @param author 作者
     * @return 版本ID
     */
    std::string createVersion(
        const knowledge::KnowledgeGraphPtr& graph,
        const std::string& description,
        const std::string& author);
    
    /**
     * @brief 获取版本列表
     * @param graphName 知识图谱名称
     * @return 版本信息列表
     */
    std::vector<VersionInfo> getVersions(const std::string& graphName) const;
    
    /**
     * @brief 获取版本详情
     * @param versionId 版本ID
     * @return 版本信息
     */
    VersionInfo getVersionDetails(const std::string& versionId) const;
    
    /**
     * @brief 回滚到指定版本
     * @param versionId 版本ID
     * @return 是否成功
     */
    bool rollbackToVersion(const std::string& versionId);
    
    /**
     * @brief 比较两个版本
     * @param versionId1 版本ID1
     * @param versionId2 版本ID2
     * @return 差异信息JSON字符串
     */
    std::string compareVersions(const std::string& versionId1, const std::string& versionId2) const;
    
    /**
     * @brief 导出版本
     * @param versionId 版本ID
     * @param exportPath 导出路径
     * @param format 导出格式
     * @return 是否成功
     */
    bool exportVersion(
        const std::string& versionId,
        const std::string& exportPath,
        const std::string& format = "json-ld") const;
    
    /**
     * @brief 获取版本图谱
     * @param versionId 版本ID
     * @return 知识图谱
     */
    knowledge::KnowledgeGraphPtr getVersionGraph(const std::string& versionId) const;
    
    /**
     * @brief 计算图谱差异
     * @param baseGraph 基础图谱
     * @param newGraph 新图谱
     * @return 版本信息
     */
    VersionInfo calculateDiff(
        const knowledge::KnowledgeGraphPtr& baseGraph,
        const knowledge::KnowledgeGraphPtr& newGraph) const;
    
    /**
     * @brief 设置版本目录
     * @param versionDir 版本目录
     * @return 是否成功
     */
    bool setVersionDirectory(const std::string& versionDir);
    
    /**
     * @brief 获取版本目录
     * @return 版本目录
     */
    std::string getVersionDirectory() const;
    
private:
    /**
     * @brief 构造函数
     */
    VersionControl();
    
    /**
     * @brief 加载版本元数据
     * @return 是否成功
     */
    bool loadVersionMetadata();
    
    /**
     * @brief 保存版本元数据
     * @return 是否成功
     */
    bool saveVersionMetadata() const;
    
    /**
     * @brief 生成版本ID
     * @param graphName 知识图谱名称
     * @return 版本ID
     */
    std::string generateVersionId(const std::string& graphName) const;
    
    /**
     * @brief 创建版本目录
     * @param versionId 版本ID
     * @return 版本路径
     */
    std::string createVersionDirectory(const std::string& versionId) const;
    
    /**
     * @brief 保存知识图谱版本
     * @param versionId 版本ID
     * @param graph 知识图谱
     * @return 是否成功
     */
    bool saveKnowledgeGraphVersion(
        const std::string& versionId,
        const knowledge::KnowledgeGraphPtr& graph) const;
    
    /**
     * @brief 加载知识图谱版本
     * @param versionId 版本ID
     * @return 知识图谱
     */
    knowledge::KnowledgeGraphPtr loadKnowledgeGraphVersion(const std::string& versionId) const;
    
    StorageInterfacePtr storage_;          ///< 存储适配器
    std::string versionDir_;               ///< 版本目录
    std::map<std::string, VersionInfo> versions_;  ///< 版本信息映射
    std::map<std::string, std::vector<std::string>> graphVersions_;  ///< 图谱版本映射
    mutable std::mutex mutex_;             ///< 互斥锁
    bool initialized_;                     ///< 是否已初始化
};

} // namespace storage
} // namespace kb

#endif // KB_VERSION_CONTROL_H 