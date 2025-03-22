#include <gtest/gtest.h>
#include "preprocessor/text_cleaner.h"
#include <string>

using namespace kb::preprocessor;

// 测试空文本清洗
TEST(TextCleanerTest, EmptyText) {
    BasicCleaner cleaner;
    std::string text = "";
    std::string cleaned = cleaner.clean(text);
    EXPECT_EQ(cleaned, "");
}

// 测试空白符处理
TEST(TextCleanerTest, WhitespaceHandling) {
    BasicCleaner cleaner;
    std::string text = "  多余的空格  \t 需要被  清理  \n  行首空格\t\n行尾空格  \t\n\n\n多余的换行";
    std::string cleaned = cleaner.clean(text);
    
    // 检查连续空格是否被合并
    EXPECT_EQ(cleaned.find("  "), std::string::npos);
    
    // 检查制表符是否被替换
    EXPECT_EQ(cleaned.find("\t"), std::string::npos);
    
    // 检查行首空格是否被删除
    EXPECT_EQ(cleaned.find("\n "), std::string::npos);
    
    // 检查行尾空格是否被删除
    EXPECT_EQ(cleaned.find(" \n"), std::string::npos);
    
    // 检查多余换行是否被合并
    EXPECT_EQ(cleaned.find("\n\n\n"), std::string::npos);
    
    // 不要直接比较整个字符串，因为不同实现的清洗规则可能会有所不同
    // 而是检查关键的条件
}

// 测试Windows和Unix换行符标准化
TEST(TextCleanerTest, NewlineNormalization) {
    BasicCleaner cleaner;
    std::string text = "Windows换行符\r\n需要被标准化\r\n为Unix换行符";
    std::string cleaned = cleaner.clean(text);
    
    // 检查Windows换行符是否被替换
    EXPECT_EQ(cleaned.find("\r\n"), std::string::npos);
    
    // 验证内容分段
    std::string segment1 = "Windows换行符";
    std::string segment2 = "需要被标准化";
    std::string segment3 = "为Unix换行符";
    
    // 检查原始文本中的内容是否存在
    EXPECT_NE(cleaned.find(segment1), std::string::npos);
    EXPECT_NE(cleaned.find(segment2), std::string::npos);
    EXPECT_NE(cleaned.find(segment3), std::string::npos);
    
    // 检查内容不是连在一起的：这些字符串之间应该有分隔（空格或换行符）
    size_t pos1 = cleaned.find(segment1);
    size_t pos2 = cleaned.find(segment2);
    size_t pos3 = cleaned.find(segment3);
    
    // 验证段落之间有分隔
    EXPECT_GT(pos2, pos1 + segment1.length());
    EXPECT_GT(pos3, pos2 + segment2.length());
}

// 测试不可打印字符过滤
TEST(TextCleanerTest, NonPrintableCharacters) {
    BasicCleaner cleaner;
    // 包含一些不可打印字符的文本
    std::string text = "包含\x01不可\x02打印\x07字符\x1F的文本";
    std::string cleaned = cleaner.clean(text);
    
    // 检查不可打印字符是否被过滤
    EXPECT_EQ(cleaned, "包含不可打印字符的文本");
}

// 测试自定义正则替换规则
TEST(TextCleanerTest, CustomRegexRule) {
    TextCleaner cleaner;
    // 添加一个将数字替换为'#'的规则
    cleaner.addRegexRule("\\d", "#");
    
    std::string text = "包含数字123和456的文本";
    std::string cleaned = cleaner.clean(text);
    
    // 检查数字是否被替换为'#'
    EXPECT_EQ(cleaned, "包含数字###和###的文本");
}

// 测试函数规则
TEST(TextCleanerTest, FunctionRule) {
    TextCleaner cleaner;
    // 添加一个将所有文本转为大写的函数规则(仅对英文有效)
    cleaner.addFunctionRule([](const std::string& text) {
        std::string result = text;
        std::transform(result.begin(), result.end(), result.begin(), 
                      [](unsigned char c) { return std::toupper(c); });
        return result;
    });
    
    std::string text = "Mixed Case Text 混合大小写文本";
    std::string cleaned = cleaner.clean(text);
    
    // 检查英文部分是否被转为大写
    EXPECT_EQ(cleaned, "MIXED CASE TEXT 混合大小写文本");
}

// 测试规则链
TEST(TextCleanerTest, RuleChain) {
    TextCleaner cleaner;
    // 添加多个规则，测试规则链处理
    cleaner.addRegexRule("\\s+", " ");  // 合并空白
    cleaner.addRegexRule("\\d", "");    // 删除数字
    cleaner.addFunctionRule([](const std::string& text) {
        // 将首字母大写(仅对英文有效)
        if (text.empty()) return text;
        std::string result = text;
        result[0] = std::toupper(result[0]);
        return result;
    });
    
    std::string text = "this is  a \t test 123 文本";
    std::string cleaned = cleaner.clean(text);
    
    // 检查规则链是否按预期工作
    EXPECT_EQ(cleaned, "This is a test  文本");
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 