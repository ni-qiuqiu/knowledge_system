#include "storage/mysql_storage.h"
#include "common/logging.h"
#include "common/config.h"
#include <mysql/mysql.h>
#include <regex>
#include <sstream>
#include <cstring>

namespace kb {
namespace storage {

// 实现日志错误方法
void MySQLStorage::logError(const std::string& errorMsg) const {
    setLastError(errorMsg);
    LOG_ERROR("{}", errorMsg);
}

// 独立的字符串连接辅助函数
std::string MySQLStorage::join(const std::vector<std::string>& elements, const std::string& delimiter) {
    std::ostringstream os;
    for (size_t i = 0; i < elements.size(); ++i) {
        if (i > 0) {
            os << delimiter;
        }
        os << elements[i];
    }
    return os.str();
}

// 默认构造函数
MySQLStorage::MySQLStorage() : mysql_(nullptr), isInTransaction_(false) {
    // 使用配置单例获取数据库配置
    auto& config = kb::common::Config::getInstance();
    const auto& dbConfig = config.getDatabaseConfig();
    
    // 初始化默认参数
    host_ = dbConfig.host;
    port_ = dbConfig.port;
    user_ = dbConfig.user;
    password_ = dbConfig.password;
    database_ = dbConfig.database;
    
    // 日志输出连接信息（密码已脱敏）
    LOG_DEBUG("使用配置文件初始化MySQLStorage: host={}, port={}, user={}, database={}", 
              host_, port_, user_, database_);
}

// 析构函数
MySQLStorage::~MySQLStorage() {
    close();
}

// 初始化连接
bool MySQLStorage::initialize(const std::string& connectionString) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 如果提供了连接字符串，则解析它
    if (!connectionString.empty()) {
        if (!parseConnectionString(connectionString)) {
            LOG_ERROR("初始化失败：无法解析连接字符串: {}", connectionString);
            return false;
        }
    }
    
    // 如果已经连接，先断开
    if (mysql_) {
        mysql_close(mysql_);
        mysql_ = nullptr;
    }
    
    // 创建MySQL连接
    mysql_ = mysql_init(nullptr);
    if (!mysql_) {
        
        logError("MySQL初始化失败");
        return false;
    }
    
    // 设置连接选项
    auto& config = kb::common::Config::getInstance();
    const auto& dbConfig = config.getDatabaseConfig();
    
    // 设置超时选项
    unsigned int connectTimeout = dbConfig.connectTimeout;
    unsigned int readTimeout = dbConfig.readTimeout;
    unsigned int writeTimeout = dbConfig.writeTimeout;
    
    if (connectTimeout > 0) {
        mysql_options(mysql_, MYSQL_OPT_CONNECT_TIMEOUT, &connectTimeout);
        LOG_DEBUG("设置MySQL连接超时: {}秒", connectTimeout);
    }
    
    if (readTimeout > 0) {
        mysql_options(mysql_, MYSQL_OPT_READ_TIMEOUT, &readTimeout);
        LOG_DEBUG("设置MySQL读取超时: {}秒", readTimeout);
    }
    
    if (writeTimeout > 0) {
        mysql_options(mysql_, MYSQL_OPT_WRITE_TIMEOUT, &writeTimeout);
        LOG_DEBUG("设置MySQL写入超时: {}秒", writeTimeout);
    }
    
    // 设置自动重连选项
    bool reconnect = dbConfig.autoReconnect;
    mysql_options(mysql_, MYSQL_OPT_RECONNECT, &reconnect);
    LOG_DEBUG("设置MySQL自动重连: {}", reconnect ? "是" : "否");
    
    // 设置字符集
    const std::string charset = dbConfig.charset.empty() ? "utf8mb4" : dbConfig.charset;
    mysql_options(mysql_, MYSQL_SET_CHARSET_NAME, charset.c_str());
    LOG_DEBUG("设置MySQL字符集: {}", charset);
    
    // 连接到MySQL服务器
    LOG_INFO("正在连接MySQL数据库: {}@{}:{}/{}", user_, host_, port_, database_);
    if (!mysql_real_connect(mysql_, host_.c_str(), user_.c_str(), password_.c_str(), 
                          database_.c_str(), port_, nullptr, 0)) {
        std::string error = mysql_error(mysql_);
        logError("MySQL连接失败: " + error);
        mysql_close(mysql_);
        mysql_ = nullptr;
        return false;
    }
    
    LOG_INFO("MySQL数据库连接成功");
    
    // 验证连接是否有效
    if (mysql_ping(mysql_) != 0) {
        std::string error = mysql_error(mysql_);
        logError("MySQL ping失败: " + error);
        mysql_close(mysql_);
        mysql_ = nullptr;
        return false;
    }
    
    // 初始化SQL语句
    statements_ = std::make_unique<PreparedStatements>();
    if (!prepareSQLStatements()) {
       
        logError("准备SQL语句失败");
        mysql_close(mysql_);
        mysql_ = nullptr;
        return false;
    }
    
    return true;
}

// 关闭存储连接
bool MySQLStorage::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 如果在事务中，回滚事务
    if (isInTransaction_) {
        rollbackTransaction();
    }
    
    // 清理预处理语句
    cleanupSQLStatements();
    
    // 释放 MySQL 连接
    if (mysql_) {
        mysql_close(mysql_);
        mysql_ = nullptr;
        LOG_INFO("MySQL 存储连接已关闭");
    }
    
    return true;
}

// 检查连接是否有效
bool MySQLStorage::isConnected() const {
    // 对于const成员函数，不使用互斥锁，因为我们只是检查mysql_指针
    if (!mysql_) {
        return false;
    }
    
    // 这里省略互斥锁，仅进行简单的连接检查
    return mysql_ping(mysql_) == 0;
}

// 解析连接字符串
bool MySQLStorage::parseConnectionString(const std::string& connectionString) {
    if (connectionString.empty()) {
        LOG_WARN("空连接字符串，将使用默认配置");
        
        // 使用Config获取默认配置
        auto& config = kb::common::Config::getInstance();
        const auto& dbConfig = config.getDatabaseConfig();
        
        // 直接使用配置
        host_ = dbConfig.host;
        port_ = dbConfig.port;
        user_ = dbConfig.user;
        password_ = dbConfig.password;
        database_ = dbConfig.database;
        
        // 确保至少有数据库名称
        if (database_.empty()) {
            LOG_ERROR("数据库名称未配置");
            return false;
        }
        
        LOG_DEBUG("使用配置文件中的数据库配置");
        return true;
    }
    
  
    
    // 确保至少有数据库名称
    if (database_.empty()) {
        LOG_ERROR("连接字符串中缺少数据库名称");
        return false;
    }
    
    LOG_DEBUG("成功解析连接字符串: host={}, port={}, user={}, database={}", 
              host_, port_, user_, database_);
    return true;
}

// 创建表结构
bool MySQLStorage::createTables() {
    // 实体表
    if (!executeSQL(
        "CREATE TABLE IF NOT EXISTS `entities` ("
        "  `id` VARCHAR(100) NOT NULL,"
        "  `name` TEXT NOT NULL,"
        "  `type` INT NOT NULL,"
        "  `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "  `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
        "  PRIMARY KEY (`id`),"
        "  INDEX `idx_entity_name` ((SUBSTRING(name, 1, 100))),"
        "  INDEX `idx_entity_type` (`type`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;")) {
        return false;
    }
    
    // 实体属性表
    if (!executeSQL(
        "CREATE TABLE IF NOT EXISTS `entity_properties` ("
        "  `entity_id` VARCHAR(100) NOT NULL,"
        "  `key` VARCHAR(100) NOT NULL,"
        "  `value` TEXT NOT NULL,"
        "  `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "  `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
        "  PRIMARY KEY (`entity_id`, `key`),"
        "  FOREIGN KEY (`entity_id`) REFERENCES `entities` (`id`) ON DELETE CASCADE,"
        "  INDEX `idx_entity_property_key` (`key`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;")) {
        return false;
    }
    
    // 关系表
    if (!executeSQL(
        "CREATE TABLE IF NOT EXISTS `relations` ("
        "  `id` VARCHAR(100) NOT NULL,"
        "  `name` TEXT NOT NULL,"
        "  `type` INT NOT NULL,"
        "  `inverse_name` TEXT NOT NULL,"
        "  `is_symmetric` BOOLEAN NOT NULL DEFAULT FALSE,"
        "  `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "  `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
        "  PRIMARY KEY (`id`),"
        "  INDEX `idx_relation_name` ((SUBSTRING(name, 1, 100))),"
        "  INDEX `idx_relation_type` (`type`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;")) {
        return false;
    }
    
    // 关系属性表
    if (!executeSQL(
        "CREATE TABLE IF NOT EXISTS `relation_properties` ("
        "  `relation_id` VARCHAR(100) NOT NULL,"
        "  `key` VARCHAR(100) NOT NULL,"
        "  `value` TEXT NOT NULL,"
        "  `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "  `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
        "  PRIMARY KEY (`relation_id`, `key`),"
        "  FOREIGN KEY (`relation_id`) REFERENCES `relations` (`id`) ON DELETE CASCADE,"
        "  INDEX `idx_relation_property_key` (`key`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;")) {
        return false;
    }
    
    // 三元组表
    if (!executeSQL(
        "CREATE TABLE IF NOT EXISTS `triples` ("
        "  `id` VARCHAR(100) NOT NULL,"
        "  `subject_id` VARCHAR(100) NOT NULL,"
        "  `relation_id` VARCHAR(100) NOT NULL,"
        "  `object_id` VARCHAR(100) NOT NULL,"
        "  `confidence` FLOAT NOT NULL DEFAULT 1.0,"
        "  `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "  `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
        "  PRIMARY KEY (`id`),"
        "  UNIQUE KEY `idx_unique_triple` (`subject_id`, `relation_id`, `object_id`),"
        "  FOREIGN KEY (`subject_id`) REFERENCES `entities` (`id`) ON DELETE CASCADE,"
        "  FOREIGN KEY (`relation_id`) REFERENCES `relations` (`id`) ON DELETE CASCADE,"
        "  FOREIGN KEY (`object_id`) REFERENCES `entities` (`id`) ON DELETE CASCADE,"
        "  INDEX `idx_triple_subject` (`subject_id`),"
        "  INDEX `idx_triple_relation` (`relation_id`),"
        "  INDEX `idx_triple_object` (`object_id`),"
        "  INDEX `idx_triple_confidence` (`confidence`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;")) {
        return false;
    }
    
    // 三元组属性表
    if (!executeSQL(
        "CREATE TABLE IF NOT EXISTS `triple_properties` ("
        "  `triple_id` VARCHAR(100) NOT NULL,"
        "  `key` VARCHAR(100) NOT NULL,"
        "  `value` TEXT NOT NULL,"
        "  `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "  `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
        "  PRIMARY KEY (`triple_id`, `key`),"
        "  FOREIGN KEY (`triple_id`) REFERENCES `triples` (`id`) ON DELETE CASCADE,"
        "  INDEX `idx_triple_property_key` (`key`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;")) {
        return false;
    }
    
    // 知识图谱表
    if (!executeSQL(
        "CREATE TABLE IF NOT EXISTS `knowledge_graphs` ("
        "  `name` VARCHAR(100) NOT NULL,"
        "  `description` TEXT,"
        "  `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "  `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
        "  PRIMARY KEY (`name`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;")) {
        return false;
    }
    
    // 知识图谱三元组关联表
    if (!executeSQL(
        "CREATE TABLE IF NOT EXISTS `knowledge_graph_triples` ("
        "  `graph_name` VARCHAR(100) NOT NULL,"
        "  `triple_id` VARCHAR(100) NOT NULL,"
        "  `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "  PRIMARY KEY (`graph_name`, `triple_id`),"
        "  FOREIGN KEY (`graph_name`) REFERENCES `knowledge_graphs` (`name`) ON DELETE CASCADE,"
        "  FOREIGN KEY (`triple_id`) REFERENCES `triples` (`id`) ON DELETE CASCADE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;")) {
        return false;
    }
    
    return true;
}

// 删除表结构
bool MySQLStorage::dropTables() {
    // 删除表（注意顺序以避免外键约束问题）
    const std::string tables[] = {
        "knowledge_graph_triples",
        "knowledge_graphs",
        "triple_properties",
        "triples",
        "relation_properties",
        "relations",
        "entity_properties",
        "entities"
    };
    
    for (const auto& table : tables) {
        if (!executeSQL("DROP TABLE IF EXISTS `" + table + "`;")) {
            return false;
        }
    }
    
    return true;
}

// 执行SQL查询
bool MySQLStorage::executeSQL(const std::string& query) {
    if (!mysql_) {
       logError("未连接到 MySQL 服务器");
        return false;
    }
    
    // 执行查询
    if (mysql_query(mysql_, query.c_str()) != 0) {
        std::string errorMsg = "执行 SQL 查询失败: " + std::string(mysql_error(mysql_)) + "\nSQL: " + query;
       
        logError(errorMsg);
        return false;
    }
    
    return true;
}

// 准备预处理语句
bool MySQLStorage::prepareSQLStatements() {
    // 清理现有的预处理语句
    cleanupSQLStatements();
    
    // 检查连接状态
    if (!mysql_) {
        logError("未连接到 MySQL 服务器");
        return false;
    }
    
    // 创建并准备预处理语句
    // 实体相关语句
    statements_->saveEntity = mysql_stmt_init(mysql_);
    statements_->loadEntity = mysql_stmt_init(mysql_);
    statements_->loadEntityProperties = mysql_stmt_init(mysql_);
    statements_->deleteEntity = mysql_stmt_init(mysql_);
    
    // 关系相关语句
    statements_->saveRelation = mysql_stmt_init(mysql_);
    statements_->loadRelation = mysql_stmt_init(mysql_);
    statements_->loadRelationProperties = mysql_stmt_init(mysql_);
    statements_->deleteRelation = mysql_stmt_init(mysql_);
    
    // 三元组相关语句
    statements_->saveTriple = mysql_stmt_init(mysql_);
    statements_->loadTriple = mysql_stmt_init(mysql_);
    statements_->loadTripleProperties = mysql_stmt_init(mysql_);
    statements_->deleteTriple = mysql_stmt_init(mysql_);
    
    // 检查是否所有语句都已创建
    if (!statements_->saveEntity || !statements_->loadEntity || 
        !statements_->loadEntityProperties || !statements_->deleteEntity ||
        !statements_->saveRelation || !statements_->loadRelation ||
        !statements_->loadRelationProperties || !statements_->deleteRelation ||
        !statements_->saveTriple || !statements_->loadTriple ||
        !statements_->loadTripleProperties || !statements_->deleteTriple) {
        
        logError("创建预处理语句失败: " + std::string(mysql_error(mysql_)));
        cleanupSQLStatements();
        return false;
    }
    
    // 准备保存实体语句
    {
        const char* query = "INSERT INTO `entities` (`id`, `name`, `type`) "
                           "VALUES (?, ?, ?) "
                           "ON DUPLICATE KEY UPDATE `name` = VALUES(`name`), `type` = VALUES(`type`)";
        if (mysql_stmt_prepare(statements_->saveEntity, query, strlen(query)) != 0) {
            logError("准备保存实体语句失败: " + std::string(mysql_stmt_error(statements_->saveEntity)));
            cleanupSQLStatements();
            return false;
        }
    }
    
    // 准备加载实体语句
    {
        const char* query = "SELECT `id`, `name`, `type` FROM `entities` WHERE `id` = ?";
        if (mysql_stmt_prepare(statements_->loadEntity, query, strlen(query)) != 0) {
            logError("准备加载实体语句失败: " + std::string(mysql_stmt_error(statements_->loadEntity)));
            cleanupSQLStatements();
            return false;
        }
    }
    
    // 准备加载实体属性语句
    {
        const char* query = "SELECT `key`, `value` FROM `entity_properties` WHERE `entity_id` = ?";
        if (mysql_stmt_prepare(statements_->loadEntityProperties, query, strlen(query)) != 0) {
            logError("准备加载实体属性语句失败: " + std::string(mysql_stmt_error(statements_->loadEntityProperties)));
            cleanupSQLStatements();
            return false;
        }
    }
    
    // 准备删除实体语句
    {
        const char* query = "DELETE FROM `entities` WHERE `id` = ?";
        if (mysql_stmt_prepare(statements_->deleteEntity, query, strlen(query)) != 0) {
            logError("准备删除实体语句失败: " + std::string(mysql_stmt_error(statements_->deleteEntity)));
            cleanupSQLStatements();
            return false;
        }
    }
    
    // 准备保存关系语句
    {
        const char* query = "INSERT INTO `relations` (`id`, `name`, `type`, `inverse_name`, `is_symmetric`) "
                           "VALUES (?, ?, ?, ?, ?) "
                           "ON DUPLICATE KEY UPDATE `name` = VALUES(`name`), `type` = VALUES(`type`), "
                           "`inverse_name` = VALUES(`inverse_name`), `is_symmetric` = VALUES(`is_symmetric`)";
        if (mysql_stmt_prepare(statements_->saveRelation, query, strlen(query)) != 0) {
            logError("准备保存关系语句失败: " + std::string(mysql_stmt_error(statements_->saveRelation)));
            cleanupSQLStatements();
            return false;
        }
    }
    
    // 准备加载关系语句
    {
        const char* query = "SELECT `id`, `name`, `type`, `inverse_name`, `is_symmetric` FROM `relations` WHERE `id` = ?";
        if (mysql_stmt_prepare(statements_->loadRelation, query, strlen(query)) != 0) {
            logError("准备加载关系语句失败: " + std::string(mysql_stmt_error(statements_->loadRelation)));
            cleanupSQLStatements();
            return false;
        }
    }
    
    // 准备加载关系属性语句
    {
        const char* query = "SELECT `key`, `value` FROM `relation_properties` WHERE `relation_id` = ?";
        if (mysql_stmt_prepare(statements_->loadRelationProperties, query, strlen(query)) != 0) {
            logError("准备加载关系属性语句失败: " + std::string(mysql_stmt_error(statements_->loadRelationProperties)));
            cleanupSQLStatements();
            return false;
        }
    }
    
    // 准备删除关系语句
    {
        const char* query = "DELETE FROM `relations` WHERE `id` = ?";
        if (mysql_stmt_prepare(statements_->deleteRelation, query, strlen(query)) != 0) {
            logError("准备删除关系语句失败: " + std::string(mysql_stmt_error(statements_->deleteRelation)));
            cleanupSQLStatements();
            return false;
        }
    }
    
    // 准备保存三元组语句
    {
        const char* query = "INSERT INTO `triples` (`id`, `subject_id`, `relation_id`, `object_id`, `confidence`) "
                           "VALUES (?, ?, ?, ?, ?) "
                           "ON DUPLICATE KEY UPDATE `confidence` = VALUES(`confidence`)";
        if (mysql_stmt_prepare(statements_->saveTriple, query, strlen(query)) != 0) {
            logError("准备保存三元组语句失败: " + std::string(mysql_stmt_error(statements_->saveTriple)));
            cleanupSQLStatements();
            return false;
        }
    }
    
    // 准备加载三元组语句
    {
        const char* query = "SELECT t.`id`, t.`subject_id`, t.`relation_id`, t.`object_id`, t.`confidence`, "
                           "e1.`name`, e1.`type`, r.`name`, r.`type`, e2.`name`, e2.`type` "
                           "FROM `triples` t "
                           "JOIN `entities` e1 ON t.`subject_id` = e1.`id` "
                           "JOIN `relations` r ON t.`relation_id` = r.`id` "
                           "JOIN `entities` e2 ON t.`object_id` = e2.`id` "
                           "WHERE t.`id` = ?";
        if (mysql_stmt_prepare(statements_->loadTriple, query, strlen(query)) != 0) {
            logError("准备加载三元组语句失败: " + std::string(mysql_stmt_error(statements_->loadTriple)));
            cleanupSQLStatements();
            return false;
        }
    }
    
    // 准备加载三元组属性语句
    {
        const char* query = "SELECT `key`, `value` FROM `triple_properties` WHERE `triple_id` = ?";
        if (mysql_stmt_prepare(statements_->loadTripleProperties, query, strlen(query)) != 0) {
            logError("准备加载三元组属性语句失败: " + std::string(mysql_stmt_error(statements_->loadTripleProperties)));
            cleanupSQLStatements();
            return false;
        }
    }
    
    // 准备删除三元组语句
    {
        const char* query = "DELETE FROM `triples` WHERE `id` = ?";
        if (mysql_stmt_prepare(statements_->deleteTriple, query, strlen(query)) != 0) {
            logError("准备删除三元组语句失败: " + std::string(mysql_stmt_error(statements_->deleteTriple)));
            cleanupSQLStatements();
            return false;
        }
    }
    
    LOG_INFO("所有预处理语句准备完成");
    return true;
}

// 关闭并释放预处理语句
void MySQLStorage::cleanupSQLStatements() {
    // 关闭实体相关语句
    if (statements_->saveEntity) {
        mysql_stmt_close(statements_->saveEntity);
        statements_->saveEntity = nullptr;
    }
    if (statements_->loadEntity) {
        mysql_stmt_close(statements_->loadEntity);
        statements_->loadEntity = nullptr;
    }
    if (statements_->loadEntityProperties) {
        mysql_stmt_close(statements_->loadEntityProperties);
        statements_->loadEntityProperties = nullptr;
    }
    if (statements_->deleteEntity) {
        mysql_stmt_close(statements_->deleteEntity);
        statements_->deleteEntity = nullptr;
    }
    
    // 关闭关系相关语句
    if (statements_->saveRelation) {
        mysql_stmt_close(statements_->saveRelation);
        statements_->saveRelation = nullptr;
    }
    if (statements_->loadRelation) {
        mysql_stmt_close(statements_->loadRelation);
        statements_->loadRelation = nullptr;
    }
    if (statements_->loadRelationProperties) {
        mysql_stmt_close(statements_->loadRelationProperties);
        statements_->loadRelationProperties = nullptr;
    }
    if (statements_->deleteRelation) {
        mysql_stmt_close(statements_->deleteRelation);
        statements_->deleteRelation = nullptr;
    }
    
    // 关闭三元组相关语句
    if (statements_->saveTriple) {
        mysql_stmt_close(statements_->saveTriple);
        statements_->saveTriple = nullptr;
    }
    if (statements_->loadTriple) {
        mysql_stmt_close(statements_->loadTriple);
        statements_->loadTriple = nullptr;
    }
    if (statements_->loadTripleProperties) {
        mysql_stmt_close(statements_->loadTripleProperties);
        statements_->loadTripleProperties = nullptr;
    }
    if (statements_->deleteTriple) {
        mysql_stmt_close(statements_->deleteTriple);
        statements_->deleteTriple = nullptr;
    }
}

// 获取存储连接信息（脱敏处理）
std::string MySQLStorage::getConnectionInfo() const {
    // 构建连接信息字符串，密码做脱敏处理
    std::ostringstream ss;
    ss << "host=" << host_ << ";port=" << port_ << ";user=" << user_
       << ";password=******;database=" << database_;
    return ss.str();
}

// 开始事务
bool MySQLStorage::beginTransaction() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!mysql_) {
        logError("未连接到 MySQL 服务器");
        return false;
    }
    
    if (isInTransaction_) {
        LOG_WARN("已经在事务中，忽略 beginTransaction 调用");
        return true;
    }
    
    if (!executeSQL("START TRANSACTION")) {
        return false;
    }
    
    isInTransaction_ = true;
    LOG_INFO("开始事务");
    return true;
}

// 提交事务
bool MySQLStorage::commitTransaction() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!mysql_) {
        logError("未连接到 MySQL 服务器");
        return false;
    }
    
    if (!isInTransaction_) {
        LOG_WARN("未在事务中，忽略 commitTransaction 调用");
        return true;
    }
    
    if (!executeSQL("COMMIT")) {
        return false;
    }
    
    isInTransaction_ = false;
    LOG_INFO("提交事务");
    return true;
}

// 回滚事务
bool MySQLStorage::rollbackTransaction() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!mysql_) {
        logError("未连接到 MySQL 服务器");
        return false;
    }
    
    if (!isInTransaction_) {
        LOG_WARN("未在事务中，忽略 rollbackTransaction 调用");
        return true;
    }
    
    if (!executeSQL("ROLLBACK")) {
        return false;
    }
    
    isInTransaction_ = false;
    LOG_INFO("回滚事务");
    return true;
}

// 执行自定义查询
std::string MySQLStorage::executeQuery(
    const std::string& query,
    const std::unordered_map<std::string, std::string>& params) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!mysql_) {
        logError("未连接到 MySQL 服务器");
        return "{}";
    }
    
    // 简单参数替换（注意：这不是安全的参数化查询）
    std::string processedQuery = query;
    for (const auto& param : params) {
        // 转义参数值
        char* escapedValue = new char[param.second.length() * 2 + 1];
        mysql_real_escape_string(mysql_, escapedValue, param.second.c_str(), param.second.length());
        
        // 替换参数
        std::string placeholder = ":" + param.first;
        size_t pos = 0;
        while ((pos = processedQuery.find(placeholder, pos)) != std::string::npos) {
            processedQuery.replace(pos, placeholder.length(), escapedValue);
            pos += strlen(escapedValue);
        }
        
        delete[] escapedValue;
    }
    
    // 执行查询
    if (mysql_query(mysql_, processedQuery.c_str()) != 0) {
        logError("执行查询失败: " + std::string(mysql_error(mysql_)));
        return "{}";
    }
    
    // 获取结果集
    MYSQL_RES* result = mysql_store_result(mysql_);
    if (!result) {
        if (mysql_field_count(mysql_) == 0) {
            // 不是查询语句，返回受影响的行数
            std::ostringstream ss;
            ss << "{\"affected_rows\": " << mysql_affected_rows(mysql_) << "}";
            return ss.str();
        } else {
            logError("获取结果集失败: " + std::string(mysql_error(mysql_)));
            return "{}";
        }
    }
    
    // 构建JSON结果
    std::ostringstream json;
    json << "[";
    
    MYSQL_ROW row;
    unsigned int numFields = mysql_num_fields(result);
    MYSQL_FIELD* fields = mysql_fetch_fields(result);
    bool firstRow = true;
    
    while ((row = mysql_fetch_row(result))) {
        if (!firstRow) {
            json << ",";
        }
        
        json << "{";
        for (unsigned int i = 0; i < numFields; i++) {
            if (i > 0) {
                json << ",";
            }
            
            json << "\"" << fields[i].name << "\":";
            if (row[i] == nullptr) {
                json << "null";
            } else {
                // 转义JSON字符串
                json << "\"";
                for (const char* c = row[i]; *c; c++) {
                    switch (*c) {
                        case '\"': json << "\\\""; break;
                        case '\\': json << "\\\\"; break;
                        case '/': json << "\\/"; break;
                        case '\b': json << "\\b"; break;
                        case '\f': json << "\\f"; break;
                        case '\n': json << "\\n"; break;
                        case '\r': json << "\\r"; break;
                        case '\t': json << "\\t"; break;
                        default:
                            if (*c >= 0 && *c < 32) {
                                char buf[8];
                                sprintf(buf, "\\u%04x", *c);
                                json << buf;
                            } else {
                                json << *c;
                            }
                    }
                }
                json << "\"";
            }
        }
        json << "}";
        
        firstRow = false;
    }
    
    json << "]";
    
    // 释放结果集
    mysql_free_result(result);
    
    return json.str();
}

// 保存实体
bool MySQLStorage::saveEntity(const knowledge::EntityPtr& entity) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!mysql_ || !statements_->saveEntity) {
        logError("未连接到 MySQL 服务器或预处理语句未准备");
        return false;
    }
    
    if (!entity) {
        logError("实体对象为空");
        return false;
    }
    
    // 绑定参数
    MYSQL_BIND bind[3];
    memset(bind, 0, sizeof(bind));
    
    // ID参数
    std::string id = entity->getId();
    unsigned long id_length = id.length();
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)id.c_str();
    bind[0].buffer_length = id_length;
    bind[0].length = &id_length;
    bind[0].is_null = nullptr;
    
    // 名称参数
    std::string name = entity->getName();
    unsigned long name_length = name.length();
    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (void*)name.c_str();
    bind[1].buffer_length = name_length;
    bind[1].length = &name_length;
    bind[1].is_null = nullptr;
    
    // 类型参数
    int type = static_cast<int>(entity->getType());
    bind[2].buffer_type = MYSQL_TYPE_LONG;
    bind[2].buffer = &type;
    bind[2].is_null = nullptr;
    
    // 绑定参数
    if (mysql_stmt_bind_param(statements_->saveEntity, bind) != 0) {
        logError("绑定参数失败: " + std::string(mysql_stmt_error(statements_->saveEntity)));
        return false;
    }
    
    // 执行语句
    if (mysql_stmt_execute(statements_->saveEntity) != 0) {
        logError("执行语句失败: " + std::string(mysql_stmt_error(statements_->saveEntity)));
        return false;
    }
    
    // 保存实体属性
    if (!entity->getAllProperties().empty() && !saveEntityProperties(id, entity->getAllProperties())) {
        LOG_WARN("保存实体属性失败: {}", id);
        // 继续执行，不返回错误
    }
    
    LOG_INFO("实体保存成功: {}", id);
    return true;
}

// 批量保存实体
size_t MySQLStorage::saveEntities(const std::vector<knowledge::EntityPtr>& entities) {
    if (entities.empty()) {
        return 0;
    }
    
    // 启动事务
    bool needsCommit = !isInTransaction_;
    if (needsCommit && !beginTransaction()) {
        LOG_ERROR("开始事务失败");
        return 0;
    }
    
    size_t successCount = 0;
    for (const auto& entity : entities) {
        if (saveEntity(entity)) {
            successCount++;
        }
    }
    
    // 提交事务
    if (needsCommit) {
        if (successCount == entities.size()) {
            if (!commitTransaction()) {
                LOG_ERROR("提交事务失败");
                return successCount;
            }
        } else {
            if (!rollbackTransaction()) {
                LOG_ERROR("回滚事务失败");
            }
            return 0;  // 如果有任何失败且事务回滚，则返回0
        }
    }
    
    return successCount;
}

// 保存实体属性
bool MySQLStorage::saveEntityProperties(
    const std::string& entityId,
    const std::unordered_map<std::string, std::string>& properties) {
    
    if (properties.empty()) {
        return true;
    }
    
    // 删除现有属性
    if (!executeSQL("DELETE FROM `entity_properties` WHERE `entity_id` = '" + entityId + "'")) {
        return false;
    }
    
    // 插入新属性
    for (const auto& prop : properties) {
        std::string key = prop.first;
        std::string value = prop.second;
        
        // 转义特殊字符
        char* escaped_key = new char[key.length() * 2 + 1];
        char* escaped_value = new char[value.length() * 2 + 1];
        
        mysql_real_escape_string(mysql_, escaped_key, key.c_str(), key.length());
        mysql_real_escape_string(mysql_, escaped_value, value.c_str(), value.length());
        
        std::string sql = "INSERT INTO `entity_properties` (`entity_id`, `key`, `value`) VALUES ('" 
                        + entityId + "', '" + escaped_key + "', '" + escaped_value + "')";
        
        delete[] escaped_key;
        delete[] escaped_value;
        
        if (!executeSQL(sql)) {
            return false;
        }
    }
    
    return true;
}

// 加载实体属性
std::unordered_map<std::string, std::string> MySQLStorage::loadEntityProperties(
    const std::string& entityId) {
    
    std::unordered_map<std::string, std::string> properties;
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!mysql_ || !statements_->loadEntityProperties) {
        logError("未连接到 MySQL 服务器或预处理语句未准备");
        return properties;
    }
    
    // 绑定参数
    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));
    
    // ID参数
    unsigned long id_length = entityId.length();
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)entityId.c_str();
    bind[0].buffer_length = id_length;
    bind[0].length = &id_length;
    
    // 绑定参数
    if (mysql_stmt_bind_param(statements_->loadEntityProperties, bind) != 0) {
        logError("绑定参数失败: " + std::string(mysql_stmt_error(statements_->loadEntityProperties)));
        return properties;
    }
    
    // 执行查询
    if (mysql_stmt_execute(statements_->loadEntityProperties) != 0) {
        logError("执行查询失败: " + std::string(mysql_stmt_error(statements_->loadEntityProperties)));
        return properties;
    }
    
    // 准备结果集
    MYSQL_BIND result[2];
    memset(result, 0, sizeof(result));
    
    // 键字段
    char key_buffer[256];
    unsigned long key_length;
    bool key_is_null;
    result[0].buffer_type = MYSQL_TYPE_STRING;
    result[0].buffer = key_buffer;
    result[0].buffer_length = sizeof(key_buffer);
    result[0].length = &key_length;
    result[0].is_null = &key_is_null;
    
    // 值字段
    char value_buffer[4096];
    unsigned long value_length;
    bool value_is_null;
    result[1].buffer_type = MYSQL_TYPE_STRING;
    result[1].buffer = value_buffer;
    result[1].buffer_length = sizeof(value_buffer);
    result[1].length = &value_length;
    result[1].is_null = &value_is_null;
    
    // 绑定结果集
    if (mysql_stmt_bind_result(statements_->loadEntityProperties, result) != 0) {
        logError("绑定结果集失败: " + std::string(mysql_stmt_error(statements_->loadEntityProperties)));
        return properties;
    }
    
    // 读取结果
    while (mysql_stmt_fetch(statements_->loadEntityProperties) == 0) {
        if (!key_is_null && !value_is_null) {
            std::string key(key_buffer, key_length);
            std::string value(value_buffer, value_length);
            properties[key] = value;
        }
    }
    
    // 关闭结果集
    mysql_stmt_free_result(statements_->loadEntityProperties);
    
    return properties;
}

// 加载实体
knowledge::EntityPtr MySQLStorage::loadEntity(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!mysql_ || !statements_->loadEntity) {
        logError("未连接到 MySQL 服务器或预处理语句未准备");
        return nullptr;
    }
    
    // 绑定参数
    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));
    
    // ID参数
    unsigned long id_length = id.length();
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)id.c_str();
    bind[0].buffer_length = id_length;
    bind[0].length = &id_length;
    
    // 绑定参数
    if (mysql_stmt_bind_param(statements_->loadEntity, bind) != 0) {
        logError("绑定参数失败: " + std::string(mysql_stmt_error(statements_->loadEntity)));
        return nullptr;
    }
    
    // 执行查询
    if (mysql_stmt_execute(statements_->loadEntity) != 0) {
        logError("执行查询失败: " + std::string(mysql_stmt_error(statements_->loadEntity)));
        return nullptr;
    }
    
    // 准备结果集
    MYSQL_BIND result[3];
    memset(result, 0, sizeof(result));
    
    // ID字段
    char id_buffer[256];
    unsigned long id_result_length;
    bool id_is_null;
    result[0].buffer_type = MYSQL_TYPE_STRING;
    result[0].buffer = id_buffer;
    result[0].buffer_length = sizeof(id_buffer);
    result[0].length = &id_result_length;
    result[0].is_null = &id_is_null;
    
    // 名称字段
    char name_buffer[1024];
    unsigned long name_length;
    bool name_is_null;
    result[1].buffer_type = MYSQL_TYPE_STRING;
    result[1].buffer = name_buffer;
    result[1].buffer_length = sizeof(name_buffer);
    result[1].length = &name_length;
    result[1].is_null = &name_is_null;
    
    // 类型字段
    int type;
    bool type_is_null;
    result[2].buffer_type = MYSQL_TYPE_LONG;
    result[2].buffer = &type;
    result[2].is_null = &type_is_null;
    
    // 绑定结果集
    if (mysql_stmt_bind_result(statements_->loadEntity, result) != 0) {
        logError("绑定结果集失败: " + std::string(mysql_stmt_error(statements_->loadEntity)));
        return nullptr;
    }
    
    // 获取结果
    if (mysql_stmt_fetch(statements_->loadEntity) != 0) {
        if (mysql_stmt_errno(statements_->loadEntity) != 0) {
            logError("获取结果失败: " + std::string(mysql_stmt_error(statements_->loadEntity)));
        } else {
            LOG_INFO("实体不存在: {}", id);
        }
        mysql_stmt_free_result(statements_->loadEntity);
        return nullptr;
    }
    
    // 关闭结果集
    mysql_stmt_free_result(statements_->loadEntity);
    
    // 创建实体对象
    if (id_is_null || name_is_null || type_is_null) {
        LOG_ERROR("加载的实体包含空值: {}", id);
        return nullptr;
    }
    
    std::string entity_id(id_buffer, id_result_length);
    std::string name(name_buffer, name_length);
    knowledge::EntityType entity_type = static_cast<knowledge::EntityType>(type);
    
    // 加载实体属性
    auto properties = loadEntityProperties(entity_id);
    
    // 创建实体
    auto entity = std::make_shared<knowledge::Entity>(entity_id, name, entity_type);
    for (const auto& prop : properties) {
        entity->addProperty(prop.first, prop.second);
    }
    
    LOG_INFO("实体加载成功: {}", entity_id);
    return entity;
}

// 查询实体
std::vector<knowledge::EntityPtr> MySQLStorage::queryEntities(
    const std::string& name,
    knowledge::EntityType type,
    const std::unordered_map<std::string, std::string>& properties,
    size_t limit) {
    
    std::vector<knowledge::EntityPtr> entities;
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!mysql_) {
        logError("未连接到 MySQL 服务器");
        return entities;
    }
    
    // 构建查询
    std::string sql = "SELECT e.`id` FROM `entities` e";
    
    // 添加属性表连接（如果需要）
    if (!properties.empty()) {
        sql += " INNER JOIN `entity_properties` ep ON e.`id` = ep.`entity_id`";
    }
    
    // 构建 WHERE 子句
    std::vector<std::string> conditions;
    
    if (!name.empty()) {
        // 转义名称
        char* escaped_name = new char[name.length() * 2 + 1];
        mysql_real_escape_string(mysql_, escaped_name, name.c_str(), name.length());
        conditions.push_back("e.`name` LIKE '%" + std::string(escaped_name) + "%'");
        delete[] escaped_name;
    }
    
    if (type != knowledge::EntityType::UNKNOWN) {
        conditions.push_back("e.`type` = " + std::to_string(static_cast<int>(type)));
    }
    
    // 添加属性条件
    if (!properties.empty()) {
        std::vector<std::string> propertyConditions;
        for (const auto& prop : properties) {
            char* escaped_key = new char[prop.first.length() * 2 + 1];
            char* escaped_value = new char[prop.second.length() * 2 + 1];
            
            mysql_real_escape_string(mysql_, escaped_key, prop.first.c_str(), prop.first.length());
            mysql_real_escape_string(mysql_, escaped_value, prop.second.c_str(), prop.second.length());
            
            propertyConditions.push_back(
                "(ep.`key` = '" + std::string(escaped_key) + "' AND ep.`value` = '" + 
                std::string(escaped_value) + "')");
            
            delete[] escaped_key;
            delete[] escaped_value;
        }
        
        if (!propertyConditions.empty()) {
            conditions.push_back("(" + join(propertyConditions, " OR ") + ")");
        }
        
        // 分组以确保所有条件都满足
        sql += " GROUP BY e.`id` HAVING COUNT(DISTINCT ep.`key`) >= " + 
               std::to_string(properties.size());
    }
    
    // 添加 WHERE 子句
    if (!conditions.empty()) {
        sql += " WHERE " + join(conditions, " AND ");
    }
    
    // 添加 LIMIT 子句
    if (limit > 0) {
        sql += " LIMIT " + std::to_string(limit);
    }
    
    // 执行查询
    if (mysql_query(mysql_, sql.c_str()) != 0) {
        logError("执行查询失败: " + std::string(mysql_error(mysql_)));
        return entities;
    }
    
    // 获取结果
    MYSQL_RES* result = mysql_store_result(mysql_);
    if (!result) {
        logError("获取结果失败: " + std::string(mysql_error(mysql_)));
        return entities;
    }
    
    // 处理结果
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        if (row[0]) {
            std::string entity_id = row[0];
            auto entity = loadEntity(entity_id);
            if (entity) {
                entities.push_back(entity);
            }
        }
    }
    
    // 释放结果集
    mysql_free_result(result);
    
    LOG_INFO("查询到 {} 个实体", entities.size());
    return entities;
}

// 删除实体
bool MySQLStorage::deleteEntity(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!mysql_ || !statements_->deleteEntity) {
        logError("未连接到 MySQL 服务器或预处理语句未准备");
        return false;
    }
    
    // 绑定参数
    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));
    
    // ID参数
    unsigned long id_length = id.length();
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)id.c_str();
    bind[0].buffer_length = id_length;
    bind[0].length = &id_length;
    
    // 绑定参数
    if (mysql_stmt_bind_param(statements_->deleteEntity, bind) != 0) {
        logError("绑定参数失败: " + std::string(mysql_stmt_error(statements_->deleteEntity)));
        return false;
    }
    
    // 执行语句
    if (mysql_stmt_execute(statements_->deleteEntity) != 0) {
        logError("执行语句失败: " + std::string(mysql_stmt_error(statements_->deleteEntity)));
        return false;
    }
    
    // 检查是否有行被影响
    int affected_rows = mysql_stmt_affected_rows(statements_->deleteEntity);
    if (affected_rows == 0) {
        LOG_WARN("没有找到要删除的实体: {}", id);
        return false;
    }
    
    LOG_INFO("实体删除成功: {}", id);
    return true;
}

// 保存关系
bool MySQLStorage::saveRelation(const knowledge::RelationPtr& relation) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!mysql_ || !statements_->saveRelation) {
        logError("未连接到 MySQL 服务器或预处理语句未准备");
        return false;
    }
    
    if (!relation) {
        logError("关系对象为空");
        return false;
    }
    
    // 绑定参数
    MYSQL_BIND bind[5];
    memset(bind, 0, sizeof(bind));
    
    // ID参数
    std::string id = relation->getId();
    unsigned long id_length = id.length();
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)id.c_str();
    bind[0].buffer_length = id_length;
    bind[0].length = &id_length;
    bind[0].is_null = nullptr;
    
    // 名称参数
    std::string name = relation->getName();
    unsigned long name_length = name.length();
    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (void*)name.c_str();
    bind[1].buffer_length = name_length;
    bind[1].length = &name_length;
    bind[1].is_null = nullptr;
    
    // 类型参数
    int type = static_cast<int>(relation->getType());
    bind[2].buffer_type = MYSQL_TYPE_LONG;
    bind[2].buffer = &type;
    bind[2].is_null = nullptr;
    
    // 反向名称参数
    std::string inverseName = relation->getInverseName();
    unsigned long inverseName_length = inverseName.length();
    bind[3].buffer_type = MYSQL_TYPE_STRING;
    bind[3].buffer = (void*)inverseName.c_str();
    bind[3].buffer_length = inverseName_length;
    bind[3].length = &inverseName_length;
    bind[3].is_null = nullptr;
    
    // 是否对称参数
    bool isSymmetric = relation->isSymmetric();
    bind[4].buffer_type = MYSQL_TYPE_TINY;
    bind[4].buffer = &isSymmetric;
    bind[4].is_null = nullptr;
    
    // 绑定参数
    if (mysql_stmt_bind_param(statements_->saveRelation, bind) != 0) {
        logError("绑定参数失败: " + std::string(mysql_stmt_error(statements_->saveRelation)));
        return false;
    }
    
    // 执行语句
    if (mysql_stmt_execute(statements_->saveRelation) != 0) {
        logError("执行语句失败: " + std::string(mysql_stmt_error(statements_->saveRelation)));
        return false;
    }
    
    // 保存关系属性
    if (!relation->getAllProperties().empty() && 
        !saveRelationProperties(id, relation->getAllProperties())) {
        LOG_WARN("保存关系属性失败: {}", id);
        // 继续执行，不返回错误
    }
    
    LOG_INFO("关系保存成功: {}", id);
    return true;
}

// 批量保存关系
size_t MySQLStorage::saveRelations(const std::vector<knowledge::RelationPtr>& relations) {
    if (relations.empty()) {
        return 0;
    }
    
    // 启动事务
    bool needsCommit = !isInTransaction_;
    if (needsCommit && !beginTransaction()) {
        LOG_ERROR("开始事务失败");
        return 0;
    }
    
    size_t successCount = 0;
    for (const auto& relation : relations) {
        if (saveRelation(relation)) {
            successCount++;
        }
    }
    
    // 提交事务
    if (needsCommit) {
        if (successCount == relations.size()) {
            if (!commitTransaction()) {
                LOG_ERROR("提交事务失败");
                return successCount;
            }
        } else {
            if (!rollbackTransaction()) {
                LOG_ERROR("回滚事务失败");
            }
            return 0;  // 如果有任何失败且事务回滚，则返回0
        }
    }
    
    return successCount;
}

// 保存关系属性
bool MySQLStorage::saveRelationProperties(
    const std::string& relationId,
    const std::unordered_map<std::string, std::string>& properties) {
    
    if (properties.empty()) {
        return true;
    }
    
    // 删除现有属性
    if (!executeSQL("DELETE FROM `relation_properties` WHERE `relation_id` = '" + relationId + "'")) {
        return false;
    }
    
    // 插入新属性
    for (const auto& prop : properties) {
        std::string key = prop.first;
        std::string value = prop.second;
        
        // 转义特殊字符
        char* escaped_key = new char[key.length() * 2 + 1];
        char* escaped_value = new char[value.length() * 2 + 1];
        
        mysql_real_escape_string(mysql_, escaped_key, key.c_str(), key.length());
        mysql_real_escape_string(mysql_, escaped_value, value.c_str(), value.length());
        
        std::string sql = "INSERT INTO `relation_properties` (`relation_id`, `key`, `value`) VALUES ('" 
                        + relationId + "', '" + escaped_key + "', '" + escaped_value + "')";
        
        delete[] escaped_key;
        delete[] escaped_value;
        
        if (!executeSQL(sql)) {
            return false;
        }
    }
    
    return true;
}

// 加载关系属性
std::unordered_map<std::string, std::string> MySQLStorage::loadRelationProperties(
    const std::string& relationId) {
    
    std::unordered_map<std::string, std::string> properties;
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!mysql_ || !statements_->loadRelationProperties) {
        logError("未连接到 MySQL 服务器或预处理语句未准备");
        return properties;
    }
    
    // 绑定参数
    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));
    
    // ID参数
    unsigned long id_length = relationId.length();
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)relationId.c_str();
    bind[0].buffer_length = id_length;
    bind[0].length = &id_length;
    
    // 绑定参数
    if (mysql_stmt_bind_param(statements_->loadRelationProperties, bind) != 0) {
        logError("绑定参数失败: " + std::string(mysql_stmt_error(statements_->loadRelationProperties)));
        return properties;
    }
    
    // 执行查询
    if (mysql_stmt_execute(statements_->loadRelationProperties) != 0) {
        logError("执行查询失败: " + std::string(mysql_stmt_error(statements_->loadRelationProperties)));
        return properties;
    }
    
    // 准备结果集
    MYSQL_BIND result[2];
    memset(result, 0, sizeof(result));
    
    // 键字段
    char key_buffer[256];
    unsigned long key_length;
    bool key_is_null;
    result[0].buffer_type = MYSQL_TYPE_STRING;
    result[0].buffer = key_buffer;
    result[0].buffer_length = sizeof(key_buffer);
    result[0].length = &key_length;
    result[0].is_null = &key_is_null;
    
    // 值字段
    char value_buffer[4096];
    unsigned long value_length;
    bool value_is_null;
    result[1].buffer_type = MYSQL_TYPE_STRING;
    result[1].buffer = value_buffer;
    result[1].buffer_length = sizeof(value_buffer);
    result[1].length = &value_length;
    result[1].is_null = &value_is_null;
    
    // 绑定结果集
    if (mysql_stmt_bind_result(statements_->loadRelationProperties, result) != 0) {
        logError("绑定结果集失败: " + std::string(mysql_stmt_error(statements_->loadRelationProperties)));
        return properties;
    }
    
    // 读取结果
    while (mysql_stmt_fetch(statements_->loadRelationProperties) == 0) {
        if (!key_is_null && !value_is_null) {
            std::string key(key_buffer, key_length);
            std::string value(value_buffer, value_length);
            properties[key] = value;
        }
    }
    
    // 关闭结果集
    mysql_stmt_free_result(statements_->loadRelationProperties);
    
    return properties;
}

// 加载关系
knowledge::RelationPtr MySQLStorage::loadRelation(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!mysql_ || !statements_->loadRelation) {
        logError("未连接到 MySQL 服务器或预处理语句未准备");
        return nullptr;
    }
    
    // 绑定参数
    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));
    
    // ID参数
    unsigned long id_length = id.length();
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)id.c_str();
    bind[0].buffer_length = id_length;
    bind[0].length = &id_length;
    
    // 绑定参数
    if (mysql_stmt_bind_param(statements_->loadRelation, bind) != 0) {
        logError("绑定参数失败: " + std::string(mysql_stmt_error(statements_->loadRelation)));
        return nullptr;
    }
    
    // 执行查询
    if (mysql_stmt_execute(statements_->loadRelation) != 0) {
        logError("执行查询失败: " + std::string(mysql_stmt_error(statements_->loadRelation)));
        return nullptr;
    }
    
    // 准备结果集
    MYSQL_BIND result[5];
    memset(result, 0, sizeof(result));
    
    // ID字段
    char id_buffer[256];
    unsigned long id_result_length;
    bool id_is_null;
    result[0].buffer_type = MYSQL_TYPE_STRING;
    result[0].buffer = id_buffer;
    result[0].buffer_length = sizeof(id_buffer);
    result[0].length = &id_result_length;
    result[0].is_null = &id_is_null;
    
    // 名称字段
    char name_buffer[1024];
    unsigned long name_length;
    bool name_is_null;
    result[1].buffer_type = MYSQL_TYPE_STRING;
    result[1].buffer = name_buffer;
    result[1].buffer_length = sizeof(name_buffer);
    result[1].length = &name_length;
    result[1].is_null = &name_is_null;
    
    // 类型字段
    int type;
    bool type_is_null;
    result[2].buffer_type = MYSQL_TYPE_LONG;
    result[2].buffer = &type;
    result[2].is_null = &type_is_null;
    
    // 反向名称字段
    char inverse_name_buffer[1024];
    unsigned long inverse_name_length;
    bool inverse_name_is_null;
    result[3].buffer_type = MYSQL_TYPE_STRING;
    result[3].buffer = inverse_name_buffer;
    result[3].buffer_length = sizeof(inverse_name_buffer);
    result[3].length = &inverse_name_length;
    result[3].is_null = &inverse_name_is_null;
    
    // 是否对称字段
    bool is_symmetric;
    bool is_symmetric_is_null;
    result[4].buffer_type = MYSQL_TYPE_TINY;
    result[4].buffer = &is_symmetric;
    result[4].is_null = &is_symmetric_is_null;
    
    // 绑定结果集
    if (mysql_stmt_bind_result(statements_->loadRelation, result) != 0) {
        logError("绑定结果集失败: " + std::string(mysql_stmt_error(statements_->loadRelation)));
        return nullptr;
    }
    
    // 获取结果
    if (mysql_stmt_fetch(statements_->loadRelation) != 0) {
        if (mysql_stmt_errno(statements_->loadRelation) != 0) {
            logError("获取结果失败: " + std::string(mysql_stmt_error(statements_->loadRelation)));
        } else {
            LOG_INFO("关系不存在: {}", id);
        }
        mysql_stmt_free_result(statements_->loadRelation);
        return nullptr;
    }
    
    // 关闭结果集
    mysql_stmt_free_result(statements_->loadRelation);
    
    // 创建关系对象
    if (id_is_null || name_is_null || type_is_null || inverse_name_is_null || is_symmetric_is_null) {
        LOG_ERROR("加载的关系包含空值: {}", id);
        return nullptr;
    }
    
    std::string relation_id(id_buffer, id_result_length);
    std::string name(name_buffer, name_length);
    knowledge::RelationType relation_type = static_cast<knowledge::RelationType>(type);
    std::string inverse_name(inverse_name_buffer, inverse_name_length);
    
    // 加载关系属性
    auto properties = loadRelationProperties(relation_id);
    
    // 创建关系
    // auto relation = std::make_shared<knowledge::Relation>(
    //     relation_id, name, relation_type, inverse_name, is_symmetric);
        auto relation = std::make_shared<knowledge::Relation>(
        relation_id, name, relation_type);
    for (const auto& prop : properties) {
        relation->addProperty(prop.first, prop.second);
    }
    
    LOG_INFO("关系加载成功: {}", relation_id);
    return relation;
}

// 查询关系
std::vector<knowledge::RelationPtr> MySQLStorage::queryRelations(
    const std::string& name,
    knowledge::RelationType type,
    const std::unordered_map<std::string, std::string>& properties,
    size_t limit) {
    
    std::vector<knowledge::RelationPtr> relations;
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!mysql_) {
        logError("未连接到 MySQL 服务器");
        return relations;
    }
    
    // 构建查询
    std::string sql = "SELECT r.`id` FROM `relations` r";
    
    // 添加属性表连接（如果需要）
    if (!properties.empty()) {
        sql += " INNER JOIN `relation_properties` rp ON r.`id` = rp.`relation_id`";
    }
    
    // 构建 WHERE 子句
    std::vector<std::string> conditions;
    
    if (!name.empty()) {
        // 转义名称
        char* escaped_name = new char[name.length() * 2 + 1];
        mysql_real_escape_string(mysql_, escaped_name, name.c_str(), name.length());
        conditions.push_back("r.`name` LIKE '%" + std::string(escaped_name) + "%'");
        delete[] escaped_name;
    }
    
    if (type != knowledge::RelationType::UNKNOWN) {
        conditions.push_back("r.`type` = " + std::to_string(static_cast<int>(type)));
    }
    
    // 添加属性条件
    if (!properties.empty()) {
        std::vector<std::string> propertyConditions;
        for (const auto& prop : properties) {
            char* escaped_key = new char[prop.first.length() * 2 + 1];
            char* escaped_value = new char[prop.second.length() * 2 + 1];
            
            mysql_real_escape_string(mysql_, escaped_key, prop.first.c_str(), prop.first.length());
            mysql_real_escape_string(mysql_, escaped_value, prop.second.c_str(), prop.second.length());
            
            propertyConditions.push_back(
                "(rp.`key` = '" + std::string(escaped_key) + "' AND rp.`value` = '" + 
                std::string(escaped_value) + "')");
            
            delete[] escaped_key;
            delete[] escaped_value;
        }
        
        if (!propertyConditions.empty()) {
            conditions.push_back("(" + join(propertyConditions, " OR ") + ")");
        }
        
        // 分组以确保所有条件都满足
        sql += " GROUP BY r.`id` HAVING COUNT(DISTINCT rp.`key`) >= " + 
               std::to_string(properties.size());
    }
    
    // 添加 WHERE 子句
    if (!conditions.empty()) {
        sql += " WHERE " + join(conditions, " AND ");
    }
    
    // 添加 LIMIT 子句
    if (limit > 0) {
        sql += " LIMIT " + std::to_string(limit);
    }
    
    // 执行查询
    if (mysql_query(mysql_, sql.c_str()) != 0) {
        logError("执行查询失败: " + std::string(mysql_error(mysql_)));
        return relations;
    }
    
    // 获取结果
    MYSQL_RES* result = mysql_store_result(mysql_);
    if (!result) {
        logError("获取结果失败: " + std::string(mysql_error(mysql_)));
        return relations;
    }
    
    // 处理结果
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        if (row[0]) {
            std::string relation_id = row[0];
            auto relation = loadRelation(relation_id);
            if (relation) {
                relations.push_back(relation);
            }
        }
    }
    
    // 释放结果集
    mysql_free_result(result);
    
    LOG_INFO("查询到 {} 个关系", relations.size());
    return relations;
}

// 删除关系
bool MySQLStorage::deleteRelation(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!mysql_ || !statements_->deleteRelation) {
        logError("未连接到 MySQL 服务器或预处理语句未准备");
        return false;
    }
    
    // 绑定参数
    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));
    
    // ID参数
    unsigned long id_length = id.length();
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)id.c_str();
    bind[0].buffer_length = id_length;
    bind[0].length = &id_length;
    
    // 绑定参数
    if (mysql_stmt_bind_param(statements_->deleteRelation, bind) != 0) {
        logError("绑定参数失败: " + std::string(mysql_stmt_error(statements_->deleteRelation)));
        return false;
    }
    
    // 执行语句
    if (mysql_stmt_execute(statements_->deleteRelation) != 0) {
        logError("执行语句失败: " + std::string(mysql_stmt_error(statements_->deleteRelation)));
        return false;
    }
    
    // 检查是否有行被影响
    int affected_rows = mysql_stmt_affected_rows(statements_->deleteRelation);
    if (affected_rows == 0) {
        LOG_WARN("没有找到要删除的关系: {}", id);
        return false;
    }
    
    LOG_INFO("关系删除成功: {}", id);
    return true;
}

// 保存三元组
bool MySQLStorage::saveTriple(const knowledge::TriplePtr& triple) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!mysql_ || !statements_->saveTriple) {
        logError("未连接到 MySQL 服务器或预处理语句未准备");
        return false;
    }
    
    if (!triple) {
        logError("三元组对象为空");
        return false;
    }
    
    // 检查主体、关系和客体是否已保存
    if (!loadEntity(triple->getSubject()->getId())) {
        if (!saveEntity(triple->getSubject())) {
            logError("保存三元组主体失败: " + triple->getSubject()->getId());
            return false;
        }
    }
    
    if (!loadRelation(triple->getRelation()->getId())) {
        if (!saveRelation(triple->getRelation())) {
            logError("保存三元组关系失败: " + triple->getRelation()->getId());
            return false;
        }
    }
    
    if (!loadEntity(triple->getObject()->getId())) {
        if (!saveEntity(triple->getObject())) {
            logError("保存三元组客体失败: " + triple->getObject()->getId());
            return false;
        }
    }
    
    // 绑定参数
    MYSQL_BIND bind[5];
    memset(bind, 0, sizeof(bind));
    
    // ID参数
    std::string id = triple->getId();
    unsigned long id_length = id.length();
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)id.c_str();
    bind[0].buffer_length = id_length;
    bind[0].length = &id_length;
    bind[0].is_null = nullptr;
    
    // 主体ID参数
    std::string subject_id = triple->getSubject()->getId();
    unsigned long subject_id_length = subject_id.length();
    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (void*)subject_id.c_str();
    bind[1].buffer_length = subject_id_length;
    bind[1].length = &subject_id_length;
    bind[1].is_null = nullptr;
    
    // 关系ID参数
    std::string relation_id = triple->getRelation()->getId();
    unsigned long relation_id_length = relation_id.length();
    bind[2].buffer_type = MYSQL_TYPE_STRING;
    bind[2].buffer = (void*)relation_id.c_str();
    bind[2].buffer_length = relation_id_length;
    bind[2].length = &relation_id_length;
    bind[2].is_null = nullptr;
    
    // 客体ID参数
    std::string object_id = triple->getObject()->getId();
    unsigned long object_id_length = object_id.length();
    bind[3].buffer_type = MYSQL_TYPE_STRING;
    bind[3].buffer = (void*)object_id.c_str();
    bind[3].buffer_length = object_id_length;
    bind[3].length = &object_id_length;
    bind[3].is_null = nullptr;
    
    // 置信度参数
    float confidence = triple->getConfidence();
    bind[4].buffer_type = MYSQL_TYPE_FLOAT;
    bind[4].buffer = &confidence;
    bind[4].is_null = nullptr;
    
    // 绑定参数
    if (mysql_stmt_bind_param(statements_->saveTriple, bind) != 0) {
        logError("绑定参数失败: " + std::string(mysql_stmt_error(statements_->saveTriple)));
        return false;
    }
    
    // 执行语句
    if (mysql_stmt_execute(statements_->saveTriple) != 0) {
        logError("执行语句失败: " + std::string(mysql_stmt_error(statements_->saveTriple)));
        return false;
    }
    
    // 保存三元组属性
    if (!triple->getAllProperties().empty() && !saveTripleProperties(id, triple->getAllProperties())) {
        LOG_WARN("保存三元组属性失败: {}", id);
        // 继续执行，不返回错误
    }
    
    LOG_INFO("三元组保存成功: {}", id);
    return true;
}

// 批量保存三元组
size_t MySQLStorage::saveTriples(const std::vector<knowledge::TriplePtr>& triples) {
    if (triples.empty()) {
        return 0;
    }
    
    // 启动事务
    bool needsCommit = !isInTransaction_;
    if (needsCommit && !beginTransaction()) {
        LOG_ERROR("开始事务失败");
        return 0;
    }
    
    size_t successCount = 0;
    for (const auto& triple : triples) {
        if (saveTriple(triple)) {
            successCount++;
        }
    }
    
    // 提交事务
    if (needsCommit) {
        if (successCount == triples.size()) {
            if (!commitTransaction()) {
                LOG_ERROR("提交事务失败");
                return successCount;
            }
        } else {
            if (!rollbackTransaction()) {
                LOG_ERROR("回滚事务失败");
            }
            return 0;  // 如果有任何失败且事务回滚，则返回0
        }
    }
    
    return successCount;
}

// 保存三元组属性
bool MySQLStorage::saveTripleProperties(
    const std::string& tripleId,
    const std::unordered_map<std::string, std::string>& properties) {
    
    if (properties.empty()) {
        return true;
    }
    
    // 删除现有属性
    if (!executeSQL("DELETE FROM `triple_properties` WHERE `triple_id` = '" + tripleId + "'")) {
        return false;
    }
    
    // 插入新属性
    for (const auto& prop : properties) {
        std::string key = prop.first;
        std::string value = prop.second;
        
        // 转义特殊字符
        char* escaped_key = new char[key.length() * 2 + 1];
        char* escaped_value = new char[value.length() * 2 + 1];
        
        mysql_real_escape_string(mysql_, escaped_key, key.c_str(), key.length());
        mysql_real_escape_string(mysql_, escaped_value, value.c_str(), value.length());
        
        std::string sql = "INSERT INTO `triple_properties` (`triple_id`, `key`, `value`) VALUES ('" 
                        + tripleId + "', '" + escaped_key + "', '" + escaped_value + "')";
        
        delete[] escaped_key;
        delete[] escaped_value;
        
        if (!executeSQL(sql)) {
            return false;
        }
    }
    
    return true;
}

// 加载三元组属性
std::unordered_map<std::string, std::string> MySQLStorage::loadTripleProperties(
    const std::string& tripleId) {
    
    std::unordered_map<std::string, std::string> properties;
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!mysql_ || !statements_->loadTripleProperties) {
        logError("未连接到 MySQL 服务器或预处理语句未准备");
        return properties;
    }
    
    // 绑定参数
    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));
    
    // ID参数
    unsigned long id_length = tripleId.length();
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)tripleId.c_str();
    bind[0].buffer_length = id_length;
    bind[0].length = &id_length;
    
    // 绑定参数
    if (mysql_stmt_bind_param(statements_->loadTripleProperties, bind) != 0) {
        logError("绑定参数失败: " + std::string(mysql_stmt_error(statements_->loadTripleProperties)));
        return properties;
    }
    
    // 执行查询
    if (mysql_stmt_execute(statements_->loadTripleProperties) != 0) {
        logError("执行查询失败: " + std::string(mysql_stmt_error(statements_->loadTripleProperties)));
        return properties;
    }
    
    // 准备结果集
    MYSQL_BIND result[2];
    memset(result, 0, sizeof(result));
    
    // 键字段
    char key_buffer[256];
    unsigned long key_length;
    bool key_is_null;
    result[0].buffer_type = MYSQL_TYPE_STRING;
    result[0].buffer = key_buffer;
    result[0].buffer_length = sizeof(key_buffer);
    result[0].length = &key_length;
    result[0].is_null = &key_is_null;
    
    // 值字段
    char value_buffer[4096];
    unsigned long value_length;
    bool value_is_null;
    result[1].buffer_type = MYSQL_TYPE_STRING;
    result[1].buffer = value_buffer;
    result[1].buffer_length = sizeof(value_buffer);
    result[1].length = &value_length;
    result[1].is_null = &value_is_null;
    
    // 绑定结果集
    if (mysql_stmt_bind_result(statements_->loadTripleProperties, result) != 0) {
        logError("绑定结果集失败: " + std::string(mysql_stmt_error(statements_->loadTripleProperties)));
        return properties;
    }
    
    // 读取结果
    while (mysql_stmt_fetch(statements_->loadTripleProperties) == 0) {
        if (!key_is_null && !value_is_null) {
            std::string key(key_buffer, key_length);
            std::string value(value_buffer, value_length);
            properties[key] = value;
        }
    }
    
    // 关闭结果集
    mysql_stmt_free_result(statements_->loadTripleProperties);
    
    return properties;
}

// 加载三元组
knowledge::TriplePtr MySQLStorage::loadTriple(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!mysql_ || !statements_->loadTriple) {
        logError("未连接到 MySQL 服务器或预处理语句未准备");
        return nullptr;
    }
    
    // 绑定参数
    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));
    
    // ID参数
    unsigned long id_length = id.length();
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)id.c_str();
    bind[0].buffer_length = id_length;
    bind[0].length = &id_length;
    
    // 绑定参数
    if (mysql_stmt_bind_param(statements_->loadTriple, bind) != 0) {
        logError("绑定参数失败: " + std::string(mysql_stmt_error(statements_->loadTriple)));
        return nullptr;
    }
    
    // 执行查询
    if (mysql_stmt_execute(statements_->loadTriple) != 0) {
        logError("执行查询失败: " + std::string(mysql_stmt_error(statements_->loadTriple)));
        return nullptr;
    }
    
    // 准备结果集
    MYSQL_BIND result[11];
    memset(result, 0, sizeof(result));
    
    // 结果字段
    char triple_id_buffer[256];
    unsigned long triple_id_length;
    bool triple_id_is_null;
    result[0].buffer_type = MYSQL_TYPE_STRING;
    result[0].buffer = triple_id_buffer;
    result[0].buffer_length = sizeof(triple_id_buffer);
    result[0].length = &triple_id_length;
    result[0].is_null = &triple_id_is_null;
    
    char subject_id_buffer[256];
    unsigned long subject_id_length;
    bool subject_id_is_null;
    result[1].buffer_type = MYSQL_TYPE_STRING;
    result[1].buffer = subject_id_buffer;
    result[1].buffer_length = sizeof(subject_id_buffer);
    result[1].length = &subject_id_length;
    result[1].is_null = &subject_id_is_null;
    
    char relation_id_buffer[256];
    unsigned long relation_id_length;
    bool relation_id_is_null;
    result[2].buffer_type = MYSQL_TYPE_STRING;
    result[2].buffer = relation_id_buffer;
    result[2].buffer_length = sizeof(relation_id_buffer);
    result[2].length = &relation_id_length;
    result[2].is_null = &relation_id_is_null;
    
    char object_id_buffer[256];
    unsigned long object_id_length;
    bool object_id_is_null;
    result[3].buffer_type = MYSQL_TYPE_STRING;
    result[3].buffer = object_id_buffer;
    result[3].buffer_length = sizeof(object_id_buffer);
    result[3].length = &object_id_length;
    result[3].is_null = &object_id_is_null;
    
    float confidence;
    bool confidence_is_null;
    result[4].buffer_type = MYSQL_TYPE_FLOAT;
    result[4].buffer = &confidence;
    result[4].is_null = &confidence_is_null;
    
    char subject_name_buffer[1024];
    unsigned long subject_name_length;
    bool subject_name_is_null;
    result[5].buffer_type = MYSQL_TYPE_STRING;
    result[5].buffer = subject_name_buffer;
    result[5].buffer_length = sizeof(subject_name_buffer);
    result[5].length = &subject_name_length;
    result[5].is_null = &subject_name_is_null;
    
    int subject_type;
    bool subject_type_is_null;
    result[6].buffer_type = MYSQL_TYPE_LONG;
    result[6].buffer = &subject_type;
    result[6].is_null = &subject_type_is_null;
    
    char relation_name_buffer[1024];
    unsigned long relation_name_length;
    bool relation_name_is_null;
    result[7].buffer_type = MYSQL_TYPE_STRING;
    result[7].buffer = relation_name_buffer;
    result[7].buffer_length = sizeof(relation_name_buffer);
    result[7].length = &relation_name_length;
    result[7].is_null = &relation_name_is_null;
    
    int relation_type;
    bool relation_type_is_null;
    result[8].buffer_type = MYSQL_TYPE_LONG;
    result[8].buffer = &relation_type;
    result[8].is_null = &relation_type_is_null;
    
    char object_name_buffer[1024];
    unsigned long object_name_length;
    bool object_name_is_null;
    result[9].buffer_type = MYSQL_TYPE_STRING;
    result[9].buffer = object_name_buffer;
    result[9].buffer_length = sizeof(object_name_buffer);
    result[9].length = &object_name_length;
    result[9].is_null = &object_name_is_null;
    
    int object_type;
    bool object_type_is_null;
    result[10].buffer_type = MYSQL_TYPE_LONG;
    result[10].buffer = &object_type;
    result[10].is_null = &object_type_is_null;
    
    // 绑定结果集
    if (mysql_stmt_bind_result(statements_->loadTriple, result) != 0) {
        logError("绑定结果集失败: " + std::string(mysql_stmt_error(statements_->loadTriple)));
        return nullptr;
    }
    
    // 获取结果
    if (mysql_stmt_fetch(statements_->loadTriple) != 0) {
        if (mysql_stmt_errno(statements_->loadTriple) != 0) {
            logError("获取结果失败: " + std::string(mysql_stmt_error(statements_->loadTriple)));
        } else {
            LOG_INFO("三元组不存在: {}", id);
        }
        mysql_stmt_free_result(statements_->loadTriple);
        return nullptr;
    }
    
    // 关闭结果集
    mysql_stmt_free_result(statements_->loadTriple);
    
    // 创建三元组对象
    if (triple_id_is_null || subject_id_is_null || relation_id_is_null || 
        object_id_is_null || confidence_is_null || subject_name_is_null || 
        subject_type_is_null || relation_name_is_null || relation_type_is_null || 
        object_name_is_null || object_type_is_null) {
        LOG_ERROR("加载的三元组包含空值: {}", id);
        return nullptr;
    }
    
    // 构造三元组组成部分
    std::string triple_id_str(triple_id_buffer, triple_id_length);
    std::string subject_id_str(subject_id_buffer, subject_id_length);
    std::string relation_id_str(relation_id_buffer, relation_id_length);
    std::string object_id_str(object_id_buffer, object_id_length);
    std::string subject_name_str(subject_name_buffer, subject_name_length);
    std::string relation_name_str(relation_name_buffer, relation_name_length);
    std::string object_name_str(object_name_buffer, object_name_length);
    
    // 创建实体和关系对象
    knowledge::EntityType subject_entity_type = static_cast<knowledge::EntityType>(subject_type);
    knowledge::RelationType relation_rel_type = static_cast<knowledge::RelationType>(relation_type);
    knowledge::EntityType object_entity_type = static_cast<knowledge::EntityType>(object_type);
    
    auto subject_entity = std::make_shared<knowledge::Entity>(
        subject_id_str, subject_name_str, subject_entity_type);
    
    auto relation_rel = std::make_shared<knowledge::Relation>(
        relation_id_str, relation_name_str, relation_rel_type);
    
    auto object_entity = std::make_shared<knowledge::Entity>(
        object_id_str, object_name_str, object_entity_type);
    
    // 创建三元组
    auto triple = std::make_shared<knowledge::Triple>(
         subject_entity, relation_rel, object_entity, confidence);
    
    // 加载三元组属性
    auto properties = loadTripleProperties(triple_id_str);
    for (const auto& prop : properties) {
        triple->addProperty(prop.first, prop.second);
    }
    
    LOG_INFO("三元组加载成功: {}", triple_id_str);
    return triple;
}

// 查询三元组
std::vector<knowledge::TriplePtr> MySQLStorage::queryTriples(
    const std::string& subjectId,
    const std::string& relationId,
    const std::string& objectId,
    const std::unordered_map<std::string, std::string>& properties,
    float minConfidence,
    size_t limit) {
    
    std::vector<knowledge::TriplePtr> triples;
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!mysql_) {
        logError("未连接到 MySQL 服务器");
        return triples;
    }
    
    // 构建查询
    std::string sql = "SELECT t.`id` FROM `triples` t";
    
    // 添加属性表连接（如果需要）
    if (!properties.empty()) {
        sql += " INNER JOIN `triple_properties` tp ON t.`id` = tp.`triple_id`";
    }
    
    // 构建 WHERE 子句
    std::vector<std::string> conditions;
    
    if (!subjectId.empty()) {
        // 转义ID
        char* escaped_subject_id = new char[subjectId.length() * 2 + 1];
        mysql_real_escape_string(mysql_, escaped_subject_id, subjectId.c_str(), subjectId.length());
        conditions.push_back("t.`subject_id` = '" + std::string(escaped_subject_id) + "'");
        delete[] escaped_subject_id;
    }
    
    if (!relationId.empty()) {
        // 转义ID
        char* escaped_relation_id = new char[relationId.length() * 2 + 1];
        mysql_real_escape_string(mysql_, escaped_relation_id, relationId.c_str(), relationId.length());
        conditions.push_back("t.`relation_id` = '" + std::string(escaped_relation_id) + "'");
        delete[] escaped_relation_id;
    }
    
    if (!objectId.empty()) {
        // 转义ID
        char* escaped_object_id = new char[objectId.length() * 2 + 1];
        mysql_real_escape_string(mysql_, escaped_object_id, objectId.c_str(), objectId.length());
        conditions.push_back("t.`object_id` = '" + std::string(escaped_object_id) + "'");
        delete[] escaped_object_id;
    }
    
    if (minConfidence > 0.0f) {
        conditions.push_back("t.`confidence` >= " + std::to_string(minConfidence));
    }
    
    // 添加属性条件
    if (!properties.empty()) {
        std::vector<std::string> propertyConditions;
        for (const auto& prop : properties) {
            char* escaped_key = new char[prop.first.length() * 2 + 1];
            char* escaped_value = new char[prop.second.length() * 2 + 1];
            
            mysql_real_escape_string(mysql_, escaped_key, prop.first.c_str(), prop.first.length());
            mysql_real_escape_string(mysql_, escaped_value, prop.second.c_str(), prop.second.length());
            
            propertyConditions.push_back(
                "(tp.`key` = '" + std::string(escaped_key) + "' AND tp.`value` = '" + 
                std::string(escaped_value) + "')");
            
            delete[] escaped_key;
            delete[] escaped_value;
        }
        
        if (!propertyConditions.empty()) {
            conditions.push_back("(" + join(propertyConditions, " OR ") + ")");
        }
        
        // 分组以确保所有条件都满足
        sql += " GROUP BY t.`id` HAVING COUNT(DISTINCT tp.`key`) >= " + 
               std::to_string(properties.size());
    }
    
    // 添加 WHERE 子句
    if (!conditions.empty()) {
        sql += " WHERE " + join(conditions, " AND ");
    }
    
    // 添加 LIMIT 子句
    if (limit > 0) {
        sql += " LIMIT " + std::to_string(limit);
    }
    
    // 执行查询
    if (mysql_query(mysql_, sql.c_str()) != 0) {
        logError("执行查询失败: " + std::string(mysql_error(mysql_)));
        return triples;
    }
    
    // 获取结果
    MYSQL_RES* result = mysql_store_result(mysql_);
    if (!result) {
        logError("获取结果失败: " + std::string(mysql_error(mysql_)));
        return triples;
    }
    
    // 处理结果
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        if (row[0]) {
            std::string triple_id = row[0];
            auto triple = loadTriple(triple_id);
            if (triple) {
                triples.push_back(triple);
            }
        }
    }
    
    // 释放结果集
    mysql_free_result(result);
    
    LOG_INFO("查询到 {} 个三元组", triples.size());
    return triples;
}

// 删除三元组
bool MySQLStorage::deleteTriple(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!mysql_ || !statements_->deleteTriple) {
        logError("未连接到 MySQL 服务器或预处理语句未准备");
        return false;
    }
    
    // 绑定参数
    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));
    
    // ID参数
    unsigned long id_length = id.length();
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)id.c_str();
    bind[0].buffer_length = id_length;
    bind[0].length = &id_length;
    
    // 绑定参数
    if (mysql_stmt_bind_param(statements_->deleteTriple, bind) != 0) {
        logError("绑定参数失败: " + std::string(mysql_stmt_error(statements_->deleteTriple)));
        return false;
    }
    
    // 执行语句
    if (mysql_stmt_execute(statements_->deleteTriple) != 0) {
        logError("执行语句失败: " + std::string(mysql_stmt_error(statements_->deleteTriple)));
        return false;
    }
    
    // 检查是否有行被影响
    int affected_rows = mysql_stmt_affected_rows(statements_->deleteTriple);
    if (affected_rows == 0) {
        LOG_WARN("没有找到要删除的三元组: {}", id);
        return false;
    }
    
    LOG_INFO("三元组删除成功: {}", id);
    return true;
}

// 保存知识图谱
bool MySQLStorage::saveKnowledgeGraph(
    const knowledge::KnowledgeGraphPtr& graph,
    const std::string& saveName) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!mysql_) {
        logError("未连接到 MySQL 服务器");
        return false;
    }
    
    if (!graph) {
        logError("知识图谱对象为空");
        return false;
    }
    
    // 使用提供的保存名称或图谱自己的名称
    std::string graphName = saveName.empty() ? graph->getName() : saveName;
    
    // 转义图谱名称
    char* escaped_name = new char[graphName.length() * 2 + 1];
    mysql_real_escape_string(mysql_, escaped_name, graphName.c_str(), graphName.length());
    std::string escaped_graph_name(escaped_name);
    delete[] escaped_name;
    
    // 开始事务
    bool needsCommit = !isInTransaction_;
    if (needsCommit && !beginTransaction()) {
        LOG_ERROR("开始事务失败");
        return false;
    }
    
    // 先保存图谱基本信息
    std::string description = ""; // 知识图谱描述，目前暂时为空
    char* escaped_description = new char[description.length() * 2 + 1];
    mysql_real_escape_string(mysql_, escaped_description, description.c_str(), description.length());
    
    std::string sql = "INSERT INTO `knowledge_graphs` (`name`, `description`) VALUES ('" 
                    + escaped_graph_name + "', '" + escaped_description + "') "
                    + "ON DUPLICATE KEY UPDATE `description` = VALUES(`description`)";
    
    delete[] escaped_description;
    
    if (!executeSQL(sql)) {
        if (needsCommit) {
            rollbackTransaction();
        }
        return false;
    }
    
    // 删除旧的图谱关联
    if (!executeSQL("DELETE FROM `knowledge_graph_triples` WHERE `graph_name` = '" + escaped_graph_name + "'")) {
        if (needsCommit) {
            rollbackTransaction();
        }
        return false;
    }
    
    // 保存所有三元组
    const auto& allTriples = graph->getAllTriples();
    for (const auto& triple : allTriples) {
        // 保存三元组
        if (!saveTriple(triple)) {
            LOG_WARN("保存三元组失败: {}", triple->getId());
            continue;
        }
        
        // 将三元组与图谱关联
        sql = "INSERT INTO `knowledge_graph_triples` (`graph_name`, `triple_id`) VALUES ('" 
            + escaped_graph_name + "', '" + triple->getId() + "')";
        
        if (!executeSQL(sql)) {
            LOG_WARN("关联三元组到图谱失败: {}", triple->getId());
            // 继续处理其他三元组
        }
    }
    
    // 提交事务
    if (needsCommit && !commitTransaction()) {
        LOG_ERROR("提交事务失败");
        return false;
    }
    
    LOG_INFO("保存知识图谱成功: {}, 包含 {} 个三元组", graphName, allTriples.size());
    return true;
}

// 加载知识图谱
knowledge::KnowledgeGraphPtr MySQLStorage::loadKnowledgeGraph(const std::string& graphName) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!mysql_) {
        logError("未连接到 MySQL 服务器");
        return nullptr;
    }
    
    // 转义图谱名称
    char* escaped_name = new char[graphName.length() * 2 + 1];
    mysql_real_escape_string(mysql_, escaped_name, graphName.c_str(), graphName.length());
    std::string escaped_graph_name(escaped_name);
    delete[] escaped_name;
    
    // 查询图谱基本信息
    std::string sql = "SELECT `name`, `description` FROM `knowledge_graphs` WHERE `name` = '" 
                    + escaped_graph_name + "'";
    
    if (mysql_query(mysql_, sql.c_str()) != 0) {
        logError("执行查询失败: " + std::string(mysql_error(mysql_)));
        return nullptr;
    }
    
    MYSQL_RES* result = mysql_store_result(mysql_);
    if (!result) {
        logError("获取结果失败: " + std::string(mysql_error(mysql_)));
        return nullptr;
    }
    
    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row) {
        mysql_free_result(result);
        LOG_WARN("知识图谱不存在: {}", graphName);
        return nullptr;
    }
    
    // 创建知识图谱对象
    std::string name = row[0] ? row[0] : "";
    // 注意：描述字段存在，但KnowledgeGraph类没有相应的设置方法，所以这里忽略这个字段
    
    mysql_free_result(result);
    
    auto graph = std::make_shared<knowledge::KnowledgeGraph>(name);
    // 不再设置描述，因为没有相应的方法
    
    // 查询关联的三元组
    sql = "SELECT gt.`triple_id` FROM `knowledge_graph_triples` gt "
          "INNER JOIN `triples` t ON gt.`triple_id` = t.`id` "
          "WHERE gt.`graph_name` = '" + escaped_graph_name + "'";
    
    if (mysql_query(mysql_, sql.c_str()) != 0) {
        logError("执行查询失败: " + std::string(mysql_error(mysql_)));
        return graph;  // 返回空图谱，不加载三元组
    }
    
    result = mysql_store_result(mysql_);
    if (!result) {
        logError("获取结果失败: " + std::string(mysql_error(mysql_)));
        return graph;  // 返回空图谱，不加载三元组
    }
    
    // 加载所有三元组
    int tripleCount = 0;
    while ((row = mysql_fetch_row(result))) {
        if (row[0]) {
            std::string triple_id = row[0];
            auto triple = loadTriple(triple_id);
            if (triple) {
                graph->addTriple(triple);
                tripleCount++;
            }
        }
    }
    
    mysql_free_result(result);
    
    LOG_INFO("加载知识图谱成功: {}, 包含 {} 个三元组", name, tripleCount);
    return graph;
}

// 删除知识图谱
bool MySQLStorage::deleteKnowledgeGraph(const std::string& graphName) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!mysql_) {
        logError("未连接到 MySQL 服务器");
        return false;
    }
    
    // 转义图谱名称
    char* escaped_name = new char[graphName.length() * 2 + 1];
    mysql_real_escape_string(mysql_, escaped_name, graphName.c_str(), graphName.length());
    std::string escaped_graph_name(escaped_name);
    delete[] escaped_name;
    
    // 开始事务
    bool needsCommit = !isInTransaction_;
    if (needsCommit && !beginTransaction()) {
        LOG_ERROR("开始事务失败");
        return false;
    }
    
    // 删除图谱关联关系
    std::string sql = "DELETE FROM `knowledge_graph_triples` WHERE `graph_name` = '" 
                    + escaped_graph_name + "'";
    
    if (!executeSQL(sql)) {
        if (needsCommit) {
            rollbackTransaction();
        }
        return false;
    }
    
    // 删除图谱基本信息
    sql = "DELETE FROM `knowledge_graphs` WHERE `name` = '" + escaped_graph_name + "'";
    
    if (!executeSQL(sql)) {
        if (needsCommit) {
            rollbackTransaction();
        }
        return false;
    }
    
    // 检查是否有行被影响
    int affected_rows = mysql_affected_rows(mysql_);
    if (affected_rows == 0) {
        LOG_WARN("没有找到要删除的知识图谱: {}", graphName);
        if (needsCommit) {
            rollbackTransaction();
        }
        return false;
    }
    
    // 提交事务
    if (needsCommit && !commitTransaction()) {
        LOG_ERROR("提交事务失败");
        return false;
    }
    
    LOG_INFO("删除知识图谱成功: {}", graphName);
    return true;
}

// 列出所有知识图谱
std::vector<std::string> MySQLStorage::listKnowledgeGraphs() {
    std::vector<std::string> graphNames;
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!mysql_) {
        logError("未连接到 MySQL 服务器");
        return graphNames;
    }
    
    // 执行查询
    std::string sql = "SELECT `name` FROM `knowledge_graphs` ORDER BY `name`";
    
    if (mysql_query(mysql_, sql.c_str()) != 0) {
        logError("执行查询失败: " + std::string(mysql_error(mysql_)));
        return graphNames;
    }
    
    // 获取结果
    MYSQL_RES* result = mysql_store_result(mysql_);
    if (!result) {
        logError("获取结果失败: " + std::string(mysql_error(mysql_)));
        return graphNames;
    }
    
    // 处理结果
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        if (row[0]) {
            graphNames.push_back(row[0]);
        }
    }
    
    // 释放结果集
    mysql_free_result(result);
    
    LOG_INFO("列出 {} 个知识图谱", graphNames.size());
    return graphNames;
}

// 创建或更新数据库架构
bool MySQLStorage::createSchema(bool force) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!mysql_) {
        logError("未连接到 MySQL 服务器");
        return false;
    }
    
    // 如果要强制重建，先删除现有表
    if (force) {
        if (!dropTables()) {
            return false;
        }
    }
    
    // 创建表结构
    if (!createTables()) {
        return false;
    }
    
    LOG_INFO("数据库架构创建/更新成功");
    return true;
}

} // namespace storage
} // namespace kb 