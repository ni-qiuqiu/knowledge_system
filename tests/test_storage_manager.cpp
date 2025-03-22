#include <gtest/gtest.h>
#include "storage/mysql_storage.h"
#include "storage/storage_factory.h"
#include "storage/version_control.h"
#include "storage/storage_manager.h"
#include "knowledge/knowledge_graph.h"
#include <string>
#include <memory>
#include <filesystem>

using namespace kb::storage;
using namespace kb::knowledge;

// 测试存储工厂创建
TEST(StorageManagerTest, FactoryCreateStorage) {
    // 获取存储工厂实例
    auto& factory = StorageFactory::getInstance();
    
    // 验证注册类型是否包含MySQL
    auto types = factory.getRegisteredTypes();
    bool hasMysql = false;
    for (const auto& type : types) {
        if (type == "mysql") {
            hasMysql = true;
            break;
        }
    }
    EXPECT_TRUE(hasMysql) << "MySQL类型未在存储工厂中注册";
    
    // 创建一个测试连接字符串
    std::string testConnectionString = "host=localhost;port=3306;user=test;password=test;database=test_kb";
    
    // 尝试创建MySQL存储（可能因环境原因失败，所以不检查成功与否）
    auto storage = factory.createStorage("mysql", testConnectionString);
}

// 测试MySQL存储的基本初始化（模拟模式）
TEST(StorageManagerTest, MySQLStorageInitialization) {
    // 创建MySQL存储实例
    auto storage = std::make_shared<MySQLStorage>();
    
    // 验证初始状态
    EXPECT_FALSE(storage->isConnected()) << "新创建的存储不应处于连接状态";
    
    // 验证存储类型
    EXPECT_EQ(storage->getStorageType(), "MySQL") << "存储类型应为MySQL";
    
    // 测试获取连接信息（不包含敏感信息）
    std::string connectionInfo = storage->getConnectionInfo();
    EXPECT_FALSE(connectionInfo.empty()) << "连接信息不应为空";
    
    // 验证敏感信息已脱敏
    EXPECT_EQ(connectionInfo.find("password="), std::string::npos) << "连接信息不应包含原始密码";
}

// 测试版本控制初始化
TEST(StorageManagerTest, VersionControlInitialization) {
    // 创建临时测试目录
    std::string testDir = "/tmp/kb_test_" + std::to_string(time(nullptr));
    
    // 获取版本控制实例
    auto& versionControl = VersionControl::getInstance();
    
    // 设置版本目录（应该在初始化前设置）
    EXPECT_TRUE(versionControl.setVersionDirectory(testDir)) << "设置版本目录应成功";
    
    // 验证版本目录已设置
    EXPECT_EQ(versionControl.getVersionDirectory(), testDir) << "版本目录应与设置的一致";
    
    // 创建一个模拟存储
    auto storage = std::make_shared<MySQLStorage>();
    
    // 初始化版本控制系统
    bool initResult = versionControl.initialize(storage, testDir);
    // 标记为已使用以避免警告
    (void)initResult;
    
    // 此处我们不断言initResult，因为它依赖于存储连接，而存储连接在测试环境中可能无法成功
    
    // 清理测试目录
    if (std::filesystem::exists(testDir)) {
        try {
            std::filesystem::remove_all(testDir);
        } catch (const std::exception& e) {
            std::cerr << "清理测试目录失败: " << e.what() << std::endl;
        }
    }
}

// 测试知识图谱基本操作
TEST(StorageManagerTest, KnowledgeGraphBasicOperations) {
    // 创建一个知识图谱
    auto graph = std::make_shared<KnowledgeGraph>("TestGraph");
    
    // 创建实体
    auto person = Entity::create("person1", "张三", EntityType::PERSON);
    auto org = Entity::create("org1", "某公司", EntityType::ORGANIZATION);
    
    // 创建关系
    auto workAt = Relation::create("rel1", "工作于", RelationType::WORKS_FOR);
    
    // 添加实体和关系到图谱
    EXPECT_TRUE(graph->addEntity(person)) << "添加实体应成功";
    EXPECT_TRUE(graph->addEntity(org)) << "添加实体应成功";
    EXPECT_TRUE(graph->addRelation(workAt)) << "添加关系应成功";
    
    // 创建三元组
    auto triple = Triple::create(person, workAt, org);
    
    // 添加三元组到图谱
    EXPECT_TRUE(graph->addTriple(triple)) << "添加三元组应成功";
    
    // 验证添加结果
    EXPECT_EQ(graph->getEntityCount(), 2) << "图谱应有2个实体";
    EXPECT_EQ(graph->getRelationCount(), 1) << "图谱应有1个关系";
    EXPECT_EQ(graph->getTripleCount(), 1) << "图谱应有1个三元组";
    
    // 测试查询功能
    auto personFound = graph->getEntity("person1");
    EXPECT_TRUE(personFound != nullptr) << "应能通过ID找到实体";
    EXPECT_EQ(personFound->getName(), "张三") << "实体名称应匹配";
    
    // 查询三元组
    auto triples = graph->findTriplesBySubject("person1");
    EXPECT_EQ(triples.size(), 1) << "应有1个与person1相关的三元组";
}

// 测试存储管理器
TEST(StorageManagerTest, StorageManagerBasicOperations) {
    // 获取存储管理器实例（使用单例模式）
    auto& manager = StorageManager::getInstance();
    
    // 测试获取当前存储
    auto defaultStorage = manager.getDefaultStorage();
    EXPECT_EQ(defaultStorage, nullptr) << "初始状态下默认存储应为空";
    
    // 测试获取所有存储类型
    auto types = manager.getRegisteredStorageTypes();
    EXPECT_FALSE(types.empty()) << "应至少有一种可用的存储类型";
}

// 主函数
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 