/**
 * @file graph_searcher.cpp
 * @brief 图谱搜索器实现
 */

#include "query/graph_searcher.h"
#include "common/logging.h"
#include <chrono>
#include <queue>
#include <algorithm>

namespace kb {
namespace query {

// 构造函数
GraphSearcher::GraphSearcher(std::shared_ptr<knowledge::KnowledgeGraph> knowledgeGraph)
    : knowledgeGraph_(knowledgeGraph), currentStrategy_(SearchStrategy::HYBRID) {
    
    if (!knowledgeGraph_) {
        LOG_ERROR("知识图谱指针为空，图谱搜索器初始化失败");
        throw std::invalid_argument("知识图谱指针不能为空");
    }
    
    LOG_INFO("图谱搜索器初始化完成");
}

// 析构函数
GraphSearcher::~GraphSearcher() {
    LOG_INFO("图谱搜索器释放资源");
}

// 执行查询
SearchResult GraphSearcher::search(const QueryParseResult& parseResult, const SearchParams& params) {
    LOG_INFO("开始执行图谱搜索，查询类型: {}", getQueryTypeName(parseResult.intent.type));
    
    // 记录开始时间
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 根据搜索策略执行搜索
    SearchResult result;
    
    switch (currentStrategy_) {
        case SearchStrategy::EXACT_MATCH:
            result = exactMatchSearch(parseResult.intent, params);
            break;
        case SearchStrategy::SEMANTIC_SEARCH:
            result = semanticSearch(parseResult.intent, params);
            break;
        case SearchStrategy::PATH_FINDING:
            result = pathFindingSearch(parseResult.intent, params);
            break;
        case SearchStrategy::HYBRID:
            result = hybridSearch(parseResult.intent, params);
            break;
        default:
            result = exactMatchSearch(parseResult.intent, params);
            break;
    }
    
    // 记录结束时间并计算执行时间
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    result.executionTime = static_cast<float>(duration.count());
    
    // 根据置信度过滤结果
    if (params.minConfidence > 0.0f) {
        result.items.erase(
            std::remove_if(result.items.begin(), result.items.end(), 
                          [params](const SearchResultItem& item) {
                              return item.confidenceScore < params.minConfidence;
                          }),
            result.items.end()
        );
    }
    
    // 按相关性排序
    std::sort(result.items.begin(), result.items.end(), 
             [](const SearchResultItem& a, const SearchResultItem& b) {
                 return a.relevanceScore > b.relevanceScore;
             });
    
    // 应用分页
    if (params.offset > 0 && params.offset < result.items.size()) {
        result.items.erase(result.items.begin(), result.items.begin() + params.offset);
    }
    
    // 限制结果数量
    if (params.maxResults > 0 && result.items.size() > params.maxResults) {
        result.items.resize(params.maxResults);
    }
    
    // 总匹配数
    result.totalMatches = result.items.size();
    
    // 记录使用的搜索策略
    result.searchStrategy = getSearchStrategyName(currentStrategy_);
    
    LOG_INFO("图谱搜索完成，找到 {} 个匹配结果，耗时 {:.2f} ms", 
              result.items.size(), result.executionTime);
    
    return result;
}

// 设置搜索策略
void GraphSearcher::setSearchStrategy(SearchStrategy strategy) {
    currentStrategy_ = strategy;
    LOG_INFO("设置搜索策略为: {}", getSearchStrategyName(strategy));
}

// 获取当前搜索策略
SearchStrategy GraphSearcher::getSearchStrategy() const {
    return currentStrategy_;
}

// 根据实体查找相关三元组
SearchResult GraphSearcher::searchByEntity(
    const knowledge::EntityPtr& entity,
    const SearchParams& params) {
    
    LOG_INFO("根据实体搜索三元组，实体: {}", entity->getName());
    
    // 记录开始时间
    auto startTime = std::chrono::high_resolution_clock::now();
    
    SearchResult result;
    result.searchStrategy = "entity_search";
    
    // 从知识图谱中获取与实体相关的三元组
    auto triples = knowledgeGraph_->findTriplesBySubject(entity->getId());
    // 同时查找以该实体为客体的三元组
    auto objectTriples = knowledgeGraph_->findTriplesByObject(entity->getId());
    // 合并结果
    triples.insert(triples.end(), objectTriples.begin(), objectTriples.end());
    
    // 转换为搜索结果项
    for (const auto& triple : triples) {
        SearchResultItem item;
        item.triple = triple;
        item.relevanceScore = 1.0f;  // 精确匹配实体，相关性最高
        item.confidenceScore = triple->getConfidence();
        
        // 添加元数据
        if (params.includeMeta) {
            item.metadata["triple_id"] = triple->getId();
            item.metadata["subject"] = triple->getSubject()->getName();
            item.metadata["relation"] = triple->getRelation()->getName();
            item.metadata["object"] = triple->getObject()->getName();
        }
        
        result.items.push_back(item);
    }
    
    // 记录结束时间并计算执行时间
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    result.executionTime = static_cast<float>(duration.count());
    
    // 应用结果限制
    applySearchLimits(result, params);
    
    LOG_INFO("实体搜索完成，找到 {} 个相关三元组", result.items.size());
    
    return result;
}

// 根据关系查找相关三元组
SearchResult GraphSearcher::searchByRelation(
    const knowledge::RelationPtr& relation,
    const SearchParams& params) {
    
    LOG_INFO("根据关系搜索三元组，关系: {}", relation->getName());
    
    // 记录开始时间
    auto startTime = std::chrono::high_resolution_clock::now();
    
    SearchResult result;
    result.searchStrategy = "relation_search";
    
    // 从知识图谱中获取与关系相关的三元组
    auto triples = knowledgeGraph_->findTriplesByRelation(relation->getId());
    
    // 转换为搜索结果项
    for (const auto& triple : triples) {
        SearchResultItem item;
        item.triple = triple;
        item.relevanceScore = 1.0f;  // 精确匹配关系，相关性最高
        item.confidenceScore = triple->getConfidence();
        
        // 添加元数据
        if (params.includeMeta) {
            item.metadata["triple_id"] = triple->getId();
            item.metadata["subject"] = triple->getSubject()->getName();
            item.metadata["relation"] = triple->getRelation()->getName();
            item.metadata["object"] = triple->getObject()->getName();
        }
        
        result.items.push_back(item);
    }
    
    // 记录结束时间并计算执行时间
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    result.executionTime = static_cast<float>(duration.count());
    
    // 应用结果限制
    applySearchLimits(result, params);
    
    LOG_INFO("关系搜索完成，找到 {} 个相关三元组", result.items.size());
    
    return result;
}

// 查找两个实体之间的路径
SearchResult GraphSearcher::findPath(
    const knowledge::EntityPtr& sourceEntity,
    const knowledge::EntityPtr& targetEntity,
    size_t maxDepth,
    const SearchParams& params) {
    
    LOG_INFO("查找实体间路径，源实体: {}，目标实体: {}, 最大深度: {}", 
              sourceEntity->getName(), targetEntity->getName(), maxDepth);
    
    // 记录开始时间
    auto startTime = std::chrono::high_resolution_clock::now();
    
    SearchResult result;
    result.searchStrategy = "path_finding";
    
    // 广度优先搜索查找路径
    std::vector<std::vector<knowledge::TriplePtr>> paths;
    bool pathFound = findEntityPaths(sourceEntity, targetEntity, maxDepth, paths);
    
    if (pathFound) {
        // 转换路径为搜索结果
        for (const auto& path : paths) {
            // 计算路径相关性 (反比于路径长度)
            float pathRelevance = 1.0f / static_cast<float>(path.size());
            
            // 添加路径上的所有三元组
            for (size_t i = 0; i < path.size(); ++i) {
                const auto& triple = path[i];
                
                SearchResultItem item;
                item.triple = triple;
                item.relevanceScore = pathRelevance;
                item.confidenceScore = triple->getConfidence();
                
                // 添加路径元数据
                if (params.includeMeta) {
                    item.metadata["triple_id"] = triple->getId();
                    item.metadata["subject"] = triple->getSubject()->getName();
                    item.metadata["relation"] = triple->getRelation()->getName();
                    item.metadata["object"] = triple->getObject()->getName();
                    item.metadata["path_index"] = std::to_string(i);
                    item.metadata["path_length"] = std::to_string(path.size());
                }
                
                result.items.push_back(item);
            }
        }
    }
    
    // 记录结束时间并计算执行时间
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    result.executionTime = static_cast<float>(duration.count());
    
    // 应用结果限制
    applySearchLimits(result, params);
    
    LOG_INFO("路径查找完成，找到 {} 条路径", paths.size());
    
    return result;
}

// 精确匹配搜索
SearchResult GraphSearcher::exactMatchSearch(
    const QueryIntent& intent,
    const SearchParams& params) {
    
    LOG_INFO("执行精确匹配搜索");
    
    SearchResult result;
    result.searchStrategy = "exact_match";
    
    // 根据查询类型执行不同的搜索
    switch (intent.type) {
        case QueryType::ENTITY:
            return searchEntityQuery(intent, params);
            
        case QueryType::RELATION:
            return searchRelationQuery(intent, params);
            
        case QueryType::ATTRIBUTE:
            return searchAttributeQuery(intent, params);
            
        case QueryType::LIST:
            return searchListQuery(intent, params);
            
        case QueryType::FACT:
            return searchFactQuery(intent, params);
            
        default:
            // 其他类型查询，尝试根据实体和关系进行匹配
            return searchGenericQuery(intent, params);
    }
    
    return result;
}

// 语义搜索
SearchResult GraphSearcher::semanticSearch(
    const QueryIntent& intent,
    const SearchParams& params) {
    
    LOG_INFO("执行语义搜索");
    
    // TODO: 实现语义搜索逻辑，这里先用精确匹配代替
    SearchResult result = exactMatchSearch(intent, params);
    result.searchStrategy = "semantic_search";
    
    return result;
}

// 路径查找搜索
SearchResult GraphSearcher::pathFindingSearch(
    const QueryIntent& intent,
    const SearchParams& params) {
    
    LOG_INFO("执行路径查找搜索");
    
    SearchResult result;
    result.searchStrategy = "path_finding";
    
    // 只有关系查询或比较查询才适合路径查找
    if (intent.type == QueryType::RELATION || intent.type == QueryType::COMPARISON) {
        // 需要至少两个实体
        if (intent.entities.size() >= 2) {
            // 假设前两个实体是需要查找路径的
            auto sourceEntity = getEntityFromKnowledgeGraph(intent.entities[0]);
            auto targetEntity = getEntityFromKnowledgeGraph(intent.entities[1]);
            
            if (sourceEntity && targetEntity) {
                // 查找路径
                return findPath(sourceEntity, targetEntity, 3, params);
            }
        }
    }
    
    // 如果不适合路径查找，回退到精确匹配
    return exactMatchSearch(intent, params);
}

// 混合策略搜索
SearchResult GraphSearcher::hybridSearch(
    const QueryIntent& intent,
    const SearchParams& params) {
    
    LOG_INFO("执行混合策略搜索");
    
    // 根据查询类型选择适当的搜索策略
    switch (intent.type) {
        case QueryType::RELATION:
        case QueryType::COMPARISON:
            // 关系和比较查询尝试路径查找
            return pathFindingSearch(intent, params);
            
        case QueryType::DEFINITION:
        case QueryType::OPEN:
            // 定义和开放查询尝试语义搜索
            return semanticSearch(intent, params);
            
        default:
            // 其他类型默认使用精确匹配
            return exactMatchSearch(intent, params);
    }
}

// 计算三元组与查询的相关性
float GraphSearcher::calculateRelevance(
    const knowledge::TriplePtr& triple,
    const QueryIntent& intent) {
    
    float relevance = 0.0f;
    
    // 主体实体匹配
    for (const auto& entityName : intent.entities) {
        if (triple->getSubject()->getName() == entityName) {
            relevance += 0.4f;
            break;
        }
    }
    
    // 客体实体匹配
    for (const auto& entityName : intent.entities) {
        if (triple->getObject()->getName() == entityName) {
            relevance += 0.3f;
            break;
        }
    }
    
    // 关系匹配
    for (const auto& relationName : intent.relations) {
        if (triple->getRelation()->getName() == relationName) {
            relevance += 0.3f;
            break;
        }
    }
    
    // 属性匹配（简单实现）
    for (const auto& attrName : intent.attributes) {
        if (triple->getRelation()->getName().find(attrName) != std::string::npos) {
            relevance += 0.2f;
            break;
        }
    }
    
    // 归一化到[0,1]范围
    return std::min(1.0f, relevance);
}

// 搜索实体查询
SearchResult GraphSearcher::searchEntityQuery(
    const QueryIntent& intent,
    const SearchParams& params) {
    
    SearchResult result;
    
    // 实体查询需要至少一个实体
    if (!intent.entities.empty()) {
        // 获取第一个实体
        auto entity = getEntityFromKnowledgeGraph(intent.entities[0]);
        
        if (entity) {
            // 搜索与该实体相关的三元组
            result = searchByEntity(entity, params);
        }
    }
    
    return result;
}

// 搜索关系查询
SearchResult GraphSearcher::searchRelationQuery(
    const QueryIntent& intent,
    const SearchParams& params) {
    
    SearchResult result;
    
    // 关系查询需要至少两个实体
    if (intent.entities.size() >= 2) {
        auto entity1 = getEntityFromKnowledgeGraph(intent.entities[0]);
        auto entity2 = getEntityFromKnowledgeGraph(intent.entities[1]);
        
        if (entity1 && entity2) {
            // 查找两个实体之间的路径
            result = findPath(entity1, entity2, 2, params);
        }
    }
    
    return result;
}

// 搜索属性查询
SearchResult GraphSearcher::searchAttributeQuery(
    const QueryIntent& intent,
    const SearchParams& params) {
    
    SearchResult result;
    
    // 属性查询需要至少一个实体和一个属性
    if (!intent.entities.empty() && !intent.attributes.empty()) {
        auto entity = getEntityFromKnowledgeGraph(intent.entities[0]);
        
        if (entity) {
            // 获取实体的所有三元组
            auto entityResult = searchByEntity(entity, params);
            
            // 过滤与属性相关的三元组
            for (const auto& item : entityResult.items) {
                const auto& triple = item.triple;
                const auto& relation = triple->getRelation();
                
                // 判断关系是否与查询的属性相关
                for (const auto& attr : intent.attributes) {
                    if (relation->getName().find(attr) != std::string::npos ||
                        relation->getType() == knowledge::RelationType::HAS_PROPERTY) {
                        
                        // 提高相关性分数
                        SearchResultItem newItem = item;
                        newItem.relevanceScore = 0.9f;
                        result.items.push_back(newItem);
                        break;
                    }
                }
            }
        }
    }
    
    return result;
}

// 搜索列表查询
SearchResult GraphSearcher::searchListQuery(
    const QueryIntent& intent,
    const SearchParams& params) {
    
    SearchResult result;
    
    // 列表查询需要至少一个实体和一个列表类型
    if (!intent.entities.empty() && intent.params.find("list_type") != intent.params.end()) {
        auto entity = getEntityFromKnowledgeGraph(intent.entities[0]);
        std::string listType = intent.params.at("list_type");
        
        if (entity) {
            // 获取实体的所有三元组
            auto entityResult = searchByEntity(entity, params);
            
            // 过滤与列表类型相关的三元组
            for (const auto& item : entityResult.items) {
                const auto& triple = item.triple;
                const auto& relation = triple->getRelation();
                
                // 判断关系是否与列表类型相关
                if (relation->getName().find(listType) != std::string::npos) {
                    SearchResultItem newItem = item;
                    newItem.relevanceScore = 0.9f;
                    result.items.push_back(newItem);
                }
            }
        }
    }
    
    return result;
}

// 搜索事实查询
SearchResult GraphSearcher::searchFactQuery(
    const QueryIntent& intent,
    const SearchParams& params) {
    
    SearchResult result;
    
    // 事实查询需要至少两个实体
    if (intent.entities.size() >= 2) {
        auto entity1 = getEntityFromKnowledgeGraph(intent.entities[0]);
        auto entity2 = getEntityFromKnowledgeGraph(intent.entities[1]);
        
        if (entity1 && entity2) {
            // 获取第一个实体的所有三元组
            auto entityResult = searchByEntity(entity1, params);
            
            // 过滤与第二个实体相关的三元组
            for (const auto& item : entityResult.items) {
                const auto& triple = item.triple;
                if (triple->getObject()->getId() == entity2->getId()) {
                    SearchResultItem newItem = item;
                    newItem.relevanceScore = 1.0f;  // 完全匹配
                    result.items.push_back(newItem);
                }
            }
        }
    }
    
    return result;
}

// 搜索通用查询
SearchResult GraphSearcher::searchGenericQuery(
    const QueryIntent& intent,
    const SearchParams& params) {
    
    SearchResult result;
    
    // 合并所有实体的搜索结果
    for (const auto& entityName : intent.entities) {
        auto entity = getEntityFromKnowledgeGraph(entityName);
        
        if (entity) {
            auto entityResult = searchByEntity(entity, params);
            
            // 计算相关性并添加到结果
            for (auto& item : entityResult.items) {
                item.relevanceScore = calculateRelevance(item.triple, intent);
                result.items.push_back(item);
            }
        }
    }
    
    // 合并所有关系的搜索结果
    for (const auto& relationName : intent.relations) {
        auto relation = getRelationFromKnowledgeGraph(relationName);
        
        if (relation) {
            auto relationResult = searchByRelation(relation, params);
            
            // 计算相关性并添加到结果
            for (auto& item : relationResult.items) {
                item.relevanceScore = calculateRelevance(item.triple, intent);
                result.items.push_back(item);
            }
        }
    }
    
    return result;
}

// 从知识图谱获取实体
knowledge::EntityPtr GraphSearcher::getEntityFromKnowledgeGraph(const std::string& entityName) {
    auto entities = knowledgeGraph_->findEntitiesByName(entityName);
    
    if (!entities.empty()) {
        return entities[0];
    }
    
    return nullptr;
}

// 从知识图谱获取关系
knowledge::RelationPtr GraphSearcher::getRelationFromKnowledgeGraph(const std::string& relationName) {
    auto relations = knowledgeGraph_->findRelationsByName(relationName);
    
    if (!relations.empty()) {
        return relations[0];
    }
    
    return nullptr;
}

// 查找实体间路径（BFS实现）
bool GraphSearcher::findEntityPaths(
    const knowledge::EntityPtr& sourceEntity,
    const knowledge::EntityPtr& targetEntity,
    size_t maxDepth,
    std::vector<std::vector<knowledge::TriplePtr>>& paths) {
    
    // 如果源实体和目标实体相同，返回空路径
    if (sourceEntity->getId() == targetEntity->getId()) {
        paths.push_back({});
        return true;
    }
    
    // 初始化BFS队列和访问记录
    std::queue<std::vector<knowledge::TriplePtr>> queue;
    std::unordered_set<std::string> visitedEntities;
    
    // 标记源实体为已访问
    visitedEntities.insert(sourceEntity->getId());
    
    // 获取源实体的所有三元组
    auto sourceTriplesSubject = knowledgeGraph_->findTriplesBySubject(sourceEntity->getId());
    auto sourceTriplesObject = knowledgeGraph_->findTriplesByObject(sourceEntity->getId());
    
    // 将源实体作为主体的三元组加入队列
    for (const auto& triple : sourceTriplesSubject) {
        std::vector<knowledge::TriplePtr> path;
        path.push_back(triple);
        queue.push(path);
    }
    
    // 将源实体作为客体的三元组加入队列（反向）
    for (const auto& triple : sourceTriplesObject) {
        std::vector<knowledge::TriplePtr> path;
        path.push_back(triple);
        queue.push(path);
    }
    
    // BFS搜索
    while (!queue.empty()) {
        auto currentPath = queue.front();
        queue.pop();
        
        // 获取当前路径的末尾三元组
        auto lastTriple = currentPath.back();
        
        // 获取下一个要访问的实体
        knowledge::EntityPtr nextEntity;
        
        // 判断当前实体是主体还是客体
        if (lastTriple->getSubject()->getId() == sourceEntity->getId() ||
            visitedEntities.find(lastTriple->getSubject()->getId()) != visitedEntities.end()) {
            nextEntity = lastTriple->getObject();
        } else {
            nextEntity = lastTriple->getSubject();
        }
        
        // 如果找到目标实体，将路径添加到结果
        if (nextEntity->getId() == targetEntity->getId()) {
            paths.push_back(currentPath);
            continue;
        }
        
        // 如果路径长度已达最大深度，不再扩展
        if (currentPath.size() >= maxDepth) {
            continue;
        }
        
        // 标记下一个实体为已访问
        visitedEntities.insert(nextEntity->getId());
        
        // 获取下一个实体的所有三元组
        auto nextTriplesSubject = knowledgeGraph_->findTriplesBySubject(nextEntity->getId());
        auto nextTriplesObject = knowledgeGraph_->findTriplesByObject(nextEntity->getId());
        
        // 将下一个实体作为主体的三元组加入队列
        for (const auto& triple : nextTriplesSubject) {
            // 避免环路
            if (visitedEntities.find(triple->getObject()->getId()) == visitedEntities.end()) {
                std::vector<knowledge::TriplePtr> newPath = currentPath;
                newPath.push_back(triple);
                queue.push(newPath);
            }
        }
        
        // 将下一个实体作为客体的三元组加入队列（反向）
        for (const auto& triple : nextTriplesObject) {
            // 避免环路
            if (visitedEntities.find(triple->getSubject()->getId()) == visitedEntities.end()) {
                std::vector<knowledge::TriplePtr> newPath = currentPath;
                newPath.push_back(triple);
                queue.push(newPath);
            }
        }
    }
    
    return !paths.empty();
}

// 应用搜索限制（分页和数量限制）
void GraphSearcher::applySearchLimits(SearchResult& result, const SearchParams& params) {
    // 应用分页
    if (params.offset > 0 && params.offset < result.items.size()) {
        result.items.erase(result.items.begin(), result.items.begin() + params.offset);
    }
    
    // 限制结果数量
    if (params.maxResults > 0 && result.items.size() > params.maxResults) {
        result.items.resize(params.maxResults);
    }
    
    result.totalMatches = result.items.size();
}

// 获取搜索策略名称
std::string GraphSearcher::getSearchStrategyName(SearchStrategy strategy) {
    switch (strategy) {
        case SearchStrategy::EXACT_MATCH: return "精确匹配";
        case SearchStrategy::SEMANTIC_SEARCH: return "语义搜索";
        case SearchStrategy::PATH_FINDING: return "路径查找";
        case SearchStrategy::HYBRID: return "混合策略";
        default: return "未知策略";
    }
}

// 获取查询类型名称
std::string GraphSearcher::getQueryTypeName(QueryType type) {
    switch (type) {
        case QueryType::FACT: return "事实查询";
        case QueryType::ENTITY: return "实体查询";
        case QueryType::RELATION: return "关系查询";
        case QueryType::ATTRIBUTE: return "属性查询";
        case QueryType::LIST: return "列表查询";
        case QueryType::COMPARISON: return "比较查询";
        case QueryType::DEFINITION: return "定义查询";
        case QueryType::OPEN: return "开放查询";
        case QueryType::UNKNOWN: return "未知查询";
        default: return "未定义查询";
    }
}

} // namespace query
} // namespace kb 