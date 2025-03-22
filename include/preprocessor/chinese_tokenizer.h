/**
 * @file chinese_tokenizer.h
 * @brief 中文分词器，定义中文分词功能
 */

#ifndef KB_CHINESE_TOKENIZER_H
#define KB_CHINESE_TOKENIZER_H

#include <string>
#include <vector>
#include <memory>
#include <unordered_set>

// 前向声明cppjieba类，避免引入所有cppjieba头文件
namespace cppjieba {
class Jieba;
class KeywordExtractor;
}

namespace kb {
namespace preprocessor {

/**
 * @brief 分词模式枚举
 */
enum class TokenizeMode {
    DEFAULT,   ///< 默认模式，精确模式
    SEARCH,    ///< 搜索引擎模式
    HMM,       ///< HMM模式
    MIX        ///< 混合模式
};

/**
 * @brief 中文分词器类，封装cppjieba
 */
class ChineseTokenizer {
public:
    /**
     * @brief 默认构造函数
     * 使用预设的默认路径初始化
     */
    ChineseTokenizer();
    
    /**
     * @brief 构造函数
     * @param dictPath jieba词典路径
     * @param hmmPath hmm模型路径
     * @param userDictPath 用户词典路径
     * @param idfPath idf文件路径
     * @param stopWordsPath 停用词文件路径
     */
    ChineseTokenizer(const std::string& dictPath,
                    const std::string& hmmPath,
                    const std::string& userDictPath,
                    const std::string& idfPath,
                    const std::string& stopWordsPath);
    
    /**
     * @brief 析构函数
     */
    ~ChineseTokenizer();
    
    /**
     * @brief 分词
     * @param text 待分词文本
     * @param mode 分词模式
     * @param removeStopWords 是否移除停用词
     * @return 分词结果
     */
    std::vector<std::string> cut(const std::string& text, 
                               TokenizeMode mode = TokenizeMode::DEFAULT,
                               bool removeStopWords = false) const;
    
    /**
     * @brief 搜索引擎模式分词
     * @param text 待分词文本
     * @param removeStopWords 是否移除停用词
     * @return 分词结果
     */
    std::vector<std::string> cutForSearch(const std::string& text, 
                                        bool removeStopWords = false) const;
    
    /**
     * @brief HMM模式分词
     * @param text 待分词文本
     * @param removeStopWords 是否移除停用词
     * @return 分词结果
     */
    std::vector<std::string> cutHMM(const std::string& text, 
                                  bool removeStopWords = false) const;
    
    /**
     * @brief 提取关键词
     * @param text 文本
     * @param topK 返回前K个关键词
     * @return 关键词及其权重
     */
    std::vector<std::pair<std::string, double>> extractKeywords(
        const std::string& text, size_t topK = 10) const;
    
    /**
     * @brief 分词（别名，功能与cut相同）
     * @param text 待分词文本
     * @param mode 分词模式
     * @param removeStopWords 是否移除停用词
     * @return 分词结果
     */
    std::vector<std::string> tokenize(const std::string& text, 
                                    TokenizeMode mode = TokenizeMode::DEFAULT,
                                    bool removeStopWords = false) const {
        return cut(text, mode, removeStopWords);
    }
    
    /**
     * @brief 添加用户自定义词语
     * @param word 词语
     * @param tag 词性标注，默认为空
     */
    void addUserWord(const std::string& word, const std::string& tag = "");
    
    /**
     * @brief 是否为停用词
     * @param word 词语
     * @return 是否为停用词
     */
    bool isStopWord(const std::string& word) const;
    
    /**
     * @brief 获取停用词表
     * @return 停用词集合
     */
    const std::unordered_set<std::string>& getStopWords() const;
    
private:
    std::unique_ptr<cppjieba::Jieba> jieba_;                 ///< cppjieba分词器
    std::unordered_set<std::string> stopWords_;              ///< 停用词集合
    
    /**
     * @brief 加载停用词
     * @param filePath 停用词文件路径
     */
    void loadStopWords(const std::string& filePath);
    
    /**
     * @brief 从分词结果中移除停用词
     * @param tokens 分词结果
     */
    void removeStopWordsFromTokens(std::vector<std::string>& tokens) const;
};

} // namespace preprocessor
} // namespace kb

#endif // KB_CHINESE_TOKENIZER_H 