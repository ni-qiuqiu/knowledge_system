/**
 * @file advanced_cache.h
 * @brief 高级缓存系统
 */

#ifndef KB_ADVANCED_CACHE_H
#define KB_ADVANCED_CACHE_H

#include "storage/storage_interface.h"
#include "common/logging.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <mutex>
#include <functional>
#include <list>
#include <any>
#include <optional>
#include <thread>
#include <atomic>

namespace kb {
namespace storage {

/**
 * @brief 缓存项配置
 */
struct CacheItemConfig {
    bool enableExpiration = true;           ///< 是否启用过期
    std::chrono::seconds ttl{300};          ///< 生存时间（秒）
    int priority = 0;                       ///< 优先级（越高越不容易被淘汰）
    size_t cost = 1;                        ///< 存储成本（用于加权淘汰）
    bool compressible = false;              ///< 是否可压缩
};

/**
 * @brief 缓存项
 */
class CacheItem {
public:
    /**
     * @brief 构造函数
     * @param key 键
     * @param value 值
     * @param config 配置
     */
    CacheItem(const std::string& key, std::any value, const CacheItemConfig& config = {})
        : key_(key), value_(value), config_(config), 
          createdAt_(std::chrono::system_clock::now()),
          lastAccessedAt_(createdAt_),
          accessCount_(0) {}
    
    /**
     * @brief 获取键
     * @return 键
     */
    const std::string& getKey() const { return key_; }
    
    /**
     * @brief 获取值
     * @return 值
     */
    template<typename T>
    std::optional<T> getValue() const {
        try {
            return std::any_cast<T>(value_);
        } catch (const std::bad_any_cast&) {
            return std::nullopt;
        }
    }
    
    /**
     * @brief 检查是否过期
     * @return 是否过期
     */
    bool isExpired() const {
        if (!config_.enableExpiration) {
            return false;
        }
        
        auto now = std::chrono::system_clock::now();
        return (now - createdAt_) > config_.ttl;
    }
    
    /**
     * @brief 更新访问信息
     */
    void updateAccess() {
        lastAccessedAt_ = std::chrono::system_clock::now();
        accessCount_++;
    }
    
    /**
     * @brief 获取创建时间
     * @return 创建时间
     */
    std::chrono::system_clock::time_point getCreatedAt() const { return createdAt_; }
    
    /**
     * @brief 获取最后访问时间
     * @return 最后访问时间
     */
    std::chrono::system_clock::time_point getLastAccessedAt() const { return lastAccessedAt_; }
    
    /**
     * @brief 获取访问次数
     * @return 访问次数
     */
    size_t getAccessCount() const { return accessCount_; }
    
    /**
     * @brief 获取配置
     * @return 配置
     */
    const CacheItemConfig& getConfig() const { return config_; }
    
private:
    std::string key_;                               ///< 键
    std::any value_;                                ///< 值
    CacheItemConfig config_;                        ///< 配置
    std::chrono::system_clock::time_point createdAt_;       ///< 创建时间
    std::chrono::system_clock::time_point lastAccessedAt_;  ///< 最后访问时间
    std::atomic<size_t> accessCount_;               ///< 访问次数
};

using CacheItemPtr = std::shared_ptr<CacheItem>;

/**
 * @brief 缓存替换策略
 */
enum class CachePolicy {
    LRU,        ///< 最近最少使用
    LFU,        ///< 最不经常使用
    FIFO,       ///< 先进先出
    PRIORITY,   ///< 基于优先级
    WEIGHTED    ///< 加权（访问频率、大小、时间）
};

/**
 * @brief 缓存层级
 */
enum class CacheTier {
    MEMORY_FAST,   ///< 快速内存缓存（小容量）
    MEMORY_LARGE,  ///< 大容量内存缓存
    LOCAL_DISK,    ///< 本地磁盘缓存
    REMOTE_CACHE   ///< 远程缓存（如Redis）
};

/**
 * @brief 缓存配置
 */
struct AdvancedCacheConfig {
    bool enabled = true;                        ///< 是否启用缓存
    std::unordered_map<CacheTier, size_t> tierSizes = {  ///< 各层级大小限制
        {CacheTier::MEMORY_FAST, 1000},         ///< 快速内存缓存容量
        {CacheTier::MEMORY_LARGE, 10000},       ///< 大容量内存缓存容量
        {CacheTier::LOCAL_DISK, 100000}         ///< 本地磁盘缓存容量
    };
    std::unordered_map<CacheTier, CachePolicy> tierPolicies = {  ///< 各层级淘汰策略
        {CacheTier::MEMORY_FAST, CachePolicy::LRU},
        {CacheTier::MEMORY_LARGE, CachePolicy::WEIGHTED},
        {CacheTier::LOCAL_DISK, CachePolicy::LFU}
    };
    std::chrono::seconds cleanupInterval{60};   ///< 清理间隔
    bool enablePrefetch = true;                 ///< 是否启用预加载
    size_t prefetchBatchSize = 10;              ///< 预加载批次大小
    double prefetchThreshold = 0.7;             ///< 预加载阈值（相似度）
    bool enableCompression = true;              ///< 是否启用压缩
    bool enableStatistics = true;               ///< 是否启用统计
};

/**
 * @brief 缓存统计
 */
struct CacheStatistics {
    std::unordered_map<CacheTier, size_t> hits;        ///< 各层级命中次数
    std::unordered_map<CacheTier, size_t> misses;      ///< 各层级未命中次数
    std::unordered_map<CacheTier, size_t> evictions;   ///< 各层级淘汰次数
    std::unordered_map<CacheTier, size_t> itemCount;   ///< 各层级项目数量
    std::unordered_map<CacheTier, size_t> totalCost;   ///< 各层级总成本
    size_t prefetchCount = 0;                          ///< 预加载次数
    size_t prefetchHitCount = 0;                       ///< 预加载命中次数
    size_t totalOperations = 0;                        ///< 总操作次数
    
    /**
     * @brief 获取命中率
     * @param tier 缓存层级
     * @return 命中率
     */
    double getHitRate(CacheTier tier) const {
        auto hitIt = hits.find(tier);
        auto missIt = misses.find(tier);
        size_t tierHits = (hitIt != hits.end()) ? hitIt->second : 0;
        size_t tierMisses = (missIt != misses.end()) ? missIt->second : 0;
        
        if (tierHits + tierMisses == 0) {
            return 0.0;
        }
        
        return static_cast<double>(tierHits) / (tierHits + tierMisses);
    }
    
    /**
     * @brief 获取总体命中率
     * @return 总体命中率
     */
    double getOverallHitRate() const {
        size_t totalHits = 0;
        size_t totalMisses = 0;
        
        for (const auto& pair : hits) {
            totalHits += pair.second;
        }
        
        for (const auto& pair : misses) {
            totalMisses += pair.second;
        }
        
        if (totalHits + totalMisses == 0) {
            return 0.0;
        }
        
        return static_cast<double>(totalHits) / (totalHits + totalMisses);
    }
    
    /**
     * @brief 获取预加载命中率
     * @return 预加载命中率
     */
    double getPrefetchHitRate() const {
        if (prefetchCount == 0) {
            return 0.0;
        }
        
        return static_cast<double>(prefetchHitCount) / prefetchCount;
    }
    
    /**
     * @brief 重置统计
     */
    void reset() {
        hits.clear();
        misses.clear();
        evictions.clear();
        itemCount.clear();
        totalCost.clear();
        prefetchCount = 0;
        prefetchHitCount = 0;
        totalOperations = 0;
    }
};

/**
 * @brief 高级缓存管理器
 */
class AdvancedCache {
public:
    /**
     * @brief 获取单例实例
     * @return 缓存管理器实例
     */
    static AdvancedCache& getInstance();
    
    /**
     * @brief 初始化缓存管理器
     * @param config 缓存配置
     * @return 是否成功
     */
    bool initialize(const AdvancedCacheConfig& config);
    
    /**
     * @brief 获取缓存项
     * @param key 键
     * @param tier 起始缓存层级
     * @return 缓存项（如果存在）
     */
    template<typename T>
    std::optional<T> get(const std::string& key, CacheTier tier = CacheTier::MEMORY_FAST) {
        if (!config_.enabled) {
            return std::nullopt;
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        statistics_.totalOperations++;
        
        // 从指定层级开始查找
        std::vector<CacheTier> tiers = {
            CacheTier::MEMORY_FAST, 
            CacheTier::MEMORY_LARGE,
            CacheTier::LOCAL_DISK,
            CacheTier::REMOTE_CACHE
        };
        
        bool startSearch = false;
        for (const auto& currentTier : tiers) {
            // 从指定的tier开始查找
            if (currentTier == tier) {
                startSearch = true;
            }
            
            if (!startSearch) {
                continue;
            }
            
            auto tierCache = caches_.find(currentTier);
            if (tierCache == caches_.end()) {
                continue;
            }
            
            auto it = tierCache->second.find(key);
            if (it != tierCache->second.end()) {
                auto cacheItem = it->second;
                
                // 检查是否过期
                if (cacheItem->isExpired()) {
                    tierCache->second.erase(it);
                    updateStatistics(currentTier, false);
                    continue;
                }
                
                // 更新访问信息
                cacheItem->updateAccess();
                
                // 更新统计
                updateStatistics(currentTier, true);
                
                // 如果在较低层级找到，提升到较高层级
                if (currentTier != CacheTier::MEMORY_FAST) {
                    promoteToHigherTier(cacheItem, currentTier);
                }
                
                // 如果启用预加载，异步加载相关项
                if (config_.enablePrefetch) {
                    triggerPrefetch(key, currentTier);
                }
                
                return cacheItem->getValue<T>();
            }
            
            // 未在当前层级找到，记录未命中
            updateStatistics(currentTier, false);
        }
        
        return std::nullopt;
    }
    
    /**
     * @brief 设置缓存项
     * @param key 键
     * @param value 值
     * @param config 缓存项配置
     * @param targetTier 目标缓存层级
     */
    template<typename T>
    void set(const std::string& key, const T& value, 
             const CacheItemConfig& itemConfig = {}, 
             CacheTier targetTier = CacheTier::MEMORY_FAST) {
        if (!config_.enabled) {
            return;
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        statistics_.totalOperations++;
        
        // 创建缓存项
        auto cacheItem = std::make_shared<CacheItem>(key, value, itemConfig);
        
        // 添加到目标层级
        auto& tierCache = caches_[targetTier];
        
        // 检查容量限制
        auto tierSizeIt = config_.tierSizes.find(targetTier);
        if (tierSizeIt != config_.tierSizes.end() && tierCache.size() >= tierSizeIt->second) {
            // 需要淘汰，根据策略选择要淘汰的项
            evictFromTier(targetTier);
        }
        
        // 添加到缓存
        tierCache[key] = cacheItem;
        
        // 更新统计
        if (config_.enableStatistics) {
            auto& count = statistics_.itemCount[targetTier];
            count++;
            
            auto& cost = statistics_.totalCost[targetTier];
            cost += itemConfig.cost;
        }
    }
    
    /**
     * @brief 移除缓存项
     * @param key 键
     * @param tier 缓存层级（如果为空，则从所有层级移除）
     * @return 是否成功
     */
    bool remove(const std::string& key, std::optional<CacheTier> tier = std::nullopt) {
        if (!config_.enabled) {
            return false;
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        statistics_.totalOperations++;
        
        bool removed = false;
        
        if (tier.has_value()) {
            // 从指定层级移除
            auto tierCache = caches_.find(tier.value());
            if (tierCache != caches_.end()) {
                auto it = tierCache->second.find(key);
                if (it != tierCache->second.end()) {
                    tierCache->second.erase(it);
                    removed = true;
                    
                    // 更新统计
                    if (config_.enableStatistics) {
                        auto& count = statistics_.itemCount[tier.value()];
                        if (count > 0) count--;
                        
                        auto& cost = statistics_.totalCost[tier.value()];
                        cost -= it->second->getConfig().cost;
                        if (cost < 0) cost = 0;
                    }
                }
            }
        } else {
            // 从所有层级移除
            for (auto& tierPair : caches_) {
                auto it = tierPair.second.find(key);
                if (it != tierPair.second.end()) {
                    // 更新统计
                    if (config_.enableStatistics) {
                        auto& count = statistics_.itemCount[tierPair.first];
                        if (count > 0) count--;
                        
                        auto& cost = statistics_.totalCost[tierPair.first];
                        cost -= it->second->getConfig().cost;
                        if (cost < 0) cost = 0;
                    }
                    
                    tierPair.second.erase(it);
                    removed = true;
                }
            }
        }
        
        return removed;
    }
    
    /**
     * @brief 清空缓存
     * @param tier 缓存层级（如果为空，则清空所有层级）
     */
    void clear(std::optional<CacheTier> tier = std::nullopt) {
        if (!config_.enabled) {
            return;
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (tier.has_value()) {
            // 清空指定层级
            auto tierCache = caches_.find(tier.value());
            if (tierCache != caches_.end()) {
                tierCache->second.clear();
                
                // 更新统计
                if (config_.enableStatistics) {
                    statistics_.itemCount[tier.value()] = 0;
                    statistics_.totalCost[tier.value()] = 0;
                }
            }
        } else {
            // 清空所有层级
            caches_.clear();
            
            // 更新统计
            if (config_.enableStatistics) {
                for (auto& pair : statistics_.itemCount) {
                    pair.second = 0;
                }
                
                for (auto& pair : statistics_.totalCost) {
                    pair.second = 0;
                }
            }
        }
    }
    
    /**
     * @brief 获取缓存统计
     * @return 缓存统计
     */
    CacheStatistics getStatistics() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return statistics_;
    }
    
    /**
     * @brief 重置统计
     */
    void resetStatistics() {
        std::lock_guard<std::mutex> lock(mutex_);
        statistics_.reset();
    }
    
    /**
     * @brief 注册预加载函数
     * @param callback 预加载回调函数
     */
    void registerPrefetchCallback(
        std::function<std::vector<std::string>(const std::string&, double)> callback) {
        prefetchCallback_ = callback;
    }
    
    /**
     * @brief 注册压缩函数
     * @param compressCallback 压缩回调函数
     * @param decompressCallback 解压回调函数
     */
    void registerCompressionCallbacks(
        std::function<std::any(const std::any&)> compressCallback,
        std::function<std::any(const std::any&)> decompressCallback) {
        compressCallback_ = compressCallback;
        decompressCallback_ = decompressCallback;
    }
    
    /**
     * @brief 获取配置
     * @return 配置
     */
    AdvancedCacheConfig getConfig() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return config_;
    }
    
    /**
     * @brief 更新配置
     * @param config 配置
     */
    void updateConfig(const AdvancedCacheConfig& config) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // 保存旧配置
        bool wasEnabled = config_.enabled;
        auto oldCleanupInterval = config_.cleanupInterval;
        
        // 更新配置
        config_ = config;
        
        // 如果启用状态改变，需要处理清理线程
        if (wasEnabled != config_.enabled) {
            if (config_.enabled) {
                startCleanupThread();
            } else {
                stopCleanupThread();
            }
        } else if (config_.enabled && oldCleanupInterval != config_.cleanupInterval) {
            // 如果清理间隔改变，重启清理线程
            restartCleanupThread();
        }
    }
    
private:
    /**
     * @brief 构造函数
     */
    AdvancedCache();
    
    /**
     * @brief 析构函数
     */
    ~AdvancedCache();
    
    /**
     * @brief 开始清理线程
     */
    void startCleanupThread();
    
    /**
     * @brief 停止清理线程
     */
    void stopCleanupThread();
    
    /**
     * @brief 重启清理线程
     */
    void restartCleanupThread();
    
    /**
     * @brief 清理过期项
     */
    void cleanupExpiredItems();
    
    /**
     * @brief 从层级中淘汰项
     * @param tier 缓存层级
     */
    void evictFromTier(CacheTier tier);
    
    /**
     * @brief 提升到较高层级
     * @param item 缓存项
     * @param currentTier 当前层级
     */
    void promoteToHigherTier(const CacheItemPtr& item, CacheTier currentTier);
    
    /**
     * @brief 触发预加载
     * @param key 键
     * @param tier 缓存层级
     */
    void triggerPrefetch(const std::string& key, CacheTier tier);
    
    /**
     * @brief 更新统计
     * @param tier 缓存层级
     * @param isHit 是否命中
     */
    void updateStatistics(CacheTier tier, bool isHit);
    
    mutable std::mutex mutex_;  ///< 互斥锁
    AdvancedCacheConfig config_;  ///< 配置
    
    /// 多层级缓存 - 层级 -> (键 -> 缓存项)
    std::unordered_map<CacheTier, std::unordered_map<std::string, CacheItemPtr>> caches_;
    
    /// 预加载回调函数
    std::function<std::vector<std::string>(const std::string&, double)> prefetchCallback_;
    
    /// 压缩回调函数
    std::function<std::any(const std::any&)> compressCallback_;
    
    /// 解压回调函数
    std::function<std::any(const std::any&)> decompressCallback_;
    
    /// 统计信息
    CacheStatistics statistics_;
    
    /// 清理线程
    std::thread cleanupThread_;
    
    /// 是否停止清理线程
    std::atomic<bool> stopCleanup_;
};

} // namespace storage
} // namespace kb

#endif // KB_ADVANCED_CACHE_H 