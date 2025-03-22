/**
 * @file config.h
 * @brief 配置管理类头文件
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace kb {
namespace common {

/**
 * @brief 日志配置结构
 */
struct LoggingConfig {
    std::string level;           ///< 日志级别（debug, info, warn, error, fatal）
    std::string file;            ///< 日志文件路径
    std::string consoleLevel;    ///< 控制台日志级别
    size_t maxSize;              ///< 单个日志文件最大大小（字节）
    int maxFiles;                ///< 最大日志文件数量
    bool colorOutput;            ///< 是否启用彩色输出
};

/**
 * @brief 模型配置结构
 */
struct ModelsConfig {
    std::string directory;                              ///< 模型目录路径
    std::string defaultModel;                           ///< 默认模型路径
    std::vector<std::string> available;                 ///< 可用模型列表
    std::unordered_map<std::string, std::string> nameMap;  ///< 模型名称映射表（友好名称到路径）
};

/**
 * @brief 生成配置结构
 */
struct GenerationConfig {
    int contextSize;                   ///< 上下文大小
    int threads;                       ///< 推理使用的线程数
    int gpuLayers;                     ///< 使用GPU的层数
    float temperature;                 ///< 温度参数
    float topP;                        ///< 取样阈值
    int topK;                          ///< 取样候选数
    float repeatPenalty;               ///< 重复惩罚系数
    int maxTokens;                     ///< 最大生成令牌数
    bool streamOutput;                 ///< 是否使用流式输出
    std::vector<std::string> stopSequences;  ///< 停止序列集合
    std::string systemPrompt;          ///< 系统提示词
};

/**
 * @brief 应用配置结构
 */
struct AppConfig {
    std::string historyDir;            ///< 聊天历史保存目录
    bool saveHistory;                  ///< 是否保存聊天历史
    int maxHistoryFiles;               ///< 最大历史文件数量
    int autoSaveInterval;              ///< 自动保存间隔（秒）
    std::string welcomeMessage;        ///< 欢迎消息
    std::string promptPrefix;          ///< 提示前缀
    std::string responsePrefix;        ///< 响应前缀
    std::string locale;                ///< 区域设置（例如 zh_CN.UTF-8）
};

/**
 * @brief 高级配置结构
 */
struct AdvancedConfig {
    std::string kvCacheType;           ///< KV缓存类型（auto, f16, q4_0等）
    int batchSize;                     ///< 批处理大小
    int chunkSize;                     ///< 分块大小
    bool dynamicPromptHandling;        ///< 是否启用动态提示词处理
    bool enableFunctionCalling;        ///< 是否启用函数调用
    std::string functionCallingFormat; ///< 函数调用格式（json, xml等）
    int memoryLimitMB;                 ///< 内存限制（MB，0表示无限制）
    float ropeFreqBase;                ///< RoPE频率基数
    float ropeFreqScale;               ///< RoPE频率缩放
};

/**
 * @brief 服务器配置结构
 */
struct ServerConfig {
    bool enabled;                      ///< 是否启用服务器
    std::string host;                  ///< 主机地址
    int port;                          ///< 端口号
    std::vector<std::string> corsOrigins;  ///< CORS起源列表
    bool authEnabled;                  ///< 是否启用认证
    std::string authToken;             ///< 认证令牌
    int requestTimeout;                ///< 请求超时（秒）
    int maxRequestTokens;              ///< 最大请求令牌数
};

/**
 * @brief 数据库配置结构
 */
struct DatabaseConfig {
    std::string host;            ///< 数据库主机地址
    int port;                    ///< 数据库端口
    std::string user;            ///< 数据库用户名
    std::string password;        ///< 数据库密码
    std::string database;        ///< 数据库名称
    std::string charset;         ///< 字符集
    int connectTimeout;          ///< 连接超时时间（秒）
    int readTimeout;             ///< 读取超时时间（秒）
    int writeTimeout;            ///< 写入超时时间（秒）
    int maxConnections;          ///< 最大连接数
    bool autoReconnect;          ///< 是否自动重连
    std::string connectionString; ///< 连接字符串（如果设置将覆盖其他选项）
};

/**
 * @brief 配置管理类，使用单例模式
 */
class Config {
public:
    /**
     * @brief 获取配置实例
     * @return 配置实例的引用
     */
    static Config& getInstance();

    /**
     * @brief 从文件加载配置
     * @param filePath 配置文件路径
     * @return 是否成功加载配置
     */
    bool loadFromFile(const std::string& filePath);

    /**
     * @brief 尝试加载默认配置文件
     * @return 是否成功加载配置
     */
    bool loadDefault();

    /**
     * @brief 保存配置到文件
     * @param filePath 配置文件路径
     * @return 是否成功保存配置
     */
    bool saveToFile(const std::string& filePath) const;

    /**
     * @brief 扫描模型目录
     * 扫描指定目录并更新可用模型列表
     */
    void scanModelDirectory();

    // =============== 日志配置相关方法 ===============
    /**
     * @brief 获取日志配置
     * @return 日志配置结构的引用
     */
    const LoggingConfig& getLoggingConfig() const;

    /**
     * @brief 获取日志级别
     * @return 日志级别
     */
    std::string getLogLevel() const;

    /**
     * @brief 获取日志文件路径
     * @return 日志文件路径
     */
    std::string getLogFilePath() const;

    /**
     * @brief 获取控制台日志级别
     * @return 控制台日志级别
     */
    std::string getConsoleLogLevel() const;

    /**
     * @brief 获取日志文件最大大小
     * @return 日志文件最大大小（字节）
     */
    size_t getLogMaxSize() const;

    /**
     * @brief 获取最大日志文件数量
     * @return 最大日志文件数量
     */
    int getLogMaxFiles() const;

    /**
     * @brief 获取日志彩色输出设置
     * @return 是否启用彩色输出
     */
    bool getLogColorOutput() const;

    // =============== 模型配置相关方法 ===============
    /**
     * @brief 获取模型配置
     * @return 模型配置结构的引用
     */
    const ModelsConfig& getModelsConfig() const;

    /**
     * @brief 获取模型目录
     * @return 模型目录路径
     */
    std::string getModelDir() const;

    /**
     * @brief 获取默认模型路径
     * @return 默认模型路径
     */
    std::string getDefaultModelPath() const;

    /**
     * @brief 获取可用模型列表
     * @return 可用模型路径列表
     */
    std::vector<std::string> getAvailableModels() const;

    /**
     * @brief 获取模型名称映射
     * @return 模型名称到路径的映射
     */
    std::unordered_map<std::string, std::string> getModelNameMap() const;

    // =============== 生成配置相关方法 ===============
    /**
     * @brief 获取生成配置
     * @return 生成配置结构的引用
     */
    const GenerationConfig& getGenerationConfig() const;

    /**
     * @brief 获取上下文大小
     * @return 上下文大小
     */
    int getContextSize() const;

    /**
     * @brief 获取线程数
     * @return 线程数
     */
    int getThreads() const;

    /**
     * @brief 获取GPU层数
     * @return GPU层数
     */
    int getGpuLayers() const;

    /**
     * @brief 获取温度参数
     * @return 温度参数
     */
    float getTemperature() const;

    /**
     * @brief 获取topP参数
     * @return topP参数
     */
    float getTopP() const;

    /**
     * @brief 获取topK参数
     * @return topK参数
     */
    int getTopK() const;

    /**
     * @brief 获取重复惩罚系数
     * @return 重复惩罚系数
     */
    float getRepeatPenalty() const;

    /**
     * @brief 获取最大生成令牌数
     * @return 最大生成令牌数
     */
    int getMaxTokens() const;

    /**
     * @brief 获取流式输出设置
     * @return 是否使用流式输出
     */
    bool getStreamOutput() const;

    /**
     * @brief 获取停止序列
     * @return 停止序列列表
     */
    std::vector<std::string> getStopSequences() const;

    /**
     * @brief 获取系统提示词
     * @return 系统提示词
     */
    std::string getSystemPrompt() const;

    // =============== 应用配置相关方法 ===============
    /**
     * @brief 获取应用配置
     * @return 应用配置结构的引用
     */
    const AppConfig& getAppConfig() const;

    /**
     * @brief 获取历史目录
     * @return 历史目录路径
     */
    std::string getHistoryDir() const;

    /**
     * @brief 获取保存历史设置
     * @return 是否保存历史
     */
    bool getSaveHistory() const;

    /**
     * @brief 获取最大历史文件数
     * @return 最大历史文件数
     */
    int getMaxHistoryFiles() const;

    /**
     * @brief 获取自动保存间隔
     * @return 自动保存间隔（秒）
     */
    int getAutoSaveInterval() const;

    /**
     * @brief 获取欢迎消息
     * @return 欢迎消息
     */
    std::string getWelcomeMessage() const;

    /**
     * @brief 获取提示前缀
     * @return 提示前缀
     */
    std::string getPromptPrefix() const;

    /**
     * @brief 获取响应前缀
     * @return 响应前缀
     */
    std::string getResponsePrefix() const;

    /**
     * @brief 获取区域设置
     * @return 区域设置
     */
    std::string getLocale() const;

    // =============== 高级配置相关方法 ===============
    /**
     * @brief 获取高级配置
     * @return 高级配置结构的引用
     */
    const AdvancedConfig& getAdvancedConfig() const;

    /**
     * @brief 获取KV缓存类型
     * @return KV缓存类型
     */
    std::string getKvCacheType() const;

    /**
     * @brief 获取批处理大小
     * @return 批处理大小
     */
    int getBatchSize() const;

    /**
     * @brief 获取分块大小
     * @return 分块大小
     */
    int getChunkSize() const;

    /**
     * @brief 获取动态提示词处理设置
     * @return 是否启用动态提示词处理
     */
    bool getDynamicPromptHandling() const;

    /**
     * @brief 获取函数调用设置
     * @return 是否启用函数调用
     */
    bool getEnableFunctionCalling() const;

    /**
     * @brief 获取函数调用格式
     * @return 函数调用格式
     */
    std::string getFunctionCallingFormat() const;

    /**
     * @brief 获取内存限制
     * @return 内存限制（MB）
     */
    int getMemoryLimitMB() const;

    /**
     * @brief 获取RoPE频率基数
     * @return RoPE频率基数
     */
    float getRopeFreqBase() const;

    /**
     * @brief 获取RoPE频率缩放
     * @return RoPE频率缩放
     */
    float getRopeFreqScale() const;

    // =============== 服务器配置相关方法 ===============
    /**
     * @brief 获取服务器配置
     * @return 服务器配置结构的引用
     */
    const ServerConfig& getServerConfig() const;

    /**
     * @brief 获取服务器启用设置
     * @return 是否启用服务器
     */
    bool getServerEnabled() const;

    /**
     * @brief 获取服务器主机
     * @return 服务器主机
     */
    std::string getServerHost() const;

    /**
     * @brief 获取服务器端口
     * @return 服务器端口
     */
    int getServerPort() const;

    /**
     * @brief 获取CORS起源列表
     * @return CORS起源列表
     */
    std::vector<std::string> getCorsOrigins() const;

    /**
     * @brief 获取认证启用设置
     * @return 是否启用认证
     */
    bool getAuthEnabled() const;

    /**
     * @brief 获取认证令牌
     * @return 认证令牌
     */
    std::string getAuthToken() const;

    /**
     * @brief 获取请求超时
     * @return 请求超时（秒）
     */
    int getRequestTimeout() const;

    /**
     * @brief 获取最大请求令牌数
     * @return 最大请求令牌数
     */
    int getMaxRequestTokens() const;

    // =============== 数据库配置相关方法 ===============
    /**
     * @brief 获取数据库配置
     * @return 数据库配置结构的引用
     */
    const DatabaseConfig& getDatabaseConfig() const;

    /**
     * @brief 获取数据库主机
     * @return 数据库主机地址
     */
    std::string getDatabaseHost() const;

    /**
     * @brief 获取数据库端口
     * @return 数据库端口
     */
    int getDatabasePort() const;

    /**
     * @brief 获取数据库用户名
     * @return 数据库用户名
     */
    std::string getDatabaseUser() const;

    /**
     * @brief 获取数据库密码
     * @return 数据库密码
     */
    std::string getDatabasePassword() const;

    /**
     * @brief 获取数据库名称
     * @return 数据库名称
     */
    std::string getDatabaseName() const;

    /**
     * @brief 获取数据库字符集
     * @return 数据库字符集
     */
    std::string getDatabaseCharset() const;

    /**
     * @brief 获取数据库连接字符串
     * @return 数据库连接字符串
     */
    std::string getDatabaseConnectionString() const;

    /**
     * @brief 获取数据库连接超时
     * @return 连接超时时间（秒）
     */
    int getDatabaseConnectTimeout() const;

    /**
     * @brief 获取数据库读取超时
     * @return 读取超时时间（秒）
     */
    int getDatabaseReadTimeout() const;

    /**
     * @brief 获取数据库写入超时
     * @return 写入超时时间（秒）
     */
    int getDatabaseWriteTimeout() const;

    /**
     * @brief 获取数据库最大连接数
     * @return 最大连接数
     */
    int getDatabaseMaxConnections() const;

    /**
     * @brief 获取数据库自动重连设置
     * @return 是否自动重连
     */
    bool getDatabaseAutoReconnect() const;

private:
    /**
     * @brief 构造函数，私有化以实现单例
     */
    Config();

    /**
     * @brief 拷贝构造函数，禁止使用
     */
    Config(const Config&) = delete;

    /**
     * @brief 赋值运算符，禁止使用
     */
    Config& operator=(const Config&) = delete;

    /**
     * @brief 重置配置为默认值
     */
    void resetToDefaults();

    /**
     * @brief 配置是否已初始化
     */
    bool initialized_;

    /**
     * @brief 线程互斥锁，保证线程安全
     */
    mutable std::mutex mutex_;

    /**
     * @brief 日志配置
     */
    LoggingConfig loggingConfig_;

    /**
     * @brief 模型配置
     */
    ModelsConfig modelsConfig_;

    /**
     * @brief 生成配置
     */
    GenerationConfig generationConfig_;

    /**
     * @brief 应用配置
     */
    AppConfig appConfig_;

    /**
     * @brief 高级配置
     */
    AdvancedConfig advancedConfig_;

    /**
     * @brief 服务器配置
     */
    ServerConfig serverConfig_;

    /**
     * @brief 数据库配置
     */
    DatabaseConfig databaseConfig_;
};

/**
 * @brief 初始化日志系统
 * 根据配置初始化日志系统
 */
void initLogging();

} // namespace common
} // namespace kb 