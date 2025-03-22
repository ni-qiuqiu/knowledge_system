#include <gtest/gtest.h>
#include "preprocessor/metadata_extractor.h"
#include <string>
#include <fstream>
#include <filesystem>
#include <unordered_map>

namespace fs = std::filesystem;
using namespace kb::preprocessor;

// 创建临时测试文件
class MetadataExtractorTest : public ::testing::Test {
protected:
    std::string tempDir;
    std::string textFilePath;
    std::string htmlFilePath;
    
    void SetUp() override {
        // 创建临时目录
        tempDir = fs::temp_directory_path().string() + "/kb_test_" + std::to_string(time(nullptr));
        fs::create_directories(tempDir);
        
        // 创建一个简单的文本文件
        textFilePath = tempDir + "/test.txt";
        std::ofstream textFile(textFilePath);
        textFile << "这是一个测试文本文件。\n";
        textFile << "用于测试元数据提取功能。\n";
        textFile << "包含一些基本信息和内容。\n";
        textFile.close();
        
        // 创建一个简单的HTML文件
        htmlFilePath = tempDir + "/test.html";
        std::ofstream htmlFile(htmlFilePath);
        htmlFile << "<!DOCTYPE html>\n";
        htmlFile << "<html>\n";
        htmlFile << "<head>\n";
        htmlFile << "  <title>测试HTML文档</title>\n";
        htmlFile << "  <meta name=\"description\" content=\"这是一个测试HTML文件\">\n";
        htmlFile << "  <meta name=\"keywords\" content=\"测试,元数据,HTML\">\n";
        htmlFile << "  <meta name=\"author\" content=\"测试作者\">\n";
        htmlFile << "</head>\n";
        htmlFile << "<body>\n";
        htmlFile << "  <h1>测试HTML文档标题</h1>\n";
        htmlFile << "  <p>这是测试HTML文档的内容。</p>\n";
        htmlFile << "</body>\n";
        htmlFile << "</html>\n";
        htmlFile.close();
    }
    
    void TearDown() override {
        // 清理临时文件和目录
        fs::remove_all(tempDir);
    }
};

// 测试文档类型检测
TEST_F(MetadataExtractorTest, DocumentTypeDetection) {
    // 测试文本文件类型检测
    DocumentType textType = MetadataExtractor::detectDocumentType(textFilePath);
    EXPECT_EQ(textType, DocumentType::TEXT);
    
    // 测试HTML文件类型检测
    DocumentType htmlType = MetadataExtractor::detectDocumentType(htmlFilePath);
    EXPECT_EQ(htmlType, DocumentType::HTML);
    
    // 测试从内容检测文档类型
    std::string htmlContent = "<!DOCTYPE html><html><head><title>Test</title></head><body></body></html>";
    DocumentType contentType = MetadataExtractor::detectDocumentTypeFromContent(htmlContent);
    EXPECT_EQ(contentType, DocumentType::HTML);
}

// 测试基本元数据提取
TEST_F(MetadataExtractorTest, BasicMetadataExtraction) {
    BasicMetadataExtractor extractor;
    
    // 从文本文件提取元数据
    auto textMetadata = extractor.extract(textFilePath);
    
    // 验证基本文件元数据
    EXPECT_EQ(textMetadata["file_name"], "test.txt");
    EXPECT_EQ(textMetadata["file_extension"], ".txt");
    EXPECT_EQ(textMetadata["document_type_name"], "TEXT");
    
    // 验证内容相关元数据
    EXPECT_TRUE(textMetadata.find("character_count") != textMetadata.end());
    EXPECT_TRUE(textMetadata.find("line_count") != textMetadata.end());
    EXPECT_TRUE(textMetadata.find("word_count") != textMetadata.end());
    
    // 从字符串内容提取元数据
    std::string content = "这是一段用于测试的文本内容。";
    auto contentMetadata = extractor.extractFromContent(content, "test_content.txt");
    
    // 验证从内容提取的元数据
    EXPECT_EQ(contentMetadata["file_name"], "test_content.txt");
    EXPECT_EQ(contentMetadata["document_type_name"], "TEXT");
    EXPECT_EQ(contentMetadata["content_size"], std::to_string(content.size()));
}

// 测试HTML元数据提取
TEST_F(MetadataExtractorTest, HTMLMetadataExtraction) {
    HTMLMetadataExtractor extractor;
    
    // 从HTML文件提取元数据
    auto htmlMetadata = extractor.extract(htmlFilePath);
    
    // 验证基本文件元数据
    EXPECT_EQ(htmlMetadata["file_name"], "test.html");
    EXPECT_EQ(htmlMetadata["document_type_name"], "HTML");
    
    // 验证HTML特有元数据
    EXPECT_EQ(htmlMetadata["title"], "测试HTML文档");
    EXPECT_EQ(htmlMetadata["meta_description"], "这是一个测试HTML文件");
    EXPECT_EQ(htmlMetadata["meta_keywords"], "测试,元数据,HTML");
    EXPECT_EQ(htmlMetadata["meta_author"], "测试作者");
    EXPECT_EQ(htmlMetadata["h1"], "测试HTML文档标题");
}

// 测试元数据提取工厂
TEST_F(MetadataExtractorTest, MetadataExtractorFactory) {
    // 测试工厂创建合适的提取器
    auto textExtractor = MetadataExtractorFactory::createExtractor(DocumentType::TEXT);
    EXPECT_NE(textExtractor, nullptr);
    
    auto htmlExtractor = MetadataExtractorFactory::createExtractor(DocumentType::HTML);
    EXPECT_NE(htmlExtractor, nullptr);
    
    auto pdfExtractor = MetadataExtractorFactory::createExtractor(DocumentType::PDF);
    EXPECT_NE(pdfExtractor, nullptr);
    
    // 测试从文件路径创建提取器
    auto fileExtractor = MetadataExtractorFactory::createExtractor(textFilePath);
    EXPECT_NE(fileExtractor, nullptr);
    
    // 测试便捷方法提取元数据
    auto metadata = MetadataExtractorFactory::extractMetadata(textFilePath);
    EXPECT_FALSE(metadata.empty());
    EXPECT_EQ(metadata["document_type_name"], "TEXT");
}

// 测试空文件和错误处理
TEST_F(MetadataExtractorTest, ErrorHandling) {
    // 不存在的文件路径
    std::string nonExistentPath = tempDir + "/non_existent.txt";
    
    // 从不存在的路径检测文档类型
    DocumentType type = MetadataExtractor::detectDocumentType(nonExistentPath);
    EXPECT_EQ(type, DocumentType::UNKNOWN);
    
    // 从不存在的路径提取元数据
    BasicMetadataExtractor extractor;
    auto metadata = extractor.extract(nonExistentPath);
    
    // 结果应该是空的或包含错误信息
    EXPECT_TRUE(metadata.empty());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 