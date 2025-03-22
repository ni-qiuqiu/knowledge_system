/**
 * @file query_manager.h
 * @brief 查询管理器，负责协调各查询组件，作为查询模块的主入口
 */

#ifndef KB_QUERY_MANAGER_H
#define KB_QUERY_MANAGER_H

#include <string>
#include <memory>
#include <vector>
#include <functional>
#include "query/query_parser.h"
#include "query/graph_searcher.h"
#include "query/answer_generator.h"
#include "query/dialogue_manager.h"
#include "knowledge/knowledge_graph.h"
#include "engine/model_interface.h"

namespace kb {
namespace query {

/**
 * @brief 查询配置结构
 */
struct QueryConfig {
    bool enableModelGeneration;          ///< 是否启用模型生成
    bool enableDialogueHistory;          ///< 是否启用对话历史
    bool enableStreamResponse;           ///< 是否启用流式回答
    size_t maxSearchResults;             ///< 最大搜索结果数
    SearchStrategy defaultSearchStrategy; ///< 默认搜索策略
    std::string defaultLanguage;         ///< 默认语言
    
    // 构造函数
    QueryConfig() 
        : enableModelGeneration(true),
          enableDialogueHistory(true),
          enableStreamResponse(false),
          maxSearchResults(10),
          defaultSearchStrategy(SearchStrategy::HYBRID),
          defaultLanguage("zh") {}
};

/**
 * @brief 查询结果回调函数类型
 * @param answer 生成的回答
 * @param isComplete 是否完成
 */
using QueryResultCallback = std::function<void(const std::string& answer, bool isComplete)>;

/**
 * @brief 查询管理器类
 */
class QueryManager {
public:
    /**
     * @brief 获取单例实例
     * @return 查询管理器实例
     */
    static QueryManager& getInstance();
    
    /**
     * @brief 析构函数
     */
    ~QueryManager();
    
    /**
     * @brief 初始化查询管理器
     * @param knowledgeGraph 知识图谱
     * @param model 语言模型
     * @param config 查询配置
     * @return 是否成功初始化
     */
    bool initialize(
        std::shared_ptr<knowledge::KnowledgeGraph> knowledgeGraph,
        std::shared_ptr<engine::ModelInterface> model,
        const QueryConfig& config = QueryConfig());
    
    /**
     * @brief 处理查询
     * @param query 用户查询文本
     * @param sessionId 会话ID（为空则创建新会话）
     * @return 查询回答
     */
    std::string processQuery(const std::string& query, const std::string& sessionId = "");
    
    /**
     * @brief 流式处理查询
     * @param query 用户查询文本
     * @param callback 结果回调函数
     * @param sessionId 会话ID（为空则创建新会话）
     */
    void processQueryStream(
        const std::string& query,
        QueryResultCallback callback,
        const std::string& sessionId = "");
    
    /**
     * @brief 获取查询配置
     * @return 当前查询配置
     */
    const QueryConfig& getConfig() const;
    
    /**
     * @brief 更新查询配置
     * @param config 新的查询配置
     */
    void updateConfig(const QueryConfig& config);
    
    /**
     * @brief 获取查询解析器
     * @return 查询解析器指针
     */
    std::shared_ptr<QueryParser> getQueryParser();
    
    /**
     * @brief 获取图谱搜索器
     * @return 图谱搜索器指针
     */
    std::shared_ptr<GraphSearcher> getGraphSearcher();
    
    /**
     * @brief 获取回答生成器
     * @return 回答生成器指针
     */
    std::shared_ptr<AnswerGenerator> getAnswerGenerator();
    
    /**
     * @brief 获取对话管理器
     * @return 对话管理器指针
     */
    std::shared_ptr<DialogueManager> getDialogueManager();
    
    /**
     * @brief 关闭会话
     * @param sessionId 会话ID
     * @return 是否成功关闭
     */
    bool closeSession(const std::string& sessionId);
    
    /**
     * @brief 清除会话历史
     * @param sessionId 会话ID
     */
    void clearSessionHistory(const std::string& sessionId);
    
    /**
     * @brief 设置用户反馈
     * @param sessionId 会话ID
     * @param queryId 查询ID
     * @param feedback 反馈信息
     */
    void setUserFeedback(
        const std::string& sessionId,
        const std::string& queryId,
        const std::string& feedback);

    /**
     * @brief 增强查询，利用上下文信息
     * @param parseResult 查询解析结果
     * @param context 对话上下文
     */
    void enhanceQueryWithContext(QueryParseResult &parseResult, std::shared_ptr<DialogueContext> context);

private:
    /**
     * @brief 私有构造函数（单例模式）
     */
    QueryManager();
    
    /**
     * @brief 禁止拷贝构造
     */
    QueryManager(const QueryManager&) = delete;
    
    /**
     * @brief 禁止赋值操作
     */
    QueryManager& operator=(const QueryManager&) = delete;
    
    /**
     * @brief 生成查询ID
     * @return 查询ID
     */
    std::string generateQueryId();
    
    std::shared_ptr<QueryParser> queryParser_;             ///< 查询解析器
    std::shared_ptr<GraphSearcher> graphSearcher_;         ///< 图谱搜索器
    std::shared_ptr<AnswerGenerator> answerGenerator_;     ///< 回答生成器
    std::shared_ptr<DialogueManager> dialogueManager_;     ///< 对话管理器
    std::shared_ptr<knowledge::KnowledgeGraph> knowledgeGraph_; ///< 知识图谱
    QueryConfig config_;                                   ///< 查询配置
    bool initialized_;                                     ///< 是否已初始化
};

} // namespace query
} // namespace kb

#endif // KB_QUERY_MANAGER_H 