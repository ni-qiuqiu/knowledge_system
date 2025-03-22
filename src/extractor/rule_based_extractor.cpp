#include "extractor/rule_based_extractor.h"
#include "common/logging.h"
#include <algorithm>
#include <random>
#include <sstream>

namespace kb {
namespace extractor {

// 构造函数
RuleBasedExtractor::RuleBasedExtractor(const std::string& dictPath,
                                     const std::string& hmmPath,
                                     const std::string& userDictPath,
                                     const std::string& idfPath,
                                     const std::string& stopWordsPath)
    : useChineseTokenizer_(true) {
    
    // 如果提供了字典路径，初始化分词器
    if (!dictPath.empty()) {
        tokenizer_ = std::make_unique<preprocessor::ChineseTokenizer>(
            dictPath, hmmPath, userDictPath, idfPath, stopWordsPath);
        LOG_INFO("创建基于规则的知识提取器，使用指定的中文分词器");
    } else {
        // 默认使用项目内置或系统配置的字典路径
        tokenizer_ = std::make_unique<preprocessor::ChineseTokenizer>();
        LOG_INFO("创建基于规则的知识提取器，使用默认中文分词器");
    }
    
    // 添加一些默认的实体和关系规则
    addChinesePersonNameRules();
    addChineseOrganizationRules();
    addChineseLocationRules();
    addChineseTimeRules();
    addChineseEventRules();
    addChineseRelationRules();
}

// 添加实体规则
void RuleBasedExtractor::addEntityRule(const EntityRecognitionRule& rule) {
    entityRules_.push_back(rule);
    LOG_DEBUG("添加实体识别规则: {}" , rule.name);
}

// 添加关系规则
void RuleBasedExtractor::addRelationRule(const RelationRecognitionRule& rule) {
    relationRules_.push_back(rule);
    LOG_DEBUG("添加关系识别规则: {}" , rule.name);
}

// 添加实体类型映射函数
void RuleBasedExtractor::addEntityTypeMapper(const std::string& typeName, 
                                           std::function<knowledge::EntityType(const std::string&)> func) {
    entityTypeMappers_[typeName] = func;
    LOG_DEBUG("添加实体类型映射函数: {}" , typeName);
}

// 添加关系类型映射函数
void RuleBasedExtractor::addRelationTypeMapper(const std::string& typeName, 
                                             std::function<knowledge::RelationType(const std::string&)> func) {
    relationTypeMappers_[typeName] = func;
    LOG_DEBUG("添加关系类型映射函数: {}" , typeName);
}

// 从文本中提取实体
std::vector<knowledge::EntityPtr> RuleBasedExtractor::extractEntities(const std::string& text) {
    std::vector<knowledge::EntityPtr> entities;
    
    // 如果启用中文分词
    if (useChineseTokenizer_ && tokenizer_) {
        // 使用分词器进行分词
        std::vector<std::string> tokens = tokenizer_->cut(text);
        
        // 从分词结果中识别实体
        std::vector<knowledge::EntityPtr> tokenEntities = recognizeEntitiesFromTokens(tokens);
        entities.insert(entities.end(), tokenEntities.begin(), tokenEntities.end());
    }
    
    // 使用正则表达式规则进行匹配
    for (const auto& rule : entityRules_) {
        std::sregex_iterator it(text.begin(), text.end(), rule.pattern);
        std::sregex_iterator end;
        
        while (it != end) {
            std::smatch match = *it;
            std::string entityText = match.str();
            
            // 使用随机ID，生产环境可能需要更健壮的ID生成策略
            std::string entityId = "entity_" + std::to_string(std::rand());
            
            // 创建实体
            knowledge::EntityPtr entity = knowledge::Entity::create(
                entityId, entityText, rule.entityType);
            
            // 添加来源信息
            entity->addProperty("source", "rule_extraction");
            entity->addProperty("rule_name", rule.name);
            
            // 添加位置信息
            entity->addProperty("position", std::to_string(match.position()));
            entity->addProperty("length", std::to_string(match.length()));
            
            // 添加到结果列表
            entities.push_back(entity);
            
            LOG_DEBUG("提取实体: {}" , entity->toString());
            ++it;
        }
    }
    
    // 去除重复实体（基于名称）
    std::vector<knowledge::EntityPtr> uniqueEntities;
    std::unordered_set<std::string> entityNames;
    
    for (const auto& entity : entities) {
        if (entityNames.find(entity->getName()) == entityNames.end()) {
            uniqueEntities.push_back(entity);
            entityNames.insert(entity->getName());
        }
    }
    
    LOG_INFO("从文本中提取了 {} 个唯一实体" , uniqueEntities.size());
    return uniqueEntities;
}

// 从文本中提取关系
std::vector<knowledge::RelationPtr> RuleBasedExtractor::extractRelations(
    const std::string& text,
    const std::vector<knowledge::EntityPtr>& entities) {
    
    std::vector<knowledge::RelationPtr> relations;
    
    // 使用正则表达式规则进行匹配
    for (const auto& rule : relationRules_) {
        std::sregex_iterator it(text.begin(), text.end(), rule.pattern);
        std::sregex_iterator end;
        
        while (it != end) {
            std::smatch match = *it;
            std::string relationText = match.str();
            
            // 关系名称，如果规则中未指定则使用匹配文本
            std::string relationName = rule.relationName.empty() ? relationText : rule.relationName;
            
            // 使用随机ID，生产环境可能需要更健壮的ID生成策略
            std::string relationId = "relation_" + std::to_string(std::rand());
            
            // 创建关系
            knowledge::RelationPtr relation = knowledge::Relation::create(
                relationId, relationName, rule.relationType);
            
            // 添加来源信息
            relation->addProperty("source", "rule_extraction");
            relation->addProperty("rule_name", rule.name);
            relation->addProperty("original_text", relationText);
            
            // 添加位置信息
            relation->addProperty("position", std::to_string(match.position()));
            relation->addProperty("length", std::to_string(match.length()));
            
            // 添加到结果列表
            relations.push_back(relation);
            
            LOG_DEBUG("提取关系: {}" , relation->toString());
            ++it;
        }
    }
    
    // 去除重复关系（基于名称）
    std::vector<knowledge::RelationPtr> uniqueRelations;
    std::unordered_set<std::string> relationNames;
    
    for (const auto& relation : relations) {
        if (relationNames.find(relation->getName()) == relationNames.end()) {
            uniqueRelations.push_back(relation);
            relationNames.insert(relation->getName());
        }
    }
    
    LOG_INFO("从文本中提取了 {} 个唯一关系" , uniqueRelations.size());
    return uniqueRelations;
}

// 从文本中提取三元组
std::vector<knowledge::TriplePtr> RuleBasedExtractor::extractTriples(const std::string& text) {
    // 提取实体
    auto entities = extractEntities(text);
    
    // 提取关系
    auto relations = extractRelations(text, entities);
    
    // 关联实体和关系，生成三元组
    return associateEntitiesAndRelations(entities, relations, text);
}

// 从文本中构建知识图谱
knowledge::KnowledgeGraphPtr RuleBasedExtractor::buildKnowledgeGraph(
    const std::string& text,
    knowledge::KnowledgeGraphPtr graph) {
    
    // 如果没有提供现有图谱，创建一个新的
    if (!graph) {
        graph = std::make_shared<knowledge::KnowledgeGraph>();
    }
    
    // 提取三元组
    auto triples = extractTriples(text);
    
    // 添加到知识图谱
    for (const auto& triple : triples) {
        // 添加主体实体
        graph->addEntity(triple->getSubject());
        
        // 添加关系
        graph->addRelation(triple->getRelation());
        
        // 添加客体实体
        graph->addEntity(triple->getObject());
        
        // 添加三元组
        graph->addTriple(triple);
    }
    
    LOG_INFO("从文本中构建知识图谱，添加了 {} 个三元组" , triples.size());
    return graph;
}

// 添加预定义规则集
void RuleBasedExtractor::addPredefinedRuleset(const std::string& rulesetName) {
    if (rulesetName == "chinese_person") {
        addChinesePersonNameRules();
    } else if (rulesetName == "chinese_organization") {
        addChineseOrganizationRules();
    } else if (rulesetName == "chinese_location") {
        addChineseLocationRules();
    } else if (rulesetName == "chinese_time") {
        addChineseTimeRules();
    } else if (rulesetName == "chinese_event") {
        addChineseEventRules();
    } else if (rulesetName == "chinese_relation") {
        addChineseRelationRules();
    } else if (rulesetName == "chinese_all") {
        addChinesePersonNameRules();
        addChineseOrganizationRules();
        addChineseLocationRules();
        addChineseTimeRules();
        addChineseEventRules();
        addChineseRelationRules();
    } else {
        LOG_WARN("未知的预定义规则集: {}" , rulesetName);
    }
}

// 添加中文人名识别规则
void RuleBasedExtractor::addChinesePersonNameRules() {
    // 简单的中文人名模式（2-4个字符）
    addEntityRule(EntityRecognitionRule(
        "chinese_person_name",
        knowledge::EntityType::PERSON,
        "[赵钱孙李周吴郑王冯陈楚魏蒋沈韩杨朱秦尤许何吕施张孔曹严华金魏陶姜戚谢邹喻柏水窦章云苏潘葛奚范彭郎鲁韦昌马苗凤花方俞任袁柳酆鲍史唐费廉岑薛雷贺倪汤]"
        "[A-Za-z\\u4e00-\\u9fa5]{1,3}"));
    
    // 带有职位头衔的人名
    addEntityRule(EntityRecognitionRule(
        "chinese_person_with_title", 
        knowledge::EntityType::PERSON,
        "(总统|总理|主席|董事长|教授|博士|先生|女士|医生|老师|同志)[A-Za-z\\u4e00-\\u9fa5]{2,4}"));
}

// 添加中文组织机构识别规则
void RuleBasedExtractor::addChineseOrganizationRules() {
    // 公司
    addEntityRule(EntityRecognitionRule(
        "chinese_company", 
        knowledge::EntityType::ORGANIZATION,
        "[A-Za-z\\u4e00-\\u9fa5]+(公司|集团|企业|有限责任公司|股份有限公司)"));
    
    // 学校
    addEntityRule(EntityRecognitionRule(
        "chinese_school", 
        knowledge::EntityType::ORGANIZATION,
        "[A-Za-z\\u4e00-\\u9fa5]+(大学|学院|中学|小学|学校)"));
    
    // 政府机构
    addEntityRule(EntityRecognitionRule(
        "chinese_government", 
        knowledge::EntityType::ORGANIZATION,
        "[A-Za-z\\u4e00-\\u9fa5]+(部|委员会|局|办公室|政府|机关)"));
}

// 添加中文地点识别规则
void RuleBasedExtractor::addChineseLocationRules() {
    // 省市
    addEntityRule(EntityRecognitionRule(
        "chinese_province_city", 
        knowledge::EntityType::LOCATION,
        "([A-Za-z\\u4e00-\\u9fa5]+(省|市|自治区|特别行政区|县|区|镇|乡))"));
    
    // 国家
    addEntityRule(EntityRecognitionRule(
        "chinese_country", 
        knowledge::EntityType::LOCATION,
        "(中国|美国|日本|英国|法国|德国|俄罗斯|加拿大|澳大利亚|新西兰)"));
    
    // 地标
    addEntityRule(EntityRecognitionRule(
        "chinese_landmark", 
        knowledge::EntityType::LOCATION,
        "[A-Za-z\\u4e00-\\u9fa5]+(公园|广场|大厦|建筑|景区|博物馆|街道|路)"));
}

// 添加中文时间识别规则
void RuleBasedExtractor::addChineseTimeRules() {
    // 年月日
    addEntityRule(EntityRecognitionRule(
        "chinese_date", 
        knowledge::EntityType::TIME,
        "((19|20)\\d{2}年)?(\\d{1,2}月)?(\\d{1,2}日)?"));
    
    // 世纪、年代
    addEntityRule(EntityRecognitionRule(
        "chinese_era", 
        knowledge::EntityType::TIME,
        "(\\d{1,2}世纪|\\d{1,2}0年代)"));
    
    // 特定时间段
    addEntityRule(EntityRecognitionRule(
        "chinese_period", 
        knowledge::EntityType::TIME,
        "(春秋时期|战国时期|汉朝|唐朝|宋朝|元朝|明朝|清朝|民国时期)"));
}

// 添加中文事件识别规则
void RuleBasedExtractor::addChineseEventRules() {
    // 历史事件
    addEntityRule(EntityRecognitionRule(
        "chinese_historical_event", 
        knowledge::EntityType::EVENT,
        "([A-Za-z\\u4e00-\\u9fa5]+(战争|革命|运动|起义|事变|会议))"));
    
    // 活动
    addEntityRule(EntityRecognitionRule(
        "chinese_activity", 
        knowledge::EntityType::EVENT,
        "([A-Za-z\\u4e00-\\u9fa5]+(节|庆典|活动|比赛|展览|论坛|峰会))"));
}

// 添加中文常见关系识别规则
void RuleBasedExtractor::addChineseRelationRules() {
    // 从属关系
    addRelationRule(RelationRecognitionRule(
        "belongs_to_relation", 
        knowledge::RelationType::BELONGS_TO,
        "属于|隶属于|归属于|是...的一部分",
        "属于"));
    
    // 地理位置关系
    addRelationRule(RelationRecognitionRule(
        "located_in_relation", 
        knowledge::RelationType::LOCATED_IN,
        "位于|坐落于|座落在|地处",
        "位于"));
    
    // 创建关系
    addRelationRule(RelationRecognitionRule(
        "created_by_relation", 
        knowledge::RelationType::CREATED_BY,
        "创建|创立|创办|成立|建立|制作|开发",
        "创建"));
    
    // 工作关系
    addRelationRule(RelationRecognitionRule(
        "works_for_relation", 
        knowledge::RelationType::WORKS_FOR,
        "就职于|供职于|为...工作|在...工作|是...的员工",
        "工作于"));
    
    // 出生关系
    addRelationRule(RelationRecognitionRule(
        "born_in_relation", 
        knowledge::RelationType::BORN_IN,
        "出生于|生于",
        "出生于"));
    
    // 属性关系
    addRelationRule(RelationRecognitionRule(
        "has_property_relation", 
        knowledge::RelationType::HAS_PROPERTY,
        "具有|拥有|有...特征|有...特点|有...性质",
        "具有特性"));
}

// 从分词结果中识别实体
std::vector<knowledge::EntityPtr> RuleBasedExtractor::recognizeEntitiesFromTokens(
    const std::vector<std::string>& tokens) {
    
    std::vector<knowledge::EntityPtr> entities;
    
    // 使用分词结果中的词汇作为候选实体
    for (const auto& token : tokens) {
        // 过滤太短的词
        if (token.length() < 2) {
            continue;
        }
        
        // 使用启发式规则判断实体类型
        knowledge::EntityType type = knowledge::EntityType::UNKNOWN;
        
        // 检查是否匹配任何实体规则
        for (const auto& rule : entityRules_) {
            if (std::regex_match(token, rule.pattern)) {
                type = rule.entityType;
                break;
            }
        }
        
        // 如果无法确定类型，跳过
        if (type == knowledge::EntityType::UNKNOWN) {
            continue;
        }
        
        // 使用随机ID，生产环境可能需要更健壮的ID生成策略
        std::string entityId = "entity_token_" + std::to_string(std::rand());
        
        // 创建实体
        knowledge::EntityPtr entity = knowledge::Entity::create(entityId, token, type);
        
        // 添加来源信息
        entity->addProperty("source", "tokenizer");
        
        // 添加到结果列表
        entities.push_back(entity);
        
        LOG_DEBUG("从分词结果识别实体: {}" , entity->toString());
    }
    
    return entities;
}

// 关联实体和关系，生成三元组
std::vector<knowledge::TriplePtr> RuleBasedExtractor::associateEntitiesAndRelations(
    const std::vector<knowledge::EntityPtr>& entities,
    const std::vector<knowledge::RelationPtr>& relations,
    const std::string& text) {
    
    std::vector<knowledge::TriplePtr> triples;
    
    // 如果实体或关系为空，直接返回
    if (entities.empty() || relations.empty()) {
        LOG_WARN("无法生成三元组：实体或关系为空");
        return triples;
    }
    
    // 对于每个关系，尝试找到匹配的主体和客体
    for (const auto& relation : relations) {
        auto subjectObject = findSubjectAndObject(relation, entities, text);
        
        // 如果找到了主体和客体
        if (subjectObject.first && subjectObject.second) {
            // 创建三元组
            auto triple = knowledge::Triple::create(
                subjectObject.first, relation, subjectObject.second);
            
            triples.push_back(triple);
            LOG_DEBUG("生成三元组: {}" , triple->toString());
        }
    }
    
    return triples;
}

// 使用启发式规则寻找主体和客体
std::pair<knowledge::EntityPtr, knowledge::EntityPtr> RuleBasedExtractor::findSubjectAndObject(
    const knowledge::RelationPtr& relation,
    const std::vector<knowledge::EntityPtr>& entities,
    const std::string& text) {
    
    // 如果实体数少于2个，无法形成三元组
    if (entities.size() < 2) {
        return {nullptr, nullptr};
    }
    
    // 获取关系在文本中的位置
    int relationPos = -1;
    try {
        relationPos = std::stoi(relation->getProperty("position"));
    } catch (const std::exception& e) {
        LOG_WARN("无法获取关系位置信息: {}" , std::string(e.what()));
        return {nullptr, nullptr};
    }
    
    // 如果无法获取位置，使用随机选择
    if (relationPos < 0) {
        // 随机选择两个不同的实体
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, entities.size() - 1);
        
        int idx1 = dis(gen);
        int idx2;
        do {
            idx2 = dis(gen);
        } while (idx2 == idx1);
        
        return {entities[idx1], entities[idx2]};
    }
    
    // 根据位置信息寻找关系前后最近的实体
    knowledge::EntityPtr subject = nullptr;
    knowledge::EntityPtr object = nullptr;
    
    // 寻找关系前面最近的实体作为主体
    int minDistanceBefore = std::numeric_limits<int>::max();
    for (const auto& entity : entities) {
        int entityPos = -1;
        try {
            entityPos = std::stoi(entity->getProperty("position"));
        } catch (const std::exception& e) {
            continue;
        }
        
        if (entityPos < relationPos && (relationPos - entityPos) < minDistanceBefore) {
            minDistanceBefore = relationPos - entityPos;
            subject = entity;
        }
    }
    
    // 寻找关系后面最近的实体作为客体
    int minDistanceAfter = std::numeric_limits<int>::max();
    for (const auto& entity : entities) {
        int entityPos = -1;
        try {
            entityPos = std::stoi(entity->getProperty("position"));
        } catch (const std::exception& e) {
            continue;
        }
        
        if (entityPos > relationPos && (entityPos - relationPos) < minDistanceAfter) {
            minDistanceAfter = entityPos - relationPos;
            object = entity;
        }
    }
    
    // 如果没有找到主体或客体，随机选择
    if (!subject || !object) {
        // 从剩余实体中随机选择
        std::vector<knowledge::EntityPtr> remainingEntities;
        for (const auto& entity : entities) {
            if (entity != subject && entity != object) {
                remainingEntities.push_back(entity);
            }
        }
        
        if (!remainingEntities.empty()) {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, remainingEntities.size() - 1);
            
            if (!subject) {
                subject = remainingEntities[dis(gen)];
            }
            
            if (!object) {
                // 确保主体和客体不是同一个实体
                do {
                    object = remainingEntities[dis(gen)];
                } while (object == subject);
            }
        }
    }
    
    return {subject, object};
}

} // namespace extractor
} // namespace kb