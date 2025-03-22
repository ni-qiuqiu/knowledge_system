#include "preprocessor/preprocessing_pipeline.h"
#include "common/logging.h"
#include <algorithm>
#include <thread>
#include <future>

namespace kb {
namespace preprocessor {

// CleaningStep实现
CleaningStep::CleaningStep(std::shared_ptr<TextCleaner> cleaner)
    : cleaner_(cleaner) {
    if (!cleaner_) {
        cleaner_ = std::make_shared<BasicCleaner>();
    }
}

void CleaningStep::process(
    PreprocessingResult& result,
    const PreprocessingOptions& options) const {
    
    LOG_INFO("开始执行文本清洗步骤");
    
    if (!options.clean) {
        LOG_INFO("文本清洗步骤已跳过（根据选项设置）");
        result.cleanedText = result.originalText;
        return;
    }
    
    if (result.originalText.empty()) {
        LOG_WARN("原始文本为空，无法执行清洗");
        result.cleanedText = "";
        return;
    }
    
    try {
        result.cleanedText = cleaner_->clean(result.originalText);
        LOG_INFO("文本清洗完成，原始文本长度: {}, 清洗后长度: {}", 
                result.originalText.length(), result.cleanedText.length());
    } catch (const std::exception& e) {
        LOG_ERROR("文本清洗过程发生异常: {}", e.what());
        // 使用原始文本作为备选
        result.cleanedText = result.originalText;
    }
}

// SplittingStep实现
void SplittingStep::process(
    PreprocessingResult& result,
    const PreprocessingOptions& options) const {
    
    LOG_INFO("开始执行文本分块步骤");
    
    if (!options.split) {
        LOG_INFO("文本分块步骤已跳过（根据选项设置）");
        return;
    }
    
    if (result.cleanedText.empty()) {
        LOG_WARN("清洗后文本为空，无法执行分块");
        return;
    }
    
    try {
        std::shared_ptr<TextSplitter> splitter;
        
        // 根据策略创建分块器
        switch (options.splitStrategy) {
            case SplitStrategy::FIXED_SIZE:
                splitter = std::make_shared<FixedSizeSplitter>();
                break;
            case SplitStrategy::SENTENCE:
                splitter = std::make_shared<SentenceSplitter>();
                break;
            case SplitStrategy::PARAGRAPH:
            default:
                splitter = std::make_shared<ParagraphSplitter>();
                break;
        }
        
        // 设置分块器选项
        for (const auto& option : options.splitterOptions) {
            splitter->setOption(option.first, option.second);
        }
        
        // 执行分块
        result.chunks = splitter->split(result.cleanedText);
        LOG_INFO("文本分块完成，共生成 {} 个文本块", result.chunks.size());
    } catch (const std::exception& e) {
        LOG_ERROR("文本分块过程发生异常: {}", e.what());
        // 至少添加一个包含整个文本的块
        if (result.chunks.empty() && !result.cleanedText.empty()) {
            TextChunk chunk;
            chunk.text = result.cleanedText;
            chunk.index = 0;
            result.chunks.push_back(chunk);
        }
    }
}

// MetadataExtractionStep实现
MetadataExtractionStep::MetadataExtractionStep(const std::string& filePath)
    : filePath_(filePath) {
}

void MetadataExtractionStep::process(
    PreprocessingResult& result,
    const PreprocessingOptions& options) const {
    
    LOG_INFO("开始执行元数据提取步骤");
    
    if (!options.extractMetadata) {
        LOG_INFO("元数据提取步骤已跳过（根据选项设置）");
        return;
    }
    
    try {
        // 提取基本文本元数据
        if (!result.cleanedText.empty()) {
            result.metadata["character_count"] = std::to_string(result.cleanedText.length());
            
            // 计算行数
            size_t lines = std::count(result.cleanedText.begin(), result.cleanedText.end(), '\n') + 1;
            result.metadata["line_count"] = std::to_string(lines);
            
            // 估算词数（简单实现）
            size_t wordCount = 0;
            bool inWord = false;
            for (char c : result.cleanedText) {
                if (std::isalnum(c) || c > 127) { // 英文词或中文字符
                    if (!inWord) {
                        inWord = true;
                        wordCount++;
                    }
                } else {
                    inWord = false;
                }
            }
            result.metadata["word_count"] = std::to_string(wordCount);
        }
        
        // 如果有文件路径，提取文件元数据
        if (!filePath_.empty()) {
            auto extractor = MetadataExtractorFactory::createExtractor(filePath_);
            if (extractor) {
                auto fileMetadata = extractor->extract(filePath_);
                // 合并文件元数据
                for (const auto& item : fileMetadata) {
                    result.metadata[item.first] = item.second;
                }
                LOG_INFO("文件元数据提取完成，获取 {} 个元数据项", fileMetadata.size());
            }
        }
        
        LOG_INFO("元数据提取完成，共获取 {} 个元数据项", result.metadata.size());
    } catch (const std::exception& e) {
        LOG_ERROR("元数据提取过程发生异常: {}", e.what());
    }
}

// TokenizationStep实现
TokenizationStep::TokenizationStep(std::shared_ptr<ChineseTokenizer> tokenizer)
    : tokenizer_(tokenizer) {
    if (!tokenizer_) {
        tokenizer_ = std::make_shared<ChineseTokenizer>();
    }
}

void TokenizationStep::process(
    PreprocessingResult& result,
    const PreprocessingOptions& options) const {
    
    LOG_INFO("开始执行分词步骤");
    
    if (!options.tokenize) {
        LOG_INFO("分词步骤已跳过（根据选项设置）");
        return;
    }
    
    if (result.cleanedText.empty()) {
        LOG_WARN("清洗后文本为空，无法执行分词");
        return;
    }
    
    try {
        result.tokens = tokenizer_->cut(result.cleanedText, options.tokenizeMode);
        LOG_INFO("分词完成，共生成 {} 个词语", result.tokens.size());
    } catch (const std::exception& e) {
        LOG_ERROR("分词过程发生异常: {}", e.what());
    }
}

// KeywordExtractionStep实现
KeywordExtractionStep::KeywordExtractionStep(std::shared_ptr<ChineseTokenizer> tokenizer)
    : tokenizer_(tokenizer) {
    if (!tokenizer_) {
        tokenizer_ = std::make_shared<ChineseTokenizer>();
    }
}

void KeywordExtractionStep::process(
    PreprocessingResult& result,
    const PreprocessingOptions& options) const {
    
    LOG_INFO("开始执行关键词提取步骤");
    
    if (!options.extractKeywords) {
        LOG_INFO("关键词提取步骤已跳过（根据选项设置）");
        return;
    }
    
    if (result.cleanedText.empty()) {
        LOG_WARN("清洗后文本为空，无法提取关键词");
        return;
    }
    
    try {
        result.keywords = tokenizer_->extractKeywords(
            result.cleanedText, options.keywordCount);
        
        LOG_INFO("关键词提取完成，共提取 {} 个关键词", result.keywords.size());
        
        // 记录关键词到元数据
        if (!result.keywords.empty()) {
            std::string keywordsStr;
            for (const auto& kw : result.keywords) {
                if (!keywordsStr.empty()) {
                    keywordsStr += ", ";
                }
                keywordsStr += kw.first;
            }
            result.metadata["keywords"] = keywordsStr;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("关键词提取过程发生异常: {}", e.what());
    }
}

// PreprocessingPipeline实现
PreprocessingPipeline::PreprocessingPipeline() 
    : loaderFactory_(std::make_shared<DocumentLoaderFactory>()) {
    
    // 添加默认步骤
    addStep(std::make_shared<CleaningStep>())
        .addStep(std::make_shared<SplittingStep>())
        .addStep(std::make_shared<MetadataExtractionStep>())
        .addStep(std::make_shared<TokenizationStep>())
        .addStep(std::make_shared<KeywordExtractionStep>());
    
    LOG_INFO("预处理流水线已初始化，包含 {} 个默认步骤", steps_.size());
}

PreprocessingPipeline& PreprocessingPipeline::addStep(std::shared_ptr<PreprocessingStep> step) {
    if (step) {
        steps_.push_back(step);
        LOG_INFO("预处理流水线添加步骤: {}", step->getName());
    }
    return *this;
}

PreprocessingPipeline& PreprocessingPipeline::clearSteps() {
    steps_.clear();
    LOG_INFO("预处理流水线已清除所有步骤");
    return *this;
}

PreprocessingResult PreprocessingPipeline::processFile(
    const std::string& filePath,
    const PreprocessingOptions& options) const {
    
    LOG_INFO("开始处理文件: {}", filePath);
    
    try {
        // 创建文档加载器并加载文件
        auto loader = loaderFactory_->createLoader(filePath);
        if (!loader) {
            LOG_ERROR("无法为文件创建加载器: {}", filePath);
            return PreprocessingResult();
        }
        
        // 加载文档内容
        std::string content = loader->loadDocument(filePath);
        if (content.empty()) {
            LOG_WARN("文件内容为空: {}", filePath);
        }
        
        // 处理文本内容
        return processText(content, options, filePath);
    } catch (const std::exception& e) {
        LOG_ERROR("处理文件过程发生异常: {}, 文件: {}", e.what(), filePath);
        return PreprocessingResult();
    }
}

PreprocessingResult PreprocessingPipeline::processText(
    const std::string& text,
    const PreprocessingOptions& options,
    const std::string& filePath) const {
    
    LOG_INFO("开始处理文本, 长度: {}", text.length());
    
    PreprocessingResult result(text);
    
    // 如果提供了文件路径，更新元数据提取步骤
    if (!filePath.empty()) {
        // 创建一个新的步骤向量，复制所有步骤
        std::vector<std::shared_ptr<PreprocessingStep>> updatedSteps = steps_;
        
        // 更新元数据提取步骤
        for (size_t i = 0; i < updatedSteps.size(); ++i) {
            if (auto metadataStep = std::dynamic_pointer_cast<MetadataExtractionStep>(updatedSteps[i])) {
                // 替换为新的元数据提取步骤，包含文件路径
                updatedSteps[i] = std::make_shared<MetadataExtractionStep>(filePath);
                break;
            }
        }
        
        // 使用更新后的步骤执行处理
        for (const auto& step : updatedSteps) {
            try {
                LOG_INFO("执行预处理步骤: {}", step->getName());
                step->process(result, options);
            } catch (const std::exception& e) {
                LOG_ERROR("执行预处理步骤异常: {}, 步骤: {}", 
                         e.what(), step->getName());
            }
        }
    }
    else {
        // 依次执行各个预处理步骤
        for (const auto& step : steps_) {
            try {
                LOG_INFO("执行预处理步骤: {}", step->getName());
                step->process(result, options);
            } catch (const std::exception& e) {
                LOG_ERROR("执行预处理步骤异常: {}, 步骤: {}", 
                         e.what(), step->getName());
            }
        }
    }
    
    LOG_INFO("文本处理完成");
    return result;
}

std::vector<PreprocessingResult> PreprocessingPipeline::batchProcessFiles(
    const std::vector<std::string>& filePaths,
    const PreprocessingOptions& options,
    std::function<void(size_t, size_t, const std::string&)> progressCallback) const {
    
    LOG_INFO("开始批量处理 {} 个文件", filePaths.size());
    
    std::vector<PreprocessingResult> results;
    results.reserve(filePaths.size());
    
    // 获取处理器数量用于并行计算
    const size_t numThreads = std::thread::hardware_concurrency();
    const size_t batchSize = std::max(size_t(1), numThreads);
    
    LOG_INFO("批处理使用 {} 个线程", batchSize);
    
    // 分批处理文件
    for (size_t i = 0; i < filePaths.size(); i += batchSize) {
        // 计算当前批次的结束索引
        size_t end = std::min(i + batchSize, filePaths.size());
        
        // 创建一组异步任务
        std::vector<std::future<PreprocessingResult>> futures;
        for (size_t j = i; j < end; ++j) {
            futures.push_back(std::async(std::launch::async, 
                [this, &filePaths, &options, j]() {
                    return this->processFile(filePaths[j], options);
                }
            ));
        }
        
        // 收集结果
        for (size_t j = 0; j < futures.size(); ++j) {
            size_t currentIndex = i + j;
            const std::string& filePath = filePaths[currentIndex];
            
            try {
                // 报告进度
                if (progressCallback) {
                    progressCallback(currentIndex + 1, filePaths.size(), filePath);
                }
                
                // 获取处理结果
                PreprocessingResult result = futures[j].get();
                results.push_back(std::move(result));
                
                LOG_INFO("文件处理完成 ({}/{}): {}", 
                         currentIndex + 1, filePaths.size(), filePath);
            } catch (const std::exception& e) {
                LOG_ERROR("获取文件处理结果异常: {}, 文件: {}", 
                         e.what(), filePath);
                // 添加空结果
                results.push_back(PreprocessingResult());
            }
        }
    }
    
    LOG_INFO("批量处理完成，成功处理 {} 个文件", results.size());
    return results;
}

} // namespace preprocessor
} // namespace kb 