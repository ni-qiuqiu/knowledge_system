#include <gtest/gtest.h>
#include "preprocessor/preprocessing_pipeline.h"
#include <string>
#include <memory>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace kb::preprocessor;

// 预处理流水线测试
class PreprocessingPipelineTest : public ::testing::Test {
protected:
    std::string tempDir;
    std::string textFilePath;
    std::unique_ptr<PreprocessingPipeline> pipeline;
    
    void SetUp() override {
        // 创建临时目录
        tempDir = fs::temp_directory_path().string() + "/kb_test_" + std::to_string(time(nullptr));
        fs::create_directories(tempDir);
        
        // 创建一个简单的文本文件
        textFilePath = tempDir + "/test.txt";
        std::ofstream textFile(textFilePath);
        textFile << "这是一个测试文本文件。\n";
        textFile << "用于测试预处理流水线功能。\n";
        textFile << "包含多个段落，用于测试分块功能。\n\n";
        textFile << "这是第二个段落。\n";
        textFile << "继续提供更多测试内容。\n\n";
        textFile << "这是第三个段落，较长，测试文本分块和关键词提取。";
        textFile << "预处理流水线包括清洗、分块、元数据提取、分词和关键词提取等步骤。";
        textFile.close();
        
        // 创建预处理流水线
        pipeline = std::make_unique<PreprocessingPipeline>();
    }
    
    void TearDown() override {
        // 清理临时文件和目录
        fs::remove_all(tempDir);
    }
};

// 测试文本处理基本功能
TEST_F(PreprocessingPipelineTest, BasicTextProcessing) {
    // 创建预处理选项
    PreprocessingOptions options;
    options.clean = true;
    options.split = true;
    options.splitStrategy = SplitStrategy::PARAGRAPH;
    options.extractMetadata = true;
    
    // 处理文本
    std::string text = "这是测试文本内容。\n\n这是另一个段落。";
    PreprocessingResult result = pipeline->processText(text, options);
    
    // 验证基本处理结果
    EXPECT_FALSE(result.cleanedText.empty());
    EXPECT_FALSE(result.chunks.empty());
    EXPECT_FALSE(result.metadata.empty());
}

// 测试文件处理功能
TEST_F(PreprocessingPipelineTest, FileProcessing) {
    // 创建预处理选项
    PreprocessingOptions options;
    options.clean = true;
    options.split = true;
    options.splitStrategy = SplitStrategy::PARAGRAPH;
    options.extractMetadata = true;
    
    // 处理文件
    PreprocessingResult result = pipeline->processFile(textFilePath, options);
    
    // 验证文件处理结果
    EXPECT_FALSE(result.originalText.empty());
    EXPECT_FALSE(result.cleanedText.empty());
    EXPECT_FALSE(result.chunks.empty());
    EXPECT_FALSE(result.metadata.empty());
    
    // 验证元数据包含文件信息
    EXPECT_TRUE(result.metadata.find("file_name") != result.metadata.end());
    EXPECT_TRUE(result.metadata.find("file_extension") != result.metadata.end());
}

// 测试完整流水线功能
TEST_F(PreprocessingPipelineTest, FullPipeline) {
    // 创建包含所有步骤的预处理选项
    PreprocessingOptions options;
    options.clean = true;
    options.split = true;
    options.splitStrategy = SplitStrategy::PARAGRAPH;
    options.extractMetadata = true;
    options.tokenize = true;
    options.tokenizeMode = TokenizeMode::DEFAULT;
    options.extractKeywords = true;
    options.keywordCount = 5;
    
    // 处理文件
    PreprocessingResult result = pipeline->processFile(textFilePath, options);
    
    // 验证所有步骤的处理结果
    EXPECT_FALSE(result.originalText.empty());
    EXPECT_FALSE(result.cleanedText.empty());
    EXPECT_FALSE(result.chunks.empty());
    EXPECT_FALSE(result.metadata.empty());
    
    // 验证分词结果
    EXPECT_FALSE(result.tokens.empty());
    
    // 验证关键词提取结果
    EXPECT_LE(result.keywords.size(), options.keywordCount);
    
    // 验证关键词信息被添加到元数据
    EXPECT_TRUE(result.metadata.find("keywords") != result.metadata.end());
}

// 测试批量处理功能
TEST_F(PreprocessingPipelineTest, BatchProcessing) {
    // 创建多个测试文件
    std::vector<std::string> filePaths;
    for (int i = 0; i < 3; ++i) {
        std::string filePath = tempDir + "/batch_test_" + std::to_string(i) + ".txt";
        std::ofstream file(filePath);
        file << "批量处理测试文件 " << i << "\n";
        file << "包含简单内容用于测试。\n";
        file.close();
        filePaths.push_back(filePath);
    }
    
    // 创建预处理选项
    PreprocessingOptions options;
    options.clean = true;
    options.split = true;
    
    // 记录进度回调
    int progressCallCount = 0;
    auto progressCallback = [&progressCallCount](size_t current, size_t total, const std::string& filePath) {
        progressCallCount++;
    };
    
    // 执行批量处理
    std::vector<PreprocessingResult> results = 
        pipeline->batchProcessFiles(filePaths, options, progressCallback);
    
    // 验证结果数量
    EXPECT_EQ(results.size(), filePaths.size());
    
    // 验证进度回调次数
    EXPECT_EQ(progressCallCount, filePaths.size());
    
    // 验证每个结果
    for (const auto& result : results) {
        EXPECT_FALSE(result.originalText.empty());
        EXPECT_FALSE(result.cleanedText.empty());
    }
}

// 测试流水线步骤定制
TEST_F(PreprocessingPipelineTest, CustomPipeline) {
    // 创建自定义流水线
    PreprocessingPipeline customPipeline;
    
    // 只添加清洗和分块步骤
    customPipeline.clearSteps()
                  .addStep(std::make_shared<CleaningStep>())
                  .addStep(std::make_shared<SplittingStep>());
    
    // 创建预处理选项
    PreprocessingOptions options;
    options.clean = true;
    options.split = true;
    options.tokenize = true;  // 这个应该不会生效，因为没有分词步骤
    
    // 处理文本
    std::string text = "这是测试文本内容。";
    PreprocessingResult result = customPipeline.processText(text, options);
    
    // 验证结果：应该有清洗和分块结果，但没有分词结果
    EXPECT_FALSE(result.cleanedText.empty());
    EXPECT_FALSE(result.chunks.empty());
    EXPECT_TRUE(result.tokens.empty());  // 没有分词步骤，所以应该为空
    
    // 创建选项对象
    std::unordered_map<std::string, std::string> splitterOptions = {
        {"chunk_size", "100"},
        {"overlap_size", "20"}
    };
    options.splitterOptions = splitterOptions;
    
    // 再次处理文本
    PreprocessingResult result2 = customPipeline.processText(text, options);
    
    // 验证结果
    EXPECT_FALSE(result2.chunks.empty());
}

// 测试跳过处理步骤
TEST_F(PreprocessingPipelineTest, SkipSteps) {
    // 创建预处理选项，禁用所有步骤
    PreprocessingOptions options;
    options.clean = false;
    options.split = false;
    options.extractMetadata = false;
    options.tokenize = false;
    options.extractKeywords = false;
    
    // 处理文本
    std::string text = "这是测试文本内容。";
    PreprocessingResult result = pipeline->processText(text, options);
    
    // 验证原始文本被保存
    EXPECT_EQ(result.originalText, text);
    
    // 验证清洗步骤被跳过，cleanedText应该等于原文
    EXPECT_EQ(result.cleanedText, text);
    
    // 验证其他步骤被跳过
    EXPECT_TRUE(result.chunks.empty());
    EXPECT_TRUE(result.tokens.empty());
    EXPECT_TRUE(result.keywords.empty());
    // 元数据通常包含一些基本信息，即使步骤被跳过
    // EXPECT_TRUE(result.metadata.empty());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 