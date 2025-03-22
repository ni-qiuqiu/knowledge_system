#include "preprocessor/chinese_tokenizer.h"
#include "common/logging.h"
#include "common/utils.h"

#include <fstream>
#include <cppjieba/Jieba.hpp>
#include <cppjieba/KeywordExtractor.hpp>
#include <filesystem>


namespace kb {
namespace preprocessor {

ChineseTokenizer::ChineseTokenizer() 
    : ChineseTokenizer(
        "/home/qiu/桌面/LLama_Chat/thirdparty/cppjieba/dict/jieba.dict.utf8",
        "/home/qiu/桌面/LLama_Chat/thirdparty/cppjieba/dict/hmm_model.utf8",
        "/home/qiu/桌面/LLama_Chat/thirdparty/cppjieba/dict/user.dict.utf8",
        "/home/qiu/桌面/LLama_Chat/thirdparty/cppjieba/dict/idf.utf8",
        "/home/qiu/桌面/LLama_Chat/thirdparty/cppjieba/dict/stop_words.utf8") {



}

ChineseTokenizer::ChineseTokenizer(const std::string& dictPath,
                                 const std::string& hmmPath,
                                 const std::string& userDictPath,
                                 const std::string& idfPath,
                                 const std::string& stopWordsPath) {
    LOG_INFO("初始化中文分词器...");
    
    try {
        //先判断文件是否存在
        if (!std::filesystem::exists(dictPath)
        || !std::filesystem::exists(hmmPath)
        || !std::filesystem::exists(userDictPath)
        || !std::filesystem::exists(idfPath)
        || !std::filesystem::exists(stopWordsPath)) {
            LOG_ERROR("词典文件不存在: {}", dictPath);
            throw std::runtime_error("词典文件不存在");
        }
        
        


        // 初始化jieba分词器
        jieba_ = std::make_unique<cppjieba::Jieba>(
            dictPath,
            hmmPath,
            userDictPath,
            idfPath,
            stopWordsPath
        );
        
        // 加载停用词
        loadStopWords(stopWordsPath);
        
        LOG_INFO("中文分词器初始化完成");
    } catch (const std::exception& e) {
        LOG_ERROR("初始化中文分词器失败: {}", e.what());
        throw;
    }
}

ChineseTokenizer::~ChineseTokenizer() {
    // 智能指针会自动处理释放
}

std::vector<std::string> ChineseTokenizer::cut(const std::string& text, 
                                              TokenizeMode mode,
                                              bool removeStopWords) const {
    std::vector<std::string> words;
    
    try {
        switch (mode) {
            case TokenizeMode::DEFAULT:
                jieba_->Cut(text, words, true);
                break;
            case TokenizeMode::SEARCH:
                jieba_->CutForSearch(text, words);
                break;
            case TokenizeMode::HMM:
                jieba_->CutHMM(text, words);
                break;
            case TokenizeMode::MIX:
                jieba_->Cut(text, words, false);
                break;
            default:
                jieba_->Cut(text, words, true);
                break;
        }
        
        if (removeStopWords) {
            removeStopWordsFromTokens(words);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("分词失败: {}", e.what());
    }
    
    return words;
}

std::vector<std::string> ChineseTokenizer::cutForSearch(const std::string& text,
                                                     bool removeStopWords) const {
    return cut(text, TokenizeMode::SEARCH, removeStopWords);
}

std::vector<std::string> ChineseTokenizer::cutHMM(const std::string& text,
                                               bool removeStopWords) const {
    return cut(text, TokenizeMode::HMM, removeStopWords);
}

std::vector<std::pair<std::string, double>> ChineseTokenizer::extractKeywords(
    const std::string& text, size_t topK) const {
    
    std::vector<std::pair<std::string, double>> keywords;
    
    try {
        jieba_->extractor.Extract(text, keywords, topK);
    } catch (const std::exception& e) {
        LOG_ERROR("提取关键词失败: {}", e.what());
    }
    
    return keywords;
}

void ChineseTokenizer::addUserWord(const std::string& word, const std::string& tag) {
    if (word.empty()) {
        return;
    }
    
    try {
        jieba_->InsertUserWord(word, tag);
        LOG_DEBUG("添加用户词: {} {}", word, tag.empty() ? "" : "[" + tag + "]");
    } catch (const std::exception& e) {
        LOG_ERROR("添加用户词失败: {}", e.what());
    }
}

bool ChineseTokenizer::isStopWord(const std::string& word) const {
    return stopWords_.find(word) != stopWords_.end();
}

const std::unordered_set<std::string>& ChineseTokenizer::getStopWords() const {
    return stopWords_;
}

void ChineseTokenizer::loadStopWords(const std::string& filePath) {
    try {
        std::ifstream ifs(filePath);
        if (!ifs) {
            LOG_WARN("无法打开停用词文件: {}", filePath);
            return;
        }
        
        std::string line;
        while (std::getline(ifs, line)) {
            line = kb::utils::trim(line);
            if (!line.empty()) {
                stopWords_.insert(line);
            }
        }
        
        LOG_INFO("加载停用词 {} 个", stopWords_.size());
    } catch (const std::exception& e) {
        LOG_ERROR("加载停用词失败: {}", e.what());
    }
}

void ChineseTokenizer::removeStopWordsFromTokens(std::vector<std::string>& tokens) const {
    tokens.erase(
        std::remove_if(tokens.begin(), tokens.end(), 
                      [this](const std::string& token) {
                          return this->isStopWord(token);
                      }),
        tokens.end()
    );
}

} // namespace preprocessor
} // namespace kb