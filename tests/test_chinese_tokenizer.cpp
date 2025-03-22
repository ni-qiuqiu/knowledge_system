#include <gtest/gtest.h>
#include "preprocessor/chinese_tokenizer.h"
#include <string>
#include <vector>
#include <unordered_set>

using namespace kb::preprocessor;

// 基本分词测试
TEST(ChineseTokenizerTest, BasicCut) {
    // 创建分词器实例
    ChineseTokenizer tokenizer;
    
    // 简单的中文文本
    std::string text = "我爱北京天安门";
    
    // 默认模式分词
    std::vector<std::string> tokens = tokenizer.cut(text);
    
    // 验证结果不为空
    EXPECT_FALSE(tokens.empty());
    
    // 验证分词结果
    std::vector<std::string> expected = {"我", "爱", "北京", "天安门"};
    EXPECT_EQ(tokens, expected);
}

// 测试不同分词模式
TEST(ChineseTokenizerTest, DifferentModes) {
    ChineseTokenizer tokenizer;
    std::string text = "小明硕士毕业于中国科学院计算所，后在日本京都大学深造";
    
    // 默认模式（精确模式）
    std::vector<std::string> defaultTokens = tokenizer.cut(text, TokenizeMode::DEFAULT);
    
    // 搜索引擎模式
    std::vector<std::string> searchTokens = tokenizer.cut(text, TokenizeMode::SEARCH);
    
    // HMM模式
    std::vector<std::string> hmmTokens = tokenizer.cut(text, TokenizeMode::HMM);
    
    // 混合模式
    std::vector<std::string> mixTokens = tokenizer.cut(text, TokenizeMode::MIX);
    
    // 验证各模式结果不为空
    EXPECT_FALSE(defaultTokens.empty());
    EXPECT_FALSE(searchTokens.empty());
    EXPECT_FALSE(hmmTokens.empty());
    EXPECT_FALSE(mixTokens.empty());
    
    // 验证搜索引擎模式的分词结果通常比精确模式更细
    EXPECT_GE(searchTokens.size(), defaultTokens.size());
    
    // 验证可以通过便捷方法获取搜索引擎模式分词
    std::vector<std::string> searchTokens2 = tokenizer.cutForSearch(text);
    EXPECT_EQ(searchTokens, searchTokens2);
}

// 测试添加自定义词语
TEST(ChineseTokenizerTest, UserDictionary) {
    ChineseTokenizer tokenizer;
    
    // 添加自定义词语
    tokenizer.addUserWord("自定义词语");
    
    // 分词测试文本
    std::string text = "这是一个自定义词语测试";
    
    // 执行分词
    std::vector<std::string> tokens = tokenizer.cut(text);
    
    // 验证自定义词语被正确识别为一个整体
    EXPECT_NE(std::find(tokens.begin(), tokens.end(), "自定义词语"), tokens.end());
}

// 测试停用词过滤
TEST(ChineseTokenizerTest, StopWords) {
    ChineseTokenizer tokenizer;
    
    // 获取停用词表
    const auto& stopWords = tokenizer.getStopWords();
    
    // 构建包含常见停用词的文本
    std::string text = "的地得和与之这是在了";
    
    // 执行不过滤停用词的分词
    std::vector<std::string> tokensWithStopWords = tokenizer.cut(text, TokenizeMode::DEFAULT, false);
    
    // 执行过滤停用词的分词
    std::vector<std::string> tokensWithoutStopWords = tokenizer.cut(text, TokenizeMode::DEFAULT, true);
    
    // 验证过滤后的结果词数少于不过滤的结果
    EXPECT_LE(tokensWithoutStopWords.size(), tokensWithStopWords.size());
    
    // 验证常见停用词已被过滤
    for (const auto& token : tokensWithoutStopWords) {
        EXPECT_FALSE(tokenizer.isStopWord(token));
    }
}

// 测试关键词提取
TEST(ChineseTokenizerTest, KeywordExtraction) {
    ChineseTokenizer tokenizer;
    
    // 测试文本
    std::string text = "北京是中华人民共和国的首都，拥有悠久的历史和灿烂的文化。"
                     "作为中国的政治、文化、国际交往中心，北京汇聚了大量的人才和资源。"
                     "天安门、故宫、长城等名胜古迹闻名于世，每年吸引数以百万计的游客前来观光。";
    
    // 提取关键词
    std::vector<std::pair<std::string, double>> keywords = tokenizer.extractKeywords(text, 5);
    
    // 验证提取的关键词数量
    EXPECT_EQ(keywords.size(), 5);
    
    // 验证关键词中包含核心词汇
    std::unordered_set<std::string> keywordSet;
    for (const auto& kw : keywords) {
        keywordSet.insert(kw.first);
    }
    
    EXPECT_TRUE(keywordSet.find("北京") != keywordSet.end() || 
                keywordSet.find("中国") != keywordSet.end() || 
                keywordSet.find("天安门") != keywordSet.end() || 
                keywordSet.find("故宫") != keywordSet.end() || 
                keywordSet.find("长城") != keywordSet.end());
    
    // 验证关键词权重是降序排列的
    for (size_t i = 1; i < keywords.size(); ++i) {
        EXPECT_GE(keywords[i-1].second, keywords[i].second);
    }
}

// 测试空文本处理
TEST(ChineseTokenizerTest, EmptyText) {
    ChineseTokenizer tokenizer;
    
    // 空文本
    std::string text = "";
    
    // 分词
    std::vector<std::string> tokens = tokenizer.cut(text);
    
    // 验证结果为空
    EXPECT_TRUE(tokens.empty());
    
    // 关键词提取
    std::vector<std::pair<std::string, double>> keywords = tokenizer.extractKeywords(text);
    
    // 验证结果为空
    EXPECT_TRUE(keywords.empty());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 