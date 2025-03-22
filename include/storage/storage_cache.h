/**
 * @file storage_cache.h
 * @brief 存储缓存管理器
 */

#ifndef KB_STORAGE_CACHE_H
#define KB_STORAGE_CACHE_H

#include "knowledge/entity.h"
#include "knowledge/relation.h"
#include "knowledge/triple.h"
#include "knowledge/knowledge_graph.h"
#include <string>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <memory>
#include <vector>
#include <functional>

namespace kb {
namespace storage {

/**
 * @brief 缓存项的基类
 */
class CacheItem {
public:
    CacheItem() : timestamp_(std::chrono::steady_clock::now()) {}
    virtual ~CacheItem() = default;

    /**
     * @brief 更新时间戳
     */
    void updateTimestamp() {
        timestamp_ = std::chrono::steady_clock::now();
    }

    /**
     * @brief 获取时间戳
     * @return 时间戳
     */
    const std::chrono::steady_clock::time_point& getTimestamp() const {
        return timestamp_;
    }

private:
    std::chrono::steady_clock::time_point timestamp_;  ///< 最后访问时间
};

/**
 * @brief 实体缓存项
 */
class EntityCacheItem : public CacheItem {
public:
    EntityCacheItem(const knowledge::EntityPtr& entity) : entity_(entity) {}

    /**
     * @brief 获取实体
     * @return 实体指针
     */
    knowledge::EntityPtr getEntity() const { return entity_; }

private:
    knowledge::EntityPtr entity_;  ///< 缓存的实体
};

/**
 * @brief 关系缓存项
 */
class RelationCacheItem : public CacheItem {
public:
    RelationCacheItem(const knowledge::RelationPtr& relation) : relation_(relation) {}

    /**
     * @brief 获取关系
     * @return 关系指针
     */
    knowledge::RelationPtr getRelation() const { return relation_; }

private:
    knowledge::RelationPtr relation_;  ///< 缓存的关系
};

/**
 * @brief 三元组缓存项
 */
class TripleCacheItem : public CacheItem {
public:
    TripleCacheItem(const knowledge::TriplePtr& triple) : triple_(triple) {}

    /**
     * @brief 获取三元组
     * @return 三元组指针
     */
    knowledge::TriplePtr getTriple() const { return triple_; }

private:
    knowledge::TriplePtr triple_;  ///< 缓存的三元组
};

/**
 * @brief 知识图谱缓存项
 */
class KnowledgeGraphCacheItem : public CacheItem {
public:
    KnowledgeGraphCacheItem(const knowledge::KnowledgeGraphPtr& graph) : graph_(graph) {}

    /**
     * @brief 获取知识图谱
     * @return 知识图谱指针
     */
    knowledge::KnowledgeGraphPtr getGraph() const { return graph_; }

private:
    knowledge::KnowledgeGraphPtr graph_;  ///< 缓存的知识图谱
};

/**
 * @brief 查询结果缓存项
 */
class QueryCacheItem : public CacheItem {
public:
    /**
     * @brief 实体查询结果缓存项
     * @param entities 实体列表
     */
    QueryCacheItem(const std::vector<knowledge::EntityPtr>& entities) 
        : entities_(entities), relations_(), triples_() {}

    /**
     * @brief 关系查询结果缓存项
     * @param relations 关系列表
     */
    QueryCacheItem(const std::vector<knowledge::RelationPtr>& relations)
        : entities_(), relations_(relations), triples_() {}

    /**
     * @brief 三元组查询结果缓存项
     * @param triples 三元组列表
     */
    QueryCacheItem(const std::vector<knowledge::TriplePtr>& triples)
        : entities_(), relations_(), triples_(triples) {}

    /**
     * @brief 获取实体列表
     * @return 实体列表
     */
    const std::vector<knowledge::EntityPtr>& getEntities() const { return entities_; }

    /**
     * @brief 获取关系列表
     * @return 关系列表
     */
    const std::vector<knowledge::RelationPtr>& getRelations() const { return relations_; }

    /**
     * @brief 获取三元组列表
     * @return 三元组列表
     */
    const std::vector<knowledge::TriplePtr>& getTriples() const { return triples_; }

private:
    std::vector<knowledge::EntityPtr> entities_;    ///< 缓存的实体列表
    std::vector<knowledge::RelationPtr> relations_; ///< 缓存的关系列表
    std::vector<knowledge::TriplePtr> triples_;     ///< 缓存的三元组列表
};

/**
 * @brief 存储缓存管理器
 */
class StorageCache {
public:
    /**
     * @brief 获取单例实例
     * @return 缓存管理器实例
     */
    static StorageCache& getInstance();

    /**
     * @brief 初始化缓存
     * @param maxSize 最大缓存项数量
     * @param ttl 缓存生存时间(毫秒)
     */
    void initialize(size_t maxSize = 1000, std::chrono::milliseconds ttl = std::chrono::minutes(10));

    /**
     * @brief 清空缓存
     */
    void clear();

    /**
     * @brief 获取缓存实体，如果不存在则返回nullptr
     * @param id 实体ID
     * @return 实体指针
     */
    knowledge::EntityPtr getEntity(const std::string& id);

    /**
     * @brief 缓存实体
     * @param entity 实体指针
     */
    void cacheEntity(const knowledge::EntityPtr& entity);

    /**
     * @brief 获取缓存关系，如果不存在则返回nullptr
     * @param id 关系ID
     * @return 关系指针
     */
    knowledge::RelationPtr getRelation(const std::string& id);

    /**
     * @brief 缓存关系
     * @param relation 关系指针
     */
    void cacheRelation(const knowledge::RelationPtr& relation);

    /**
     * @brief 获取缓存三元组，如果不存在则返回nullptr
     * @param id 三元组ID
     * @return 三元组指针
     */
    knowledge::TriplePtr getTriple(const std::string& id);

    /**
     * @brief 缓存三元组
     * @param triple 三元组指针
     */
    void cacheTriple(const knowledge::TriplePtr& triple);

    /**
     * @brief 获取缓存知识图谱，如果不存在则返回nullptr
     * @param name 图谱名称
     * @return 知识图谱指针
     */
    knowledge::KnowledgeGraphPtr getKnowledgeGraph(const std::string& name);

    /**
     * @brief 缓存知识图谱
     * @param graph 知识图谱指针
     */
    void cacheKnowledgeGraph(const knowledge::KnowledgeGraphPtr& graph);

    /**
     * @brief 获取实体查询结果缓存
     * @param queryKey 查询键
     * @return 实体列表，如果不存在则返回空列表
     */
    std::vector<knowledge::EntityPtr> getEntityQueryResult(const std::string& queryKey);

    /**
     * @brief 缓存实体查询结果
     * @param queryKey 查询键
     * @param entities 实体列表
     */
    void cacheEntityQueryResult(const std::string& queryKey, const std::vector<knowledge::EntityPtr>& entities);

    /**
     * @brief 获取关系查询结果缓存
     * @param queryKey 查询键
     * @return 关系列表，如果不存在则返回空列表
     */
    std::vector<knowledge::RelationPtr> getRelationQueryResult(const std::string& queryKey);

    /**
     * @brief 缓存关系查询结果
     * @param queryKey 查询键
     * @param relations 关系列表
     */
    void cacheRelationQueryResult(const std::string& queryKey, const std::vector<knowledge::RelationPtr>& relations);

    /**
     * @brief 获取三元组查询结果缓存
     * @param queryKey 查询键
     * @return 三元组列表，如果不存在则返回空列表
     */
    std::vector<knowledge::TriplePtr> getTripleQueryResult(const std::string& queryKey);

    /**
     * @brief 缓存三元组查询结果
     * @param queryKey 查询键
     * @param triples 三元组列表
     */
    void cacheTripleQueryResult(const std::string& queryKey, const std::vector<knowledge::TriplePtr>& triples);

    /**
     * @brief 从缓存中删除实体
     * @param id 实体ID
     */
    void removeEntity(const std::string& id);

    /**
     * @brief 从缓存中删除关系
     * @param id 关系ID
     */
    void removeRelation(const std::string& id);

    /**
     * @brief 从缓存中删除三元组
     * @param id 三元组ID
     */
    void removeTriple(const std::string& id);

    /**
     * @brief 从缓存中删除知识图谱
     * @param name 图谱名称
     */
    void removeKnowledgeGraph(const std::string& name);

    /**
     * @brief 从缓存中删除查询结果
     * @param queryKey 查询键
     */
    void removeQueryResult(const std::string& queryKey);

    /**
     * @brief 统计缓存使用情况
     * @return 缓存状态信息
     */
    std::string getStats() const;

private:
    /**
     * @brief 构造函数
     */
    StorageCache();

    /**
     * @brief 清理过期缓存项
     */
    void cleanup();

    std::unordered_map<std::string, std::shared_ptr<EntityCacheItem>> entityCache_;        ///< 实体缓存
    std::unordered_map<std::string, std::shared_ptr<RelationCacheItem>> relationCache_;    ///< 关系缓存
    std::unordered_map<std::string, std::shared_ptr<TripleCacheItem>> tripleCache_;        ///< 三元组缓存
    std::unordered_map<std::string, std::shared_ptr<KnowledgeGraphCacheItem>> graphCache_; ///< 知识图谱缓存
    std::unordered_map<std::string, std::shared_ptr<QueryCacheItem>> queryCache_;          ///< 查询结果缓存

    mutable std::mutex mutex_;              ///< 互斥锁
    size_t maxSize_;                        ///< 最大缓存项数量
    std::chrono::milliseconds ttl_;         ///< 缓存生存时间
    std::chrono::steady_clock::time_point lastCleanup_;  ///< 上次清理时间

    // 统计信息
    mutable std::atomic<size_t> hits_;      ///< 缓存命中次数
    mutable std::atomic<size_t> misses_;    ///< 缓存未命中次数
};

} // namespace storage
} // namespace kb

#endif // KB_STORAGE_CACHE_H 