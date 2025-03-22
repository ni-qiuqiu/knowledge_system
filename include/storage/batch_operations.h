/**
 * @file batch_operations.h
 * @brief 批量操作管理器
 */

#ifndef KB_BATCH_OPERATIONS_H
#define KB_BATCH_OPERATIONS_H

#include "storage/storage_interface.h"
#include "knowledge/entity.h"
#include "knowledge/relation.h"
#include "knowledge/triple.h"
#include "knowledge/knowledge_graph.h"
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <mutex>

namespace kb {
namespace storage {

/**
 * @brief 批量操作类型
 */
enum class BatchOperationType {
    INSERT,     ///< 插入操作
    UPDATE,     ///< 更新操作
    DELETE,     ///< 删除操作
    IMPORT,     ///< 导入操作
    EXPORT      ///< 导出操作
};

/**
 * @brief 批量操作状态
 */
struct BatchOperationStatus {
    size_t totalItems;          ///< 总项数
    size_t processedItems;      ///< 已处理项数
    size_t successItems;        ///< 成功项数
    size_t failedItems;         ///< 失败项数
    std::string currentStep;    ///< 当前步骤
    bool isCompleted;           ///< 是否完成
    bool hasError;              ///< 是否有错误
    std::string errorMessage;   ///< 错误信息
    
    /**
     * @brief 构造函数
     */
    BatchOperationStatus() 
        : totalItems(0), processedItems(0), successItems(0), failedItems(0),
          isCompleted(false), hasError(false) {}
    
    /**
     * @brief 获取进度百分比
     * @return 进度百分比
     */
    double getProgressPercentage() const {
        return totalItems > 0 ? (static_cast<double>(processedItems) / totalItems * 100.0) : 0.0;
    }
    
    /**
     * @brief 更新状态
     * @param processed 已处理项数
     * @param success 成功项数
     * @param failed 失败项数
     * @param step 当前步骤
     */
    void update(size_t processed, size_t success, size_t failed, const std::string& step) {
        processedItems = processed;
        successItems = success;
        failedItems = failed;
        currentStep = step;
    }
    
    /**
     * @brief 设置错误
     * @param message 错误信息
     */
    void setError(const std::string& message) {
        hasError = true;
        errorMessage = message;
    }
    
    /**
     * @brief 设置完成
     */
    void setCompleted() {
        isCompleted = true;
    }
};

/**
 * @brief 批量操作配置
 */
struct BatchOperationConfig {
    size_t batchSize;           ///< 批次大小
    bool useTransaction;        ///< 是否使用事务
    bool continueOnError;       ///< 出错时是否继续
    size_t maxErrorCount;       ///< 最大错误数量
    bool parallelProcessing;    ///< 是否并行处理
    size_t numThreads;          ///< 线程数量
    
    /**
     * @brief 构造函数
     */
    BatchOperationConfig() 
        : batchSize(1000), useTransaction(true), continueOnError(false),
          maxErrorCount(100), parallelProcessing(false), numThreads(4) {}
};

/**
 * @brief 批量操作管理器
 */
class BatchOperationManager {
public:
    /**
     * @brief 获取单例实例
     * @return 批量操作管理器实例
     */
    static BatchOperationManager& getInstance();
    
    /**
     * @brief 批量保存实体
     * @param storage 存储适配器
     * @param entities 实体列表
     * @param config 批量操作配置
     * @return 操作ID
     */
    std::string batchSaveEntities(
        StorageInterfacePtr storage,
        const std::vector<knowledge::EntityPtr>& entities,
        const BatchOperationConfig& config = BatchOperationConfig());
    
    /**
     * @brief 批量保存关系
     * @param storage 存储适配器
     * @param relations 关系列表
     * @param config 批量操作配置
     * @return 操作ID
     */
    std::string batchSaveRelations(
        StorageInterfacePtr storage,
        const std::vector<knowledge::RelationPtr>& relations,
        const BatchOperationConfig& config = BatchOperationConfig());
    
    /**
     * @brief 批量保存三元组
     * @param storage 存储适配器
     * @param triples 三元组列表
     * @param config 批量操作配置
     * @return 操作ID
     */
    std::string batchSaveTriples(
        StorageInterfacePtr storage,
        const std::vector<knowledge::TriplePtr>& triples,
        const BatchOperationConfig& config = BatchOperationConfig());
    
    /**
     * @brief 批量删除实体
     * @param storage 存储适配器
     * @param entityIds 实体ID列表
     * @param config 批量操作配置
     * @return 操作ID
     */
    std::string batchDeleteEntities(
        StorageInterfacePtr storage,
        const std::vector<std::string>& entityIds,
        const BatchOperationConfig& config = BatchOperationConfig());
    
    /**
     * @brief 批量删除关系
     * @param storage 存储适配器
     * @param relationIds 关系ID列表
     * @param config 批量操作配置
     * @return 操作ID
     */
    std::string batchDeleteRelations(
        StorageInterfacePtr storage,
        const std::vector<std::string>& relationIds,
        const BatchOperationConfig& config = BatchOperationConfig());
    
    /**
     * @brief 批量删除三元组
     * @param storage 存储适配器
     * @param tripleIds 三元组ID列表
     * @param config 批量操作配置
     * @return 操作ID
     */
    std::string batchDeleteTriples(
        StorageInterfacePtr storage,
        const std::vector<std::string>& tripleIds,
        const BatchOperationConfig& config = BatchOperationConfig());
    
    /**
     * @brief 导入知识图谱
     * @param storage 存储适配器
     * @param filePath 文件路径
     * @param format 格式，支持"rdf", "json-ld", "csv"等
     * @param config 批量操作配置
     * @return 操作ID
     */
    std::string importKnowledgeGraph(
        StorageInterfacePtr storage,
        const std::string& filePath,
        const std::string& format,
        const BatchOperationConfig& config = BatchOperationConfig());
    
    /**
     * @brief 导出知识图谱
     * @param storage 存储适配器
     * @param graphName 知识图谱名称
     * @param filePath 文件路径
     * @param format 格式，支持"rdf", "json-ld", "csv"等
     * @param config 批量操作配置
     * @return 操作ID
     */
    std::string exportKnowledgeGraph(
        StorageInterfacePtr storage,
        const std::string& graphName,
        const std::string& filePath,
        const std::string& format,
        const BatchOperationConfig& config = BatchOperationConfig());
    
    /**
     * @brief 获取操作状态
     * @param operationId 操作ID
     * @return 操作状态
     */
    BatchOperationStatus getOperationStatus(const std::string& operationId) const;
    
    /**
     * @brief 取消操作
     * @param operationId 操作ID
     * @return 是否成功取消
     */
    bool cancelOperation(const std::string& operationId);
    
    /**
     * @brief 清理已完成的操作
     */
    void cleanupCompletedOperations();
    
private:
    /**
     * @brief 构造函数
     */
    BatchOperationManager();
    
    /**
     * @brief 处理批量保存实体
     * @param operationId 操作ID
     * @param storage 存储适配器
     * @param entities 实体列表
     * @param config 批量操作配置
     */
    void processBatchSaveEntities(
        const std::string& operationId,
        StorageInterfacePtr storage,
        const std::vector<knowledge::EntityPtr>& entities,
        const BatchOperationConfig& config);
    
    /**
     * @brief 处理批量保存关系
     * @param operationId 操作ID
     * @param storage 存储适配器
     * @param relations 关系列表
     * @param config 批量操作配置
     */
    void processBatchSaveRelations(
        const std::string& operationId,
        StorageInterfacePtr storage,
        const std::vector<knowledge::RelationPtr>& relations,
        const BatchOperationConfig& config);
    
    /**
     * @brief 处理批量保存三元组
     * @param operationId 操作ID
     * @param storage 存储适配器
     * @param triples 三元组列表
     * @param config 批量操作配置
     */
    void processBatchSaveTriples(
        const std::string& operationId,
        StorageInterfacePtr storage,
        const std::vector<knowledge::TriplePtr>& triples,
        const BatchOperationConfig& config);
    
    /**
     * @brief 处理批量删除实体
     * @param operationId 操作ID
     * @param storage 存储适配器
     * @param entityIds 实体ID列表
     * @param config 批量操作配置
     */
    void processBatchDeleteEntities(
        const std::string& operationId,
        StorageInterfacePtr storage,
        const std::vector<std::string>& entityIds,
        const BatchOperationConfig& config);
    
    /**
     * @brief 处理批量删除关系
     * @param operationId 操作ID
     * @param storage 存储适配器
     * @param relationIds 关系ID列表
     * @param config 批量操作配置
     */
    void processBatchDeleteRelations(
        const std::string& operationId,
        StorageInterfacePtr storage,
        const std::vector<std::string>& relationIds,
        const BatchOperationConfig& config);
    
    /**
     * @brief 处理批量删除三元组
     * @param operationId 操作ID
     * @param storage 存储适配器
     * @param tripleIds 三元组ID列表
     * @param config 批量操作配置
     */
    void processBatchDeleteTriples(
        const std::string& operationId,
        StorageInterfacePtr storage,
        const std::vector<std::string>& tripleIds,
        const BatchOperationConfig& config);
    
    /**
     * @brief 处理知识图谱导入
     * @param operationId 操作ID
     * @param storage 存储适配器
     * @param filePath 文件路径
     * @param format 格式
     * @param config 批量操作配置
     */
    void processImportKnowledgeGraph(
        const std::string& operationId,
        StorageInterfacePtr storage,
        const std::string& filePath,
        const std::string& format,
        const BatchOperationConfig& config);
    
    /**
     * @brief 处理知识图谱导出
     * @param operationId 操作ID
     * @param storage 存储适配器
     * @param graphName 知识图谱名称
     * @param filePath 文件路径
     * @param format 格式
     * @param config 批量操作配置
     */
    void processExportKnowledgeGraph(
        const std::string& operationId,
        StorageInterfacePtr storage,
        const std::string& graphName,
        const std::string& filePath,
        const std::string& format,
        const BatchOperationConfig& config);
    
    /**
     * @brief 生成操作ID
     * @return 操作ID
     */
    std::string generateOperationId() const;
    
    mutable std::mutex mutex_;  ///< 互斥锁
    std::unordered_map<std::string, BatchOperationStatus> operationStatuses_;  ///< 操作状态映射
    std::unordered_map<std::string, bool> cancelFlags_;  ///< 取消标志映射
};

} // namespace storage
} // namespace kb

#endif // KB_BATCH_OPERATIONS_H 