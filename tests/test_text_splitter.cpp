#include <gtest/gtest.h>
#include "preprocessor/text_splitter.h"
#include <string>
#include <vector>

using namespace kb::preprocessor;

// 测试FixedSizeSplitter
TEST(TextSplitterTest, FixedSizeSplitter) {
    // 创建固定大小分块器，块大小500，重叠大小100
    FixedSizeSplitter splitter(500, 100);
    
    // 生成1200个字符的测试文本
    std::string text(1200, 'A');
    for (int i = 0; i < 12; ++i) {
        // 每百个字符添加一个标记点，便于验证分块位置
        text[i * 100] = '0' + (i % 10);
    }
    
    // 执行分块
    std::vector<TextChunk> chunks = splitter.split(text);
    
    // 验证分块数量
    EXPECT_EQ(chunks.size(), 3);
    
    // 验证第一个块的大小
    EXPECT_EQ(chunks[0].text.size(), 500);
    
    // 验证第二个块与第一个块重叠部分
    EXPECT_EQ(chunks[1].text.substr(0, 100), chunks[0].text.substr(400, 100));
    
    // 验证第三个块与第二个块重叠部分
    EXPECT_EQ(chunks[2].text.substr(0, 100), chunks[1].text.substr(400, 100));
    
    // 验证块索引
    EXPECT_EQ(chunks[0].index, 0);
    EXPECT_EQ(chunks[1].index, 1);
    EXPECT_EQ(chunks[2].index, 2);
}

// 测试带元数据的分块
TEST(TextSplitterTest, SplitWithMetadata) {
    FixedSizeSplitter splitter(500, 100);
    
    std::string text = "测试文本，用于测试带元数据的分块功能。";
    std::unordered_map<std::string, std::string> metadata = {
        {"title", "测试文档"},
        {"author", "测试作者"},
        {"source", "测试源"}
    };
    
    // 执行带元数据的分块
    std::vector<TextChunk> chunks = splitter.split(text, metadata);
    
    // 验证元数据是否被复制到每个块
    EXPECT_FALSE(chunks.empty());
    for (const auto& chunk : chunks) {
        EXPECT_EQ(chunk.getMetadata("title"), "测试文档");
        EXPECT_EQ(chunk.getMetadata("author"), "测试作者");
        EXPECT_EQ(chunk.getMetadata("source"), "测试源");
    }
}

// 测试ParagraphSplitter
TEST(TextSplitterTest, ParagraphSplitter) {
    // 创建段落分块器
    ParagraphSplitter splitter(1000, 200);
    
    // 多段落文本
    std::string text = 
        "第一段落内容，测试段落分块功能。\n\n"
        "第二段落内容，测试段落分块功能。\n\n"
        "第三段落内容，测试段落分块功能。\n\n"
        "第四段落内容，这是一个较长的段落，包含更多的内容和信息，用于测试段落分块器的行为。"
        "特别是当段落非常长的时候，分块器应当能够根据最大块大小进行合理分块。\n\n"
        "第五段落，继续测试。";
    
    // 执行分块
    std::vector<TextChunk> chunks = splitter.split(text);
    
    // 由于段落较短，应该被合并到较少的块中
    EXPECT_GT(chunks.size(), 0);
    
    // 验证块之间的边界是否在段落边界
    for (const auto& chunk : chunks) {
        // 段落块的开始不应该是换行符
        if (!chunk.text.empty()) {
            EXPECT_NE(chunk.text[0], '\n');
        }
        
        // 检查每个块是否都在合理的大小范围内
        EXPECT_LE(chunk.text.size(), 1000);
        
        // 如果不是最后一个块，应该以段落结束
        if (&chunk != &chunks.back()) {
            std::string end = chunk.text.substr(chunk.text.size() - 2);
            EXPECT_TRUE(end == "\n\n" || end.find('\n') != std::string::npos);
        }
    }
}

// 测试SentenceSplitter
TEST(TextSplitterTest, SentenceSplitter) {
    // 创建句子分块器
    SentenceSplitter splitter(500, 100);
    
    // 多句子文本
    std::string text = 
        "这是第一个句子。这是第二个句子！这是第三个句子？"
        "这是第四个句子。这是第五个句子。这是第六个句子。"
        "这是一个较长的句子，包含更多的内容，用于测试句子分块器的行为，"
        "特别是对于那些包含标点符号的长句子。";
    
    // 执行分块
    std::vector<TextChunk> chunks = splitter.split(text);
    
    // 验证分块
    EXPECT_GT(chunks.size(), 0);
    
    // 验证块大小不超过最大块大小
    for (const auto& chunk : chunks) {
        EXPECT_LE(chunk.text.size(), 500);
    }
    
    // 验证每个块的边界通常是句子边界
    for (const auto& chunk : chunks) {
        if (!chunk.text.empty() && &chunk != &chunks.back()) {
            char lastChar = chunk.text.back();
            EXPECT_TRUE(lastChar == '。' || lastChar == '！' || lastChar == '？');
        }
    }
}

// 测试文本分块器工厂
TEST(TextSplitterTest, TextSplitterFactory) {
    // 测试不同策略的分块器创建
    auto fixedSizeSplitter = TextSplitterFactory::createSplitter(SplitStrategy::FIXED_SIZE);
    EXPECT_NE(fixedSizeSplitter, nullptr);
    
    auto sentenceSplitter = TextSplitterFactory::createSplitter(SplitStrategy::SENTENCE);
    EXPECT_NE(sentenceSplitter, nullptr);
    
    auto paragraphSplitter = TextSplitterFactory::createSplitter(SplitStrategy::PARAGRAPH);
    EXPECT_NE(paragraphSplitter, nullptr);
    
    // 测试工厂创建的分块器可以正确设置选项
    auto splitter = TextSplitterFactory::createSplitter(
        SplitStrategy::FIXED_SIZE,
        {{"chunk_size", "300"}, {"overlap_size", "50"}}
    );
    EXPECT_NE(splitter, nullptr);
    
    // 测试分块结果
    std::string text(1000, 'A');
    std::vector<TextChunk> chunks = splitter->split(text);
    
    // 应该有4个块: 0-300, 250-550, 500-800, 750-1000
    EXPECT_EQ(chunks.size(), 4);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 