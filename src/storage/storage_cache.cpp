/**
 * @file storage_cache.cpp
 * @brief 存储缓存管理器实现
 */

#include "storage/storage_cache.h"
#include "common/logging.h"
#include <sstream>
#include <chrono>
#include <algorithm>

namespace kb {
namespace storage {

StorageCache& StorageCache::getInstance() {
    static StorageCache instance;
    return instance;
}

StorageCache::StorageCache()
    : maxSize_(1000),
      ttl_(std::chrono::minutes(10)),
      lastCleanup_(std::chrono::steady_clock::now()),
      hits_(0),
      misses_(0) {
}

void StorageCache::initialize(size_t maxSize, std::chrono::milliseconds ttl) {
    std::lock_guard<std::mutex> lock(mutex_);
    maxSize_ = maxSize;
    ttl_ = ttl;
    
    // 清空现有缓存
    clear();
    
    LOG_INFO("存储缓存初始化完成，最大项数: {}，TTL: {}ms", maxSize_, ttl_.count());
}

void StorageCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    entityCache_.clear();
    relationCache_.clear();
    tripleCache_.clear();
    graphCache_.clear();
    queryCache_.clear();
    
    hits_ = 0;
    misses_ = 0;
    
    LOG_INFO("存储缓存已清空");
}

knowledge::EntityPtr StorageCache::getEntity(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = entityCache_.find(id);
    if (it != entityCache_.end()) {
        it->second->updateTimestamp();
        hits_++;
        return it->second->getEntity();
    }
    
    misses_++;
    return nullptr;
}

void StorageCache::cacheEntity(const knowledge::EntityPtr& entity) {
    if (!entity) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 如果缓存已满，清理过期项
    if (entityCache_.size() + relationCache_.size() + tripleCache_.size() + 
        graphCache_.size() + queryCache_.size() >= maxSize_) {
        cleanup();
    }
    
    // 添加或更新缓存项
    entityCache_[entity->getId()] = std::make_shared<EntityCacheItem>(entity);
    
    // 每100次操作检查一次是否需要清理
    auto now = std::chrono::steady_clock::now();
    if (now - lastCleanup_ > std::chrono::minutes(5)) {
        cleanup();
        lastCleanup_ = now;
    }
}

knowledge::RelationPtr StorageCache::getRelation(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = relationCache_.find(id);
    if (it != relationCache_.end()) {
        it->second->updateTimestamp();
        hits_++;
        return it->second->getRelation();
    }
    
    misses_++;
    return nullptr;
}

void StorageCache::cacheRelation(const knowledge::RelationPtr& relation) {
    if (!relation) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 如果缓存已满，清理过期项
    if (entityCache_.size() + relationCache_.size() + tripleCache_.size() + 
        graphCache_.size() + queryCache_.size() >= maxSize_) {
        cleanup();
    }
    
    // 添加或更新缓存项
    relationCache_[relation->getId()] = std::make_shared<RelationCacheItem>(relation);
    
    // 定期清理
    auto now = std::chrono::steady_clock::now();
    if (now - lastCleanup_ > std::chrono::minutes(5)) {
        cleanup();
        lastCleanup_ = now;
    }
}

knowledge::TriplePtr StorageCache::getTriple(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = tripleCache_.find(id);
    if (it != tripleCache_.end()) {
        it->second->updateTimestamp();
        hits_++;
        return it->second->getTriple();
    }
    
    misses_++;
    return nullptr;
}

void StorageCache::cacheTriple(const knowledge::TriplePtr& triple) {
    if (!triple) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 如果缓存已满，清理过期项
    if (entityCache_.size() + relationCache_.size() + tripleCache_.size() + 
        graphCache_.size() + queryCache_.size() >= maxSize_) {
        cleanup();
    }
    
    // 添加或更新缓存项
    tripleCache_[triple->getId()] = std::make_shared<TripleCacheItem>(triple);
    
    // 定期清理
    auto now = std::chrono::steady_clock::now();
    if (now - lastCleanup_ > std::chrono::minutes(5)) {
        cleanup();
        lastCleanup_ = now;
    }
}

knowledge::KnowledgeGraphPtr StorageCache::getKnowledgeGraph(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = graphCache_.find(name);
    if (it != graphCache_.end()) {
        it->second->updateTimestamp();
        hits_++;
        return it->second->getGraph();
    }
    
    misses_++;
    return nullptr;
}

void StorageCache::cacheKnowledgeGraph(const knowledge::KnowledgeGraphPtr& graph) {
    if (!graph) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 如果缓存已满，清理过期项
    if (entityCache_.size() + relationCache_.size() + tripleCache_.size() + 
        graphCache_.size() + queryCache_.size() >= maxSize_) {
        cleanup();
    }
    
    // 添加或更新缓存项
    graphCache_[graph->getName()] = std::make_shared<KnowledgeGraphCacheItem>(graph);
    
    // 定期清理
    auto now = std::chrono::steady_clock::now();
    if (now - lastCleanup_ > std::chrono::minutes(5)) {
        cleanup();
        lastCleanup_ = now;
    }
}

std::vector<knowledge::EntityPtr> StorageCache::getEntityQueryResult(const std::string& queryKey) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = queryCache_.find("entity:" + queryKey);
    if (it != queryCache_.end()) {
        it->second->updateTimestamp();
        hits_++;
        return it->second->getEntities();
    }
    
    misses_++;
    return {};
}

void StorageCache::cacheEntityQueryResult(const std::string& queryKey, 
                                         const std::vector<knowledge::EntityPtr>& entities) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 如果缓存已满，清理过期项
    if (entityCache_.size() + relationCache_.size() + tripleCache_.size() + 
        graphCache_.size() + queryCache_.size() >= maxSize_) {
        cleanup();
    }
    
    // 添加或更新缓存项
    queryCache_["entity:" + queryKey] = std::make_shared<QueryCacheItem>(entities);
    
    // 定期清理
    auto now = std::chrono::steady_clock::now();
    if (now - lastCleanup_ > std::chrono::minutes(5)) {
        cleanup();
        lastCleanup_ = now;
    }
}

std::vector<knowledge::RelationPtr> StorageCache::getRelationQueryResult(const std::string& queryKey) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = queryCache_.find("relation:" + queryKey);
    if (it != queryCache_.end()) {
        it->second->updateTimestamp();
        hits_++;
        return it->second->getRelations();
    }
    
    misses_++;
    return {};
}

void StorageCache::cacheRelationQueryResult(const std::string& queryKey, 
                                           const std::vector<knowledge::RelationPtr>& relations) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 如果缓存已满，清理过期项
    if (entityCache_.size() + relationCache_.size() + tripleCache_.size() + 
        graphCache_.size() + queryCache_.size() >= maxSize_) {
        cleanup();
    }
    
    // 添加或更新缓存项
    queryCache_["relation:" + queryKey] = std::make_shared<QueryCacheItem>(relations);
    
    // 定期清理
    auto now = std::chrono::steady_clock::now();
    if (now - lastCleanup_ > std::chrono::minutes(5)) {
        cleanup();
        lastCleanup_ = now;
    }
}

std::vector<knowledge::TriplePtr> StorageCache::getTripleQueryResult(const std::string& queryKey) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = queryCache_.find("triple:" + queryKey);
    if (it != queryCache_.end()) {
        it->second->updateTimestamp();
        hits_++;
        return it->second->getTriples();
    }
    
    misses_++;
    return {};
}

void StorageCache::cacheTripleQueryResult(const std::string& queryKey, 
                                         const std::vector<knowledge::TriplePtr>& triples) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 如果缓存已满，清理过期项
    if (entityCache_.size() + relationCache_.size() + tripleCache_.size() + 
        graphCache_.size() + queryCache_.size() >= maxSize_) {
        cleanup();
    }
    
    // 添加或更新缓存项
    queryCache_["triple:" + queryKey] = std::make_shared<QueryCacheItem>(triples);
    
    // 定期清理
    auto now = std::chrono::steady_clock::now();
    if (now - lastCleanup_ > std::chrono::minutes(5)) {
        cleanup();
        lastCleanup_ = now;
    }
}

void StorageCache::removeEntity(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    entityCache_.erase(id);
}

void StorageCache::removeRelation(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    relationCache_.erase(id);
}

void StorageCache::removeTriple(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    tripleCache_.erase(id);
}

void StorageCache::removeKnowledgeGraph(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    graphCache_.erase(name);
}

void StorageCache::removeQueryResult(const std::string& queryKey) {
    std::lock_guard<std::mutex> lock(mutex_);
    queryCache_.erase("entity:" + queryKey);
    queryCache_.erase("relation:" + queryKey);
    queryCache_.erase("triple:" + queryKey);
}

void StorageCache::cleanup() {
    auto now = std::chrono::steady_clock::now();
    size_t removedCount = 0;
    
    // 清理过期的实体缓存项
    for (auto it = entityCache_.begin(); it != entityCache_.end();) {
        if (now - it->second->getTimestamp() > ttl_) {
            it = entityCache_.erase(it);
            removedCount++;
        } else {
            ++it;
        }
    }
    
    // 清理过期的关系缓存项
    for (auto it = relationCache_.begin(); it != relationCache_.end();) {
        if (now - it->second->getTimestamp() > ttl_) {
            it = relationCache_.erase(it);
            removedCount++;
        } else {
            ++it;
        }
    }
    
    // 清理过期的三元组缓存项
    for (auto it = tripleCache_.begin(); it != tripleCache_.end();) {
        if (now - it->second->getTimestamp() > ttl_) {
            it = tripleCache_.erase(it);
            removedCount++;
        } else {
            ++it;
        }
    }
    
    // 清理过期的知识图谱缓存项
    for (auto it = graphCache_.begin(); it != graphCache_.end();) {
        if (now - it->second->getTimestamp() > ttl_) {
            it = graphCache_.erase(it);
            removedCount++;
        } else {
            ++it;
        }
    }
    
    // 清理过期的查询结果缓存项
    for (auto it = queryCache_.begin(); it != queryCache_.end();) {
        if (now - it->second->getTimestamp() > ttl_) {
            it = queryCache_.erase(it);
            removedCount++;
        } else {
            ++it;
        }
    }
    
    // 如果缓存仍然超过最大尺寸，按最近使用时间排序并删除最旧的
    size_t totalSize = entityCache_.size() + relationCache_.size() + 
                      tripleCache_.size() + graphCache_.size() + queryCache_.size();
    
    if (totalSize > maxSize_) {
        // 收集所有缓存项
        std::vector<std::pair<std::chrono::steady_clock::time_point, std::string>> items;
        
        for (const auto& item : entityCache_) {
            items.emplace_back(item.second->getTimestamp(), "entity:" + item.first);
        }
        
        for (const auto& item : relationCache_) {
            items.emplace_back(item.second->getTimestamp(), "relation:" + item.first);
        }
        
        for (const auto& item : tripleCache_) {
            items.emplace_back(item.second->getTimestamp(), "triple:" + item.first);
        }
        
        for (const auto& item : graphCache_) {
            items.emplace_back(item.second->getTimestamp(), "graph:" + item.first);
        }
        
        for (const auto& item : queryCache_) {
            items.emplace_back(item.second->getTimestamp(), "query:" + item.first);
        }
        
        // 按时间戳排序（最旧的在前）
        std::sort(items.begin(), items.end(), 
            [](const auto& a, const auto& b) {
                return a.first < b.first;
            });
        
        // 删除最旧的项直到缓存大小符合要求
        size_t toRemove = totalSize - maxSize_;
        for (size_t i = 0; i < toRemove && i < items.size(); ++i) {
            const auto& item = items[i];
            std::string type = item.second.substr(0, item.second.find(':'));
            std::string key = item.second.substr(item.second.find(':') + 1);
            
            if (type == "entity") {
                entityCache_.erase(key);
            } else if (type == "relation") {
                relationCache_.erase(key);
            } else if (type == "triple") {
                tripleCache_.erase(key);
            } else if (type == "graph") {
                graphCache_.erase(key);
            } else if (type == "query") {
                queryCache_.erase(key);
            }
            
            removedCount++;
        }
    }
    
    if (removedCount > 0) {
        LOG_DEBUG("清理了 {} 个过期或过旧的缓存项", removedCount);
    }
}

std::string StorageCache::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t entityCount = entityCache_.size();
    size_t relationCount = relationCache_.size();
    size_t tripleCount = tripleCache_.size();
    size_t graphCount = graphCache_.size();
    size_t queryCount = queryCache_.size();
    size_t totalSize = entityCount + relationCount + tripleCount + graphCount + queryCount;
    
    size_t hitsCount = hits_;
    size_t missesCount = misses_;
    double hitRatio = (hitsCount + missesCount > 0) 
                     ? (static_cast<double>(hitsCount) / (hitsCount + missesCount)) 
                     : 0.0;
    
    std::ostringstream oss;
    oss << "存储缓存统计信息:\n";
    oss << "总项数: " << totalSize << "/" << maxSize_ << " (" 
        << static_cast<int>(100.0 * totalSize / maxSize_) << "%)\n";
    oss << "实体缓存: " << entityCount << " 项\n";
    oss << "关系缓存: " << relationCount << " 项\n";
    oss << "三元组缓存: " << tripleCount << " 项\n";
    oss << "知识图谱缓存: " << graphCount << " 项\n";
    oss << "查询结果缓存: " << queryCount << " 项\n";
    oss << "命中率: " << static_cast<int>(hitRatio * 100) << "% (" 
        << hitsCount << " 命中, " << missesCount << " 未命中)\n";
    
    return oss.str();
}

} // namespace storage
} // namespace kb
 