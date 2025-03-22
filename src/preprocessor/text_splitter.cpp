/**
 * @file text_splitter.cpp
 * @brief 文本分块器实现
 */

#include "preprocessor/text_splitter.h"
#include "common/logging.h"
#include <regex>
#include <algorithm>
#include <sstream>
#include <cmath>
#include <uuid/uuid.h>

namespace kb {
namespace preprocessor {

// 生成唯一ID
std::string generateUUID() {
    uuid_t uuid;
    char uuid_str[37];
    uuid_generate(uuid);
    uuid_unparse_lower(uuid, uuid_str);
    return std::string(uuid_str);
}

// ===== FixedSizeSplitter 实现 =====

FixedSizeSplitter::FixedSizeSplitter(size_t chunkSize, size_t overlapSize, 
                                     const std::unordered_map<std::string, std::string>& options)
    : chunkSize_(chunkSize), overlapSize_(overlapSize) {
    if (chunkSize <= overlapSize) {
        LOG_WARN("分块大小应大于重叠大小，已自动调整");
        overlapSize_ = chunkSize_ / 2;
    }
    
    // 保存选项
    options_ = options;
    
    // 添加配置到选项
    options_["chunk_size"] = std::to_string(chunkSize_);
    options_["overlap"] = std::to_string(overlapSize_);
}

std::vector<TextChunk> FixedSizeSplitter::split(const std::string& text) const {
    LOG_INFO("使用固定大小分块器处理文本 (长度: {})", text.length());
    
    std::vector<TextChunk> chunks;
    if (text.empty()) {
        LOG_WARN("输入文本为空，无法分块");
        return chunks;
    }
    
    // 获取块大小配置，默认为4000字符
    size_t chunkSize = 4000;
    try {
        std::string sizeStr = getOption("chunk_size", "4000");
        chunkSize = std::stoul(sizeStr);
        LOG_INFO("使用块大小: {}", chunkSize);
    } catch (const std::exception& e) {
        LOG_ERROR("解析块大小选项时出错: {}", e.what());
    }
    
    // 获取重叠配置，默认为200字符
    size_t overlap = 200;
    try {
        std::string overlapStr = getOption("overlap", "200");
        overlap = std::stoul(overlapStr);
        LOG_INFO("使用重叠大小: {}", overlap);
    } catch (const std::exception& e) {
        LOG_ERROR("解析重叠大小选项时出错: {}", e.what());
    }
    
    // 计算块数
    const size_t effectiveSize = (chunkSize > overlap) ? (chunkSize - overlap) : chunkSize;
    const size_t numChunks = (text.length() + effectiveSize - 1) / effectiveSize;
    LOG_INFO("预计产生 {} 个文本块", numChunks);
    
    for (size_t i = 0; i < numChunks; ++i) {
        size_t startPos = i * effectiveSize;
        size_t endPos = std::min(startPos + chunkSize, text.length());
        
        TextChunk chunk;
        chunk.text = text.substr(startPos, endPos - startPos);
        chunk.index = i;
        
        chunks.push_back(chunk);
        LOG_DEBUG("创建文本块 #{}, 起始位置: {}, 结束位置: {}, 大小: {}", 
                 i, startPos, endPos, chunk.text.length());
    }
    
    LOG_INFO("文本分块完成，共生成 {} 个块", chunks.size());
    return chunks;
}

std::vector<TextChunk> FixedSizeSplitter::split(
    const std::string& text,
    const std::unordered_map<std::string, std::string>& metadata) const {
    
    std::vector<TextChunk> chunks = split(text);
    
    // 添加元数据到每个块
    for (auto& chunk : chunks) {
        for (const auto& meta : metadata) {
            chunk.addMetadata(meta.first, meta.second);
        }
    }
    
    return chunks;
}

// ===== SentenceSplitter 实现 =====

SentenceSplitter::SentenceSplitter(size_t maxChunkSize, size_t minChunkSize,
                                 const std::unordered_map<std::string, std::string>& options)
    : maxChunkSize_(maxChunkSize), minChunkSize_(minChunkSize) {
    if (minChunkSize > maxChunkSize) {
        LOG_WARN("最小块大小应小于最大块大小，已自动调整");
        minChunkSize_ = maxChunkSize_ / 2;
    }
    
    // 保存选项
    options_ = options;
    
    // 添加配置到选项
    options_["max_chunk_size"] = std::to_string(maxChunkSize_);
    options_["min_chunk_size"] = std::to_string(minChunkSize_);
}

std::vector<TextChunk> SentenceSplitter::split(const std::string& text) const {
    LOG_INFO("使用句子分块器处理文本 (长度: {})", text.length());
    
    std::vector<TextChunk> chunks;
    if (text.empty()) {
        LOG_WARN("输入文本为空，无法分块");
        return chunks;
    }
    
    // 获取每个块的最大句子数，默认为10
    size_t maxSentences = 10;
    try {
        std::string maxStr = getOption("max_sentences", "10");
        maxSentences = std::stoul(maxStr);
        LOG_INFO("每个块最大句子数: {}", maxSentences);
    } catch (const std::exception& e) {
        LOG_ERROR("解析最大句子数选项时出错: {}", e.what());
    }
    
    // 获取每个块的最大字符数，默认为4000
    size_t maxChunkSize = 4000;
    try {
        std::string sizeStr = getOption("max_chunk_size", "4000");
        maxChunkSize = std::stoul(sizeStr);
        LOG_INFO("每个块最大字符数: {}", maxChunkSize);
    } catch (const std::exception& e) {
        LOG_ERROR("解析最大块大小选项时出错: {}", e.what());
    }
    
    // 使用正则表达式分割句子
    // 匹配中文句号、问号、感叹号，以及英文句号后跟空格或换行的情况
    std::regex sentenceDelimiter("([。！？\\.])([\\s\\n]|$)");
    
    std::sregex_token_iterator it(text.begin(), text.end(), sentenceDelimiter, {-1, 0});
    std::sregex_token_iterator end;
    
    std::vector<std::string> sentences;
    for (; it != end; ++it) {
        if (it->length() > 0) {
            sentences.push_back(*it);
        }
    }
    
    LOG_INFO("文本共分割为 {} 个句子", sentences.size());
    
    // 根据句子数和字符数组合句子成块
    size_t sentenceCount = 0;
    size_t charCount = 0;
    size_t chunkIndex = 0;
    std::string currentChunk;
    
    for (const auto& sentence : sentences) {
        if (sentenceCount >= maxSentences || charCount + sentence.length() > maxChunkSize) {
            // 当前块已满，创建新块
            if (!currentChunk.empty()) {
                TextChunk chunk;
                chunk.text = currentChunk;
                chunk.index = chunkIndex++;
                chunks.push_back(chunk);
                
                LOG_DEBUG("创建文本块 #{}, 句子数: {}, 大小: {}", 
                         chunk.index, sentenceCount, chunk.text.length());
                
                currentChunk.clear();
                sentenceCount = 0;
                charCount = 0;
            }
        }
        
        currentChunk += sentence;
        sentenceCount++;
        charCount += sentence.length();
    }
    
    // 添加最后一个块
    if (!currentChunk.empty()) {
        TextChunk chunk;
        chunk.text = currentChunk;
        chunk.index = chunkIndex;
        chunks.push_back(chunk);
        
        LOG_DEBUG("创建文本块 #{}, 句子数: {}, 大小: {}", 
                 chunk.index, sentenceCount, chunk.text.length());
    }
    
    LOG_INFO("文本分块完成，共生成 {} 个块", chunks.size());
    return chunks;
}

std::vector<TextChunk> SentenceSplitter::split(
    const std::string& text,
    const std::unordered_map<std::string, std::string>& metadata) const {
    
    std::vector<TextChunk> chunks = split(text);
    
    // 添加元数据到每个块
    for (auto& chunk : chunks) {
        for (const auto& meta : metadata) {
            chunk.addMetadata(meta.first, meta.second);
        }
    }
    
    return chunks;
}

// ===== ParagraphSplitter 实现 =====

ParagraphSplitter::ParagraphSplitter(size_t maxChunkSize, size_t minChunkSize,
                                   const std::unordered_map<std::string, std::string>& options)
    : maxChunkSize_(maxChunkSize), minChunkSize_(minChunkSize) {
    if (minChunkSize > maxChunkSize) {
        LOG_WARN("最小块大小应小于最大块大小，已自动调整");
        minChunkSize_ = maxChunkSize_ / 2;
    }
    
    // 保存选项
    options_ = options;
    
    // 添加配置到选项
    options_["max_chunk_size"] = std::to_string(maxChunkSize_);
    options_["min_chunk_size"] = std::to_string(minChunkSize_);
}

std::vector<TextChunk> ParagraphSplitter::split(const std::string& text) const {
    LOG_INFO("使用段落分块器处理文本 (长度: {})", text.length());
    
    std::vector<TextChunk> chunks;
    if (text.empty()) {
        LOG_WARN("输入文本为空，无法分块");
        return chunks;
    }
    
    // 获取每个块的最大段落数，默认为5
    size_t maxParagraphs = 5;
    try {
        std::string maxStr = getOption("max_paragraphs", "5");
        maxParagraphs = std::stoul(maxStr);
        LOG_INFO("每个块最大段落数: {}", maxParagraphs);
    } catch (const std::exception& e) {
        LOG_ERROR("解析最大段落数选项时出错: {}", e.what());
    }
    
    // 获取每个块的最大字符数，默认为4000
    size_t maxChunkSize = 4000;
    try {
        std::string sizeStr = getOption("max_chunk_size", "4000");
        maxChunkSize = std::stoul(sizeStr);
        LOG_INFO("每个块最大字符数: {}", maxChunkSize);
    } catch (const std::exception& e) {
        LOG_ERROR("解析最大块大小选项时出错: {}", e.what());
    }
    
    // 使用双换行符分割段落
    std::regex paragraphDelimiter("\\n\\s*\\n");
    std::sregex_token_iterator it(text.begin(), text.end(), paragraphDelimiter, -1);
    std::sregex_token_iterator end;
    
    std::vector<std::string> paragraphs;
    for (; it != end; ++it) {
        std::string paragraph = it->str();
        if (!paragraph.empty()) {
            // 删除开头和结尾的空白
            paragraph.erase(0, paragraph.find_first_not_of(" \t\n\r"));
            paragraph.erase(paragraph.find_last_not_of(" \t\n\r") + 1);
            
            if (!paragraph.empty()) {
                paragraphs.push_back(paragraph);
            }
        }
    }
    
    LOG_INFO("文本共分割为 {} 个段落", paragraphs.size());
    
    // 根据段落数和字符数组合段落成块
    size_t paragraphCount = 0;
    size_t charCount = 0;
    size_t chunkIndex = 0;
    std::string currentChunk;
    
    for (const auto& paragraph : paragraphs) {
        if (paragraphCount >= maxParagraphs || charCount + paragraph.length() > maxChunkSize) {
            // 当前块已满，创建新块
            if (!currentChunk.empty()) {
                TextChunk chunk;
                chunk.text = currentChunk;
                chunk.index = chunkIndex++;
                chunks.push_back(chunk);
                
                LOG_DEBUG("创建文本块 #{}, 段落数: {}, 大小: {}", 
                         chunk.index, paragraphCount, chunk.text.length());
                
                currentChunk.clear();
                paragraphCount = 0;
                charCount = 0;
            }
        }
        
        if (!currentChunk.empty()) {
            currentChunk += "\n\n";
        }
        
        currentChunk += paragraph;
        paragraphCount++;
        charCount += paragraph.length() + 2; // +2 for the newlines
    }
    
    // 添加最后一个块
    if (!currentChunk.empty()) {
        TextChunk chunk;
        chunk.text = currentChunk;
        chunk.index = chunkIndex;
        chunks.push_back(chunk);
        
        LOG_DEBUG("创建文本块 #{}, 段落数: {}, 大小: {}", 
                 chunk.index, paragraphCount, chunk.text.length());
    }
    
    LOG_INFO("文本分块完成，共生成 {} 个块", chunks.size());
    return chunks;
}

std::vector<TextChunk> ParagraphSplitter::split(
    const std::string& text,
    const std::unordered_map<std::string, std::string>& metadata) const {
    
    std::vector<TextChunk> chunks = split(text);
    
    // 添加元数据到每个块
    for (auto& chunk : chunks) {
        for (const auto& meta : metadata) {
            chunk.addMetadata(meta.first, meta.second);
        }
    }
    
    return chunks;
}

// ===== TextSplitterFactory 实现 =====

std::unique_ptr<TextSplitter> TextSplitterFactory::createSplitter(
    SplitStrategy strategy,
    const std::unordered_map<std::string, std::string>& options) {
    
    // 解析选项
    auto getOptionValue = [&options](const std::string& key, const std::string& defaultValue) {
        auto it = options.find(key);
        return it != options.end() ? it->second : defaultValue;
    };
    
    auto getOptionValueInt = [&getOptionValue](const std::string& key, int defaultValue) {
        std::string value = getOptionValue(key, std::to_string(defaultValue));
        try {
            return std::stoi(value);
        } catch (const std::exception&) {
            return defaultValue;
        }
    };
    
    // 根据策略创建分块器
    switch (strategy) {
        case SplitStrategy::FIXED_SIZE: {
            size_t chunkSize = getOptionValueInt("chunk_size", 1000);
            size_t overlapSize = getOptionValueInt("overlap_size", 200);
            return std::make_unique<FixedSizeSplitter>(chunkSize, overlapSize, options);
        }
        
        case SplitStrategy::SENTENCE: {
            size_t maxChunkSize = getOptionValueInt("max_chunk_size", 1000);
            size_t minChunkSize = getOptionValueInt("min_chunk_size", 200);
            return std::make_unique<SentenceSplitter>(maxChunkSize, minChunkSize, options);
        }
        
        case SplitStrategy::PARAGRAPH: {
            size_t maxChunkSize = getOptionValueInt("max_chunk_size", 2000);
            size_t minChunkSize = getOptionValueInt("min_chunk_size", 500);
            return std::make_unique<ParagraphSplitter>(maxChunkSize, minChunkSize, options);
        }
        
        case SplitStrategy::SEMANTIC:
        case SplitStrategy::HYBRID:
            // 这两种策略更复杂，将在单独的实现中处理
            LOG_WARN("请求的分块策略尚未实现，使用段落分块作为替代");
            return std::make_unique<ParagraphSplitter>(2000, 500, options);
            
        default:
            LOG_WARN("未知的分块策略，使用固定大小分块");
            return std::make_unique<FixedSizeSplitter>(1000, 200, options);
    }
}

} // namespace preprocessor
} // namespace kb 