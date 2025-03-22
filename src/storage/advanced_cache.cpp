/**
 * @file advanced_cache.cpp
 * @brief 高级缓存系统实现
 */

#include "storage/advanced_cache.h"
#include <algorithm>
#include <chrono>
#include <future>

namespace kb {
namespace storage {

AdvancedCache& AdvancedCache::getInstance() {
    static AdvancedCache instance;
    return instance;
}

AdvancedCache::AdvancedCache() : stopCleanup_(false) {
    // 默认配置
    config_.enabled = true;
    config_.tierSizes = {
        {CacheTier::MEMORY_FAST, 1000},
        {CacheTier::MEMORY_LARGE, 10000},
        {CacheTier::LOCAL_DISK, 100000}
    };
    config_.tierPolicies = {
        {CacheTier::MEMORY_FAST, CachePolicy::LRU},
        {CacheTier::MEMORY_LARGE, CachePolicy::WEIGHTED},
        {CacheTier::LOCAL_DISK, CachePolicy::LFU}
    };
    config_.cleanupInterval = std::chrono::seconds(60);
    config_.enablePrefetch = true;
    config_.prefetchBatchSize = 10;
    config_.prefetchThreshold = 0.7;
    config_.enableCompression = true;
    config_.enableStatistics = true;
    
    // 初始化统计信息
    statistics_.reset();
    
    LOG_INFO("高级缓存系统已创建");
}

AdvancedCache::~AdvancedCache() {
    stopCleanupThread();
    LOG_INFO("高级缓存系统已销毁");
}

bool AdvancedCache::initialize(const AdvancedCacheConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 更新配置
    config_ = config;
    
    // 清空缓存
    caches_.clear();
    
    // 重置统计信息
    statistics_.reset();
    
    // 如果启用缓存，启动清理线程
    if (config_.enabled) {
        startCleanupThread();
    }
    
    LOG_INFO("高级缓存系统已初始化");
    return true;
}

void AdvancedCache::startCleanupThread() {
    // 如果已经有清理线程在运行，先停止它
    stopCleanupThread();
    
    // 创建新的清理线程
    stopCleanup_ = false;
    cleanupThread_ = std::thread([this]() {
        LOG_INFO("缓存清理线程已启动");
        
        while (!stopCleanup_) {
            // 清理过期项
            cleanupExpiredItems();
            
            // 等待清理间隔
            std::this_thread::sleep_for(config_.cleanupInterval);
        }
        
        LOG_INFO("缓存清理线程已停止");
    });
}

void AdvancedCache::stopCleanupThread() {
    if (cleanupThread_.joinable()) {
        stopCleanup_ = true;
        cleanupThread_.join();
    }
}

void AdvancedCache::restartCleanupThread() {
    stopCleanupThread();
    startCleanupThread();
}

void AdvancedCache::cleanupExpiredItems() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!config_.enabled) {
        return;
    }
    
    size_t totalExpired = 0;
    
    // 遍历所有缓存层级
    for (auto& tierPair : caches_) {
        CacheTier tier = tierPair.first;
        auto& tierCache = tierPair.second;
        
        // 遍历层级中的所有缓存项
        for (auto it = tierCache.begin(); it != tierCache.end();) {
            if (it->second->isExpired()) {
                // 更新统计
                if (config_.enableStatistics) {
                    auto& count = statistics_.itemCount[tier];
                    if (count > 0) count--;
                    
                    auto& cost = statistics_.totalCost[tier];
                    cost -= it->second->getConfig().cost;
                    if (cost < 0) cost = 0;
                }
                
                // 删除过期项
                it = tierCache.erase(it);
                totalExpired++;
            } else {
                ++it;
            }
        }
    }
    
    if (totalExpired > 0) {
        LOG_INFO("清理了 {} 个过期缓存项", totalExpired);
    }
}

void AdvancedCache::evictFromTier(CacheTier tier) {
    auto& tierCache = caches_[tier];
    if (tierCache.empty()) {
        return;
    }
    
    auto policyIt = config_.tierPolicies.find(tier);
    CachePolicy policy = (policyIt != config_.tierPolicies.end()) ? 
                          policyIt->second : CachePolicy::LRU;
    
    // 根据不同策略选择要淘汰的项
    switch (policy) {
        case CachePolicy::LRU: {
            // 找出最久未使用的项
            std::string lruKey;
            std::chrono::system_clock::time_point oldestAccess = std::chrono::system_clock::now();
            
            for (const auto& pair : tierCache) {
                const auto& accessTime = pair.second->getLastAccessedAt();
                if (accessTime < oldestAccess) {
                    oldestAccess = accessTime;
                    lruKey = pair.first;
                }
            }
            
            if (!lruKey.empty()) {
                // 更新统计
                if (config_.enableStatistics) {
                    statistics_.evictions[tier]++;
                    
                    auto& count = statistics_.itemCount[tier];
                    if (count > 0) count--;
                    
                    auto& cost = statistics_.totalCost[tier];
                    cost -= tierCache[lruKey]->getConfig().cost;
                    if (cost < 0) cost = 0;
                }
                
                // 淘汰项
                tierCache.erase(lruKey);
            }
            break;
        }
        
        case CachePolicy::LFU: {
            // 找出最不常用的项
            std::string lfuKey;
            size_t lowestAccess = std::numeric_limits<size_t>::max();
            
            for (const auto& pair : tierCache) {
                const auto accessCount = pair.second->getAccessCount();
                if (accessCount < lowestAccess) {
                    lowestAccess = accessCount;
                    lfuKey = pair.first;
                }
            }
            
            if (!lfuKey.empty()) {
                // 更新统计
                if (config_.enableStatistics) {
                    statistics_.evictions[tier]++;
                    
                    auto& count = statistics_.itemCount[tier];
                    if (count > 0) count--;
                    
                    auto& cost = statistics_.totalCost[tier];
                    cost -= tierCache[lfuKey]->getConfig().cost;
                    if (cost < 0) cost = 0;
                }
                
                // 淘汰项
                tierCache.erase(lfuKey);
            }
            break;
        }
        
        case CachePolicy::FIFO: {
            // 找出最早创建的项
            std::string fifoKey;
            std::chrono::system_clock::time_point oldestCreation = std::chrono::system_clock::now();
            
            for (const auto& pair : tierCache) {
                const auto& creationTime = pair.second->getCreatedAt();
                if (creationTime < oldestCreation) {
                    oldestCreation = creationTime;
                    fifoKey = pair.first;
                }
            }
            
            if (!fifoKey.empty()) {
                // 更新统计
                if (config_.enableStatistics) {
                    statistics_.evictions[tier]++;
                    
                    auto& count = statistics_.itemCount[tier];
                    if (count > 0) count--;
                    
                    auto& cost = statistics_.totalCost[tier];
                    cost -= tierCache[fifoKey]->getConfig().cost;
                    if (cost < 0) cost = 0;
                }
                
                // 淘汰项
                tierCache.erase(fifoKey);
            }
            break;
        }
        
        case CachePolicy::PRIORITY: {
            // 找出优先级最低的项
            std::string lowestPriorityKey;
            int lowestPriority = std::numeric_limits<int>::max();
            
            for (const auto& pair : tierCache) {
                const int priority = pair.second->getConfig().priority;
                if (priority < lowestPriority) {
                    lowestPriority = priority;
                    lowestPriorityKey = pair.first;
                }
            }
            
            if (!lowestPriorityKey.empty()) {
                // 更新统计
                if (config_.enableStatistics) {
                    statistics_.evictions[tier]++;
                    
                    auto& count = statistics_.itemCount[tier];
                    if (count > 0) count--;
                    
                    auto& cost = statistics_.totalCost[tier];
                    cost -= tierCache[lowestPriorityKey]->getConfig().cost;
                    if (cost < 0) cost = 0;
                }
                
                // 淘汰项
                tierCache.erase(lowestPriorityKey);
            }
            break;
        }
        
        case CachePolicy::WEIGHTED: {
            // 使用加权策略，考虑访问频率、大小和时间
            std::string evictKey;
            double lowestScore = std::numeric_limits<double>::max();
            
            auto now = std::chrono::system_clock::now();
            
            for (const auto& pair : tierCache) {
                const auto& item = pair.second;
                
                // 计算各个因素的归一化得分
                double accessScore = 1.0 / (1.0 + item->getAccessCount());
                
                auto ageMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - item->getLastAccessedAt()).count();
                double ageScore = static_cast<double>(ageMs) / (1000.0 * 3600.0); // 归一化到小时
                
                double costScore = static_cast<double>(item->getConfig().cost) / 1000.0;
                
                // 计算加权得分，分数越低越容易被淘汰
                double score = 0.5 * accessScore + 0.3 * ageScore + 0.2 * costScore;
                
                if (score < lowestScore) {
                    lowestScore = score;
                    evictKey = pair.first;
                }
            }
            
            if (!evictKey.empty()) {
                // 更新统计
                if (config_.enableStatistics) {
                    statistics_.evictions[tier]++;
                    
                    auto& count = statistics_.itemCount[tier];
                    if (count > 0) count--;
                    
                    auto& cost = statistics_.totalCost[tier];
                    cost -= tierCache[evictKey]->getConfig().cost;
                    if (cost < 0) cost = 0;
                }
                
                // 淘汰项
                tierCache.erase(evictKey);
            }
            break;
        }
    }
}

void AdvancedCache::promoteToHigherTier(const CacheItemPtr& item, CacheTier currentTier) {
    // 确定要提升到的目标层级
    CacheTier targetTier;
    
    if (currentTier == CacheTier::LOCAL_DISK) {
        targetTier = CacheTier::MEMORY_LARGE;
    } else if (currentTier == CacheTier::MEMORY_LARGE) {
        targetTier = CacheTier::MEMORY_FAST;
    } else if (currentTier == CacheTier::REMOTE_CACHE) {
        targetTier = CacheTier::MEMORY_LARGE;
    } else {
        // 已经是最高层级，不需要提升
        return;
    }
    
    // 检查目标层级是否存在
    auto targetCacheIt = caches_.find(targetTier);
    if (targetCacheIt == caches_.end()) {
        // 创建目标层级
        caches_[targetTier] = std::unordered_map<std::string, CacheItemPtr>();
        targetCacheIt = caches_.find(targetTier);
    }
    
    // 检查目标层级是否已满
    auto tierSizeIt = config_.tierSizes.find(targetTier);
    if (tierSizeIt != config_.tierSizes.end() && 
        targetCacheIt->second.size() >= tierSizeIt->second) {
        // 需要淘汰一项
        evictFromTier(targetTier);
    }
    
    // 将项添加到目标层级
    targetCacheIt->second[item->getKey()] = item;
    
    // 更新统计
    if (config_.enableStatistics) {
        auto& targetCount = statistics_.itemCount[targetTier];
        targetCount++;
        
        auto& targetCost = statistics_.totalCost[targetTier];
        targetCost += item->getConfig().cost;
    }
}

void AdvancedCache::triggerPrefetch(const std::string& key, CacheTier tier) {
    if (!prefetchCallback_) {
        return;
    }
    
    // 在单独的线程中执行预加载
    std::thread([this, key, tier]() {
        // 获取相关键
        auto relatedKeys = prefetchCallback_(key, config_.prefetchThreshold);
        
        if (relatedKeys.empty()) {
            return;
        }
        
        // 限制预加载数量
        if (relatedKeys.size() > config_.prefetchBatchSize) {
            relatedKeys.resize(config_.prefetchBatchSize);
        }
        
        // 更新统计
        {
            std::lock_guard<std::mutex> lock(mutex_);
            statistics_.prefetchCount += relatedKeys.size();
        }
        
        // 过滤掉已经在当前或更高层级的键
        std::vector<std::string> keysToFetch;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            for (const auto& relatedKey : relatedKeys) {
                bool alreadyCached = false;
                
                // 检查是否已经在缓存中
                for (CacheTier checkTier : {CacheTier::MEMORY_FAST, CacheTier::MEMORY_LARGE}) {
                    auto tierCacheIt = caches_.find(checkTier);
                    if (tierCacheIt != caches_.end() && 
                        tierCacheIt->second.find(relatedKey) != tierCacheIt->second.end()) {
                        alreadyCached = true;
                        break;
                    }
                }
                
                if (!alreadyCached) {
                    keysToFetch.push_back(relatedKey);
                }
            }
        }
        
        if (keysToFetch.empty()) {
            return;
        }
        
        LOG_INFO("预加载 {} 个相关键", keysToFetch.size());
        
        // 这里你需要实现实际的预加载逻辑，根据你的系统设计
        // 例如，你可能需要从数据库加载这些键对应的数据
        // 然后将它们添加到缓存中
        
        // 模拟预加载
        {
            std::lock_guard<std::mutex> lock(mutex_);
            statistics_.prefetchHitCount += keysToFetch.size();
        }
    }).detach();
}

void AdvancedCache::updateStatistics(CacheTier tier, bool isHit) {
    if (!config_.enableStatistics) {
        return;
    }
    
    if (isHit) {
        statistics_.hits[tier]++;
    } else {
        statistics_.misses[tier]++;
    }
}

} // namespace storage
} // namespace kb 