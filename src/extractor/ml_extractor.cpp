#include "extractor/ml_extractor.h"
#include "common/logging.h"
#include <random>
#include <algorithm>
#include <sstream>

namespace kb {
namespace extractor {

// 构造函数
MLExtractor::MLExtractor(const std::string& modelPath,
                       const std::string& dictPath,
                       const std::string& hmmPath,
                       const std::string& userDictPath,
                       const std::string& idfPath,
                       const std::string& stopWordsPath)
    : modelPath_(modelPath),
      entityConfidenceThreshold_(0.5f),
      relationConfidenceThreshold_(0.3f) {
    
    // 初始化分词器
    if (!dictPath.empty()) {
        tokenizer_ = std::make_unique<preprocessor::ChineseTokenizer>(
            dictPath, hmmPath, userDictPath, idfPath, stopWordsPath);
    } else {
        tokenizer_ = std::make_unique<preprocessor::ChineseTokenizer>();
    }
    
    // 初始化向量化器
    vectorizer_ = std::make_unique<vectorizer::TfidfVectorizer>();
    
    // 尝试加载模型
    if (!modelPath.empty()) {
        loadModel(modelPath);
    }
    
    LOG_INFO("创建基于机器学习的知识提取器");
}

// 从文本中提取实体
std::vector<knowledge::EntityPtr> MLExtractor::extractEntities(const std::string& text) {
    // 预处理文本
    std::string processedText = preprocessText(text);
    
    // 分词
    std::vector<std::string> tokens = tokenizeText(processedText);
    
    // 执行实体识别
    lastEntityResults_ = recognizeEntities(processedText, tokens);
    
    // 转换结果为实体对象
    std::vector<knowledge::EntityPtr> entities;
    entities.reserve(lastEntityResults_.size());
    
    for (const auto& result : lastEntityResults_) {
        // 如果置信度低于阈值，跳过
        if (result.confidence < entityConfidenceThreshold_) {
            continue;
        }
        
        // 生成实体ID
        std::string entityId = "entity_ml_" + std::to_string(std::rand());
        
        // 创建实体
        knowledge::EntityPtr entity = knowledge::Entity::create(
            entityId, result.text, result.entityType);
        
        // 添加元数据
        entity->addProperty("source", "ml_extraction");
        entity->addProperty("confidence", std::to_string(result.confidence));
        entity->addProperty("start_pos", std::to_string(result.startPos));
        entity->addProperty("end_pos", std::to_string(result.endPos));
        
        // 添加到结果列表
        entities.push_back(entity);
        
        LOG_DEBUG("提取实体: {}" , entity->toString() + 
                   ", 置信度: " + std::to_string(result.confidence));
    }
    
    LOG_INFO("从文本中提取了 {} 个实体" , entities.size());
    return entities;
}

// 从文本中提取关系
std::vector<knowledge::RelationPtr> MLExtractor::extractRelations(
    const std::string& text,
    const std::vector<knowledge::EntityPtr>& entities) {
    
    // 预处理文本
    std::string processedText = preprocessText(text);
    
    // 分词
    std::vector<std::string> tokens = tokenizeText(processedText);
    
    // 如果没有现有实体识别结果，先执行实体识别
    if (lastEntityResults_.empty() || !entities.empty()) {
        // 使用提供的实体或重新执行实体识别
        if (!entities.empty()) {
            // 从提供的实体构建实体识别结果
            lastEntityResults_.clear();
            for (size_t i = 0; i < entities.size(); ++i) {
                const auto& entity = entities[i];
                
                // 尝试获取位置信息
                size_t startPos = 0;
                size_t endPos = 0;
                float confidence = 1.0f;
                
                try {
                    // 从实体属性中获取位置信息
                    const std::string& startPosStr = entity->getProperty("start_pos");
                    const std::string& endPosStr = entity->getProperty("end_pos");
                    const std::string& confStr = entity->getProperty("confidence");
                    
                    if (!startPosStr.empty()) {
                        startPos = std::stoul(startPosStr);
                    }
                    
                    if (!endPosStr.empty()) {
                        endPos = std::stoul(endPosStr);
                    } else if (startPos > 0) {
                        // 估计结束位置
                        endPos = startPos + entity->getName().length();
                    }
                    
                    if (!confStr.empty()) {
                        confidence = std::stof(confStr);
                    }
                } catch (const std::exception& e) {
                    LOG_WARN("无法解析实体属性: {}" , std::string(e.what()));
                }
                
                // 在文本中查找实体，如果位置未知
                if (startPos == 0 && endPos == 0) {
                    size_t pos = processedText.find(entity->getName());
                    if (pos != std::string::npos) {
                        startPos = pos;
                        endPos = pos + entity->getName().length();
                    }
                }
                
                // 创建实体识别结果
                lastEntityResults_.emplace_back(
                    entity->getName(), entity->getType(), 
                    startPos, endPos, confidence);
            }
        } else {
            // 执行实体识别
            lastEntityResults_ = recognizeEntities(processedText, tokens);
        }
    }
    
    // 执行关系提取
    lastRelationResults_ = extractRelationsFromEntities(processedText, tokens, lastEntityResults_);
    
    // 转换结果为关系对象
    std::vector<knowledge::RelationPtr> relations;
    relations.reserve(lastRelationResults_.size());
    
    for (const auto& result : lastRelationResults_) {
        // 如果置信度低于阈值，跳过
        if (result.confidence < relationConfidenceThreshold_) {
            continue;
        }
        
        // 生成关系ID
        std::string relationId = "relation_ml_" + std::to_string(std::rand());
        
        // 创建关系
        knowledge::RelationPtr relation = knowledge::Relation::create(
            relationId, result.text, result.relationType);
        
        // 添加元数据
        relation->addProperty("source", "ml_extraction");
        relation->addProperty("confidence", std::to_string(result.confidence));
        relation->addProperty("start_pos", std::to_string(result.startPos));
        relation->addProperty("end_pos", std::to_string(result.endPos));
        relation->addProperty("subject_index", std::to_string(result.subjectIndex));
        relation->addProperty("object_index", std::to_string(result.objectIndex));
        
        // 添加到结果列表
        relations.push_back(relation);
        
        LOG_DEBUG("提取关系: {}" , relation->toString() + 
                   ", 置信度: " + std::to_string(result.confidence));
    }
    
    LOG_INFO("从文本中提取了 {} 个关系" , relations.size());
    return relations;
}

// 从文本中提取三元组
std::vector<knowledge::TriplePtr> MLExtractor::extractTriples(const std::string& text) {
    std::vector<knowledge::TriplePtr> triples;
    
    // 预处理文本
    std::string processedText = preprocessText(text);
    
    // 分词
    std::vector<std::string> tokens = tokenizeText(processedText);
    
    // 执行实体识别
    lastEntityResults_ = recognizeEntities(processedText, tokens);
    
    // 如果没有实体，返回空结果
    if (lastEntityResults_.empty()) {
        LOG_WARN("未发现实体，无法提取三元组");
        return triples;
    }
    
    // 执行关系提取
    lastRelationResults_ = extractRelationsFromEntities(processedText, tokens, lastEntityResults_);
    
    // 创建三元组
    for (const auto& result : lastRelationResults_) {
        // 如果置信度低于阈值，跳过
        if (result.confidence < relationConfidenceThreshold_) {
            continue;
        }
        
        // 确保主体和客体索引有效
        if (result.subjectIndex >= lastEntityResults_.size() || 
            result.objectIndex >= lastEntityResults_.size()) {
            LOG_WARN("关系的主体或客体索引无效");
            continue;
        }
        
        // 获取主体和客体实体
        const auto& subjectResult = lastEntityResults_[result.subjectIndex];
        const auto& objectResult = lastEntityResults_[result.objectIndex];
        
        // 如果实体置信度低于阈值，跳过
        if (subjectResult.confidence < entityConfidenceThreshold_ || 
            objectResult.confidence < entityConfidenceThreshold_) {
            continue;
        }
        
        // 创建主体实体
        std::string subjectId = "entity_ml_" + std::to_string(std::rand());
        knowledge::EntityPtr subject = knowledge::Entity::create(
            subjectId, subjectResult.text, subjectResult.entityType);
        
        // 创建关系
        std::string relationId = "relation_ml_" + std::to_string(std::rand());
        knowledge::RelationPtr relation = knowledge::Relation::create(
            relationId, result.text, result.relationType);
        
        // 创建客体实体
        std::string objectId = "entity_ml_" + std::to_string(std::rand());
        knowledge::EntityPtr object = knowledge::Entity::create(
            objectId, objectResult.text, objectResult.entityType);
        
        // 创建三元组
        knowledge::TriplePtr triple = knowledge::Triple::create(
            subject, relation, object, result.confidence);
        
        // 添加来源信息
        triple->addProperty("source", "ml_extraction");
        
        // 添加到结果列表
        triples.push_back(triple);
        
        LOG_DEBUG("生成三元组: {}" , triple->toString());
    }
    
    LOG_INFO("从文本中提取了 {} 个三元组" , triples.size());
    return triples;
}

// 从文本中构建知识图谱
knowledge::KnowledgeGraphPtr MLExtractor::buildKnowledgeGraph(
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

// 加载预训练模型
bool MLExtractor::loadModel(const std::string& modelPath) {
    modelPath_ = modelPath;
    LOG_INFO("尝试加载模型: {}" , modelPath);
    
    // 此处应该实现具体的模型加载逻辑
    // 由于这是一个抽象类，实际的模型加载由子类实现
    return false;
}

// 对文本进行预处理
std::string MLExtractor::preprocessText(const std::string& text) {
    // 基本的预处理逻辑：移除多余空格、标准化标点等
    std::string processedText = text;
    
    // 移除多余空格
    processedText.erase(
        std::unique(processedText.begin(), processedText.end(),
                   [](char a, char b) { return a == ' ' && b == ' '; }),
        processedText.end());
    
    return processedText;
}

// 对文本进行分词
std::vector<std::string> MLExtractor::tokenizeText(const std::string& text) {
    if (tokenizer_) {
        return tokenizer_->cut(text);
    }
    
    // 如果没有分词器，返回简单的按空格分割的结果
    std::vector<std::string> tokens;
    std::istringstream iss(text);
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    
    return tokens;
}

} // namespace extractor
} // namespace kb 