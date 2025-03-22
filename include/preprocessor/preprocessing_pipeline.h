/**
 * @file preprocessing_pipeline.h
 * @brief 预处理流水线，定义文本预处理流程
 */

#ifndef KB_PREPROCESSING_PIPELINE_H
#define KB_PREPROCESSING_PIPELINE_H

#include "preprocessor/document_loader.h"
#include "preprocessor/text_cleaner.h"
#include "preprocessor/text_splitter.h"
#include "preprocessor/metadata_extractor.h"
#include "preprocessor/chinese_tokenizer.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace kb {
namespace preprocessor {

/**
 * @brief 预处理结果结构体
 */
struct PreprocessingResult {
    std::string originalText;                                  ///< 原始文本
    std::string cleanedText;                                   ///< 清洗后的文本
    std::vector<TextChunk> chunks;                             ///< 分块结果
    std::unordered_map<std::string, std::string> metadata;     ///< 元数据
    std::vector<std::string> tokens;                           ///< 分词结果
    std::vector<std::pair<std::string, double>> keywords;      ///< 关键词及权重
    
    /**
     * @brief 构造函数
     * @param originalText 原始文本
     */
    explicit PreprocessingResult(const std::string& originalText = "")
        : originalText(originalText) {}
};

/**
 * @brief 预处理选项结构体
 */
struct PreprocessingOptions {
    bool clean = true;                     ///< 是否清洗文本
    bool split = true;                     ///< 是否分块
    bool extractMetadata = true;           ///< 是否提取元数据
    bool tokenize = true;                  ///< 是否分词
    bool extractKeywords = true;           ///< 是否提取关键词
    
    SplitStrategy splitStrategy = SplitStrategy::PARAGRAPH;  ///< 分块策略
    TokenizeMode tokenizeMode = TokenizeMode::MIX;           ///< 分词模式
    size_t keywordCount = 10;                                ///< 关键词数量
    
    /**
     * @brief 分块器选项
     */
    std::unordered_map<std::string, std::string> splitterOptions;
};

/**
 * @brief 预处理步骤接口
 */
class PreprocessingStep {
public:
    /**
     * @brief 析构函数
     */
    virtual ~PreprocessingStep() = default;
    
    /**
     * @brief 处理文本
     * @param result 预处理结果
     * @param options 预处理选项
     */
    virtual void process(
        PreprocessingResult& result,
        const PreprocessingOptions& options) const = 0;
    
    /**
     * @brief 获取步骤名称
     * @return 步骤名称
     */
    virtual std::string getName() const = 0;
};

/**
 * @brief 文本清洗步骤
 */
class CleaningStep : public PreprocessingStep {
public:
    /**
     * @brief 构造函数
     * @param cleaner 文本清洗器
     */
    explicit CleaningStep(std::shared_ptr<TextCleaner> cleaner = nullptr);
    
    /**
     * @brief 处理文本
     * @param result 预处理结果
     * @param options 预处理选项
     */
    void process(
        PreprocessingResult& result,
        const PreprocessingOptions& options) const override;
    
    /**
     * @brief 获取步骤名称
     * @return 步骤名称
     */
    std::string getName() const override {
        return "文本清洗";
    }
    
private:
    std::shared_ptr<TextCleaner> cleaner_;  ///< 文本清洗器
};

/**
 * @brief 文本分块步骤
 */
class SplittingStep : public PreprocessingStep {
public:
    /**
     * @brief 处理文本
     * @param result 预处理结果
     * @param options 预处理选项
     */
    void process(
        PreprocessingResult& result,
        const PreprocessingOptions& options) const override;
    
    /**
     * @brief 获取步骤名称
     * @return 步骤名称
     */
    std::string getName() const override {
        return "文本分块";
    }
};

/**
 * @brief 元数据提取步骤
 */
class MetadataExtractionStep : public PreprocessingStep {
public:
    /**
     * @brief 构造函数
     * @param filePath 文件路径（可选）
     */
    explicit MetadataExtractionStep(const std::string& filePath = "");
    
    /**
     * @brief 处理文本
     * @param result 预处理结果
     * @param options 预处理选项
     */
    void process(
        PreprocessingResult& result,
        const PreprocessingOptions& options) const override;
    
    /**
     * @brief 获取步骤名称
     * @return 步骤名称
     */
    std::string getName() const override {
        return "元数据提取";
    }
    
private:
    std::string filePath_;  ///< 文件路径
};

/**
 * @brief 分词步骤
 */
class TokenizationStep : public PreprocessingStep {
public:
    /**
     * @brief 构造函数
     * @param tokenizer 分词器
     */
    explicit TokenizationStep(std::shared_ptr<ChineseTokenizer> tokenizer = nullptr);
    
    /**
     * @brief 处理文本
     * @param result 预处理结果
     * @param options 预处理选项
     */
    void process(
        PreprocessingResult& result,
        const PreprocessingOptions& options) const override;
    
    /**
     * @brief 获取步骤名称
     * @return 步骤名称
     */
    std::string getName() const override {
        return "分词";
    }
    
private:
    std::shared_ptr<ChineseTokenizer> tokenizer_;  ///< 分词器
};

/**
 * @brief 关键词提取步骤
 */
class KeywordExtractionStep : public PreprocessingStep {
public:
    /**
     * @brief 构造函数
     * @param tokenizer 分词器
     */
    explicit KeywordExtractionStep(std::shared_ptr<ChineseTokenizer> tokenizer = nullptr);
    
    /**
     * @brief 处理文本
     * @param result 预处理结果
     * @param options 预处理选项
     */
    void process(
        PreprocessingResult& result,
        const PreprocessingOptions& options) const override;
    
    /**
     * @brief 获取步骤名称
     * @return 步骤名称
     */
    std::string getName() const override {
        return "关键词提取";
    }
    
private:
    std::shared_ptr<ChineseTokenizer> tokenizer_;  ///< 分词器
};

/**
 * @brief 预处理流水线
 */
class PreprocessingPipeline {
public:
    /**
     * @brief 默认构造函数
     */
    PreprocessingPipeline();
    
    /**
     * @brief 添加处理步骤
     * @param step 处理步骤
     * @return 流水线引用，用于链式调用
     */
    PreprocessingPipeline& addStep(std::shared_ptr<PreprocessingStep> step);
    
    /**
     * @brief 清除所有处理步骤
     * @return 流水线引用，用于链式调用
     */
    PreprocessingPipeline& clearSteps();
    
    /**
     * @brief 处理文件
     * @param filePath 文件路径
     * @param options 预处理选项
     * @return 处理结果
     */
    PreprocessingResult processFile(
        const std::string& filePath,
        const PreprocessingOptions& options = PreprocessingOptions()) const;
    
    /**
     * @brief 处理文本
     * @param text 输入文本
     * @param options 预处理选项
     * @param filePath 文件路径（可选，用于元数据提取）
     * @return 预处理结果
     */
    PreprocessingResult processText(
        const std::string& text,
        const PreprocessingOptions& options = {},
        const std::string& filePath = "") const;
    
    /**
     * @brief 批量处理文件
     * @param filePaths 文件路径列表
     * @param options 预处理选项
     * @param progressCallback 进度回调函数
     * @return 预处理结果列表
     */
    std::vector<PreprocessingResult> batchProcessFiles(
        const std::vector<std::string>& filePaths,
        const PreprocessingOptions& options = {},
        std::function<void(size_t, size_t, const std::string&)> progressCallback = nullptr) const;
    
private:
    std::vector<std::shared_ptr<PreprocessingStep>> steps_;  ///< 预处理步骤列表
    std::shared_ptr<DocumentLoaderFactory> loaderFactory_;  ///< 文档加载器工厂
};

} // namespace preprocessor
} // namespace kb

#endif // KB_PREPROCESSING_PIPELINE_H 