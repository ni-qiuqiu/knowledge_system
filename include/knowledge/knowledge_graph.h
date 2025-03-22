#ifndef KB_KNOWLEDGE_GRAPH_H
#define KB_KNOWLEDGE_GRAPH_H

#include "knowledge/entity.h"
#include "knowledge/relation.h"
#include "knowledge/triple.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <functional>

namespace kb {
namespace knowledge {

/**
 * @brief 知识图谱类，管理实体、关系和三元组
 */
class KnowledgeGraph {
public:
    /**
     * @brief 构造函数
     * @param name 知识图谱名称
     */
    explicit KnowledgeGraph(const std::string& name = "");

    /**
     * @brief 构造函数
     * @param name 知识图谱名称
     * @param description 知识图谱描述
     */
    explicit KnowledgeGraph(const std::string& name, const std::string& description);
    
    /**
     * @brief 析构函数
     */
    ~KnowledgeGraph() = default;
    
    /**
     * @brief 获取知识图谱名称
     * @return 知识图谱名称
     */
    const std::string& getName() const;
    
    /**
     * @brief 设置知识图谱名称
     * @param name 新名称
     */
    void setName(const std::string& name);
    
    /**
     * @brief 添加实体
     * @param entity 实体
     * @return 是否添加成功
     */
    bool addEntity(EntityPtr entity);
    
    /**
     * @brief 获取实体
     * @param id 实体ID
     * @return 实体，如果不存在返回nullptr
     */
    EntityPtr getEntity(const std::string& id) const;
    
    /**
     * @brief 根据名称查找实体
     * @param name 实体名称
     * @return 匹配的实体列表
     */
    std::vector<EntityPtr> findEntitiesByName(const std::string& name) const;
    
    /**
     * @brief 根据类型查找实体
     * @param type 实体类型
     * @return 匹配的实体列表
     */
    std::vector<EntityPtr> findEntitiesByType(EntityType type) const;
    
    /**
     * @brief 移除实体
     * @param id 实体ID
     * @return 是否移除成功
     */
    bool removeEntity(const std::string& id);
    
    /**
     * @brief 获取所有实体
     * @return 所有实体
     */
    std::vector<EntityPtr> getAllEntities() const;
    
    /**
     * @brief 获取实体数量
     * @return 实体数量
     */
    size_t getEntityCount() const;
    
    /**
     * @brief 添加关系
     * @param relation 关系
     * @return 是否添加成功
     */
    bool addRelation(RelationPtr relation);
    
    /**
     * @brief 获取关系
     * @param id 关系ID
     * @return 关系，如果不存在返回nullptr
     */
    RelationPtr getRelation(const std::string& id) const;
    
    /**
     * @brief 根据名称查找关系
     * @param name 关系名称
     * @return 匹配的关系列表
     */
    std::vector<RelationPtr> findRelationsByName(const std::string& name) const;
    
    /**
     * @brief 根据类型查找关系
     * @param type 关系类型
     * @return 匹配的关系列表
     */
    std::vector<RelationPtr> findRelationsByType(RelationType type) const;
    
    /**
     * @brief 移除关系
     * @param id 关系ID
     * @return 是否移除成功
     */
    bool removeRelation(const std::string& id);
    
    /**
     * @brief 获取所有关系
     * @return 所有关系
     */
    std::vector<RelationPtr> getAllRelations() const;
    
    /**
     * @brief 获取关系数量
     * @return 关系数量
     */
    size_t getRelationCount() const;
    
    /**
     * @brief 添加三元组
     * @param triple 三元组
     * @return 是否添加成功
     */
    bool addTriple(TriplePtr triple);
    
    /**
     * @brief 添加三元组
     * @param subjectId 主体实体ID
     * @param relationId 关系ID
     * @param objectId 客体实体ID
     * @param confidence 置信度
     * @return 添加的三元组，如果添加失败返回nullptr
     */
    TriplePtr addTriple(const std::string& subjectId, 
                       const std::string& relationId, 
                       const std::string& objectId,
                       float confidence = 1.0);
    
    /**
     * @brief 获取三元组
     * @param id 三元组ID
     * @return 三元组，如果不存在返回nullptr
     */
    TriplePtr getTriple(const std::string& id) const;
    
    /**
     * @brief 移除三元组
     * @param id 三元组ID
     * @return 是否移除成功
     */
    bool removeTriple(const std::string& id);
    
    /**
     * @brief 获取所有三元组
     * @return 所有三元组
     */
    TripleSet getAllTriples() const;
    
    /**
     * @brief 获取三元组数量
     * @return 三元组数量
     */
    size_t getTripleCount() const;
    
    /**
     * @brief 查询以指定实体为主体的三元组
     * @param subjectId 主体实体ID
     * @return 匹配的三元组集合
     */
    TripleSet findTriplesBySubject(const std::string& subjectId) const;
    
    /**
     * @brief 查询以指定实体为客体的三元组
     * @param objectId 客体实体ID
     * @return 匹配的三元组集合
     */
    TripleSet findTriplesByObject(const std::string& objectId) const;
    
    /**
     * @brief 查询使用指定关系的三元组
     * @param relationId 关系ID
     * @return 匹配的三元组集合
     */
    TripleSet findTriplesByRelation(const std::string& relationId) const;
    
    /**
     * @brief 查询特定主体和关系的三元组
     * @param subjectId 主体实体ID
     * @param relationId 关系ID
     * @return 匹配的三元组集合
     */
    TripleSet findTriplesBySubjectAndRelation(const std::string& subjectId, 
                                             const std::string& relationId) const;
    
    /**
     * @brief 查询特定关系和客体的三元组
     * @param relationId 关系ID
     * @param objectId 客体实体ID
     * @return 匹配的三元组集合
     */
    TripleSet findTriplesByRelationAndObject(const std::string& relationId, 
                                            const std::string& objectId) const;
    
    /**
     * @brief 查询特定主体和客体的三元组
     * @param subjectId 主体实体ID
     * @param objectId 客体实体ID
     * @return 匹配的三元组集合
     */
    TripleSet findTriplesBySubjectAndObject(const std::string& subjectId, 
                                           const std::string& objectId) const;
    
    /**
     * @brief 合并另一个知识图谱
     * @param other 另一个知识图谱
     * @param conflictStrategy 冲突处理策略，返回true表示使用新值，false表示保留原值
     * @return 合并的实体和三元组数量
     */
    std::pair<size_t, size_t> merge(const KnowledgeGraph& other, 
                                   std::function<bool(const TriplePtr&, const TriplePtr&)> conflictStrategy = nullptr);
    
    /**
     * @brief 从知识图谱中提取子图
     * @param entityIds 子图中包含的实体ID集合
     * @param includeConnected 是否包含相连的实体
     * @return 提取的子图
     */
    std::shared_ptr<KnowledgeGraph> extractSubgraph(const std::unordered_set<std::string>& entityIds, 
                                                  bool includeConnected = false) const;
    
    /**
     * @brief 保存知识图谱到文件
     * @param filepath 文件路径
     * @return 是否保存成功
     */
    bool save(const std::string& filepath) const;
    
    /**
     * @brief 从文件加载知识图谱
     * @param filepath 文件路径
     * @return 是否加载成功
     */
    bool load(const std::string& filepath);
    
    /**
     * @brief 清空知识图谱
     */
    void clear();

private:
    std::string name_;                                      ///< 知识图谱名称
    std::string description_;                               ///< 知识图谱描述
    std::unordered_map<std::string, EntityPtr> entities_;   ///< 实体映射表
    std::unordered_map<std::string, RelationPtr> relations_; ///< 关系映射表
    std::unordered_map<std::string, TriplePtr> triples_;     ///< 三元组映射表
    
    // 索引
    std::unordered_map<std::string, std::unordered_set<std::string>> subjectIndex_;  ///< 主体索引
    std::unordered_map<std::string, std::unordered_set<std::string>> objectIndex_;   ///< 客体索引
    std::unordered_map<std::string, std::unordered_set<std::string>> relationIndex_; ///< 关系索引
    std::unordered_map<EntityType, std::unordered_set<std::string>> entityTypeIndex_; ///< 实体类型索引
    std::unordered_map<RelationType, std::unordered_set<std::string>> relationTypeIndex_; ///< 关系类型索引
    
    /**
     * @brief 添加到索引
     * @param triple 三元组
     */
    void addToIndices(TriplePtr triple);
    
    /**
     * @brief 从索引中移除
     * @param triple 三元组
     */
    void removeFromIndices(TriplePtr triple);
    
    /**
     * @brief 验证三元组
     * @param triple 三元组
     * @return 是否有效
     */
    bool validateTriple(TriplePtr triple) const;
};

// 定义知识图谱的共享指针类型
using KnowledgeGraphPtr = std::shared_ptr<KnowledgeGraph>;

} // namespace knowledge
} // namespace kb

#endif // KB_KNOWLEDGE_GRAPH_H 