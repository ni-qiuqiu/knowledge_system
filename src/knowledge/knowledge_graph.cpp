#include "knowledge/knowledge_graph.h"
#include "common/logging.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace kb {
namespace knowledge {

// 构造函数
KnowledgeGraph::KnowledgeGraph(const std::string& name)
    : name_(name) {
    LOG_INFO("创建知识图谱: {}" , name_);
}

// 构造函数
KnowledgeGraph::KnowledgeGraph(const std::string& name, const std::string& description)
    : name_(name), description_(description) {
    LOG_INFO("创建知识图谱: {}" , name_);
}

// 获取知识图谱名称
const std::string& KnowledgeGraph::getName() const {
    return name_;
}

// 设置知识图谱名称
void KnowledgeGraph::setName(const std::string& name) {
    name_ = name;
}

// 添加实体
bool KnowledgeGraph::addEntity(EntityPtr entity) {
    if (!entity) {
        LOG_ERROR("尝试添加空实体");
        return false;
    }
    
    const std::string& id = entity->getId();
    if (id.empty()) {
        LOG_ERROR("尝试添加ID为空的实体");
        return false;
    }
    
    if (entities_.find(id) != entities_.end()) {
        LOG_WARN("实体ID已存在: {}" , id);
        return false;
    }
    
    entities_[id] = entity;
    entityTypeIndex_[entity->getType()].insert(id);
    
    LOG_DEBUG("添加实体: {}" , entity->toString());
    return true;
}

// 获取实体
EntityPtr KnowledgeGraph::getEntity(const std::string& id) const {
    auto it = entities_.find(id);
    if (it != entities_.end()) {
        return it->second;
    }
    return nullptr;
}

// 根据名称查找实体
std::vector<EntityPtr> KnowledgeGraph::findEntitiesByName(const std::string& name) const {
    std::vector<EntityPtr> result;
    
    for (const auto& pair : entities_) {
        if (pair.second->getName() == name) {
            result.push_back(pair.second);
        }
    }
    
    return result;
}

// 根据类型查找实体
std::vector<EntityPtr> KnowledgeGraph::findEntitiesByType(EntityType type) const {
    std::vector<EntityPtr> result;
    
    auto it = entityTypeIndex_.find(type);
    if (it != entityTypeIndex_.end()) {
        const auto& idSet = it->second;
        for (const auto& id : idSet) {
            auto entityIt = entities_.find(id);
            if (entityIt != entities_.end()) {
                result.push_back(entityIt->second);
            }
        }
    }
    
    return result;
}

// 移除实体
bool KnowledgeGraph::removeEntity(const std::string& id) {
    auto it = entities_.find(id);
    if (it == entities_.end()) {
        return false;
    }
    
    EntityPtr entity = it->second;
    
    // 从类型索引中移除
    auto typeIt = entityTypeIndex_.find(entity->getType());
    if (typeIt != entityTypeIndex_.end()) {
        typeIt->second.erase(id);
    }
    
    // 移除相关的三元组
    auto subjTriples = findTriplesBySubject(id);
    for (const auto& triple : subjTriples) {
        removeTriple(triple->getId());
    }
    
    auto objTriples = findTriplesByObject(id);
    for (const auto& triple : objTriples) {
        removeTriple(triple->getId());
    }
    
    // 移除实体
    entities_.erase(it);
    
    LOG_DEBUG("移除实体: {}" , id);
    return true;
}

// 获取所有实体
std::vector<EntityPtr> KnowledgeGraph::getAllEntities() const {
    std::vector<EntityPtr> result;
    result.reserve(entities_.size());
    
    for (const auto& pair : entities_) {
        result.push_back(pair.second);
    }
    
    return result;
}

// 获取实体数量
size_t KnowledgeGraph::getEntityCount() const {
    return entities_.size();
}

// 添加关系
bool KnowledgeGraph::addRelation(RelationPtr relation) {
    if (!relation) {
        LOG_ERROR("尝试添加空关系");
        return false;
    }
    
    const std::string& id = relation->getId();
    if (id.empty()) {
        LOG_ERROR("尝试添加ID为空的关系");
        return false;
    }
    
    if (relations_.find(id) != relations_.end()) {
        LOG_WARN("关系ID已存在: {}" , id);
        return false;
    }
    
    relations_[id] = relation;
    relationTypeIndex_[relation->getType()].insert(id);
    
    LOG_DEBUG("添加关系: {}" , relation->toString());
    return true;
}

// 获取关系
RelationPtr KnowledgeGraph::getRelation(const std::string& id) const {
    auto it = relations_.find(id);
    if (it != relations_.end()) {
        return it->second;
    }
    return nullptr;
}

// 根据名称查找关系
std::vector<RelationPtr> KnowledgeGraph::findRelationsByName(const std::string& name) const {
    std::vector<RelationPtr> result;
    
    for (const auto& pair : relations_) {
        if (pair.second->getName() == name) {
            result.push_back(pair.second);
        }
    }
    
    return result;
}

// 根据类型查找关系
std::vector<RelationPtr> KnowledgeGraph::findRelationsByType(RelationType type) const {
    std::vector<RelationPtr> result;
    
    auto it = relationTypeIndex_.find(type);
    if (it != relationTypeIndex_.end()) {
        const auto& idSet = it->second;
        for (const auto& id : idSet) {
            auto relationIt = relations_.find(id);
            if (relationIt != relations_.end()) {
                result.push_back(relationIt->second);
            }
        }
    }
    
    return result;
}

// 移除关系
bool KnowledgeGraph::removeRelation(const std::string& id) {
    auto it = relations_.find(id);
    if (it == relations_.end()) {
        return false;
    }
    
    RelationPtr relation = it->second;
    
    // 从类型索引中移除
    auto typeIt = relationTypeIndex_.find(relation->getType());
    if (typeIt != relationTypeIndex_.end()) {
        typeIt->second.erase(id);
    }
    
    // 移除使用此关系的三元组
    auto relTriples = findTriplesByRelation(id);
    for (const auto& triple : relTriples) {
        removeTriple(triple->getId());
    }
    
    // 移除关系
    relations_.erase(it);
    
    LOG_DEBUG("移除关系: {}" , id);
    return true;
}

// 获取所有关系
std::vector<RelationPtr> KnowledgeGraph::getAllRelations() const {
    std::vector<RelationPtr> result;
    result.reserve(relations_.size());
    
    for (const auto& pair : relations_) {
        result.push_back(pair.second);
    }
    
    return result;
}

// 获取关系数量
size_t KnowledgeGraph::getRelationCount() const {
    return relations_.size();
}

// 添加三元组
bool KnowledgeGraph::addTriple(TriplePtr triple) {
    if (!triple) {
        LOG_ERROR("尝试添加空三元组");
        return false;
    }
    
    if (!validateTriple(triple)) {
        LOG_ERROR("三元组验证失败");
        return false;
    }
    
    const std::string id = triple->getId();
    if (triples_.find(id) != triples_.end()) {
        LOG_WARN("三元组已存在: {}" , id);
        return false;
    }
    
    triples_[id] = triple;
    addToIndices(triple);
    
    LOG_DEBUG("添加三元组: {}" , triple->toString());
    return true;
}

// 添加三元组（使用ID）
TriplePtr KnowledgeGraph::addTriple(const std::string& subjectId, 
                                  const std::string& relationId, 
                                  const std::string& objectId,
                                  float confidence) {
    EntityPtr subject = getEntity(subjectId);
    RelationPtr relation = getRelation(relationId);
    EntityPtr object = getEntity(objectId);
    
    if (!subject || !relation || !object) {
        LOG_ERROR("创建三元组失败：实体或关系不存在");
        return nullptr;
    }
    
    TriplePtr triple = Triple::create(subject, relation, object, confidence);
    if (addTriple(triple)) {
        return triple;
    }
    
    return nullptr;
}

// 获取三元组
TriplePtr KnowledgeGraph::getTriple(const std::string& id) const {
    auto it = triples_.find(id);
    if (it != triples_.end()) {
        return it->second;
    }
    return nullptr;
}

// 移除三元组
bool KnowledgeGraph::removeTriple(const std::string& id) {
    auto it = triples_.find(id);
    if (it == triples_.end()) {
        return false;
    }
    
    TriplePtr triple = it->second;
    removeFromIndices(triple);
    triples_.erase(it);
    
    LOG_DEBUG("移除三元组: {}" , id);
    return true;
}

// 获取所有三元组
TripleSet KnowledgeGraph::getAllTriples() const {
    TripleSet result;
    result.reserve(triples_.size());
    
    for (const auto& pair : triples_) {
        result.push_back(pair.second);
    }
    
    return result;
}

// 获取三元组数量
size_t KnowledgeGraph::getTripleCount() const {
    return triples_.size();
}

// 查询以指定实体为主体的三元组
TripleSet KnowledgeGraph::findTriplesBySubject(const std::string& subjectId) const {
    TripleSet result;
    
    auto it = subjectIndex_.find(subjectId);
    if (it != subjectIndex_.end()) {
        const auto& tripleIds = it->second;
        for (const auto& id : tripleIds) {
            auto tripleIt = triples_.find(id);
            if (tripleIt != triples_.end()) {
                result.push_back(tripleIt->second);
            }
        }
    }
    
    return result;
}

// 查询以指定实体为客体的三元组
TripleSet KnowledgeGraph::findTriplesByObject(const std::string& objectId) const {
    TripleSet result;
    
    auto it = objectIndex_.find(objectId);
    if (it != objectIndex_.end()) {
        const auto& tripleIds = it->second;
        for (const auto& id : tripleIds) {
            auto tripleIt = triples_.find(id);
            if (tripleIt != triples_.end()) {
                result.push_back(tripleIt->second);
            }
        }
    }
    
    return result;
}

// 查询使用指定关系的三元组
TripleSet KnowledgeGraph::findTriplesByRelation(const std::string& relationId) const {
    TripleSet result;
    
    auto it = relationIndex_.find(relationId);
    if (it != relationIndex_.end()) {
        const auto& tripleIds = it->second;
        for (const auto& id : tripleIds) {
            auto tripleIt = triples_.find(id);
            if (tripleIt != triples_.end()) {
                result.push_back(tripleIt->second);
            }
        }
    }
    
    return result;
}

// 查询特定主体和关系的三元组
TripleSet KnowledgeGraph::findTriplesBySubjectAndRelation(const std::string& subjectId, 
                                                        const std::string& relationId) const {
    TripleSet result;
    
    // 获取主体相关的三元组
    auto subjTriples = findTriplesBySubject(subjectId);
    
    // 过滤关系
    for (const auto& triple : subjTriples) {
        if (triple->getRelation()->getId() == relationId) {
            result.push_back(triple);
        }
    }
    
    return result;
}

// 查询特定关系和客体的三元组
TripleSet KnowledgeGraph::findTriplesByRelationAndObject(const std::string& relationId, 
                                                       const std::string& objectId) const {
    TripleSet result;
    
    // 获取客体相关的三元组
    auto objTriples = findTriplesByObject(objectId);
    
    // 过滤关系
    for (const auto& triple : objTriples) {
        if (triple->getRelation()->getId() == relationId) {
            result.push_back(triple);
        }
    }
    
    return result;
}

// 查询特定主体和客体的三元组
TripleSet KnowledgeGraph::findTriplesBySubjectAndObject(const std::string& subjectId, 
                                                      const std::string& objectId) const {
    TripleSet result;
    
    // 获取主体相关的三元组
    auto subjTriples = findTriplesBySubject(subjectId);
    
    // 过滤客体
    for (const auto& triple : subjTriples) {
        if (triple->getObject()->getId() == objectId) {
            result.push_back(triple);
        }
    }
    
    return result;
}

// 合并另一个知识图谱
std::pair<size_t, size_t> KnowledgeGraph::merge(const KnowledgeGraph& other, 
                                              std::function<bool(const TriplePtr&, const TriplePtr&)> conflictStrategy) {
    size_t addedEntities = 0;
    size_t addedTriples = 0;
    
    // 合并实体
    for (const auto& pair : other.entities_) {
        const EntityPtr& entity = pair.second;
        const std::string& id = entity->getId();
        
        if (entities_.find(id) == entities_.end()) {
            // 新实体，直接添加
            if (addEntity(entity)) {
                addedEntities++;
            }
        }
    }
    
    // 合并关系
    for (const auto& pair : other.relations_) {
        const RelationPtr& relation = pair.second;
        const std::string& id = relation->getId();
        
        if (relations_.find(id) == relations_.end()) {
            // 新关系，直接添加
            addRelation(relation);
        }
    }
    
    // 合并三元组
    for (const auto& pair : other.triples_) {
        const TriplePtr& triple = pair.second;
        const std::string& id = triple->getId();
        
        auto it = triples_.find(id);
        if (it == triples_.end()) {
            // 新三元组，直接添加
            if (addTriple(triple)) {
                addedTriples++;
            }
        } else if (conflictStrategy) {
            // 存在冲突，使用策略处理
            const TriplePtr& existingTriple = it->second;
            if (conflictStrategy(triple, existingTriple)) {
                // 使用新三元组替换旧三元组
                removeTriple(id);
                if (addTriple(triple)) {
                    addedTriples++;
                }
            }
        }
    }
    
    LOG_INFO("合并图谱完成，添加了 {} 个实体和 {} 个三元组" , addedEntities , addedTriples);
    
    return {addedEntities, addedTriples};
}

// 从知识图谱中提取子图
std::shared_ptr<KnowledgeGraph> KnowledgeGraph::extractSubgraph(
    const std::unordered_set<std::string>& entityIds, 
    bool includeConnected) const {
    
    auto subgraph = std::make_shared<KnowledgeGraph>(name_ + "_subgraph");
    std::unordered_set<std::string> includedEntityIds = entityIds;
    
    // 如果需要包含相连的实体，先找出所有相连的实体ID
    if (includeConnected) {
        for (const auto& id : entityIds) {
            // 获取作为主体的三元组
            auto subjTriples = findTriplesBySubject(id);
            for (const auto& triple : subjTriples) {
                includedEntityIds.insert(triple->getObject()->getId());
            }
            
            // 获取作为客体的三元组
            auto objTriples = findTriplesByObject(id);
            for (const auto& triple : objTriples) {
                includedEntityIds.insert(triple->getSubject()->getId());
            }
        }
    }
    
    // 添加实体
    for (const auto& id : includedEntityIds) {
        auto entity = getEntity(id);
        if (entity) {
            subgraph->addEntity(entity);
        }
    }
    
    // 添加关系和三元组
    for (const auto& entityId : includedEntityIds) {
        // 添加以此实体为主体的三元组
        auto subjTriples = findTriplesBySubject(entityId);
        for (const auto& triple : subjTriples) {
            // 如果客体实体也在子图中，添加此三元组
            if (includedEntityIds.find(triple->getObject()->getId()) != includedEntityIds.end()) {
                // 确保关系存在
                auto relation = triple->getRelation();
                subgraph->addRelation(relation);
                
                // 添加三元组
                subgraph->addTriple(triple);
            }
        }
    }
    
    LOG_INFO("提取子图完成，包含 {} 个实体和 {} 个三元组" , subgraph->getEntityCount() , subgraph->getTripleCount());
    
    return subgraph;
}

// 保存知识图谱到文件
bool KnowledgeGraph::save(const std::string& filepath) const {
    try {
        std::ofstream ofs(filepath);
        if (!ofs) {
            LOG_ERROR("无法打开文件保存知识图谱: {}" , filepath);
            return false;
        }
        
        // 使用JSON格式保存
        ofs << "{\n";
        
        // 保存图谱名称
        ofs << "  \"name\": \"" << name_ << "\",\n";
        
        // 保存实体
        ofs << "  \"entities\": [\n";
        bool firstEntity = true;
        for (const auto& pair : entities_) {
            const EntityPtr& entity = pair.second;
            
            if (!firstEntity) {
                ofs << ",\n";
            }
            
            ofs << "    {\n";
            ofs << "      \"id\": \"" << entity->getId() << "\",\n";
            ofs << "      \"name\": \"" << entity->getName() << "\",\n";
            ofs << "      \"type\": \"" << entityTypeToString(entity->getType()) << "\"";
            
            // 保存属性
            const auto& props = entity->getAllProperties();
            if (!props.empty()) {
                ofs << ",\n      \"properties\": {\n";
                bool firstProp = true;
                for (const auto& propPair : props) {
                    if (!firstProp) {
                        ofs << ",\n";
                    }
                    ofs << "        \"" << propPair.first << "\": \"" << propPair.second << "\"";
                    firstProp = false;
                }
                ofs << "\n      }";
            }
            
            ofs << "\n    }";
            firstEntity = false;
        }
        ofs << "\n  ],\n";
        
        // 保存关系
        ofs << "  \"relations\": [\n";
        bool firstRelation = true;
        for (const auto& pair : relations_) {
            const RelationPtr& relation = pair.second;
            
            if (!firstRelation) {
                ofs << ",\n";
            }
            
            ofs << "    {\n";
            ofs << "      \"id\": \"" << relation->getId() << "\",\n";
            ofs << "      \"name\": \"" << relation->getName() << "\",\n";
            ofs << "      \"type\": \"" << relationTypeToString(relation->getType()) << "\",\n";
            ofs << "      \"inverseName\": \"" << relation->getInverseName() << "\",\n";
            ofs << "      \"symmetric\": " << (relation->isSymmetric() ? "true" : "false");
            
            // 保存属性
            const auto& props = relation->getAllProperties();
            if (!props.empty()) {
                ofs << ",\n      \"properties\": {\n";
                bool firstProp = true;
                for (const auto& propPair : props) {
                    if (!firstProp) {
                        ofs << ",\n";
                    }
                    ofs << "        \"" << propPair.first << "\": \"" << propPair.second << "\"";
                    firstProp = false;
                }
                ofs << "\n      }";
            }
            
            ofs << "\n    }";
            firstRelation = false;
        }
        ofs << "\n  ],\n";
        
        // 保存三元组
        ofs << "  \"triples\": [\n";
        bool firstTriple = true;
        for (const auto& pair : triples_) {
            const TriplePtr& triple = pair.second;
            
            if (!firstTriple) {
                ofs << ",\n";
            }
            
            ofs << "    {\n";
            ofs << "      \"subject\": \"" << triple->getSubject()->getId() << "\",\n";
            ofs << "      \"relation\": \"" << triple->getRelation()->getId() << "\",\n";
            ofs << "      \"object\": \"" << triple->getObject()->getId() << "\",\n";
            ofs << "      \"confidence\": " << std::fixed << std::setprecision(3) << triple->getConfidence();
            
            // 保存属性
            const auto& props = triple->getAllProperties();
            if (!props.empty()) {
                ofs << ",\n      \"properties\": {\n";
                bool firstProp = true;
                for (const auto& propPair : props) {
                    if (!firstProp) {
                        ofs << ",\n";
                    }
                    ofs << "        \"" << propPair.first << "\": \"" << propPair.second << "\"";
                    firstProp = false;
                }
                ofs << "\n      }";
            }
            
            ofs << "\n    }";
            firstTriple = false;
        }
        ofs << "\n  ]\n";
        
        ofs << "}\n";
        
        LOG_INFO("保存知识图谱到文件: {}" , filepath);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("保存知识图谱失败: {}" , std::string(e.what()));
        return false;
    }
}

// 从文件加载知识图谱
bool KnowledgeGraph::load(const std::string& filepath) {
    // 这个函数应该解析JSON并重建知识图谱
    // 由于JSON解析较为复杂，这里只提供一个基本框架
    // 实际实现可能需要使用第三方JSON库如nlohmann/json
    
    LOG_ERROR("知识图谱加载功能未实现，需要JSON解析库");
    return false;
}

// 清空知识图谱
void KnowledgeGraph::clear() {
    entities_.clear();
    relations_.clear();
    triples_.clear();
    
    subjectIndex_.clear();
    objectIndex_.clear();
    relationIndex_.clear();
    entityTypeIndex_.clear();
    relationTypeIndex_.clear();
    
    LOG_INFO("清空知识图谱: {}" , name_);
}

// 添加到索引
void KnowledgeGraph::addToIndices(TriplePtr triple) {
    if (!triple) {
        return;
    }
    
    const std::string& id = triple->getId();
    const std::string& subjectId = triple->getSubject()->getId();
    const std::string& relationId = triple->getRelation()->getId();
    const std::string& objectId = triple->getObject()->getId();
    
    // 添加到主体索引
    subjectIndex_[subjectId].insert(id);
    
    // 添加到关系索引
    relationIndex_[relationId].insert(id);
    
    // 添加到客体索引
    objectIndex_[objectId].insert(id);
}

// 从索引中移除
void KnowledgeGraph::removeFromIndices(TriplePtr triple) {
    if (!triple) {
        return;
    }
    
    const std::string& id = triple->getId();
    const std::string& subjectId = triple->getSubject()->getId();
    const std::string& relationId = triple->getRelation()->getId();
    const std::string& objectId = triple->getObject()->getId();
    
    // 从主体索引中移除
    auto subjectIt = subjectIndex_.find(subjectId);
    if (subjectIt != subjectIndex_.end()) {
        subjectIt->second.erase(id);
        if (subjectIt->second.empty()) {
            subjectIndex_.erase(subjectIt);
        }
    }
    
    // 从关系索引中移除
    auto relationIt = relationIndex_.find(relationId);
    if (relationIt != relationIndex_.end()) {
        relationIt->second.erase(id);
        if (relationIt->second.empty()) {
            relationIndex_.erase(relationIt);
        }
    }
    
    // 从客体索引中移除
    auto objectIt = objectIndex_.find(objectId);
    if (objectIt != objectIndex_.end()) {
        objectIt->second.erase(id);
        if (objectIt->second.empty()) {
            objectIndex_.erase(objectIt);
        }
    }
}

// 验证三元组
bool KnowledgeGraph::validateTriple(TriplePtr triple) const {
    if (!triple) {
        return false;
    }
    
    // 验证主体实体
    EntityPtr subject = triple->getSubject();
    if (!subject || entities_.find(subject->getId()) == entities_.end()) {
        LOG_ERROR("三元组验证失败：主体实体不存在");
        return false;
    }
    
    // 验证关系
    RelationPtr relation = triple->getRelation();
    if (!relation || relations_.find(relation->getId()) == relations_.end()) {
        LOG_ERROR("三元组验证失败：关系不存在");
        return false;
    }
    
    // 验证客体实体
    EntityPtr object = triple->getObject();
    if (!object || entities_.find(object->getId()) == entities_.end()) {
        LOG_ERROR("三元组验证失败：客体实体不存在");
        return false;
    }
    
    return true;
}

} // namespace knowledge
} // namespace kb 