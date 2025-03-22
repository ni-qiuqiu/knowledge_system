/**
 * @file test_query.cpp
 * @brief 查询模块测试程序
 */

#include "query/query_parser.h"
#include "query/graph_searcher.h"
#include "query/answer_generator.h"
#include "query/dialogue_manager.h"
#include "query/query_manager.h"
#include "knowledge/knowledge_graph.h"
#include "engine/model_interface.h"
#include "engine/llama_model.h"
#include "common/logging.h"
#include "common/utils.h"
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace kb;
using namespace kb::query;
using namespace kb::knowledge;
using namespace kb::engine;

// 创建测试知识图谱
std::shared_ptr<KnowledgeGraph> createTestKnowledgeGraph() {
    auto kg = std::make_shared<KnowledgeGraph>("test_kg");
    
    // 创建实体
    auto entity1 = Entity::create("e1", "北京", EntityType::LOCATION);
    auto entity2 = Entity::create("e2", "上海", EntityType::LOCATION);
    auto entity3 = Entity::create("e3", "中国", EntityType::LOCATION);
    auto entity4 = Entity::create("e4", "长江", EntityType::RIVER);
    auto entity5 = Entity::create("e5", "黄河", EntityType::RIVER);
    auto entity6 = Entity::create("e6", "李白", EntityType::PERSON);
    auto entity7 = Entity::create("e7", "杜甫", EntityType::PERSON);
    
    // 创建关系
    auto relation1 = Relation::create("r1", "位于", RelationType::LOCATED_IN);
    auto relation2 = Relation::create("r2", "首都", RelationType::CAPITAL_OF);
    auto relation3 = Relation::create("r3", "流经", RelationType::FLOWS_THROUGH);
    auto relation4 = Relation::create("r4", "朋友", RelationType::FRIEND_OF);
    
    // 添加实体到知识图谱
    kg->addEntity(entity1);
    kg->addEntity(entity2);
    kg->addEntity(entity3);
    kg->addEntity(entity4);
    kg->addEntity(entity5);
    kg->addEntity(entity6);
    kg->addEntity(entity7);
    
    // 添加关系到知识图谱
    kg->addRelation(relation1);
    kg->addRelation(relation2);
    kg->addRelation(relation3);
    kg->addRelation(relation4);
    
    // 创建三元组
    auto triple1 = Triple::create(entity1, relation1, entity3, 1.0f);
    auto triple2 = Triple::create(entity2, relation1, entity3, 1.0f);
    auto triple3 = Triple::create(entity1, relation2, entity3, 1.0f);
    auto triple4 = Triple::create(entity4, relation3, entity3, 1.0f);
    auto triple5 = Triple::create(entity5, relation3, entity3, 1.0f);
    auto triple6 = Triple::create(entity6, relation4, entity7, 0.9f);
    
    // 添加三元组到知识图谱
    kg->addTriple(triple1);
    kg->addTriple(triple2);
    kg->addTriple(triple3);
    kg->addTriple(triple4);
    kg->addTriple(triple5);
    kg->addTriple(triple6);
    
    return kg;
}

// 测试查询解析
void testQueryParser() {
    std::cout << "\n--- 测试查询解析器 ---\n" << std::endl;
    
    QueryParser parser;
    
    std::vector<std::string> testQueries = {
        "北京是哪个国家的首都？",
        "李白和杜甫是什么关系？",
        "长江的长度是多少？",
        "中国有哪些主要城市？",
        "长江和黄河相比，哪个更长？",
        "什么是人工智能？"
    };
    
    for (const auto& query : testQueries) {
        std::cout << "查询: " << query << std::endl;
        
        auto result = parser.parse(query);
        
        //std::cout << "  查询类型: " << queryTypeToString(result.intent.type) << std::endl;
        std::cout << "  置信度: " << result.confidence << std::endl;
        
        std::cout << "  识别的实体: ";
        for (const auto& entity : result.intent.entities) {
            std::cout << entity << " ";
        }
        std::cout << std::endl;
        
        std::cout << "  识别的关系: ";
        for (const auto& relation : result.intent.relations) {
            std::cout << relation << " ";
        }
        std::cout << std::endl;
        
        std::cout << "  识别的属性: ";
        for (const auto& attr : result.intent.attributes) {
            std::cout << attr << " ";
        }
        std::cout << std::endl;
        
        std::cout << std::endl;
    }
}

// 测试图谱搜索
void testGraphSearcher(std::shared_ptr<KnowledgeGraph> kg) {
    std::cout << "\n--- 测试图谱搜索器 ---\n" << std::endl;
    
    GraphSearcher searcher(kg);
    QueryParser parser;
    
    std::vector<std::string> testQueries = {
        "北京是哪个国家的首都？",
        "李白和杜甫是什么关系？",
        "长江流经哪些地方？"
    };
    
    for (const auto& query : testQueries) {
        std::cout << "查询: " << query << std::endl;
        
        auto parseResult = parser.parse(query);
        auto searchResult = searcher.search(parseResult);
        
        std::cout << "  搜索策略: " << searchResult.searchStrategy << std::endl;
        std::cout << "  找到 " << searchResult.totalMatches << " 个匹配结果" << std::endl;
        std::cout << "  执行时间: " << searchResult.executionTime << " ms" << std::endl;
        
        for (size_t i = 0; i < searchResult.items.size(); ++i) {
            const auto& item = searchResult.items[i];
            const auto& triple = item.triple;
            
            std::cout << "  结果 " << (i+1) << ": " 
                     << triple->getSubject()->getName() << " - " 
                     << triple->getRelation()->getName() << " - " 
                     << triple->getObject()->getName() 
                     << " (相关性: " << item.relevanceScore 
                     << ", 置信度: " << item.confidenceScore << ")" << std::endl;
        }
        
        std::cout << std::endl;
    }
}

// 测试对话管理器
void testDialogueManager() {
    std::cout << "\n--- 测试对话管理器 ---\n" << std::endl;
    
    DialogueManager dialogueManager;
    
    // 创建会话
    std::string userId = "user123";
    std::string sessionId = dialogueManager.createSession(userId);
    
    std::cout << "创建会话: " << sessionId << std::endl;
    
    // 添加用户消息
    dialogueManager.addUserMessage(sessionId, "你好，我想了解关于北京的信息。");
    
    // 添加系统回复
    dialogueManager.addSystemResponse(sessionId, "北京是中国的首都，位于华北平原的北部，是中国的政治、文化和国际交往中心。");
    
    // 添加用户消息
    dialogueManager.addUserMessage(sessionId, "北京有哪些著名景点？");
    
    // 添加系统回复
    dialogueManager.addSystemResponse(sessionId, "北京的著名景点包括故宫、天安门、长城、颐和园、天坛等。");
    
    // 获取会话历史
    auto history = dialogueManager.getHistory(sessionId);
    
    std::cout << "会话历史: " << std::endl;
    for (const auto& message : history) {
        std::string role;
        switch (message.role) {
            case DialogueMessage::Role::USER:
                role = "用户";
                break;
            case DialogueMessage::Role::SYSTEM:
                role = "系统";
                break;
            case DialogueMessage::Role::ASSISTANT:
                role = "助手";
                break;
        }
        
        std::cout << "  " << role << ": " << message.content << std::endl;
    }
    
    // 设置会话属性
    dialogueManager.setContextAttribute(sessionId, "last_topic", "北京景点");
    
    // 获取会话属性
    std::string lastTopic = dialogueManager.getContextAttribute(sessionId, "last_topic");
    std::cout << "会话属性 'last_topic': " << lastTopic << std::endl;
    
    // 清除会话历史
    dialogueManager.clearHistory(sessionId);
    history = dialogueManager.getHistory(sessionId);
    std::cout << "清除历史后的消息数量: " << history.size() << std::endl;
    
    // 删除会话
    bool removed = dialogueManager.removeSession(sessionId);
    std::cout << "会话已删除: " << (removed ? "是" : "否") << std::endl;
}

// 主函数
int main() {
    // 初始化日志
    // LOG_INIT();
    // LOG_SET_LEVEL(LogLevel::INFO);
    
    try {
        // 创建测试知识图谱
        auto kg = createTestKnowledgeGraph();
        
        // 测试查询解析器
        testQueryParser();
        
        // 测试图谱搜索器
        testGraphSearcher(kg);
        
        // 测试对话管理器
        testDialogueManager();
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
} 