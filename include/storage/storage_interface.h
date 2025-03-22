#ifndef KB_STORAGE_INTERFACE_H
#define KB_STORAGE_INTERFACE_H

#include "knowledge/entity.h"
#include "knowledge/relation.h"
#include "knowledge/triple.h"
#include "knowledge/knowledge_graph.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace kb {
namespace storage {

/**
 * @brief 存储接口类，定义知识库存储的基本接口
 */
class StorageInterface {
public:
    /**
     * @brief 默认构造函数
     */
    StorageInterface() = default;
    
    /**
     * @brief 虚析构函数
     */
    virtual ~StorageInterface() = default;
    
    /**
     * @brief 初始化存储连接
     * @param connectionString 连接字符串
     * @return 是否成功
     */
    virtual bool initialize(const std::string& connectionString) = 0;
    
    /**
     * @brief 关闭存储连接
     * @return 是否成功
     */
    virtual bool close() = 0;
    
    /**
     * @brief 检查连接是否有效
     * @return 是否有效
     */
    virtual bool isConnected() const = 0;
    
    /**
     * @brief 创建或更新数据库架构
     * @param force 是否强制重建（删除现有数据）
     * @return 是否成功
     */
    virtual bool createSchema(bool force = false) = 0;
    
    /**
     * @brief 保存实体
     * @param entity 实体对象
     * @return 是否成功
     */
    virtual bool saveEntity(const knowledge::EntityPtr& entity) = 0;
    
    /**
     * @brief 批量保存实体
     * @param entities 实体列表
     * @return 成功保存的实体数
     */
    virtual size_t saveEntities(const std::vector<knowledge::EntityPtr>& entities) = 0;
    
    /**
     * @brief 加载实体
     * @param id 实体ID
     * @return 实体对象，失败返回nullptr
     */
    virtual knowledge::EntityPtr loadEntity(const std::string& id) = 0;
    
    /**
     * @brief 查询实体
     * @param name 实体名称（可选）
     * @param type 实体类型（可选）
     * @param properties 实体属性（可选）
     * @param limit 返回数量限制（0表示不限制）
     * @return 实体列表
     */
    virtual std::vector<knowledge::EntityPtr> queryEntities(
        const std::string& name = "",
        knowledge::EntityType type = knowledge::EntityType::UNKNOWN,
        const std::unordered_map<std::string, std::string>& properties = {},
        size_t limit = 0) = 0;
    
    /**
     * @brief 删除实体
     * @param id 实体ID
     * @return 是否成功
     */
    virtual bool deleteEntity(const std::string& id) = 0;
    
    /**
     * @brief 保存关系
     * @param relation 关系对象
     * @return 是否成功
     */
    virtual bool saveRelation(const knowledge::RelationPtr& relation) = 0;
    
    /**
     * @brief 批量保存关系
     * @param relations 关系列表
     * @return 成功保存的关系数
     */
    virtual size_t saveRelations(const std::vector<knowledge::RelationPtr>& relations) = 0;
    
    /**
     * @brief 加载关系
     * @param id 关系ID
     * @return 关系对象，失败返回nullptr
     */
    virtual knowledge::RelationPtr loadRelation(const std::string& id) = 0;
    
    /**
     * @brief 查询关系
     * @param name 关系名称（可选）
     * @param type 关系类型（可选）
     * @param properties 关系属性（可选）
     * @param limit 返回数量限制（0表示不限制）
     * @return 关系列表
     */
    virtual std::vector<knowledge::RelationPtr> queryRelations(
        const std::string& name = "",
        knowledge::RelationType type = knowledge::RelationType::UNKNOWN,
        const std::unordered_map<std::string, std::string>& properties = {},
        size_t limit = 0) = 0;
    
    /**
     * @brief 删除关系
     * @param id 关系ID
     * @return 是否成功
     */
    virtual bool deleteRelation(const std::string& id) = 0;
    
    /**
     * @brief 保存三元组
     * @param triple 三元组对象
     * @return 是否成功
     */
    virtual bool saveTriple(const knowledge::TriplePtr& triple) = 0;
    
    /**
     * @brief 批量保存三元组
     * @param triples 三元组列表
     * @return 成功保存的三元组数
     */
    virtual size_t saveTriples(const std::vector<knowledge::TriplePtr>& triples) = 0;
    
    /**
     * @brief 加载三元组
     * @param id 三元组ID
     * @return 三元组对象，失败返回nullptr
     */
    virtual knowledge::TriplePtr loadTriple(const std::string& id) = 0;
    
    /**
     * @brief 查询三元组
     * @param subjectId 主体ID（可选）
     * @param relationId 关系ID（可选）
     * @param objectId 客体ID（可选）
     * @param properties 三元组属性（可选）
     * @param minConfidence 最小置信度（可选）
     * @param limit 返回数量限制（0表示不限制）
     * @return 三元组列表
     */
    virtual std::vector<knowledge::TriplePtr> queryTriples(
        const std::string& subjectId = "",
        const std::string& relationId = "",
        const std::string& objectId = "",
        const std::unordered_map<std::string, std::string>& properties = {},
        float minConfidence = 0.0f,
        size_t limit = 0) = 0;
    
    /**
     * @brief 删除三元组
     * @param id 三元组ID
     * @return 是否成功
     */
    virtual bool deleteTriple(const std::string& id) = 0;
    
    /**
     * @brief 保存知识图谱
     * @param graph 知识图谱对象
     * @param saveName 保存名称（可选）
     * @return 是否成功
     */
    virtual bool saveKnowledgeGraph(
        const knowledge::KnowledgeGraphPtr& graph,
        const std::string& saveName = "") = 0;
    
    /**
     * @brief 加载知识图谱
     * @param graphName 知识图谱名称
     * @return 知识图谱对象，失败返回nullptr
     */
    virtual knowledge::KnowledgeGraphPtr loadKnowledgeGraph(
        const std::string& graphName) = 0;
    
    /**
     * @brief 删除知识图谱
     * @param graphName 知识图谱名称
     * @return 是否成功
     */
    virtual bool deleteKnowledgeGraph(const std::string& graphName) = 0;
    
    /**
     * @brief 列出所有知识图谱
     * @return 知识图谱名称列表
     */
    virtual std::vector<std::string> listKnowledgeGraphs() = 0;
    
    /**
     * @brief 执行自定义查询
     * @param query 查询语句
     * @param params 查询参数
     * @return 查询结果（JSON格式）
     */
    virtual std::string executeQuery(
        const std::string& query,
        const std::unordered_map<std::string, std::string>& params = {}) = 0;
    
    /**
     * @brief 开始事务
     * @return 是否成功
     */
    virtual bool beginTransaction() = 0;
    
    /**
     * @brief 提交事务
     * @return 是否成功
     */
    virtual bool commitTransaction() = 0;
    
    /**
     * @brief 回滚事务
     * @return 是否成功
     */
    virtual bool rollbackTransaction() = 0;
    
    /**
     * @brief 获取存储类型名称
     * @return 存储类型名称
     */
    virtual std::string getStorageType() const = 0;
    
    /**
     * @brief 获取存储连接信息
     * @return 连接信息（脱敏处理）
     */
    virtual std::string getConnectionInfo() const = 0;
    
    /**
     * @brief 获取最后一次错误消息
     * @return 错误消息
     */
    virtual std::string getLastError() const = 0;
};

// 定义存储接口的共享指针类型
using StorageInterfacePtr = std::shared_ptr<StorageInterface>;

} // namespace storage
} // namespace kb

#endif // KB_STORAGE_INTERFACE_H 