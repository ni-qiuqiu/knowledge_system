/**
 * @file answer_generator.h
 * @brief 回答生成器，负责基于检索结果生成回答
 */

#ifndef KB_ANSWER_GENERATOR_H
#define KB_ANSWER_GENERATOR_H

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "engine/model_interface.h"
#include "query/graph_searcher.h"
#include "query/query_parser.h"

namespace kb {
namespace query {

/**
 * @brief 回答生成选项结构
 */
struct AnswerGenerationOptions {
    bool useModelGeneration;            ///< 是否使用模型生成
    bool includeSourceInfo;             ///< 是否包含来源信息
    bool includeConfidenceInfo;         ///< 是否包含置信度信息
    float minConfidence;                ///< 最小置信度阈值
    size_t maxLength;                   ///< 最大生成长度
    std::string language;               ///< 生成语言
    
    // 构造函数
    AnswerGenerationOptions() 
        : useModelGeneration(true),
          includeSourceInfo(false),
          includeConfidenceInfo(false),
          minConfidence(0.2f),
          maxLength(1000),
          language("zh") {}
};

/**
 * @brief 回答生成结果结构
 */
struct AnswerGenerationResult {
    std::string answer;                  ///< 生成的回答
    float confidence;                    ///< 回答的置信度
    std::vector<std::string> sources;    ///< 信息来源
    bool isDirectAnswer;                 ///< 是否直接从知识库获取的答案
    bool isModelGenerated;               ///< 是否由模型生成
    
    // 构造函数
    AnswerGenerationResult() 
        : confidence(0.0f),
          isDirectAnswer(false),
          isModelGenerated(false) {}
};

/**
 * @brief 流式回答回调函数类型
 * @param token 生成的标记（可能是字符或词）
 * @param isFinished 是否生成完成
 */
using StreamAnswerCallback = std::function<void(const std::string& token, bool isFinished)>;

/**
 * @brief 回答生成器类
 */
class AnswerGenerator {
public:
    /**
     * @brief 构造函数
     * @param model 语言模型指针
     */
    explicit AnswerGenerator(std::shared_ptr<engine::ModelInterface> model);
    
    /**
     * @brief 析构函数
     */
    virtual ~AnswerGenerator();
    
    /**
     * @brief 生成回答
     * @param parseResult 查询解析结果
     * @param searchResult 搜索结果
     * @param options 生成选项
     * @return 生成的回答
     */
    virtual AnswerGenerationResult generateAnswer(
        const QueryParseResult& parseResult,
        const SearchResult& searchResult,
        const AnswerGenerationOptions& options = AnswerGenerationOptions());
    
    /**
     * @brief 流式生成回答
     * @param parseResult 查询解析结果
     * @param searchResult 搜索结果
     * @param callback 回调函数
     * @param options 生成选项
     */
    virtual void generateAnswerStream(
        const QueryParseResult& parseResult,
        const SearchResult& searchResult,
        StreamAnswerCallback callback,
        const AnswerGenerationOptions& options = AnswerGenerationOptions());
    
    /**
     * @brief 设置语言模型
     * @param model 语言模型指针
     */
    void setModel(std::shared_ptr<engine::ModelInterface> model);
    
    /**
     * @brief 获取当前语言模型
     * @return 当前语言模型指针
     */
    std::shared_ptr<engine::ModelInterface> getModel() const;

protected:
    /**
     * @brief 构建提示词
     * @param parseResult 查询解析结果
     * @param searchResult 搜索结果
     * @param options 生成选项
     * @return 构建的提示词
     */
    virtual std::string buildPrompt(
        const QueryParseResult& parseResult,
        const SearchResult& searchResult,
        const AnswerGenerationOptions& options);
    
    /**
     * @brief 直接构建回答
     * @param parseResult 查询解析结果
     * @param searchResult 搜索结果
     * @param options 生成选项
     * @return 构建的回答
     */
    virtual AnswerGenerationResult buildDirectAnswer(
        const QueryParseResult& parseResult,
        const SearchResult& searchResult,
        const AnswerGenerationOptions& options);
    
    /**
     * @brief 后处理生成的回答
     * @param answer 原始回答
     * @param parseResult 查询解析结果
     * @param options 生成选项
     * @return 处理后的回答
     */
    virtual std::string postprocessAnswer(
        const std::string& answer,
        const QueryParseResult& parseResult,
        const AnswerGenerationOptions& options);
    
    /**
     * @brief 计算回答的置信度
     * @param answer 回答
     * @param searchResult 搜索结果
     * @return 置信度
     */
    virtual float calculateConfidence(
        const std::string& answer,
        const SearchResult& searchResult);

private:
    std::shared_ptr<engine::ModelInterface> model_; ///< 语言模型
};

} // namespace query
} // namespace kb

#endif // KB_ANSWER_GENERATOR_H 