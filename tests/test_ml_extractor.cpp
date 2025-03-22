#include <gtest/gtest.h>
#include "extractor/ml_extractor.h"
#include "common/logging.h"
#include <filesystem>
#include <fstream>

namespace kb {
namespace test {

// 创建一个 MockMLExtractor 类，实现所有纯虚函数
class MockMLExtractor : public extractor::MLExtractor {
public:
    // 构造函数需要传递参数给父类构造函数
    MockMLExtractor(const std::string& modelPath,
                  const std::string& dictPath,
                  const std::string& hmmPath,
                  const std::string& userDictPath,
                  const std::string& idfPath,
                  const std::string& stopWordsPath)
        : extractor::MLExtractor(modelPath, dictPath, hmmPath, userDictPath, idfPath, stopWordsPath) {
    }

    // 实现纯虚函数 recognizeEntities
    virtual std::vector<extractor::EntityRecognitionResult> recognizeEntities(
        const std::string& text, 
        const std::vector<std::string>& tokens) override {
        
        // 标记参数为已使用，避免编译警告
        (void)tokens;
        
        std::vector<extractor::EntityRecognitionResult> results;
        
        // 模拟实体识别结果
        if (text.find("清华大学") != std::string::npos) {
            size_t pos = text.find("清华大学");
            // 使用知识库中的实体类型
            knowledge::EntityType orgType(knowledge::EntityType::ORGANIZATION);
            results.emplace_back(std::string("清华大学"), orgType, pos, pos + 9, 0.9f);
        }
        
        if (text.find("计算机科学") != std::string::npos) {
            size_t pos = text.find("计算机科学");
            // 使用知识库中的实体类型
            knowledge::EntityType conceptType(knowledge::EntityType::CONCEPT);
            results.emplace_back(std::string("计算机科学"), conceptType, pos, pos + 12, 0.8f);
        }
        
        if (text.find("我") != std::string::npos) {
            size_t pos = text.find("我");
            // 使用知识库中的实体类型
            knowledge::EntityType personType(knowledge::EntityType::PERSON);
            results.emplace_back(std::string("我"), personType, pos, pos + 3, 0.95f);
        }
        
        return results;
    }
    
    // 实现纯虚函数 extractRelationsFromEntities
    virtual std::vector<extractor::RelationExtractionResult> extractRelationsFromEntities(
        const std::string& text,
        const std::vector<std::string>& tokens,
        const std::vector<extractor::EntityRecognitionResult>& entities) override {
        
        // 标记参数为已使用，避免编译警告
        (void)tokens;
        
        std::vector<extractor::RelationExtractionResult> results;
        
        // 如果实体少于2个，无法建立关系
        if (entities.size() < 2) {
            return results;
        }
        
        // 查找"学习"关系
        if (text.find("学习") != std::string::npos) {
            size_t pos = text.find("学习");
            
            // 找到主体和客体
            size_t subjectIndex = 0;
            size_t objectIndex = 0;
            
            // 简单逻辑：找到PERSON类型的实体作为主体，CONCEPT类型的实体作为客体
            for (size_t i = 0; i < entities.size(); ++i) {
                if (entities[i].entityType == knowledge::EntityType(knowledge::EntityType::PERSON)) {
                    subjectIndex = i;
                } else if (entities[i].entityType == knowledge::EntityType(knowledge::EntityType::CONCEPT)) {
                    objectIndex = i;
                }
            }
            
            // 添加关系
            knowledge::RelationType relatedToType(knowledge::RelationType::RELATED_TO);
            results.emplace_back(std::string("学习"), relatedToType, pos, pos + 6, 0.85f, subjectIndex, objectIndex);
        }
        
        return results;
    }
};

class MLExtractorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 设置测试数据目录
        //testDataDir_ = std::filesystem::temp_directory_path() / "kb_test";
        //std::filesystem::create_directories(testDataDir_);
        
        std::string testDataDir = "/home/qiu/桌面/LLama_Chat/thirdparty/cppjieba/dict";
        // 创建测试文件
        //createTestFiles();

        std::string modelPath = "";
        std::string dictPath = testDataDir + "/jieba.dict.utf8";
        std::string hmmPath = testDataDir + "/hmm_model.utf8";
        std::string userDictPath = testDataDir + "/user.dict.utf8";
        std::string stopWordsPath = testDataDir + "/stop_words.utf8";
        std::string idfPath = testDataDir + "/idf.utf8";
        // LOG_INFO("modelPath: {}", modelPath);
        // LOG_INFO("dictPath: {}", dictPath);
        // LOG_INFO("hmmPath: {}", hmmPath);
        // LOG_INFO("userDictPath: {}", userDictPath);
        // LOG_INFO("stopWordsPath: {}", stopWordsPath);
        // LOG_INFO("idfPath: {}", idfPath);
        
        // 初始化提取器，使用 MockMLExtractor 而不是 MLExtractor
        extractor_ = std::make_unique<MockMLExtractor>(
            modelPath,
            dictPath,
            hmmPath,
            userDictPath,
            idfPath,
            stopWordsPath
        );
    }
    
    void TearDown() override {
        // 清理测试文件
        std::filesystem::remove_all(testDataDir_);
    }
    
    void createTestFiles() {
        // 创建词典文件
        std::ofstream dictFile(testDataDir_ / "dict.txt");
        dictFile << "我 1\n"
                 << "是 1\n"
                 << "一个 1\n"
                 << "学生 1\n"
                 << "在 1\n"
                 << "清华大学 1\n"
                 << "学习 1\n"
                 << "计算机 1\n"
                 << "科学 1\n";
        dictFile.close();
        
        // 创建HMM模型文件
        std::ofstream hmmFile(testDataDir_ / "hmm.txt");
        hmmFile << "B 0.5\n"
                << "M 0.3\n"
                << "E 0.2\n";
        hmmFile.close();
        
        // 创建用户词典文件
        std::ofstream userDictFile(testDataDir_ / "user_dict.txt");
        userDictFile << "清华大学 1\n"
                    << "计算机科学 1\n";
        userDictFile.close();
        
        // 创建停用词文件
        std::ofstream stopWordsFile(testDataDir_ / "stop_words.txt");
        stopWordsFile << "的\n"
                     << "了\n"
                     << "和\n"
                     << "是\n"
                     << "在\n";
        stopWordsFile.close();
    }
    
    std::filesystem::path testDataDir_;
    std::unique_ptr<extractor::MLExtractor> extractor_;
};

// 测试实体提取
TEST_F(MLExtractorTest, EntityExtraction) {
    std::string text = "我在清华大学学习计算机科学";
    
    auto entities = extractor_->extractEntities(text);
    
    ASSERT_FALSE(entities.empty());
    
    // 验证提取的实体
    bool foundUniversity = false;
    bool foundSubject = false;
    
    for (const auto& entity : entities) {
        if (entity->getName() == "清华大学") {
            foundUniversity = true;
        } else if (entity->getName() == "计算机科学") {
            foundSubject = true;
        }
    }
    
    EXPECT_TRUE(foundUniversity);
    EXPECT_TRUE(foundSubject);
}

// 测试关系提取
TEST_F(MLExtractorTest, RelationExtraction) {
    std::string text = "我在清华大学学习计算机科学";
    
    // 先提取实体
    auto entities = extractor_->extractEntities(text);
    ASSERT_FALSE(entities.empty());
    
    // 提取关系
    auto relations = extractor_->extractRelations(text, entities);
    
    ASSERT_FALSE(relations.empty());
    
    // 验证提取的关系
    bool foundStudyRelation = false;
    for (const auto& relation : relations) {
        if (relation->getName() == "学习") {
            foundStudyRelation = true;
            break;
        }
    }
    
    EXPECT_TRUE(foundStudyRelation);
}

// 测试三元组提取
TEST_F(MLExtractorTest, TripleExtraction) {
    std::string text = "我在清华大学学习计算机科学";
    
    auto triples = extractor_->extractTriples(text);
    
    ASSERT_FALSE(triples.empty());
    
    // 验证提取的三元组
    bool foundValidTriple = false;
    for (const auto& triple : triples) {
        if (triple->getSubject()->getName() == "我" &&
            triple->getRelation()->getName() == "学习" &&
            triple->getObject()->getName() == "计算机科学") {
            foundValidTriple = true;
            break;
        }
    }
    
    EXPECT_TRUE(foundValidTriple);
}

// 测试知识图谱构建
TEST_F(MLExtractorTest, KnowledgeGraphBuilding) {
    std::string text = "我在清华大学学习计算机科学";
    
    auto graph = extractor_->buildKnowledgeGraph(text);
    
    ASSERT_NE(graph, nullptr);
    
    // 验证图谱中的实体和关系
    EXPECT_GT(graph->getEntityCount(), 0);
    EXPECT_GT(graph->getRelationCount(), 0);
    EXPECT_GT(graph->getTripleCount(), 0);
}

// 测试空文本处理
TEST_F(MLExtractorTest, EmptyTextHandling) {
    std::string text = "";
    
    auto entities = extractor_->extractEntities(text);
    auto relations = extractor_->extractRelations(text, entities);
    auto triples = extractor_->extractTriples(text);
    
    EXPECT_TRUE(entities.empty());
    EXPECT_TRUE(relations.empty());
    EXPECT_TRUE(triples.empty());
}

} // namespace test
} // namespace kb 