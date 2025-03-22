#include <gtest/gtest.h>
#include "storage/mysql_storage.h"
#include "knowledge/knowledge_graph.h"
#include "knowledge/entity.h"
#include "knowledge/relation.h"
#include "knowledge/triple.h"
#include <string>
#include <memory>

using namespace kb::storage;
using namespace kb::knowledge;

class MySQLStorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建存储实例
        storage = std::make_shared<MySQLStorage>();
    }

    void TearDown() override {
        // 确保关闭连接
        if (storage && storage->isConnected()) {
            storage->close();
        }
    }

    // 辅助方法：创建测试知识图谱
    KnowledgeGraphPtr createTestGraph(const std::string& name) {
        auto graph = std::make_shared<KnowledgeGraph>(name);
        
        // 添加实体
        auto entity1 = Entity::create("e1", "李四", EntityType::PERSON);
        auto entity2 = Entity::create("e2", "北京大学", EntityType::ORGANIZATION);
        
        // 添加属性
        entity1->addProperty("age", "30");
        entity2->addProperty("location", "北京");
        
        // 添加关系
        auto relation = Relation::create("r1", "就读于", RelationType::BELONGS_TO);
        
        // 添加到图谱
        graph->addEntity(entity1);
        graph->addEntity(entity2);
        graph->addRelation(relation);
        
        // 创建三元组
        auto triple = Triple::create(entity1, relation, entity2);
        graph->addTriple(triple);
        
        return graph;
    }

    std::shared_ptr<MySQLStorage> storage;
};

// 测试基本初始化
TEST_F(MySQLStorageTest, BasicInitialization) {
    // 验证初始状态
    EXPECT_FALSE(storage->isConnected());
    EXPECT_EQ(storage->getStorageType(), "MySQL");
}

// 测试连接字符串解析
TEST_F(MySQLStorageTest, ConnectionStringParsing) {
    // 创建一个有效的连接字符串
    std::string connStr = "host=localhost;port=3306;user=testuser;password=testpass;database=testdb";
    
    // 尝试解析连接字符串
    bool parseResult = storage->parseConnectionString(connStr);
    EXPECT_TRUE(parseResult) << "解析有效的连接字符串应成功";
    
    // 验证解析结果
    EXPECT_EQ(storage->getHost(), "localhost");
    EXPECT_EQ(storage->getPort(), 3306);
    EXPECT_EQ(storage->getUsername(), "testuser");
    EXPECT_EQ(storage->getPassword(), "testpass");
    EXPECT_EQ(storage->getDatabaseName(), "testdb");
    
    // 测试无效连接字符串
    std::string invalidConnStr = "invalidformat";
    parseResult = storage->parseConnectionString(invalidConnStr);
    EXPECT_FALSE(parseResult) << "解析无效的连接字符串应失败";
}

// 测试初始化和关闭连接
// 注意：此测试不会实际连接到数据库，仅验证接口行为
TEST_F(MySQLStorageTest, InitializeAndClose) {
    // 构造一个有效的连接字符串，但不使用真实的数据库凭据
    std::string connStr = "host=localhost;port=3306;user=testuser;password=testpass;database=testdb";
    
    // 尝试初始化（此处会失败，因为没有实际的MySQL服务器）
    // 我们不断言结果，因为这取决于测试环境
    storage->initialize(connStr);
    
    // 无论成功与否，close应该不会崩溃
    EXPECT_NO_THROW(storage->close()) << "关闭连接不应抛出异常";
}

// 测试知识图谱相关操作
// 注意：此测试不会实际保存到数据库，仅验证接口行为
TEST_F(MySQLStorageTest, KnowledgeGraphOperations) {
    // 创建一个测试知识图谱
    auto graph = createTestGraph("TestGraph");
    
    // 验证图谱创建成功
    EXPECT_EQ(graph->getName(), "TestGraph");
    EXPECT_EQ(graph->getEntityCount(), 2);
    EXPECT_EQ(graph->getRelationCount(), 1);
    EXPECT_EQ(graph->getTripleCount(), 1);
    
    // 尝试保存图谱（因为未连接到实际数据库，预期会失败）
    bool saveResult = storage->saveKnowledgeGraph(graph);
    
    // 我们不断言saveResult，因为它取决于数据库连接状态
    
    // 尝试加载图谱（因为未连接到实际数据库，预期会失败）
    auto loadedGraph = storage->loadKnowledgeGraph("TestGraph");
    
    // 我们期望加载失败，返回nullptr
    EXPECT_EQ(loadedGraph, nullptr) << "在未连接数据库的情况下，加载应该失败";
}

// 测试错误处理
TEST_F(MySQLStorageTest, ErrorHandling) {
    // 在未初始化的情况下执行操作应该返回错误
    EXPECT_FALSE(storage->createSchema()) << "未初始化时创建模式应失败";
    
    // 验证错误消息不为空
    EXPECT_FALSE(storage->getLastError().empty()) << "应设置错误消息";
}

// 主函数
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 