/**
 * @file query_manager.cpp
 * @brief 查询管理器实现
 */

#include "query/query_manager.h"
#include "query/query_parser.h"
#include "query/graph_searcher.h"
#include "query/answer_generator.h"
#include "common/logging.h"
#include "common/utils.h"
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include <mutex>

namespace kb {
namespace query {

// 获取单例实例
QueryManager& QueryManager::getInstance() {
    static QueryManager instance;
    return instance;
}

// 私有构造函数
QueryManager::QueryManager() 
    : initialized_(false) {
    LOG_INFO("查询管理器创建");
}

// 析构函数
QueryManager::~QueryManager() {
    LOG_INFO("查询管理器释放资源");
}

// 初始化查询管理器
bool QueryManager::initialize(
    std::shared_ptr<knowledge::KnowledgeGraph> knowledgeGraph,
    std::shared_ptr<engine::ModelInterface> model,
    const QueryConfig& config) {
    
    LOG_INFO("初始化查询管理器");
    
    // 检查知识图谱
    if (!knowledgeGraph) {
        LOG_ERROR("无法初始化查询管理器：知识图谱为空");
        return false;
    }
    
    // 保存配置和依赖
    knowledgeGraph_ = knowledgeGraph;
    config_ = config;
    
    // 创建查询解析器
    queryParser_ = std::make_shared<QueryParser>();
    LOG_INFO("创建查询解析器");
    
    // 创建图谱搜索器
    graphSearcher_ = std::make_shared<GraphSearcher>(knowledgeGraph);
    graphSearcher_->setSearchStrategy(config.defaultSearchStrategy);
    LOG_INFO("创建图谱搜索器，默认搜索策略: {}", 
            graphSearcher_->getSearchStrategyName(config.defaultSearchStrategy));
    
    // 创建回答生成器
    answerGenerator_ = std::make_shared<AnswerGenerator>(model);
    LOG_INFO("创建回答生成器");
    
    // 创建对话管理器
    dialogueManager_ = std::make_shared<DialogueManager>();
    LOG_INFO("创建对话管理器");
    
    initialized_ = true;
    LOG_INFO("查询管理器初始化完成");
    
    return true;
}

// 处理查询
std::string QueryManager::processQuery(const std::string& query, const std::string& sessionId) {
    LOG_INFO("处理查询: {}", query);
    
    // 检查初始化状态
    if (!initialized_) {
        LOG_ERROR("查询管理器未初始化");
        return "系统错误：查询管理器未初始化";
    }
    
    // 使用或创建会话ID
    std::string currentSessionId = sessionId;
    if (currentSessionId.empty()) {
        currentSessionId = dialogueManager_->createSession("anonymous");
        LOG_INFO("创建新会话: {}", currentSessionId);
    }
    
    // 添加用户消息到会话
    auto context = dialogueManager_->addUserMessage(currentSessionId, query);
    if (!context) {
        LOG_ERROR("无法添加用户消息，会话ID可能无效: {}", currentSessionId);
        return "系统错误：无效的会话标识";
    }
    
    // 解析查询
    QueryParseResult parseResult = queryParser_->parse(query);
    
    // 准备搜索参数
    SearchParams searchParams;
    searchParams.maxResults = config_.maxSearchResults;
    searchParams.includeProperties = true;
    searchParams.minConfidence = 0.2f;
    
    // 使用上下文增强查询
    if (config_.enableDialogueHistory && context->messages.size() > 1) {
        enhanceQueryWithContext(parseResult, context);
    }
    
    // 执行搜索
    SearchResult searchResult = graphSearcher_->search(parseResult, searchParams);
    
    // 准备生成选项
    AnswerGenerationOptions genOptions;
    genOptions.useModelGeneration = config_.enableModelGeneration;
    genOptions.includeSourceInfo = true;
    genOptions.language = config_.defaultLanguage;
    
    // 生成回答
    AnswerGenerationResult answer = answerGenerator_->generateAnswer(
        parseResult, searchResult, genOptions);
    
    // 添加回答到会话
    dialogueManager_->addAssistantResponse(currentSessionId, answer.answer);
    
    // 添加查询元数据到会话
    std::string queryId = generateQueryId();
    dialogueManager_->setContextAttribute(currentSessionId, "last_query_id", queryId);
    dialogueManager_->setContextAttribute(currentSessionId, "last_query_type", 
                                        std::to_string(static_cast<int>(parseResult.intent.type)));
    dialogueManager_->setContextAttribute(currentSessionId, "last_confidence", 
                                        std::to_string(answer.confidence));
    
    LOG_INFO("查询处理完成，回答长度: {}, 置信度: {}", 
            answer.answer.length(), answer.confidence);
    
    return answer.answer;
}

// 流式处理查询
void QueryManager::processQueryStream(
    const std::string& query,
    QueryResultCallback callback,
    const std::string& sessionId) {
    
    LOG_INFO("流式处理查询: {}", query);
    
    // 检查初始化状态
    if (!initialized_) {
        LOG_ERROR("查询管理器未初始化");
        callback("系统错误：查询管理器未初始化", true);
        return;
    }
    
    // 检查回调函数
    if (!callback) {
        LOG_ERROR("流式处理查询时未提供回调函数");
        return;
    }
    
    // 使用或创建会话ID
    std::string currentSessionId = sessionId;
    if (currentSessionId.empty()) {
        currentSessionId = dialogueManager_->createSession("anonymous");
        LOG_INFO("创建新会话: {}", currentSessionId);
    }
    
    // 添加用户消息到会话
    auto context = dialogueManager_->addUserMessage(currentSessionId, query);
    if (!context) {
        LOG_ERROR("无法添加用户消息，会话ID可能无效: {}", currentSessionId);
        callback("系统错误：无效的会话标识", true);
        return;
    }
    
    // 解析查询
    QueryParseResult parseResult = queryParser_->parse(query);
    
    // 准备搜索参数
    SearchParams searchParams;
    searchParams.maxResults = config_.maxSearchResults;
    searchParams.includeProperties = true;
    searchParams.minConfidence = 0.2f;
    
    // 使用上下文增强查询
    if (config_.enableDialogueHistory && context->messages.size() > 1) {
        enhanceQueryWithContext(parseResult, context);
    }
    
    // 执行搜索
    SearchResult searchResult = graphSearcher_->search(parseResult, searchParams);
    
    // 准备生成选项
    AnswerGenerationOptions genOptions;
    genOptions.useModelGeneration = config_.enableModelGeneration;
    genOptions.includeSourceInfo = true;
    genOptions.language = config_.defaultLanguage;
    
    // 生成查询ID
    std::string queryId = generateQueryId();
    dialogueManager_->setContextAttribute(currentSessionId, "last_query_id", queryId);
    
    // 缓存完整回答
    std::string fullAnswer;
    
    // 流式生成回答
    answerGenerator_->generateAnswerStream(
        parseResult, 
        searchResult, 
        [this, &fullAnswer, currentSessionId, callback](const std::string& token, bool isFinished) {
            // 将token传递给调用者
            callback(token, isFinished);
            
            // 缓存完整回答
            fullAnswer += token;
            
            // 如果完成，将完整回答添加到会话
            if (isFinished) {
                this->dialogueManager_->addAssistantResponse(currentSessionId, fullAnswer);
                LOG_INFO("流式回答完成，总长度: {}", fullAnswer.length());
            }
        },
        genOptions
    );
}

// 获取查询配置
const QueryConfig& QueryManager::getConfig() const {
    return config_;
}

// 更新查询配置
void QueryManager::updateConfig(const QueryConfig& config) {
    config_ = config;
    
    // 更新相关组件的配置
    if (graphSearcher_) {
        graphSearcher_->setSearchStrategy(config.defaultSearchStrategy);
    }
    
    LOG_INFO("更新查询配置，默认搜索策略: {}", 
            graphSearcher_ ? graphSearcher_->getSearchStrategyName(config.defaultSearchStrategy) : "未知");
}

// 获取查询解析器
std::shared_ptr<QueryParser> QueryManager::getQueryParser() {
    return queryParser_;
}

// 获取图谱搜索器
std::shared_ptr<GraphSearcher> QueryManager::getGraphSearcher() {
    return graphSearcher_;
}

// 获取回答生成器
std::shared_ptr<AnswerGenerator> QueryManager::getAnswerGenerator() {
    return answerGenerator_;
}

// 获取对话管理器
std::shared_ptr<DialogueManager> QueryManager::getDialogueManager() {
    return dialogueManager_;
}

// 关闭会话
bool QueryManager::closeSession(const std::string& sessionId) {
    if (dialogueManager_) {
        return dialogueManager_->removeSession(sessionId);
    }
    return false;
}

// 清除会话历史
void QueryManager::clearSessionHistory(const std::string& sessionId) {
    if (dialogueManager_) {
        dialogueManager_->clearHistory(sessionId);
    }
}

// 设置用户反馈
void QueryManager::setUserFeedback(
    const std::string& sessionId,
    const std::string& queryId,
    const std::string& feedback) {
    
    LOG_INFO("设置用户反馈，会话ID: {}, 查询ID: {}, 反馈: {}", 
            sessionId, queryId, feedback);
    
    if (dialogueManager_) {
        // 保存反馈信息
        std::string feedbackKey = "feedback_" + queryId;
        dialogueManager_->setContextAttribute(sessionId, feedbackKey, feedback);
        
        // TODO: 用反馈信息改进模型或搜索策略
    }
}

// 增强查询，利用上下文信息
void QueryManager::enhanceQueryWithContext(
    QueryParseResult& parseResult, 
    std::shared_ptr<DialogueContext> context) {
    
    // 简单的上下文增强：检查最近的消息，提取关键实体和关系
    std::vector<DialogueMessage> history(context->messages.begin(), context->messages.end());
    
    // 只使用最近的几条消息
    const size_t maxContextMessages = 5;
    size_t startIdx = (history.size() > maxContextMessages) ? 
                      (history.size() - maxContextMessages) : 0;
    
    // 检查最近的用户消息，寻找可能的实体和关系引用
    for (size_t i = startIdx; i < history.size(); ++i) {
        const auto& msg = history[i];
        
        // 只处理用户消息
        if (msg.role == DialogueMessage::Role::USER) {
            // 此处可以添加更复杂的逻辑，例如指代消解
            // 简单实现：尝试识别代词引用
            if (parseResult.linkedEntities.empty() && 
                (parseResult.normalizedQuery.find("它") != std::string::npos ||
                 parseResult.normalizedQuery.find("他") != std::string::npos ||
                 parseResult.normalizedQuery.find("她") != std::string::npos ||
                 parseResult.normalizedQuery.find("这个") != std::string::npos)) {
                
                // 尝试从上下文属性中获取最后一个实体
                std::string lastEntity = context->attributes["last_entity"];
                if (!lastEntity.empty()) {
                    // 创建一个指向实体的指针并添加到链接实体列表中
                    auto entity = std::make_shared<knowledge::Entity>(
                        lastEntity,                    // id
                        "引用的实体",                  // name
                        knowledge::EntityType::UNKNOWN // type
                    );
                    
                    parseResult.linkedEntities.push_back(entity);
                    LOG_INFO("从上下文中解析代词引用，链接到实体: {}", lastEntity);
                }
            }
        }
    }
}

// 生成查询ID
std::string QueryManager::generateQueryId() {
    // 使用当前时间和随机数生成查询ID
    static std::mt19937 gen(std::random_device{}());
    static std::uniform_int_distribution<> dis(0, 9999);
    
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time);
    
    std::stringstream ss;
    ss << std::put_time(&tm, "%Y%m%d%H%M%S") << "-" << std::setw(4) << std::setfill('0') << dis(gen);
    
    return ss.str();
}

} // namespace query
} // namespace kb 