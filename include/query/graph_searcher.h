/**
 * @file graph_searcher.h
 * @brief 图谱搜索器，负责执行知识图谱的搜索操作
 */

#ifndef KB_GRAPH_SEARCHER_H
#define KB_GRAPH_SEARCHER_H

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "knowledge/knowledge_graph.h"
#include "knowledge/triple.h"
#include "query/query_parser.h"

namespace kb {
namespace query {

/**
 * @brief 搜索结果项结构
 */
struct SearchResultItem {
    knowledge::TriplePtr triple;              ///< 检索到的三元组
    float relevanceScore;                     ///< 与查询的相关性分数
    float confidenceScore;                    ///< 信息的可信度
    std::unordered_map<std::string, std::string> metadata; ///< 元数据
    
    // 构造函数
    SearchResultItem() : relevanceScore(0.0f), confidenceScore(0.0f) {}
};

/**
 * @brief 搜索结果结构
 */
struct SearchResult {
    std::vector<SearchResultItem> items;      ///< 搜索结果项列表
    size_t totalMatches;                      ///< 匹配的总数量
    float executionTime;                      ///< 执行时间（毫秒）
    std::string searchStrategy;               ///< 使用的搜索策略
    
    // 构造函数
    SearchResult() : totalMatches(0), executionTime(0.0f) {}
};

/**
 * @brief 搜索参数结构
 */
struct SearchParams {
    size_t maxResults;                        ///< 最大结果数量
    size_t offset;                            ///< 分页偏移量
    float minConfidence;                      ///< 最小可信度阈值
    bool includeProperties;                   ///< 是否包含属性
    bool includeMeta;                         ///< 是否包含元数据
    std::unordered_map<std::string, std::string> filters; ///< 过滤条件
    
    // 构造函数 - 修复初始化顺序
    SearchParams() : maxResults(10), offset(0), minConfidence(0.0f), 
                    includeProperties(true), includeMeta(true) {}
};

/**
 * @brief 搜索策略枚举
 */
enum class SearchStrategy {
    EXACT_MATCH,      ///< 精确匹配
    SEMANTIC_SEARCH,  ///< 语义搜索
    PATH_FINDING,     ///< 路径查找
    HYBRID            ///< 混合策略
};

/**
 * @brief 图谱搜索器类
 */
class GraphSearcher {
public:
    /**
     * @brief 构造函数
     * @param knowledgeGraph 知识图谱指针
     */
    explicit GraphSearcher(std::shared_ptr<knowledge::KnowledgeGraph> knowledgeGraph);
    
    /**
     * @brief 析构函数
     */
    virtual ~GraphSearcher();
    
    /**
     * @brief 执行查询
     * @param parseResult 查询解析结果
     * @param params 搜索参数
     * @return 搜索结果
     */
    virtual SearchResult search(const QueryParseResult& parseResult, 
                              const SearchParams& params = SearchParams());
    
    /**
     * @brief 设置搜索策略
     * @param strategy 搜索策略
     */
    void setSearchStrategy(SearchStrategy strategy);
    
    /**
     * @brief 获取当前搜索策略
     * @return 当前使用的搜索策略
     */
    SearchStrategy getSearchStrategy() const;
    
    /**
     * @brief 根据实体查找相关三元组
     * @param entity 实体
     * @param params 搜索参数
     * @return 搜索结果
     */
    virtual SearchResult searchByEntity(const knowledge::EntityPtr& entity,
                                      const SearchParams& params = SearchParams());
    
    /**
     * @brief 根据关系查找相关三元组
     * @param relation 关系
     * @param params 搜索参数
     * @return 搜索结果
     */
    virtual SearchResult searchByRelation(const knowledge::RelationPtr& relation,
                                        const SearchParams& params = SearchParams());
    
    /**
     * @brief 查找两个实体之间的路径
     * @param sourceEntity 源实体
     * @param targetEntity 目标实体
     * @param maxDepth 最大深度
     * @param params 搜索参数
     * @return 搜索结果
     */
    virtual SearchResult findPath(const knowledge::EntityPtr& sourceEntity,
                                const knowledge::EntityPtr& targetEntity,
                                size_t maxDepth = 3,
                                const SearchParams& params = SearchParams());

    /**
     * @brief 获取搜索策略名称
     * @param strategy 搜索策略
     * @return 策略名称
     */
    std::string getSearchStrategyName(SearchStrategy strategy);
    
    /**
     * @brief 获取查询类型名称
     * @param type 查询类型
     * @return 类型名称
     */
    std::string getQueryTypeName(QueryType type);

protected:
    /**
     * @brief 根据查询意图执行精确匹配
     * @param intent 查询意图
     * @param params 搜索参数
     * @return 搜索结果
     */
    SearchResult exactMatchSearch(const QueryIntent& intent, const SearchParams& params);
    
    /**
     * @brief 根据查询意图执行语义搜索
     * @param intent 查询意图
     * @param params 搜索参数
     * @return 搜索结果
     */
    SearchResult semanticSearch(const QueryIntent& intent, const SearchParams& params);
    
    /**
     * @brief 执行路径查找
     * @param intent 查询意图
     * @param params 搜索参数
     * @return 搜索结果
     */
    SearchResult pathFindingSearch(const QueryIntent& intent, const SearchParams& params);
    
    /**
     * @brief 执行混合策略搜索
     * @param intent 查询意图
     * @param params 搜索参数
     * @return 搜索结果
     */
    SearchResult hybridSearch(const QueryIntent& intent, const SearchParams& params);
    
    /**
     * @brief 计算三元组与查询的相关性
     * @param triple 三元组
     * @param intent 查询意图
     * @return 相关性分数
     */
    float calculateRelevance(const knowledge::TriplePtr& triple, const QueryIntent& intent);

private:
    std::shared_ptr<knowledge::KnowledgeGraph> knowledgeGraph_; ///< 知识图谱
    SearchStrategy currentStrategy_; ///< 当前搜索策略

    /**
     * @brief 搜索实体查询
     * @param intent 查询意图
     * @param params 搜索参数
     * @return 搜索结果
     */
    SearchResult searchEntityQuery(const QueryIntent& intent, const SearchParams& params);
    
    /**
     * @brief 搜索关系查询
     * @param intent 查询意图
     * @param params 搜索参数
     * @return 搜索结果
     */
    SearchResult searchRelationQuery(const QueryIntent& intent, const SearchParams& params);
    
    /**
     * @brief 搜索属性查询
     * @param intent 查询意图
     * @param params 搜索参数
     * @return 搜索结果
     */
    SearchResult searchAttributeQuery(const QueryIntent& intent, const SearchParams& params);
    
    /**
     * @brief 搜索列表查询
     * @param intent 查询意图
     * @param params 搜索参数
     * @return 搜索结果
     */
    SearchResult searchListQuery(const QueryIntent& intent, const SearchParams& params);
    
    /**
     * @brief 搜索事实查询
     * @param intent 查询意图
     * @param params 搜索参数
     * @return 搜索结果
     */
    SearchResult searchFactQuery(const QueryIntent& intent, const SearchParams& params);
    
    /**
     * @brief 搜索通用查询
     * @param intent 查询意图
     * @param params 搜索参数
     * @return 搜索结果
     */
    SearchResult searchGenericQuery(const QueryIntent& intent, const SearchParams& params);
    
    /**
     * @brief 从知识图谱中获取实体
     * @param entityName 实体名称
     * @return 实体指针
     */
    knowledge::EntityPtr getEntityFromKnowledgeGraph(const std::string& entityName);
    
    /**
     * @brief 从知识图谱中获取关系
     * @param relationName 关系名称
     * @return 关系指针
     */
    knowledge::RelationPtr getRelationFromKnowledgeGraph(const std::string& relationName);
    
    /**
     * @brief 查找两个实体之间的路径
     * @param sourceEntity 源实体
     * @param targetEntity 目标实体
     * @param maxDepth 最大深度
     * @param paths 存储找到的路径
     * @return 是否找到路径
     */
    bool findEntityPaths(const knowledge::EntityPtr& sourceEntity, 
                         const knowledge::EntityPtr& targetEntity, 
                         size_t maxDepth, 
                         std::vector<std::vector<knowledge::TriplePtr>>& paths);
    
    /**
     * @brief 应用搜索结果限制（分页和最大数量）
     * @param result 搜索结果
     * @param params 搜索参数
     */
    void applySearchLimits(SearchResult& result, const SearchParams& params);
};

} // namespace query
} // namespace kb

#endif // KB_GRAPH_SEARCHER_H 