/**
 * @file batch_operations.cpp
 * @brief 批量操作管理器实现
 */

#include "storage/batch_operations.h"
#include "common/logging.h"
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>
#include <random>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <algorithm>

namespace kb {
namespace storage {

BatchOperationManager& BatchOperationManager::getInstance() {
    static BatchOperationManager instance;
    return instance;
}

BatchOperationManager::BatchOperationManager() {
    LOG_INFO("批量操作管理器初始化");
}

std::string BatchOperationManager::generateOperationId() const {
    // 生成随机操作ID
    const size_t ID_LENGTH = 16;
    static const char CHARACTERS[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<> distribution(0, sizeof(CHARACTERS) - 2);
    
    std::string id;
    id.reserve(ID_LENGTH);
    
    for (size_t i = 0; i < ID_LENGTH; ++i) {
        id.push_back(CHARACTERS[distribution(generator)]);
    }
    
    // 添加时间戳
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream timestamp;
    timestamp << std::put_time(std::localtime(&now_time_t), "%Y%m%d%H%M%S");
    
    return timestamp.str() + "_" + id;
}

BatchOperationStatus BatchOperationManager::getOperationStatus(const std::string& operationId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = operationStatuses_.find(operationId);
    if (it != operationStatuses_.end()) {
        return it->second;
    }
    
    // 如果找不到操作，返回默认状态
    BatchOperationStatus status;
    status.setError("操作ID不存在: " + operationId);
    return status;
}

bool BatchOperationManager::cancelOperation(const std::string& operationId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto statusIt = operationStatuses_.find(operationId);
    if (statusIt == operationStatuses_.end()) {
        LOG_WARN("尝试取消不存在的操作: {}", operationId);
        return false;
    }
    
    if (statusIt->second.isCompleted) {
        LOG_WARN("尝试取消已完成的操作: {}", operationId);
        return false;
    }
    
    cancelFlags_[operationId] = true;
    LOG_INFO("操作取消标记已设置: {}", operationId);
    return true;
}

void BatchOperationManager::cleanupCompletedOperations() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto it = operationStatuses_.begin(); it != operationStatuses_.end();) {
        if (it->second.isCompleted) {
            auto cancelIt = cancelFlags_.find(it->first);
            if (cancelIt != cancelFlags_.end()) {
                cancelFlags_.erase(cancelIt);
            }
            it = operationStatuses_.erase(it);
        } else {
            ++it;
        }
    }
    
    LOG_INFO("已清理完成的操作");
}

// 批量保存实体的公共接口
std::string BatchOperationManager::batchSaveEntities(
    StorageInterfacePtr storage,
    const std::vector<knowledge::EntityPtr>& entities,
    const BatchOperationConfig& config) {
    
    if (!storage) {
        LOG_ERROR("存储适配器为空");
        return "";
    }
    
    if (entities.empty()) {
        LOG_WARN("实体列表为空，无需批量保存");
        return "";
    }
    
    std::string operationId = generateOperationId();
    
    // 初始化操作状态
    BatchOperationStatus status;
    status.totalItems = entities.size();
    status.currentStep = "准备批量保存实体";
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        operationStatuses_[operationId] = status;
        cancelFlags_[operationId] = false;
    }
    
    // 启动异步线程处理批量保存
    std::thread([this, operationId, storage, entities, config]() {
        processBatchSaveEntities(operationId, storage, entities, config);
    }).detach();
    
    LOG_INFO("批量保存实体操作已启动，ID: {}, 实体数量: {}", operationId, entities.size());
    
    return operationId;
}

// 实现批量保存实体
void BatchOperationManager::processBatchSaveEntities(
    const std::string& operationId,
    StorageInterfacePtr storage,
    const std::vector<knowledge::EntityPtr>& entities,
    const BatchOperationConfig& config) {
    
    size_t totalCount = entities.size();
    size_t successCount = 0;
    size_t failedCount = 0;
    size_t processedCount = 0;
    
    try {
        // 分批处理
        for (size_t i = 0; i < totalCount; i += config.batchSize) {
            // 检查取消标志
            {
                std::lock_guard<std::mutex> lock(mutex_);
                auto it = cancelFlags_.find(operationId);
                if (it != cancelFlags_.end() && it->second) {
                    LOG_INFO("批量保存实体操作已取消: {}", operationId);
                    
                    BatchOperationStatus& status = operationStatuses_[operationId];
                    status.update(processedCount, successCount, failedCount, "操作已取消");
                    status.setCompleted();
                    return;
                }
            }
            
            // 计算当前批次的大小
            size_t batchEnd = std::min(i + config.batchSize, totalCount);
            size_t currentBatchSize = batchEnd - i;
            
            // 更新状态
            {
                std::lock_guard<std::mutex> lock(mutex_);
                BatchOperationStatus& status = operationStatuses_[operationId];
                status.update(processedCount, successCount, failedCount, 
                             "处理批次 " + std::to_string(i / config.batchSize + 1) + 
                             "/" + std::to_string((totalCount + config.batchSize - 1) / config.batchSize));
            }
            
            // 准备当前批次的实体
            std::vector<knowledge::EntityPtr> batch;
            for (size_t j = i; j < batchEnd; ++j) {
                batch.push_back(entities[j]);
            }
            
            // 使用事务处理当前批次
            bool batchSuccess = true;
            if (config.useTransaction) {
                if (!storage->beginTransaction()) {
                    LOG_ERROR("开始事务失败");
                    batchSuccess = false;
                }
            }
            
            if (batchSuccess) {
                // 保存当前批次的实体
                for (const auto& entity : batch) {
                    try {
                        if (storage->saveEntity(entity)) {
                            successCount++;
                        } else {
                            failedCount++;
                            LOG_ERROR("保存实体失败: {}", entity->getId());
                            
                            if (!config.continueOnError && failedCount >= config.maxErrorCount) {
                                batchSuccess = false;
                                break;
                            }
                        }
                    } catch (const std::exception& e) {
                        failedCount++;
                        LOG_ERROR("保存实体异常: {}, 错误: {}", entity->getId(), e.what());
                        
                        if (!config.continueOnError && failedCount >= config.maxErrorCount) {
                            batchSuccess = false;
                            break;
                        }
                    }
                    
                    processedCount++;
                }
                
                // 处理事务
                if (config.useTransaction) {
                    if (batchSuccess) {
                        if (!storage->commitTransaction()) {
                            LOG_ERROR("提交事务失败");
                            // 回滚当前批次的统计
                            successCount -= (currentBatchSize - (processedCount - i));
                            failedCount += (currentBatchSize - (processedCount - i));
                            processedCount = i + currentBatchSize;
                        }
                    } else {
                        if (!storage->rollbackTransaction()) {
                            LOG_ERROR("回滚事务失败");
                        }
                        // 回滚当前批次的统计
                        successCount -= (processedCount - i);
                        failedCount += (currentBatchSize - (processedCount - i));
                        processedCount = i + currentBatchSize;
                    }
                }
            } else {
                // 批次失败，更新统计
                failedCount += currentBatchSize;
                processedCount += currentBatchSize;
            }
            
            // 如果超过最大错误数且不继续处理错误，则中断处理
            if (!config.continueOnError && failedCount >= config.maxErrorCount) {
                LOG_WARN("达到最大错误数，中断处理");
                break;
            }
        }
        
        // 更新最终状态
        {
            std::lock_guard<std::mutex> lock(mutex_);
            BatchOperationStatus& status = operationStatuses_[operationId];
            status.update(processedCount, successCount, failedCount, "操作已完成");
            status.setCompleted();
        }
        
        LOG_INFO("批量保存实体操作完成: {}, 总数: {}, 成功: {}, 失败: {}", operationId, totalCount, successCount, failedCount);
    } catch (const std::exception& e) {
        // 处理异常
        std::lock_guard<std::mutex> lock(mutex_);
        BatchOperationStatus& status = operationStatuses_[operationId];
        status.update(processedCount, successCount, failedCount, "操作发生异常");
        status.setError("批量保存实体异常: " + std::string(e.what()));
        status.setCompleted();
        
        LOG_ERROR("批量保存实体操作异常: {}, 错误: {}", operationId, e.what());
    }
}

// 批量保存关系的公共接口
std::string BatchOperationManager::batchSaveRelations(
    StorageInterfacePtr storage,
    const std::vector<knowledge::RelationPtr>& relations,
    const BatchOperationConfig& config) {
    
    if (!storage) {
        LOG_ERROR("存储适配器为空");
        return "";
    }
    
    if (relations.empty()) {
        LOG_WARN("关系列表为空，无需批量保存");
        return "";
    }
    
    std::string operationId = generateOperationId();
    
    // 初始化操作状态
    BatchOperationStatus status;
    status.totalItems = relations.size();
    status.currentStep = "准备批量保存关系";
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        operationStatuses_[operationId] = status;
        cancelFlags_[operationId] = false;
    }
    
    // 启动异步线程处理批量保存
    std::thread([this, operationId, storage, relations, config]() {
        processBatchSaveRelations(operationId, storage, relations, config);
    }).detach();
    
    LOG_INFO("批量保存关系操作已启动，ID: {}, 关系数量: {}", operationId, relations.size());
    
    return operationId;
}

// 这里可以继续实现其他批量操作函数
// 由于代码量较大，这里只实现了部分功能

// 简单实现知识图谱导入（仅JSON-LD格式）
std::string BatchOperationManager::importKnowledgeGraph(
    StorageInterfacePtr storage,
    const std::string& filePath,
    const std::string& format,
    const BatchOperationConfig& config) {
    
    if (!storage) {
        LOG_ERROR("存储适配器为空");
        return "";
    }
    
    if (format != "json-ld" && format != "rdf" && format != "csv") {
        LOG_ERROR("不支持的格式: {}", format);
        return "";
    }
    
    std::string operationId = generateOperationId();
    
    // 初始化操作状态
    BatchOperationStatus status;
    status.totalItems = 0; // 暂时不知道总数
    status.currentStep = "准备导入知识图谱";
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        operationStatuses_[operationId] = status;
        cancelFlags_[operationId] = false;
    }
    
    // 启动异步线程处理导入
    std::thread([this, operationId, storage, filePath, format, config]() {
        processImportKnowledgeGraph(operationId, storage, filePath, format, config);
    }).detach();
    
    LOG_INFO("知识图谱导入操作已启动，ID: {}, 文件: {}, 格式: {}", operationId, filePath, format);
    
    return operationId;
}

// 简单实现的知识图谱导入处理（仅JSON-LD格式）
void BatchOperationManager::processImportKnowledgeGraph(
    const std::string& operationId,
    StorageInterfacePtr storage,
    const std::string& filePath,
    const std::string& format,
    const BatchOperationConfig& config) {
    
    size_t successCount = 0;
    size_t failedCount = 0;
    size_t processedCount = 0;
    
    try {
        // 更新状态
        {
            std::lock_guard<std::mutex> lock(mutex_);
            BatchOperationStatus& status = operationStatuses_[operationId];
            status.currentStep = "正在读取文件";
        }
        
        if (format == "json-ld") {
            // 读取JSON-LD文件
            std::ifstream file(filePath);
            if (!file.is_open()) {
                throw std::runtime_error("无法打开文件: " + filePath);
            }
            
            nlohmann::json j;
            try {
                file >> j;
            } catch (const nlohmann::json::exception& e) {
                throw std::runtime_error("解析JSON文件失败: " + std::string(e.what()));
            }
            
            // 创建知识图谱
            std::string graphName = j.contains("name") ? j["name"].get<std::string>() : "imported_graph";
            std::string graphDescription = j.contains("description") ? j["description"].get<std::string>() : "";
            
            auto kg = std::make_shared<knowledge::KnowledgeGraph>(graphName, graphDescription);
            
            // 解析实体
            if (j.contains("entities") && j["entities"].is_array()) {
                std::vector<knowledge::EntityPtr> entities;
                
                for (const auto& entityJson : j["entities"]) {
                    try {
                        std::string id = entityJson.contains("id") ? entityJson["id"].get<std::string>() : "";
                        std::string name = entityJson.contains("name") ? entityJson["name"].get<std::string>() : "";
                        std::string type = entityJson.contains("type") ? entityJson["type"].get<std::string>() : "";
                        
                        if (id.empty()) {
                            LOG_WARN("实体缺少ID，跳过");
                            failedCount++;
                            continue;
                        }
                        
                        auto entity = std::make_shared<knowledge::Entity>(id, name, knowledge::stringToEntityType(type));
                        
                        // 解析属性
                        if (entityJson.contains("properties") && entityJson["properties"].is_object()) {
                            for (auto it = entityJson["properties"].begin(); it != entityJson["properties"].end(); ++it) {
                                entity->addProperty(it.key(), it.value().dump());
                            }
                        }
                        
                        entities.push_back(entity);
                        kg->addEntity(entity);
                        successCount++;
                    } catch (const std::exception& e) {
                        LOG_ERROR("解析实体失败: {}", std::string(e.what()));
                        failedCount++;
                    }
                    
                    processedCount++;
                }
                
                // 更新状态
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    BatchOperationStatus& status = operationStatuses_[operationId];
                    status.totalItems = j["entities"].size() + (j.contains("relations") ? j["relations"].size() : 0) + 
                                       (j.contains("triples") ? j["triples"].size() : 0);
                    status.update(processedCount, successCount, failedCount, "正在解析实体");
                }
                
                // 批量保存实体
                if (!entities.empty()) {
                    if (!storage->saveEntities(entities)) {
                        LOG_ERROR("批量保存实体失败");
                    }
                }
            }
            
            // 解析关系和三元组（省略具体实现）
            // ...
            
            // 最后保存知识图谱
            if (!storage->saveKnowledgeGraph(kg)) {
                LOG_ERROR("保存知识图谱失败: {}", graphName);
            } else {
                LOG_INFO("知识图谱导入成功: {}", graphName);
            }
        } else if (format == "rdf") {
            // RDF格式导入（未实现）
            throw std::runtime_error("RDF格式导入尚未实现");
        } else if (format == "csv") {
            // CSV格式导入（未实现）
            throw std::runtime_error("CSV格式导入尚未实现");
        }
        
        // 更新最终状态
        {
            std::lock_guard<std::mutex> lock(mutex_);
            BatchOperationStatus& status = operationStatuses_[operationId];
            status.update(processedCount, successCount, failedCount, "操作已完成");
            status.setCompleted();
        }
        
        LOG_INFO("知识图谱导入操作完成: {}, 总数: {}, 成功: {}, 失败: {}", operationId, processedCount, successCount, failedCount);
    } catch (const std::exception& e) {
        // 处理异常
        std::lock_guard<std::mutex> lock(mutex_);
        BatchOperationStatus& status = operationStatuses_[operationId];
        status.update(processedCount, successCount, failedCount, "操作发生异常");
        status.setError("知识图谱导入异常: " + std::string(e.what()));
        status.setCompleted();
        
        LOG_ERROR("知识图谱导入操作异常: {}, 错误: {}", operationId, e.what());
    }
}

// 更多功能将在后续实现...

} // namespace storage
} // namespace kb
 