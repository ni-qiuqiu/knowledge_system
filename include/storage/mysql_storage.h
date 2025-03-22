#ifndef KB_MYSQL_STORAGE_H
#define KB_MYSQL_STORAGE_H

#include "storage/storage_interface.h"
#include <string>
#include <memory>
#include <mutex>

// 前向声明MySQLConnection
struct MYSQL;
struct MYSQL_STMT;

namespace kb {
namespace storage {

/**
 * @brief MySQL存储适配器，实现MySQL数据库的存储接口
 */
class MySQLStorage : public StorageInterface {
public:
    /**
     * @brief 构造函数
     */
    MySQLStorage();
    
    /**
     * @brief 析构函数
     */
    ~MySQLStorage() override;
    
    /**
     * @brief 初始化存储连接
     * @param connectionString 连接字符串，格式: "host=<host>;port=<port>;user=<user>;password=<password>;database=<database>"
     * @return 是否成功
     */
    bool initialize(const std::string& connectionString) override;
    
    /**
     * @brief 关闭存储连接
     * @return 是否成功
     */
    bool close() override;
    
    /**
     * @brief 检查连接是否有效
     * @return 是否有效
     */
    bool isConnected() const override;
    
    /**
     * @brief 创建或更新数据库架构
     * @param force 是否强制重建（删除现有数据）
     * @return 是否成功
     */
    bool createSchema(bool force = false) override;
    
    /**
     * @brief 保存实体
     * @param entity 实体对象
     * @return 是否成功
     */
    bool saveEntity(const knowledge::EntityPtr& entity) override;
    
    /**
     * @brief 批量保存实体
     * @param entities 实体列表
     * @return 成功保存的实体数
     */
    size_t saveEntities(const std::vector<knowledge::EntityPtr>& entities) override;
    
    /**
     * @brief 加载实体
     * @param id 实体ID
     * @return 实体对象，失败返回nullptr
     */
    knowledge::EntityPtr loadEntity(const std::string& id) override;
    
    /**
     * @brief 查询实体
     * @param name 实体名称（可选）
     * @param type 实体类型（可选）
     * @param properties 实体属性（可选）
     * @param limit 返回数量限制（0表示不限制）
     * @return 实体列表
     */
    std::vector<knowledge::EntityPtr> queryEntities(
        const std::string& name = "",
        knowledge::EntityType type = knowledge::EntityType::UNKNOWN,
        const std::unordered_map<std::string, std::string>& properties = {},
        size_t limit = 0) override;
    
    /**
     * @brief 删除实体
     * @param id 实体ID
     * @return 是否成功
     */
    bool deleteEntity(const std::string& id) override;
    
    /**
     * @brief 保存关系
     * @param relation 关系对象
     * @return 是否成功
     */
    bool saveRelation(const knowledge::RelationPtr& relation) override;
    
    /**
     * @brief 批量保存关系
     * @param relations 关系列表
     * @return 成功保存的关系数
     */
    size_t saveRelations(const std::vector<knowledge::RelationPtr>& relations) override;
    
    /**
     * @brief 加载关系
     * @param id 关系ID
     * @return 关系对象，失败返回nullptr
     */
    knowledge::RelationPtr loadRelation(const std::string& id) override;
    
    /**
     * @brief 查询关系
     * @param name 关系名称（可选）
     * @param type 关系类型（可选）
     * @param properties 关系属性（可选）
     * @param limit 返回数量限制（0表示不限制）
     * @return 关系列表
     */
    std::vector<knowledge::RelationPtr> queryRelations(
        const std::string& name = "",
        knowledge::RelationType type = knowledge::RelationType::UNKNOWN,
        const std::unordered_map<std::string, std::string>& properties = {},
        size_t limit = 0) override;
    
    /**
     * @brief 删除关系
     * @param id 关系ID
     * @return 是否成功
     */
    bool deleteRelation(const std::string& id) override;
    
    /**
     * @brief 保存三元组
     * @param triple 三元组对象
     * @return 是否成功
     */
    bool saveTriple(const knowledge::TriplePtr& triple) override;
    
    /**
     * @brief 批量保存三元组
     * @param triples 三元组列表
     * @return 成功保存的三元组数
     */
    size_t saveTriples(const std::vector<knowledge::TriplePtr>& triples) override;
    
    /**
     * @brief 加载三元组
     * @param id 三元组ID
     * @return 三元组对象，失败返回nullptr
     */
    knowledge::TriplePtr loadTriple(const std::string& id) override;
    
    /**
     * @brief 查询三元组
     * @param subjectId 主体ID（可选）
     * @param relationId 关系ID（可选）
     * @param objectId 客体ID（可选）
     * @param properties 三元组属性（可选）
     * @param minConfidence 最小置信度（可选）
     * @param limit 返回数量限制（0表示不限制）
     * @return 三元组列表
     */
    std::vector<knowledge::TriplePtr> queryTriples(
        const std::string& subjectId = "",
        const std::string& relationId = "",
        const std::string& objectId = "",
        const std::unordered_map<std::string, std::string>& properties = {},
        float minConfidence = 0.0f,
        size_t limit = 0) override;
    
    /**
     * @brief 删除三元组
     * @param id 三元组ID
     * @return 是否成功
     */
    bool deleteTriple(const std::string& id) override;
    
    /**
     * @brief 保存知识图谱
     * @param graph 知识图谱对象
     * @param saveName 保存名称（可选）
     * @return 是否成功
     */
    bool saveKnowledgeGraph(
        const knowledge::KnowledgeGraphPtr& graph,
        const std::string& saveName = "") override;
    
    /**
     * @brief 加载知识图谱
     * @param graphName 知识图谱名称
     * @return 知识图谱对象，失败返回nullptr
     */
    knowledge::KnowledgeGraphPtr loadKnowledgeGraph(
        const std::string& graphName) override;
    
    /**
     * @brief 删除知识图谱
     * @param graphName 知识图谱名称
     * @return 是否成功
     */
    bool deleteKnowledgeGraph(const std::string& graphName) override;
    
    /**
     * @brief 列出所有知识图谱
     * @return 知识图谱名称列表
     */
    std::vector<std::string> listKnowledgeGraphs() override;
    
    /**
     * @brief 执行自定义查询
     * @param query 查询语句
     * @param params 查询参数
     * @return 查询结果（JSON格式）
     */
    std::string executeQuery(
        const std::string& query,
        const std::unordered_map<std::string, std::string>& params = {}) override;
    
    /**
     * @brief 开始事务
     * @return 是否成功
     */
    bool beginTransaction() override;
    
    /**
     * @brief 提交事务
     * @return 是否成功
     */
    bool commitTransaction() override;
    
    /**
     * @brief 回滚事务
     * @return 是否成功
     */
    bool rollbackTransaction() override;
    
    /**
     * @brief 获取存储类型名称
     * @return 存储类型名称
     */
    std::string getStorageType() const override {
        return "MySQL";
    }
    
    /**
     * @brief 获取存储连接信息
     * @return 连接信息（脱敏处理）
     */
    std::string getConnectionInfo() const override;
    
    /**
     * @brief 获取最后一次错误消息
     * @return 错误消息
     */
    std::string getLastError() const override {
        return lastError_;
    }

    /**
     * @brief 获取主机名
     * @return 主机名
     */
    std::string getHost() const { return host_; }

    /**
     * @brief 获取端口号
     * @return 端口号
     */
    int getPort() const { return port_; }

    /**
     * @brief 获取用户名
     * @return 用户名
     */
    std::string getUsername() const { return user_; }

    /**
     * @brief 获取密码
     * @return 密码
     */
    std::string getPassword() const { return password_; }

    /**
     * @brief 获取数据库名
     * @return 数据库名
     */
    std::string getDatabaseName() const { return database_; }

    /**
     * @brief 获取字符集
     * @return 字符集
     */
    std::string getCharset() const { return "utf8mb4"; }  // MySQL默认字符集
    
    /**
     * @brief 解析连接字符串
     * @param connectionString 连接字符串
     * @return 是否解析成功
     */
    bool parseConnectionString(const std::string& connectionString);

private:
    MYSQL* mysql_;                       ///< MySQL连接句柄
    std::mutex mutex_;                   ///< 互斥锁，保证线程安全
    std::string host_;                   ///< 数据库主机
    std::string user_;                   ///< 数据库用户名
    std::string password_;               ///< 数据库密码
    std::string database_;               ///< 数据库名称
    unsigned int port_;                  ///< 数据库端口
    mutable std::string lastError_;      ///< 最后一次错误消息
    bool isInTransaction_;               ///< 是否在事务中
    
    // 预处理语句
    struct PreparedStatements {
        MYSQL_STMT* saveEntity;          ///< 保存实体语句
        MYSQL_STMT* loadEntity;          ///< 加载实体语句
        MYSQL_STMT* loadEntityProperties; ///< 加载实体属性语句
        MYSQL_STMT* deleteEntity;        ///< 删除实体语句
        MYSQL_STMT* saveRelation;        ///< 保存关系语句
        MYSQL_STMT* loadRelation;        ///< 加载关系语句
        MYSQL_STMT* loadRelationProperties; ///< 加载关系属性语句
        MYSQL_STMT* deleteRelation;      ///< 删除关系语句
        MYSQL_STMT* saveTriple;          ///< 保存三元组语句
        MYSQL_STMT* loadTriple;          ///< 加载三元组语句
        MYSQL_STMT* loadTripleProperties; ///< 加载三元组属性语句
        MYSQL_STMT* deleteTriple;        ///< 删除三元组语句
        // 可能还有更多预处理语句...
    };
    
    std::unique_ptr<PreparedStatements> statements_; ///< 预处理语句集合
    
    /**
     * @brief 创建表结构
     * @return 是否成功
     */
    bool createTables();
    
    /**
     * @brief 删除表结构
     * @return 是否成功
     */
    bool dropTables();
    
    /**
     * @brief 准备预处理语句
     * @return 是否成功
     */
    bool prepareSQLStatements();
    
    /**
     * @brief 关闭并释放预处理语句
     */
    void cleanupSQLStatements();
    
    /**
     * @brief 执行SQL查询
     * @param query SQL查询语句
     * @return 是否成功
     */
    bool executeSQL(const std::string& query);
    
    /**
     * @brief 保存实体属性
     * @param entityId 实体ID
     * @param properties 属性映射
     * @return 是否成功
     */
    bool saveEntityProperties(
        const std::string& entityId,
        const std::unordered_map<std::string, std::string>& properties);
    
    /**
     * @brief 加载实体属性
     * @param entityId 实体ID
     * @return 属性映射
     */
    std::unordered_map<std::string, std::string> loadEntityProperties(
        const std::string& entityId);
    
    /**
     * @brief 保存关系属性
     * @param relationId 关系ID
     * @param properties 属性映射
     * @return 是否成功
     */
    bool saveRelationProperties(
        const std::string& relationId,
        const std::unordered_map<std::string, std::string>& properties);
    
    /**
     * @brief 加载关系属性
     * @param relationId 关系ID
     * @return 属性映射
     */
    std::unordered_map<std::string, std::string> loadRelationProperties(
        const std::string& relationId);
    
    /**
     * @brief 保存三元组属性
     * @param tripleId 三元组ID
     * @param properties 属性映射
     * @return 是否成功
     */
    bool saveTripleProperties(
        const std::string& tripleId,
        const std::unordered_map<std::string, std::string>& properties);
    
    /**
     * @brief 加载三元组属性
     * @param tripleId 三元组ID
     * @return 属性映射
     */
    std::unordered_map<std::string, std::string> loadTripleProperties(
        const std::string& tripleId);
    
    /**
     * @brief 构建属性查询条件
     * @param properties 属性映射
     * @param tablePrefix 表前缀
     * @return SQL查询条件
     */
    std::string buildPropertyConditions(
        const std::unordered_map<std::string, std::string>& properties,
        const std::string& tablePrefix);
    
    /**
     * @brief 设置错误消息
     * @param error 错误消息
     */
    void setLastError(const std::string& error) const {
        lastError_ = error;
    }
    
    /**
     * @brief 记录错误消息并设置最后错误
     * @param errorMsg 错误消息
     */
    void logError(const std::string& errorMsg) const;
    
    /**
     * @brief 辅助函数：连接字符串数组
     * @param elements 字符串数组
     * @param delimiter 分隔符
     * @return 连接后的字符串
     */
    static std::string join(const std::vector<std::string>& elements, const std::string& delimiter);
};

} // namespace storage
} // namespace kb

#endif // KB_MYSQL_STORAGE_H 