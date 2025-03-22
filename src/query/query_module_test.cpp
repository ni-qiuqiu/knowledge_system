/**
 * @file query_module_test.cpp
 * @brief 查询模块功能测试程序
 */

#include "query/query_manager.h"
#include "engine/model_interface.h"
#include "engine/llama_model.h"
#include "knowledge/knowledge_graph.h"
#include "common/logging.h"
#include <iostream>
#include <string>
#include <memory>
#include <thread>

using namespace kb;
using namespace kb::query;

// 创建一个简单的测试知识图谱
std::shared_ptr<knowledge::KnowledgeGraph> createTestKnowledgeGraph() {
    auto kg = std::make_shared<knowledge::KnowledgeGraph>("测试知识图谱");
    
    // 创建实体
    auto beijing = knowledge::Entity::create("e001", "北京", knowledge::EntityType::LOCATION);
    beijing->addProperty("人口", "2170万");
    beijing->addProperty("面积", "16410平方公里");
    beijing->addProperty("首都", "是");
    beijing->addProperty("行政区划", "直辖市");
    
    auto shanghai = knowledge::Entity::create("e002", "上海", knowledge::EntityType::LOCATION);
    shanghai->addProperty("人口", "2487万");
    shanghai->addProperty("面积", "6340平方公里");
    shanghai->addProperty("首都", "否");
    shanghai->addProperty("行政区划", "直辖市");
    
    auto china = knowledge::Entity::create("e003", "中国", knowledge::EntityType::COUNTRY);
    china->addProperty("人口", "14亿");
    china->addProperty("面积", "960万平方公里");
    china->addProperty("首都", "北京");
    china->addProperty("官方语言", "汉语");
    
    auto ai = knowledge::Entity::create("e004", "人工智能", knowledge::EntityType::TECHNOLOGY);
    ai->addProperty("英文名", "Artificial Intelligence");
    ai->addProperty("缩写", "AI");
    ai->addProperty("定义", "研究和开发用于模拟、延伸和扩展人类智能的理论、方法、技术及应用系统的一门新的技术科学");
    
    auto llm = knowledge::Entity::create("e005", "大语言模型", knowledge::EntityType::TECHNOLOGY);
    llm->addProperty("英文名", "Large Language Model");
    llm->addProperty("缩写", "LLM");
    llm->addProperty("定义", "一种基于深度学习的自然语言处理模型，能够理解和生成人类语言");
    
    // 添加实体到图谱
    kg->addEntity(beijing);
    kg->addEntity(shanghai);
    kg->addEntity(china);
    kg->addEntity(ai);
    kg->addEntity(llm);
    
    // 创建关系类型
    auto locatedIn = knowledge::Relation::create("r001", "位于", knowledge::RelationType::LOCATED_IN);
    auto capitalOf = knowledge::Relation::create("r002", "是首都", knowledge::RelationType::IS_CAPITAL_OF);
    auto partOf = knowledge::Relation::create("r003", "是一种", knowledge::RelationType::IS_A);
    
    // 添加关系类型到图谱
    kg->addRelationType(locatedIn);
    kg->addRelationType(capitalOf);
    kg->addRelationType(partOf);
    
    // 创建三元组
    kg->addTriple(beijing, locatedIn, china);
    kg->addTriple(shanghai, locatedIn, china);
    kg->addTriple(beijing, capitalOf, china);
    kg->addTriple(llm, partOf, ai);
    
    std::cout << "测试知识图谱创建完成，包含 " << kg->getEntityCount() << " 个实体，" 
              << kg->getTripleCount() << " 条三元组" << std::endl;
    
    return kg;
}

// 测试查询处理功能
void testQueryProcessing() {
    std::cout << "\n===== 测试查询处理 =====" << std::endl;
    
    // 获取查询管理器实例
    auto& queryManager = QueryManager::getInstance();
    
    // 执行查询
    std::string sessionId = queryManager.getDialogueManager()->createSession("test_user");
    
    // 测试不同类型的查询
    std::vector<std::string> testQueries = {
        "北京是哪个国家的首都？",
        "上海的人口是多少？",
        "人工智能包括什么？",
        "中国的面积有多大？",
        "大语言模型是什么？"
    };
    
    for (const auto& query : testQueries) {
        std::cout << "\n用户查询：" << query << std::endl;
        std::string answer = queryManager.processQuery(query, sessionId);
        std::cout << "系统回答：" << answer << std::endl;
    }
    
    // 测试上下文相关的查询
    std::cout << "\n测试上下文相关查询：" << std::endl;
    std::string contextQuery = "它的面积是多少？";  // 应该指向中国
    std::cout << "用户查询：" << contextQuery << std::endl;
    std::string answer = queryManager.processQuery(contextQuery, sessionId);
    std::cout << "系统回答：" << answer << std::endl;
}

// 测试流式回答生成
void testStreamGeneration() {
    std::cout << "\n===== 测试流式回答生成 =====" << std::endl;
    
    // 获取查询管理器实例
    auto& queryManager = QueryManager::getInstance();
    
    // 创建新会话
    std::string sessionId = queryManager.getDialogueManager()->createSession("stream_test_user");
    
    // 测试查询
    std::string query = "请详细介绍一下人工智能的发展历史";
    std::cout << "用户查询：" << query << std::endl;
    
    std::cout << "系统回答：";
    std::cout.flush();
    
    // 使用互斥锁保护输出
    std::mutex outputMutex;
    bool completed = false;
    
    // 流式处理查询
    queryManager.processQueryStream(query, 
                                  [&outputMutex, &completed](const std::string& token, bool isFinished) {
                                      std::lock_guard<std::mutex> lock(outputMutex);
                                      std::cout << token;
                                      std::cout.flush();
                                      
                                      if (isFinished) {
                                          std::cout << std::endl;
                                          completed = true;
                                      }
                                  }, 
                                  sessionId);
    
    // 等待流式生成完成
    while (!completed) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// 测试对话历史管理
void testDialogueHistory() {
    std::cout << "\n===== 测试对话历史管理 =====" << std::endl;
    
    // 获取查询管理器实例
    auto& queryManager = QueryManager::getInstance();
    auto dialogueManager = queryManager.getDialogueManager();
    
    // 创建新会话
    std::string sessionId = dialogueManager->createSession("history_test_user");
    std::cout << "创建会话：" << sessionId << std::endl;
    
    // 添加一系列消息
    dialogueManager->addUserMessage(sessionId, "你好，我是用户");
    dialogueManager->addAssistantResponse(sessionId, "你好，我是知识库助手，有什么可以帮助你的？");
    dialogueManager->addUserMessage(sessionId, "我想了解人工智能");
    dialogueManager->addAssistantResponse(sessionId, "人工智能是研究和开发用于模拟、延伸和扩展人类智能的理论、方法、技术及应用系统的一门新的技术科学。");
    
    // 获取历史
    auto history = dialogueManager->getHistory(sessionId);
    
    // 打印历史
    std::cout << "对话历史（" << history.size() << "条消息）：" << std::endl;
    for (const auto& msg : history) {
        std::string role;
        switch (msg.role) {
            case DialogueMessage::Role::USER:
                role = "用户";
                break;
            case DialogueMessage::Role::ASSISTANT:
                role = "助手";
                break;
            case DialogueMessage::Role::SYSTEM:
                role = "系统";
                break;
        }
        
        std::cout << "[" << msg.timestamp << "] " << role << ": " << msg.content << std::endl;
    }
    
    // 清空历史
    dialogueManager->clearHistory(sessionId);
    std::cout << "清空历史后，消息数量：" << dialogueManager->getHistory(sessionId).size() << std::endl;
}

int main() {
    // 初始化日志
    // TODO: 添加日志初始化代码
    
    try {
        std::cout << "开始查询模块测试..." << std::endl;
        
        // 创建测试知识图谱
        auto kg = createTestKnowledgeGraph();
        
        // 创建语言模型
        auto model = std::make_shared<engine::LlamaModel>();
        
        // 配置查询管理器
        QueryConfig config;
        config.enableModelGeneration = true;
        config.enableDialogueHistory = true;
        config.maxSearchResults = 10;
        config.defaultSearchStrategy = SearchStrategy::HYBRID;
        
        // 初始化查询管理器
        auto& queryManager = QueryManager::getInstance();
        if (!queryManager.initialize(kg, model, config)) {
            std::cerr << "初始化查询管理器失败" << std::endl;
            return 1;
        }
        
        // 运行测试
        testQueryProcessing();
        testStreamGeneration();
        testDialogueHistory();
        
        std::cout << "\n测试完成" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "测试过程中发生异常：" << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 