/**
 * @file version_control.cpp
 * @brief 版本控制系统
 */

#include "storage/version_control.h"
#include "common/logging.h"
#include "common/utils.h"
#include "nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <random>
#include <iomanip>

namespace kb {
namespace storage {

using json = nlohmann::json;
namespace fs = std::filesystem;

std::string VersionInfo::toJson() const {
    json j;
    j["id"] = id;
    j["graphName"] = graphName;
    j["description"] = description;
    // 将时间戳转换为ISO 8601格式字符串
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    j["timestamp"] = ss.str();
    j["author"] = author;
    j["addedEntities"] = addedEntities;
    j["modifiedEntities"] = modifiedEntities;
    j["deletedEntities"] = deletedEntities;
    j["addedRelations"] = addedRelations;
    j["modifiedRelations"] = modifiedRelations;
    j["deletedRelations"] = deletedRelations;
    j["addedTriples"] = addedTriples;
    j["modifiedTriples"] = modifiedTriples;
    j["deletedTriples"] = deletedTriples;
    return j.dump(4);
}

bool VersionInfo::fromJson(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);
        id = j["id"];
        graphName = j["graphName"];
        description = j["description"];
        
        // 将ISO 8601格式字符串转换为时间戳
        std::tm tm = {};
        std::istringstream ss(j["timestamp"].get<std::string>());
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
        timestamp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
        
        author = j["author"];
        
        // 解析实体和关系列表
        addedEntities = j["addedEntities"].get<std::vector<std::string>>();
        modifiedEntities = j["modifiedEntities"].get<std::vector<std::string>>();
        deletedEntities = j["deletedEntities"].get<std::vector<std::string>>();
        addedRelations = j["addedRelations"].get<std::vector<std::string>>();
        modifiedRelations = j["modifiedRelations"].get<std::vector<std::string>>();
        deletedRelations = j["deletedRelations"].get<std::vector<std::string>>();
        addedTriples = j["addedTriples"].get<std::vector<std::string>>();
        modifiedTriples = j["modifiedTriples"].get<std::vector<std::string>>();
        deletedTriples = j["deletedTriples"].get<std::vector<std::string>>();
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("解析版本信息JSON失败: {}", e.what());
        return false;
    }
}

VersionControl& VersionControl::getInstance() {
    static VersionControl instance;
    return instance;
}

VersionControl::VersionControl() : initialized_(false) {
    LOG_INFO("版本控制系统初始化");
}

bool VersionControl::initialize(StorageInterfacePtr storage, const std::string& versionDir) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        LOG_WARN("版本控制系统已经初始化");
        return true;
    }
    
    if (!storage) {
        LOG_ERROR("存储适配器为空");
        return false;
    }
    
    storage_ = storage;
    versionDir_ = versionDir;
    
    // 确保版本目录存在
    try {
        if (!fs::exists(versionDir_)) {
            LOG_INFO("创建版本目录: {}", versionDir_);
            fs::create_directories(versionDir_);
        }
    } catch (const fs::filesystem_error& e) {
        LOG_ERROR("创建版本目录失败: {}", e.what());
        return false;
    }
    
    // 加载版本元数据
    if (!loadVersionMetadata()) {
        LOG_WARN("加载版本元数据失败，将创建新的元数据");
        // 创建空的元数据文件仍继续
        saveVersionMetadata();
    }
    
    initialized_ = true;
    LOG_INFO("版本控制系统初始化完成，版本目录: {}", versionDir_);
    return true;
}

bool VersionControl::loadVersionMetadata() {
    std::string metadataPath = versionDir_ + "/metadata.json";
    
    if (!fs::exists(metadataPath)) {
        LOG_INFO("版本元数据文件不存在: {}", metadataPath);
        return false;
    }
    
    try {
        std::ifstream file(metadataPath);
        json j;
        file >> j;
        
        versions_.clear();
        graphVersions_.clear();
        
        for (auto& versionJson : j["versions"]) {
            VersionInfo version;
            if (version.fromJson(versionJson.dump())) {
                versions_[version.id] = version;
                graphVersions_[version.graphName].push_back(version.id);
            }
        }
        
        LOG_INFO("加载版本元数据完成，共加载 {} 个版本", versions_.size());
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("加载版本元数据失败: {}", e.what());
        return false;
    }
}

bool VersionControl::saveVersionMetadata() const {
    if (!initialized_) {
        LOG_ERROR("版本控制系统未初始化");
        return false;
    }
    
    std::string metadataPath = versionDir_ + "/metadata.json";
    
    try {
        json j;
        json versionsArray = json::array();
        
        for (const auto& pair : versions_) {
            json versionJson = json::parse(pair.second.toJson());
            versionsArray.push_back(versionJson);
        }
        
        j["versions"] = versionsArray;
        
        std::ofstream file(metadataPath);
        file << j.dump(4);
        
        LOG_INFO("保存版本元数据完成，共保存 {} 个版本", versions_.size());
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("保存版本元数据失败: {}", e.what());
        return false;
    }
}

std::string VersionControl::generateVersionId(const std::string& graphName) const {
    // 生成基于时间戳和随机字符串的版本ID
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    auto timestamp = now_ms.time_since_epoch().count();
    
    // 使用图谱名称的前8个字符，避免路径过长
    std::string graphPrefix = graphName.substr(0, 8);
    
    // 添加随机字符串
    std::string randomStr = utils::generateRandomString(8);
    
    std::stringstream ss;
    ss << graphPrefix << "_" << timestamp << "_" << randomStr;
    return ss.str();
}

std::string VersionControl::createVersionDirectory(const std::string& versionId) const {
    std::string versionPath = versionDir_ + "/" + versionId;
    
    try {
        if (!fs::exists(versionPath)) {
            LOG_INFO("创建版本目录: {}", versionPath);
            fs::create_directories(versionPath);
        }
    } catch (const fs::filesystem_error& e) {
        LOG_ERROR("创建版本目录失败: {}", e.what());
        return "";
    }
    
    return versionPath;
}

std::string VersionControl::createVersion(
    const knowledge::KnowledgeGraphPtr& graph,
    const std::string& description,
    const std::string& author) {
    
    if (!initialized_) {
        LOG_ERROR("版本控制系统未初始化");
        return "";
    }
    
    if (!graph) {
        LOG_ERROR("知识图谱为空");
        return "";
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 获取图谱名称
    std::string graphName = graph->getName();
    
    // 生成版本ID
    std::string versionId = generateVersionId(graphName);
    
    // 创建版本目录
    std::string versionPath = createVersionDirectory(versionId);
    if (versionPath.empty()) {
        return "";
    }
    
    // 创建版本信息
    VersionInfo versionInfo;
    versionInfo.id = versionId;
    versionInfo.graphName = graphName;
    versionInfo.description = description;
    versionInfo.timestamp = std::chrono::system_clock::now();
    versionInfo.author = author;
    
    // 如果存在旧版本，计算差异
    if (!graphVersions_[graphName].empty()) {
        std::string lastVersionId = graphVersions_[graphName].back();
        knowledge::KnowledgeGraphPtr lastVersionGraph = getVersionGraph(lastVersionId);
        
        if (lastVersionGraph) {
            // 计算与上一版本的差异
            versionInfo = calculateDiff(lastVersionGraph, graph);
            versionInfo.id = versionId;  // 重置为新的版本ID
            versionInfo.description = description;
            versionInfo.timestamp = std::chrono::system_clock::now();
            versionInfo.author = author;
        }
    } else {
        // 第一个版本，所有实体和关系都是新增的
        auto entities = graph->getAllEntities();
        auto relations = graph->getAllRelations();
        auto triples = graph->getAllTriples();
        
        for (const auto& entity : entities) {
            versionInfo.addedEntities.push_back(entity->getId());
        }
        
        for (const auto& relation : relations) {
            versionInfo.addedRelations.push_back(relation->getId());
        }
        
        for (const auto& triple : triples) {
            versionInfo.addedTriples.push_back(triple->getId());
        }
    }
    
    // 保存知识图谱版本
    if (!saveKnowledgeGraphVersion(versionId, graph)) {
        LOG_ERROR("保存知识图谱版本失败");
        return "";
    }
    
    // 更新版本信息
    versions_[versionId] = versionInfo;
    graphVersions_[graphName].push_back(versionId);
    
    // 保存元数据
    if (!saveVersionMetadata()) {
        LOG_ERROR("保存版本元数据失败");
        // 继续处理，不返回错误
    }
    
    LOG_INFO("创建版本成功: {} ({})", versionId, description);
    return versionId;
}

VersionInfo VersionControl::calculateDiff(
    const knowledge::KnowledgeGraphPtr& baseGraph,
    const knowledge::KnowledgeGraphPtr& newGraph) const {
    
    VersionInfo diff;
    diff.graphName = newGraph->getName();
    
    // 收集基础图谱的所有实体、关系和三元组ID
    std::map<std::string, knowledge::EntityPtr> baseEntities;
    std::map<std::string, knowledge::RelationPtr> baseRelations;
    std::map<std::string, knowledge::TriplePtr> baseTriples;
    
    for (const auto& entity : baseGraph->getAllEntities()) {
        baseEntities[entity->getId()] = entity;
    }
    
    for (const auto& relation : baseGraph->getAllRelations()) {
        baseRelations[relation->getId()] = relation;
    }
    
    for (const auto& triple : baseGraph->getAllTriples()) {
        baseTriples[triple->getId()] = triple;
    }
    
    // 比较新图谱中的实体
    for (const auto& entity : newGraph->getAllEntities()) {
        std::string entityId = entity->getId();
        auto it = baseEntities.find(entityId);
        
        if (it == baseEntities.end()) {
            // 新增实体
            diff.addedEntities.push_back(entityId);
        } else {
            // 检查实体是否被修改
            if (!entity->equals(*it->second)) {
                diff.modifiedEntities.push_back(entityId);
            }
            // 从基础集合中移除，剩下的将是已删除的
            baseEntities.erase(it);
        }
    }
    
    // 剩余的基础实体已被删除
    for (const auto& pair : baseEntities) {
        diff.deletedEntities.push_back(pair.first);
    }
    
    // 比较新图谱中的关系
    for (const auto& relation : newGraph->getAllRelations()) {
        std::string relationId = relation->getId();
        auto it = baseRelations.find(relationId);
        
        if (it == baseRelations.end()) {
            // 新增关系
            diff.addedRelations.push_back(relationId);
        } else {
            // 检查关系是否被修改
            if (!relation->equals(*it->second)) {
                diff.modifiedRelations.push_back(relationId);
            }
            // 从基础集合中移除，剩下的将是已删除的
            baseRelations.erase(it);
        }
    }
    
    // 剩余的基础关系已被删除
    for (const auto& pair : baseRelations) {
        diff.deletedRelations.push_back(pair.first);
    }
    
    // 比较新图谱中的三元组
    for (const auto& triple : newGraph->getAllTriples()) {
        std::string tripleId = triple->getId();
        auto it = baseTriples.find(tripleId);
        
        if (it == baseTriples.end()) {
            // 新增三元组
            diff.addedTriples.push_back(tripleId);
        } else {
            // 检查三元组是否被修改
            if (!triple->equals(*it->second)) {
                diff.modifiedTriples.push_back(tripleId);
            }
            // 从基础集合中移除，剩下的将是已删除的
            baseTriples.erase(it);
        }
    }
    
    // 剩余的基础三元组已被删除
    for (const auto& pair : baseTriples) {
        diff.deletedTriples.push_back(pair.first);
    }
    
    return diff;
}

bool VersionControl::saveKnowledgeGraphVersion(
    const std::string& versionId,
    const knowledge::KnowledgeGraphPtr& graph) const {
    
    if (!initialized_) {
        LOG_ERROR("版本控制系统未初始化");
        return false;
    }
    
    if (!graph) {
        LOG_ERROR("知识图谱为空");
        return false;
    }
    
    std::string versionPath = versionDir_ + "/" + versionId;
    
    try {
        // 确保版本目录存在
        if (!fs::exists(versionPath)) {
            LOG_INFO("创建版本目录: {}", versionPath);
            fs::create_directories(versionPath);
        }
        
        // 保存图谱基本信息
        json graphInfo;
        graphInfo["name"] = graph->getName();
        graphInfo["description"] = "";
        
        std::ofstream graphFile(versionPath + "/graph_info.json");
        graphFile << graphInfo.dump(4);
        
        // 保存实体
        json entitiesJson = json::array();
        for (const auto& entity : graph->getAllEntities()) {
            json entityJson;
            entityJson["id"] = entity->getId();
            entityJson["type"] = entity->getType();
            entityJson["name"] = entity->getName();
            
            // 保存属性
            json propertiesJson = json::object();
            for (const auto& prop : entity->getAllProperties()) {
                propertiesJson[prop.first] = prop.second;
            }
            entityJson["properties"] = propertiesJson;
            
            entitiesJson.push_back(entityJson);
        }
        
        std::ofstream entitiesFile(versionPath + "/entities.json");
        entitiesFile << entitiesJson.dump(4);
        
        // 保存关系
        json relationsJson = json::array();
        for (const auto& relation : graph->getAllRelations()) {
            json relationJson;
            relationJson["id"] = relation->getId();
            relationJson["type"] = relation->getType();
            relationJson["name"] = relation->getName();
            
            // 保存属性
            json propertiesJson = json::object();
            for (const auto& prop : relation->getAllProperties()) {
                propertiesJson[prop.first] = prop.second;
            }
            relationJson["properties"] = propertiesJson;
            
            relationsJson.push_back(relationJson);
        }
        
        std::ofstream relationsFile(versionPath + "/relations.json");
        relationsFile << relationsJson.dump(4);
        
        // 保存三元组
        json triplesJson = json::array();
        for (const auto& triple : graph->getAllTriples()) {
            json tripleJson;
            tripleJson["id"] = triple->getId();
            tripleJson["subject_id"] = triple->getSubject()->getId();
            tripleJson["relation_id"] = triple->getRelation()->getId();
            tripleJson["object_id"] = triple->getObject()->getId();
            tripleJson["confidence"] = triple->getConfidence();
            
            // 保存属性
            json propertiesJson = json::object();
            for (const auto& prop : triple->getAllProperties()) {
                propertiesJson[prop.first] = prop.second;
            }
            tripleJson["properties"] = propertiesJson;
            
            triplesJson.push_back(tripleJson);
        }
        
        std::ofstream triplesFile(versionPath + "/triples.json");
        triplesFile << triplesJson.dump(4);
        
        LOG_INFO("保存知识图谱版本成功: {}", versionId);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("保存知识图谱版本失败: {}", e.what());
        return false;
    }
}

knowledge::KnowledgeGraphPtr VersionControl::loadKnowledgeGraphVersion(const std::string& versionId) const {
    if (!initialized_) {
        LOG_ERROR("版本控制系统未初始化");
        return nullptr;
    }
    
    std::string versionPath = versionDir_ + "/" + versionId;
    
    if (!fs::exists(versionPath)) {
        LOG_ERROR("版本目录不存在: {}", versionPath);
        return nullptr;
    }
    
    try {
        // 加载图谱基本信息
        std::ifstream graphFile(versionPath + "/graph_info.json");
        if (!graphFile.is_open()) {
            LOG_ERROR("无法打开图谱信息文件: {}/graph_info.json", versionPath);
            return nullptr;
        }
        
        json graphInfo;
        graphFile >> graphInfo;
        
        std::string graphName = graphInfo["name"];
        std::string graphDescription = graphInfo["description"];
        
        // 创建新的知识图谱
        auto graph = std::make_shared<knowledge::KnowledgeGraph>(graphName);
        
        // 加载实体
        std::map<std::string, knowledge::EntityPtr> entitiesMap;
        std::ifstream entitiesFile(versionPath + "/entities.json");
        if (entitiesFile.is_open()) {
            json entitiesJson;
            entitiesFile >> entitiesJson;
            
            for (const auto& entityJson : entitiesJson) {
                std::string id = entityJson["id"];
                std::string type = entityJson["type"];
                std::string name = entityJson["name"];
                
                auto entity = std::make_shared<knowledge::Entity>(id, name, knowledge::stringToEntityType(type));
                
                // 加载属性
                if (entityJson.contains("properties") && entityJson["properties"].is_object()) {
                    for (auto it = entityJson["properties"].begin(); it != entityJson["properties"].end(); ++it) {
                        entity->addProperty(it.key(), it.value());
                    }
                }
                
                entitiesMap[id] = entity;
                graph->addEntity(entity);
            }
        }
        
        // 加载关系
        std::map<std::string, knowledge::RelationPtr> relationsMap;
        std::ifstream relationsFile(versionPath + "/relations.json");
        if (relationsFile.is_open()) {
            json relationsJson;
            relationsFile >> relationsJson;
            
            for (const auto& relationJson : relationsJson) {
                std::string id = relationJson["id"];
                std::string type = relationJson["type"];
                std::string name = relationJson["name"];
                
                // 将字符串类型转换为RelationType枚举
                knowledge::RelationType relationType = knowledge::stringToRelationType(type);

                auto relation = std::make_shared<knowledge::Relation>(id, name, relationType);
                
                // 加载属性
                if (relationJson.contains("properties") && relationJson["properties"].is_object()) {
                    for (auto it = relationJson["properties"].begin(); it != relationJson["properties"].end(); ++it) {
                        relation->addProperty(it.key(), it.value());
                    }
                }
                
                relationsMap[id] = relation;
                graph->addRelation(relation);
            }
        }
        
        // 加载三元组
        std::ifstream triplesFile(versionPath + "/triples.json");
        if (triplesFile.is_open()) {
            json triplesJson;
            triplesFile >> triplesJson;
            
            for (const auto& tripleJson : triplesJson) {
                std::string id = tripleJson["id"];
                std::string subjectId = tripleJson["subject_id"];
                std::string relationId = tripleJson["relation_id"];
                std::string objectId = tripleJson["object_id"];
                float confidence = tripleJson["confidence"];
                
                // 查找实体和关系
                auto subjectIt = entitiesMap.find(subjectId);
                auto relationIt = relationsMap.find(relationId);
                auto objectIt = entitiesMap.find(objectId);
                
                if (subjectIt != entitiesMap.end() && 
                    relationIt != relationsMap.end() && 
                    objectIt != entitiesMap.end()) {
                    
                    auto triple = std::make_shared<knowledge::Triple>(
                        subjectIt->second, 
                        relationIt->second, 
                        objectIt->second,
                        confidence);
                    
                    // 验证生成的ID与存储的ID是否一致
                    std::string generatedId = triple->getId();
                    
                    // 如果ID不匹配，可以记录警告
                    if (generatedId != id) {
                        LOG_WARN("三元组ID不匹配：生成的ID {} 与存储的ID {} 不同", generatedId, id);
                    }
                    
                    // 加载属性
                    if (tripleJson.contains("properties") && tripleJson["properties"].is_object()) {
                        for (auto it = tripleJson["properties"].begin(); it != tripleJson["properties"].end(); ++it) {
                            triple->addProperty(it.key(), it.value());
                        }
                    }
                    
                    graph->addTriple(triple);
                } else {
                    LOG_WARN("三元组 {} 引用的实体或关系不存在", id);
                }
            }
        }
        
        LOG_INFO("加载知识图谱版本成功: {}", versionId);
        return graph;
    } catch (const std::exception& e) {
        LOG_ERROR("加载知识图谱版本失败: {}", e.what());
        return nullptr;
    }
}

std::vector<VersionInfo> VersionControl::getVersions(const std::string& graphName) const {
    if (!initialized_) {
        LOG_ERROR("版本控制系统未初始化");
        return {};
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<VersionInfo> result;
    auto it = graphVersions_.find(graphName);
    
    if (it != graphVersions_.end()) {
        for (const auto& versionId : it->second) {
            auto versionIt = versions_.find(versionId);
            if (versionIt != versions_.end()) {
                result.push_back(versionIt->second);
            }
        }
    }
    
    return result;
}

VersionInfo VersionControl::getVersionDetails(const std::string& versionId) const {
    if (!initialized_) {
        LOG_ERROR("版本控制系统未初始化");
        return VersionInfo();
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = versions_.find(versionId);
    if (it != versions_.end()) {
        return it->second;
    }
    
    LOG_ERROR("版本不存在: {}", versionId);
    return VersionInfo();
}

knowledge::KnowledgeGraphPtr VersionControl::getVersionGraph(const std::string& versionId) const {
    if (!initialized_) {
        LOG_ERROR("版本控制系统未初始化");
        return nullptr;
    }
    
    auto it = versions_.find(versionId);
    if (it == versions_.end()) {
        LOG_ERROR("版本不存在: {}", versionId);
        return nullptr;
    }
    
    return loadKnowledgeGraphVersion(versionId);
}

bool VersionControl::rollbackToVersion(const std::string& versionId) {
    if (!initialized_) {
        LOG_ERROR("版本控制系统未初始化");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = versions_.find(versionId);
    if (it == versions_.end()) {
        LOG_ERROR("版本不存在: {}", versionId);
        return false;
    }
    
    // 加载指定版本的图谱
    knowledge::KnowledgeGraphPtr graph = loadKnowledgeGraphVersion(versionId);
    if (!graph) {
        LOG_ERROR("加载版本图谱失败: {}", versionId);
        return false;
    }
    
    // 使用存储适配器保存图谱，实现回滚
    if (!storage_->saveKnowledgeGraph(graph)) {
        LOG_ERROR("回滚图谱失败: {}", versionId);
        return false;
    }
    
    LOG_INFO("成功回滚到版本: {}", versionId);
    return true;
}

std::string VersionControl::compareVersions(const std::string& versionId1, const std::string& versionId2) const {
    if (!initialized_) {
        LOG_ERROR("版本控制系统未初始化");
        return "{}";
    }
    
    auto it1 = versions_.find(versionId1);
    auto it2 = versions_.find(versionId2);
    
    if (it1 == versions_.end() || it2 == versions_.end()) {
        LOG_ERROR("版本不存在: {} 或 {}", versionId1, versionId2);
        return "{}";
    }
    
    // 加载两个版本的图谱
    knowledge::KnowledgeGraphPtr graph1 = loadKnowledgeGraphVersion(versionId1);
    knowledge::KnowledgeGraphPtr graph2 = loadKnowledgeGraphVersion(versionId2);
    
    if (!graph1 || !graph2) {
        LOG_ERROR("加载版本图谱失败");
        return "{}";
    }
    
    // 计算差异
    VersionInfo diff = calculateDiff(graph1, graph2);
    
    // 创建比较结果JSON
    json result;
    result["version1"] = versionId1;
    result["version2"] = versionId2;
    result["differences"] = json::parse(diff.toJson());
    
    return result.dump(4);
}

bool VersionControl::exportVersion(
    const std::string& versionId,
    const std::string& exportPath,
    const std::string& format) const {
    
    if (!initialized_) {
        LOG_ERROR("版本控制系统未初始化");
        return false;
    }
    
    auto it = versions_.find(versionId);
    if (it == versions_.end()) {
        LOG_ERROR("版本不存在: {}", versionId);
        return false;
    }
    
    // 加载指定版本的图谱
    knowledge::KnowledgeGraphPtr graph = loadKnowledgeGraphVersion(versionId);
    if (!graph) {
        LOG_ERROR("加载版本图谱失败: {}", versionId);
        return false;
    }
    
    try {
        if (format == "json-ld") {
            // 导出为JSON-LD格式
            std::string versionDir = versionDir_ + "/" + versionId;
            std::string targetDir = exportPath;
            
            // 确保目标目录存在
            if (!fs::exists(targetDir)) {
                fs::create_directories(targetDir);
            }
            
            // 复制版本文件到目标目录
            fs::copy(versionDir + "/graph_info.json", targetDir + "/graph_info.json", fs::copy_options::overwrite_existing);
            fs::copy(versionDir + "/entities.json", targetDir + "/entities.json", fs::copy_options::overwrite_existing);
            fs::copy(versionDir + "/relations.json", targetDir + "/relations.json", fs::copy_options::overwrite_existing);
            fs::copy(versionDir + "/triples.json", targetDir + "/triples.json", fs::copy_options::overwrite_existing);
            
            // 创建JSON-LD上下文文件
            json context;
            context["@context"] = {
                {"@vocab", "http://example.org/ontology#"},
                {"name", "@id"},
                {"type", "@type"},
                {"subject", {"@id", "subject_id"}},
                {"relation", {"@id", "relation_id"}},
                {"object", {"@id", "object_id"}}
            };
            
            std::ofstream contextFile(targetDir + "/context.jsonld");
            contextFile << context.dump(4);
            
            LOG_INFO("导出版本 {} 到 {} 成功", versionId, exportPath);
            return true;
        } else {
            LOG_ERROR("不支持的导出格式: {}", format);
            return false;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("导出版本失败: {}", e.what());
        return false;
    }
}

bool VersionControl::setVersionDirectory(const std::string& versionDir) {
    if (initialized_) {
        LOG_ERROR("版本控制系统已初始化，无法更改版本目录");
        return false;
    }
    
    versionDir_ = versionDir;
    LOG_INFO("设置版本目录为: {}", versionDir_);
    return true;
}

std::string VersionControl::getVersionDirectory() const {
    return versionDir_;
}

} // namespace storage
} // namespace kb 