/**
 * @file backup_manager.cpp
 * @brief 备份管理器实现
 */

#include "storage/backup_manager.h"
#include "common/logging.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <random>
#include <algorithm>
#include <thread>

namespace kb {
namespace storage {

namespace fs = std::filesystem;

// BackupMetadata JSON序列化/反序列化方法
std::string BackupMetadata::toJson() const {
    nlohmann::json j;
    j["id"] = id;
    j["name"] = name;
    j["description"] = description;
    j["path"] = path;
    j["type"] = (type == BackupType::FULL) ? "full" : "incremental";
    j["baseBackupId"] = baseBackupId;
    
    // 将时间戳转换为ISO 8601格式
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    j["timestamp"] = ss.str();
    
    j["sizeBytes"] = sizeBytes;
    
    switch (status) {
        case BackupStatus::PENDING:
            j["status"] = "pending";
            break;
        case BackupStatus::RUNNING:
            j["status"] = "running";
            break;
        case BackupStatus::COMPLETED:
            j["status"] = "completed";
            break;
        case BackupStatus::FAILED:
            j["status"] = "failed";
            break;
    }
    
    j["errorMessage"] = errorMessage;
    
    return j.dump(4);
}

bool BackupMetadata::fromJson(const std::string& json) {
    try {
        nlohmann::json j = nlohmann::json::parse(json);
        
        id = j["id"].get<std::string>();
        name = j["name"].get<std::string>();
        description = j["description"].get<std::string>();
        path = j["path"].get<std::string>();
        
        std::string typeStr = j["type"].get<std::string>();
        type = (typeStr == "full") ? BackupType::FULL : BackupType::INCREMENTAL;
        
        baseBackupId = j["baseBackupId"].get<std::string>();
        
        // 解析ISO 8601格式的时间戳
        std::string timestampStr = j["timestamp"].get<std::string>();
        std::tm tm = {};
        std::istringstream ss(timestampStr);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        timestamp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
        
        sizeBytes = j["sizeBytes"].get<size_t>();
        
        std::string statusStr = j["status"].get<std::string>();
        if (statusStr == "pending") {
            status = BackupStatus::PENDING;
        } else if (statusStr == "running") {
            status = BackupStatus::RUNNING;
        } else if (statusStr == "completed") {
            status = BackupStatus::COMPLETED;
        } else if (statusStr == "failed") {
            status = BackupStatus::FAILED;
        }
        
        errorMessage = j["errorMessage"].get<std::string>();
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("解析备份元数据JSON失败: {}", std::string(e.what()));
        return false;
    }
}

// BackupManager 实现
BackupManager& BackupManager::getInstance() {
    static BackupManager instance;
    return instance;
}

BackupManager::BackupManager() : initialized_(false) {
    LOG_INFO("备份管理器初始化");
}

bool BackupManager::initialize(const std::string& backupDir) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    backupDir_ = backupDir;
    
    // 确保备份目录存在
    try {
        if (!fs::exists(backupDir_)) {
            if (!fs::create_directories(backupDir_)) {
                LOG_ERROR("无法创建备份目录: {}", backupDir_);
                return false;
            }
        }
    } catch (const fs::filesystem_error& e) {
        LOG_ERROR("创建备份目录异常: {}", std::string(e.what()));
        return false;
    }
    
    // 加载备份元数据
    if (!loadBackupMetadata()) {
        LOG_WARN("加载备份元数据失败，将创建新的元数据");
        backups_.clear();
    }
    
    initialized_ = true;
    LOG_INFO("备份管理器初始化成功，备份目录: {}", backupDir_);
    return true;
}

std::string BackupManager::generateBackupId() const {
    // 生成随机备份ID
    const size_t ID_LENGTH = 8;
    static const char CHARACTERS[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<> distribution(0, sizeof(CHARACTERS) - 2);
    
    std::string id;
    id.reserve(ID_LENGTH);
    
    for (size_t i = 0; i < ID_LENGTH; ++i) {
        id.push_back(CHARACTERS[distribution(generator)]);
    }
    
    // 添加时间戳前缀
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream timestamp;
    timestamp << std::put_time(std::localtime(&now_time_t), "%Y%m%d%H%M%S");
    
    return "BK" + timestamp.str() + "_" + id;
}

bool BackupManager::loadBackupMetadata() {
    std::string metadataFile = backupDir_ + "/backup_metadata.json";
    
    if (!fs::exists(metadataFile)) {
        LOG_WARN("备份元数据文件不存在: {}", metadataFile);
        return false;
    }
    
    try {
        std::ifstream file(metadataFile);
        if (!file.is_open()) {
            LOG_ERROR("无法打开备份元数据文件: {}", metadataFile);
            return false;
        }
        
        nlohmann::json j;
        file >> j;
        
        backups_.clear();
        
        for (const auto& backupJson : j["backups"]) {
            BackupMetadata metadata;
            if (metadata.fromJson(backupJson.dump())) {
                backups_.push_back(metadata);
            } else {
                LOG_WARN("解析备份元数据失败，跳过");
            }
        }
        
        LOG_INFO("成功加载备份元数据，共 {} 个备份", backups_.size());
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("加载备份元数据异常: {}", std::string(e.what()));
        return false;
    }
}

bool BackupManager::saveBackupMetadata() const {
    std::string metadataFile = backupDir_ + "/backup_metadata.json";
    
    try {
        nlohmann::json j;
        nlohmann::json backupsArray = nlohmann::json::array();
        
        for (const auto& backup : backups_) {
            backupsArray.push_back(nlohmann::json::parse(backup.toJson()));
        }
        
        j["backups"] = backupsArray;
        j["lastUpdated"] = std::chrono::system_clock::now().time_since_epoch().count();
        
        std::ofstream file(metadataFile);
        if (!file.is_open()) {
            LOG_ERROR("无法打开备份元数据文件进行写入: {}", metadataFile);
            return false;
        }
        
        file << j.dump(4);
        file.close();
        
        LOG_INFO("成功保存备份元数据，共 {} 个备份", backups_.size());
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("保存备份元数据异常: {}", std::string(e.what()));
        return false;
    }
}

std::string BackupManager::createBackupDirectory(const std::string& backupId) const {
    std::string backupPath = backupDir_ + "/" + backupId;
    
    try {
        if (!fs::create_directories(backupPath)) {
            LOG_ERROR("无法创建备份目录: {}", backupPath);
            return "";
        }
    } catch (const fs::filesystem_error& e) {
        LOG_ERROR("创建备份目录异常: {}", std::string(e.what()));
        return "";
    }
    
    return backupPath;
}

std::string BackupManager::createFullBackup(
    StorageInterfacePtr storage,
    const std::string& name,
    const std::string& description,
    BackupProgressCallback callback) {
    
    if (!initialized_) {
        LOG_ERROR("备份管理器未初始化");
        return "";
    }
    
    if (!storage) {
        LOG_ERROR("存储适配器为空");
        return "";
    }
    
    std::string backupId = generateBackupId();
    
    // 创建备份元数据
    BackupMetadata metadata;
    metadata.id = backupId;
    metadata.name = name;
    metadata.description = description;
    metadata.type = BackupType::FULL;
    metadata.baseBackupId = "";
    metadata.timestamp = std::chrono::system_clock::now();
    metadata.sizeBytes = 0;
    metadata.status = BackupStatus::PENDING;
    metadata.errorMessage = "";
    
    // 创建备份目录
    std::string backupPath = createBackupDirectory(backupId);
    if (backupPath.empty()) {
        return "";
    }
    
    metadata.path = backupPath;
    
    // 更新备份列表
    {
        std::lock_guard<std::mutex> lock(mutex_);
        backups_.push_back(metadata);
        saveBackupMetadata();
    }
    
    // 启动异步线程执行备份
    std::thread([this, storage, backupId, metadata, callback]() mutable {
        // 更新状态为运行中
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& backup : backups_) {
                if (backup.id == backupId) {
                    backup.status = BackupStatus::RUNNING;
                    saveBackupMetadata();
                    break;
                }
            }
        }
        
        bool success = createFullBackupInternal(storage, backupId, metadata, callback);
        
        // 更新备份状态
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& backup : backups_) {
                if (backup.id == backupId) {
                    backup = metadata;  // 更新元数据
                    backup.status = success ? BackupStatus::COMPLETED : BackupStatus::FAILED;
                    if (!success && backup.errorMessage.empty()) {
                        backup.errorMessage = "创建全量备份失败";
                    }
                    saveBackupMetadata();
                    break;
                }
            }
        }
        
        LOG_INFO("全量备份 {} {}", (success ? "成功" : "失败"), backupId);
    }).detach();
    
    LOG_INFO("启动全量备份: {}", backupId);
    return backupId;
}

bool BackupManager::createFullBackupInternal(
    StorageInterfacePtr storage,
    const std::string& backupId,
    BackupMetadata& metadata,
    BackupProgressCallback callback) {
    
    try {
        // 目前只是一个基本实现框架
        if (callback) {
            callback(0.0f, "开始全量备份");
        }
        
        // 导出知识图谱列表
        if (callback) {
            callback(0.1f, "导出知识图谱列表");
        }
        
        std::vector<std::string> graphNames = storage->listKnowledgeGraphs();
        nlohmann::json graphsJson = nlohmann::json::array();
        
        for (const auto& name : graphNames) {
            graphsJson.push_back(name);
        }
        
        std::string graphsFile = metadata.path + "/graphs.json";
        std::ofstream graphsOut(graphsFile);
        graphsOut << graphsJson.dump(4);
        graphsOut.close();
        
        // 为每个知识图谱创建一个备份
        float progressPerGraph = 0.8f / graphNames.size();
        size_t totalSize = 0;
        
        for (size_t i = 0; i < graphNames.size(); ++i) {
            const auto& graphName = graphNames[i];
            
            if (callback) {
                callback(0.1f + i * progressPerGraph, "导出知识图谱: " + graphName);
            }
            
            // 导出知识图谱
            auto graph = storage->loadKnowledgeGraph(graphName);
            if (!graph) {
                LOG_ERROR("无法加载知识图谱: {}", graphName);
                continue;
            }
            
            // 导出为JSON
            nlohmann::json graphJson;
            graphJson["name"] = graph->getName();
            graphJson["description"] = graph->getDescription();
            
            // 导出实体
            nlohmann::json entitiesJson = nlohmann::json::array();
            auto entities = graph->getEntities();
            
            for (const auto& entity : entities) {
                nlohmann::json entityJson;
                entityJson["id"] = entity->getId();
                entityJson["name"] = entity->getName();
                entityJson["type"] = entity->getType();
                
                // 导出属性
                nlohmann::json propertiesJson = nlohmann::json::object();
                auto properties = entity->getProperties();
                for (const auto& prop : properties) {
                    propertiesJson[prop.first] = nlohmann::json::parse(prop.second);
                }
                
                entityJson["properties"] = propertiesJson;
                entitiesJson.push_back(entityJson);
            }
            
            graphJson["entities"] = entitiesJson;
            
            // 导出关系和三元组...
            // 省略具体实现，类似于实体的导出
            
            // 保存到文件
            std::string graphFile = metadata.path + "/" + graphName + ".json";
            std::ofstream graphOut(graphFile);
            std::string jsonStr = graphJson.dump(4);
            graphOut << jsonStr;
            graphOut.close();
            
            totalSize += jsonStr.size();
            
            if (callback) {
                callback(0.1f + (i + 1) * progressPerGraph, "完成知识图谱导出: " + graphName);
            }
        }
        
        // 创建备份摘要文件
        if (callback) {
            callback(0.9f, "创建备份摘要");
        }
        
        nlohmann::json summaryJson;
        summaryJson["id"] = metadata.id;
        summaryJson["name"] = metadata.name;
        summaryJson["description"] = metadata.description;
        summaryJson["type"] = "full";
        summaryJson["timestamp"] = std::chrono::system_clock::to_time_t(metadata.timestamp);
        summaryJson["graphs"] = graphNames;
        summaryJson["entityCount"] = 0;  // 这里应该是实际的数量
        summaryJson["relationCount"] = 0;  // 这里应该是实际的数量
        summaryJson["tripleCount"] = 0;  // 这里应该是实际的数量
        
        std::string summaryFile = metadata.path + "/summary.json";
        std::ofstream summaryOut(summaryFile);
        summaryOut << summaryJson.dump(4);
        summaryOut.close();
        
        // 更新备份大小
        metadata.sizeBytes = totalSize;
        
        if (callback) {
            callback(1.0f, "备份完成");
        }
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("创建全量备份异常: {}", std::string(e.what()));
        metadata.errorMessage = "备份异常: " + std::string(e.what());
        return false;
    }
}

std::vector<BackupMetadata> BackupManager::getBackupList() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        LOG_ERROR("备份管理器未初始化");
        return {};
    }
    
    return backups_;
}

BackupMetadata BackupManager::getBackupDetails(const std::string& backupId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        LOG_ERROR("备份管理器未初始化");
        BackupMetadata empty;
        empty.errorMessage = "备份管理器未初始化";
        return empty;
    }
    
    for (const auto& backup : backups_) {
        if (backup.id == backupId) {
            return backup;
        }
    }
    
    // 如果找不到，返回空元数据
    BackupMetadata empty;
    empty.errorMessage = "找不到备份: " + backupId;
    return empty;
}

bool BackupManager::deleteBackup(const std::string& backupId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        LOG_ERROR("备份管理器未初始化");
        return false;
    }
    
    for (auto it = backups_.begin(); it != backups_.end(); ++it) {
        if (it->id == backupId) {
            // 检查是否有增量备份依赖此备份
            bool hasDependencies = false;
            for (const auto& backup : backups_) {
                if (backup.type == BackupType::INCREMENTAL && backup.baseBackupId == backupId) {
                    hasDependencies = true;
                    break;
                }
            }
            
            if (hasDependencies) {
                LOG_ERROR("无法删除备份，有增量备份依赖它: {}", backupId);
                return false;
            }
            
            // 删除备份文件
            try {
                if (fs::exists(it->path)) {
                    fs::remove_all(it->path);
                }
            } catch (const fs::filesystem_error& e) {
                LOG_ERROR("删除备份文件异常: {}", std::string(e.what()));
                return false;
            }
            
            // 从列表中移除
            backups_.erase(it);
            saveBackupMetadata();
            
            LOG_INFO("成功删除备份: {}", backupId);
            return true;
        }
    }
    
    LOG_WARN("找不到要删除的备份: {}", backupId);
    return false;
}

// 在这里可以继续实现其他功能...

} // namespace storage
} // namespace kb
 