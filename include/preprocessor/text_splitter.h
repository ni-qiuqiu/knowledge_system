/**
 * @file text_splitter.h
 * @brief 文本分块器，定义文本分块功能
 */

#ifndef KB_TEXT_SPLITTER_H
#define KB_TEXT_SPLITTER_H

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>

namespace kb {
namespace preprocessor {

/**
 * @brief 文本块结构体
 */
struct TextChunk {
    std::string text;                                  ///< 文本内容
    std::unordered_map<std::string, std::string> metadata; ///< 元数据信息
    size_t index = 0;                                 ///< 块索引
    std::string id;                                   ///< 块唯一标识符
    
    /**
     * @brief 构造函数
     * @param text 文本内容
     * @param index 块索引
     */
    TextChunk(const std::string& text = "", size_t index = 0)
        : text(text), index(index) {}
    
    /**
     * @brief 添加元数据
     * @param key 元数据键
     * @param value 元数据值
     */
    void addMetadata(const std::string& key, const std::string& value) {
        metadata[key] = value;
    }
    
    /**
     * @brief 获取元数据
     * @param key 元数据键
     * @param defaultValue 默认值
     * @return 元数据值
     */
    std::string getMetadata(const std::string& key, const std::string& defaultValue = "") const {
        auto it = metadata.find(key);
        return it != metadata.end() ? it->second : defaultValue;
    }
};

/**
 * @brief 分块策略枚举
 */
enum class SplitStrategy {
    FIXED_SIZE,       ///< 固定大小分块
    SENTENCE,         ///< 按句子分块
    PARAGRAPH,        ///< 按段落分块
    SEMANTIC,         ///< 按语义分块
    HYBRID            ///< 混合策略
};

/**
 * @brief 文本分块器接口
 */
class TextSplitter {
public:
    /**
     * @brief 析构函数
     */
    virtual ~TextSplitter() = default;
    
    /**
     * @brief 分割文本
     * @param text 输入文本
     * @return 文本块列表
     */
    virtual std::vector<TextChunk> split(const std::string& text) const = 0;
    
    /**
     * @brief 分割文本，带上下文信息
     * @param text 输入文本
     * @param metadata 元数据信息
     * @return 文本块列表
     */
    virtual std::vector<TextChunk> split(
        const std::string& text,
        const std::unordered_map<std::string, std::string>& metadata) const = 0;
    
    /**
     * @brief 设置选项
     * @param key 选项键
     * @param value 选项值
     */
    void setOption(const std::string& key, const std::string& value) {
        options_[key] = value;
    }
    
protected:
    /**
     * @brief 获取选项值
     * @param key 选项键
     * @param defaultValue 默认值
     * @return 选项值
     */
    std::string getOption(const std::string& key, const std::string& defaultValue) const {
        auto it = options_.find(key);
        return (it != options_.end()) ? it->second : defaultValue;
    }
    
    std::unordered_map<std::string, std::string> options_; ///< 配置选项
};

/**
 * @brief 固定大小分块器
 */
class FixedSizeSplitter : public TextSplitter {
public:
    /**
     * @brief 构造函数
     * @param chunkSize 块大小（字符数）
     * @param overlapSize 重叠大小（字符数）
     * @param options 配置选项
     */
    FixedSizeSplitter(
        size_t chunkSize = 1000, 
        size_t overlapSize = 200,
        const std::unordered_map<std::string, std::string>& options = {});
    
    /**
     * @brief 分割文本
     * @param text 输入文本
     * @return 文本块列表
     */
    std::vector<TextChunk> split(const std::string& text) const override;
    
    /**
     * @brief 分割文本，带上下文信息
     * @param text 输入文本
     * @param metadata 元数据信息
     * @return 文本块列表
     */
    std::vector<TextChunk> split(
        const std::string& text,
        const std::unordered_map<std::string, std::string>& metadata) const override;
    
private:
    size_t chunkSize_;    ///< 块大小
    size_t overlapSize_;  ///< 重叠大小
};

/**
 * @brief 按句子分块器
 */
class SentenceSplitter : public TextSplitter {
public:
    /**
     * @brief 构造函数
     * @param maxChunkSize 最大块大小（字符数）
     * @param minChunkSize 最小块大小（字符数）
     * @param options 配置选项
     */
    SentenceSplitter(
        size_t maxChunkSize = 1000, 
        size_t minChunkSize = 200,
        const std::unordered_map<std::string, std::string>& options = {});
    
    /**
     * @brief 分割文本
     * @param text 输入文本
     * @return 文本块列表
     */
    std::vector<TextChunk> split(const std::string& text) const override;
    
    /**
     * @brief 分割文本，带上下文信息
     * @param text 输入文本
     * @param metadata 元数据信息
     * @return 文本块列表
     */
    std::vector<TextChunk> split(
        const std::string& text,
        const std::unordered_map<std::string, std::string>& metadata) const override;
    
private:
    size_t maxChunkSize_;  ///< 最大块大小
    size_t minChunkSize_;  ///< 最小块大小
    
    /**
     * @brief 分割句子
     * @param text 输入文本
     * @return 句子列表
     */
    std::vector<std::string> splitSentences(const std::string& text) const;
};

/**
 * @brief 按段落分块器
 */
class ParagraphSplitter : public TextSplitter {
public:
    /**
     * @brief 构造函数
     * @param maxChunkSize 最大块大小（字符数）
     * @param minChunkSize 最小块大小（字符数）
     * @param options 配置选项
     */
    ParagraphSplitter(
        size_t maxChunkSize = 2000, 
        size_t minChunkSize = 500,
        const std::unordered_map<std::string, std::string>& options = {});
    
    /**
     * @brief 分割文本
     * @param text 输入文本
     * @return 文本块列表
     */
    std::vector<TextChunk> split(const std::string& text) const override;
    
    /**
     * @brief 分割文本，带上下文信息
     * @param text 输入文本
     * @param metadata 元数据信息
     * @return 文本块列表
     */
    std::vector<TextChunk> split(
        const std::string& text,
        const std::unordered_map<std::string, std::string>& metadata) const override;
    
private:
    size_t maxChunkSize_;  ///< 最大块大小
    size_t minChunkSize_;  ///< 最小块大小
    
    /**
     * @brief 分割段落
     * @param text 输入文本
     * @return 段落列表
     */
    std::vector<std::string> splitParagraphs(const std::string& text) const;
};

/**
 * @brief 语义分块器
 * 尝试根据语义单元来分块，保持语义完整性
 */
class SemanticSplitter : public TextSplitter {
public:
    /**
     * @brief 构造函数
     * @param maxChunkSize 最大块大小（字符数）
     * @param minChunkSize 最小块大小（字符数）
     * @param modelPath 模型路径，用于语义切分
     */
    SemanticSplitter(
        size_t maxChunkSize = 2000, 
        size_t minChunkSize = 500,
        const std::string& modelPath = "");
    
    /**
     * @brief 分割文本
     * @param text 输入文本
     * @return 文本块列表
     */
    std::vector<TextChunk> split(const std::string& text) const override;
    
    /**
     * @brief 分割文本，带上下文信息
     * @param text 输入文本
     * @param metadata 元数据信息
     * @return 文本块列表
     */
    std::vector<TextChunk> split(
        const std::string& text,
        const std::unordered_map<std::string, std::string>& metadata) const override;
    
private:
    size_t maxChunkSize_;  ///< 最大块大小
    size_t minChunkSize_;  ///< 最小块大小
    std::string modelPath_; ///< 模型路径
    
    /**
     * @brief 计算两个文本段之间的语义相似度
     * @param text1 文本1
     * @param text2 文本2
     * @return 相似度分数(0-1)
     */
    float computeSimilarity(const std::string& text1, const std::string& text2) const;
};

/**
 * @brief 混合分块器
 * 结合多种分块策略的优点
 */
class HybridSplitter : public TextSplitter {
public:
    /**
     * @brief 构造函数
     * @param splitters 分块器列表
     */
    explicit HybridSplitter(std::vector<std::shared_ptr<TextSplitter>> splitters);
    
    /**
     * @brief 分割文本
     * @param text 输入文本
     * @return 文本块列表
     */
    std::vector<TextChunk> split(const std::string& text) const override;
    
    /**
     * @brief 分割文本，带上下文信息
     * @param text 输入文本
     * @param metadata 元数据信息
     * @return 文本块列表
     */
    std::vector<TextChunk> split(
        const std::string& text,
        const std::unordered_map<std::string, std::string>& metadata) const override;
    
private:
    std::vector<std::shared_ptr<TextSplitter>> splitters_;  ///< 分块器列表
};

/**
 * @brief 文本分块器工厂
 */
class TextSplitterFactory {
public:
    /**
     * @brief 创建文本分块器
     * @param strategy 分块策略
     * @param options 选项参数
     * @return 文本分块器指针
     */
    static std::unique_ptr<TextSplitter> createSplitter(
        SplitStrategy strategy,
        const std::unordered_map<std::string, std::string>& options = {});
};

} // namespace preprocessor
} // namespace kb

#endif // KB_TEXT_SPLITTER_H 