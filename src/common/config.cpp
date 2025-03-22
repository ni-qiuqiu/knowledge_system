/**
 * @file config.cpp
 * @brief 配置管理类实现
 */

#include "common/config.h"
#include "common/logging.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace kb {
namespace common {

// 使用nlohmann/json进行JSON解析
using json = nlohmann::json;

// 单例实现
Config& Config::getInstance() {
    static Config instance;
    return instance;
}

// 构造函数
Config::Config() : initialized_(false) {
    // 加载默认配置
    resetToDefaults();
    loadDefault();
}

// 重置为默认值
void Config::resetToDefaults() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 日志配置默认值
    loggingConfig_.level = "info";
    loggingConfig_.file = "logs/llama_chat.log";
    loggingConfig_.consoleLevel = "info";
    loggingConfig_.maxSize = 10485760; // 10MB
    loggingConfig_.maxFiles = 5;
    loggingConfig_.colorOutput = true;
    
    // 模型配置默认值
    modelsConfig_.directory = "models";
    modelsConfig_.defaultModel = "";
    modelsConfig_.available.clear();
    modelsConfig_.nameMap.clear();
    
    // 生成配置默认值
    generationConfig_.contextSize = 2048;
    generationConfig_.threads = 4;
    generationConfig_.gpuLayers = 0;
    generationConfig_.temperature = 0.7f;
    generationConfig_.topP = 0.9f;
    generationConfig_.topK = 40;
    generationConfig_.repeatPenalty = 1.1f;
    generationConfig_.maxTokens = 2048;
    generationConfig_.streamOutput = true;
    generationConfig_.stopSequences = {"\n\n\n"};
    generationConfig_.systemPrompt = "你是一个有用的中文AI助手，请简洁、准确地回答问题。";
    
    // 应用配置默认值
    appConfig_.historyDir = "history";
    appConfig_.saveHistory = true;
    appConfig_.maxHistoryFiles = 100;
    appConfig_.autoSaveInterval = 300;
    appConfig_.welcomeMessage = "欢迎使用LLama聊天应用！输入'exit'或'quit'退出。";
    appConfig_.promptPrefix = "用户> ";
    appConfig_.responsePrefix = "助手> ";
    appConfig_.locale = "zh_CN.UTF-8";
    
    // 高级配置默认值
    advancedConfig_.kvCacheType = "auto";
    advancedConfig_.batchSize = 512;
    advancedConfig_.chunkSize = 256;
    advancedConfig_.dynamicPromptHandling = true;
    advancedConfig_.enableFunctionCalling = false;
    advancedConfig_.functionCallingFormat = "json";
    advancedConfig_.memoryLimitMB = 0;
    advancedConfig_.ropeFreqBase = 10000.0f;
    advancedConfig_.ropeFreqScale = 1.0f;
    
    // 服务器配置默认值
    serverConfig_.enabled = false;
    serverConfig_.host = "127.0.0.1";
    serverConfig_.port = 8080;
    serverConfig_.corsOrigins = {"*"};
    serverConfig_.authEnabled = false;
    serverConfig_.authToken = "";
    serverConfig_.requestTimeout = 60;
    serverConfig_.maxRequestTokens = 4096;

    // 数据库配置默认值
    databaseConfig_.host = "localhost";
    databaseConfig_.port = 3306;
    databaseConfig_.user = "root";
    databaseConfig_.password = "";
    databaseConfig_.database = "knowledge_base";
    databaseConfig_.charset = "utf8mb4";
    databaseConfig_.connectTimeout = 10;
    databaseConfig_.readTimeout = 30;
    databaseConfig_.writeTimeout = 30;
    databaseConfig_.maxConnections = 10;
    databaseConfig_.autoReconnect = true;
    databaseConfig_.connectionString = "";
}

// 从文件加载配置
bool Config::loadFromFile(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // 检查文件是否存在
        if (!std::filesystem::exists(filePath)) {
            LOG_WARN("配置文件不存在: {}", filePath);
            return false;
        }
        
        // 读取文件内容
        std::ifstream file(filePath);
        if (!file.is_open()) {
            LOG_ERROR("无法打开配置文件: {}", filePath);
            return false;
        }
        
        // 解析JSON
        json configJson;
        file >> configJson;
        
        // 读取日志配置
        if (configJson.contains("logging")) {
            auto& logging = configJson["logging"];
            if (logging.contains("level")) {
                loggingConfig_.level = logging["level"].get<std::string>();
            }
            if (logging.contains("file")) {
                loggingConfig_.file = logging["file"].get<std::string>();
            }
            if (logging.contains("console_level")) {
                loggingConfig_.consoleLevel = logging["console_level"].get<std::string>();
            }
            if (logging.contains("max_size")) {
                loggingConfig_.maxSize = logging["max_size"].get<size_t>();
            }
            if (logging.contains("max_files")) {
                loggingConfig_.maxFiles = logging["max_files"].get<int>();
            }
            if (logging.contains("color_output")) {
                loggingConfig_.colorOutput = logging["color_output"].get<bool>();
            }
        }
        
        // 读取模型配置
        if (configJson.contains("models")) {
            auto& models = configJson["models"];
            if (models.contains("directory")) {
                modelsConfig_.directory = models["directory"].get<std::string>();
            }
            if (models.contains("default")) {
                modelsConfig_.defaultModel = models["default"].get<std::string>();
            }
            
            // 读取模型列表
            if (models.contains("available") && models["available"].is_array()) {
                modelsConfig_.available.clear();
                for (const auto& modelPath : models["available"]) {
                    modelsConfig_.available.push_back(modelPath.get<std::string>());
                }
            }
            
            // 读取模型名称映射
            if (models.contains("name_map") && models["name_map"].is_object()) {
                modelsConfig_.nameMap.clear();
                for (auto it = models["name_map"].begin(); it != models["name_map"].end(); ++it) {
                    modelsConfig_.nameMap[it.key()] = it.value().get<std::string>();
                }
            }
        }
        
        // 读取生成配置
        if (configJson.contains("generation")) {
            auto& generation = configJson["generation"];
            if (generation.contains("context_size")) {
                generationConfig_.contextSize = generation["context_size"].get<int>();
            }
            if (generation.contains("threads")) {
                generationConfig_.threads = generation["threads"].get<int>();
            }
            if (generation.contains("gpu_layers")) {
                generationConfig_.gpuLayers = generation["gpu_layers"].get<int>();
            }
            if (generation.contains("temperature")) {
                generationConfig_.temperature = generation["temperature"].get<float>();
            }
            if (generation.contains("top_p")) {
                generationConfig_.topP = generation["top_p"].get<float>();
            }
            if (generation.contains("top_k")) {
                generationConfig_.topK = generation["top_k"].get<int>();
            }
            if (generation.contains("repeat_penalty")) {
                generationConfig_.repeatPenalty = generation["repeat_penalty"].get<float>();
            }
            if (generation.contains("max_tokens")) {
                generationConfig_.maxTokens = generation["max_tokens"].get<int>();
            }
            if (generation.contains("stream_output")) {
                generationConfig_.streamOutput = generation["stream_output"].get<bool>();
            }
            if (generation.contains("stop_sequences") && generation["stop_sequences"].is_array()) {
                generationConfig_.stopSequences.clear();
                for (const auto& stop : generation["stop_sequences"]) {
                    generationConfig_.stopSequences.push_back(stop.get<std::string>());
                }
            }
            if (generation.contains("system_prompt")) {
                generationConfig_.systemPrompt = generation["system_prompt"].get<std::string>();
            }
        }
        
        // 读取应用配置
        if (configJson.contains("app")) {
            auto& app = configJson["app"];
            if (app.contains("history_dir")) {
                appConfig_.historyDir = app["history_dir"].get<std::string>();
            }
            if (app.contains("save_history")) {
                appConfig_.saveHistory = app["save_history"].get<bool>();
            }
            if (app.contains("max_history_files")) {
                appConfig_.maxHistoryFiles = app["max_history_files"].get<int>();
            }
            if (app.contains("auto_save_interval")) {
                appConfig_.autoSaveInterval = app["auto_save_interval"].get<int>();
            }
            if (app.contains("welcome_message")) {
                appConfig_.welcomeMessage = app["welcome_message"].get<std::string>();
            }
            if (app.contains("prompt_prefix")) {
                appConfig_.promptPrefix = app["prompt_prefix"].get<std::string>();
            }
            if (app.contains("response_prefix")) {
                appConfig_.responsePrefix = app["response_prefix"].get<std::string>();
            }
            if (app.contains("locale")) {
                appConfig_.locale = app["locale"].get<std::string>();
            }
        }
        
        // 读取高级配置
        if (configJson.contains("advanced")) {
            auto& advanced = configJson["advanced"];
            if (advanced.contains("kv_cache_type")) {
                advancedConfig_.kvCacheType = advanced["kv_cache_type"].get<std::string>();
            }
            if (advanced.contains("batch_size")) {
                advancedConfig_.batchSize = advanced["batch_size"].get<int>();
            }
            if (advanced.contains("chunk_size")) {
                advancedConfig_.chunkSize = advanced["chunk_size"].get<int>();
            }
            if (advanced.contains("dynamic_prompt_handling")) {
                advancedConfig_.dynamicPromptHandling = advanced["dynamic_prompt_handling"].get<bool>();
            }
            if (advanced.contains("enable_function_calling")) {
                advancedConfig_.enableFunctionCalling = advanced["enable_function_calling"].get<bool>();
            }
            if (advanced.contains("function_calling_format")) {
                advancedConfig_.functionCallingFormat = advanced["function_calling_format"].get<std::string>();
            }
            if (advanced.contains("memory_limit_mb")) {
                advancedConfig_.memoryLimitMB = advanced["memory_limit_mb"].get<int>();
            }
            if (advanced.contains("rope_freq_base")) {
                advancedConfig_.ropeFreqBase = advanced["rope_freq_base"].get<float>();
            }
            if (advanced.contains("rope_freq_scale")) {
                advancedConfig_.ropeFreqScale = advanced["rope_freq_scale"].get<float>();
            }
        }
        
        // 读取服务器配置
        if (configJson.contains("server")) {
            auto& server = configJson["server"];
            if (server.contains("enabled")) {
                serverConfig_.enabled = server["enabled"].get<bool>();
            }
            if (server.contains("host")) {
                serverConfig_.host = server["host"].get<std::string>();
            }
            if (server.contains("port")) {
                serverConfig_.port = server["port"].get<int>();
            }
            if (server.contains("cors_origins") && server["cors_origins"].is_array()) {
                serverConfig_.corsOrigins.clear();
                for (const auto& origin : server["cors_origins"]) {
                    serverConfig_.corsOrigins.push_back(origin.get<std::string>());
                }
            }
            if (server.contains("auth_enabled")) {
                serverConfig_.authEnabled = server["auth_enabled"].get<bool>();
            }
            if (server.contains("auth_token")) {
                serverConfig_.authToken = server["auth_token"].get<std::string>();
            }
            if (server.contains("request_timeout")) {
                serverConfig_.requestTimeout = server["request_timeout"].get<int>();
            }
            if (server.contains("max_request_tokens")) {
                serverConfig_.maxRequestTokens = server["max_request_tokens"].get<int>();
            }
        }
        
        // 读取数据库配置
        if (configJson.contains("database")) {
            auto& database = configJson["database"];
            if (database.contains("host")) {
                databaseConfig_.host = database["host"].get<std::string>();
            }
            if (database.contains("port")) {
                databaseConfig_.port = database["port"].get<int>();
            }
            if (database.contains("user")) {
                databaseConfig_.user = database["user"].get<std::string>();
            }
            if (database.contains("password")) {
                databaseConfig_.password = database["password"].get<std::string>();
            }
            if (database.contains("database")) {
                databaseConfig_.database = database["database"].get<std::string>();
            }
            if (database.contains("charset")) {
                databaseConfig_.charset = database["charset"].get<std::string>();
            }
            if (database.contains("connect_timeout")) {
                databaseConfig_.connectTimeout = database["connect_timeout"].get<int>();
            }
            if (database.contains("read_timeout")) {
                databaseConfig_.readTimeout = database["read_timeout"].get<int>();
            }
            if (database.contains("write_timeout")) {
                databaseConfig_.writeTimeout = database["write_timeout"].get<int>();
            }
            if (database.contains("max_connections")) {
                databaseConfig_.maxConnections = database["max_connections"].get<int>();
            }
            if (database.contains("auto_reconnect")) {
                databaseConfig_.autoReconnect = database["auto_reconnect"].get<bool>();
            }
            if (database.contains("connection_string")) {
                databaseConfig_.connectionString = database["connection_string"].get<std::string>();
            }
        }
        
        initialized_ = true;
        
        // 如果模型目录存在但没有可用模型，扫描模型目录
        if (modelsConfig_.available.empty() && !modelsConfig_.directory.empty()) {
            scanModelDirectory();
        }
        
        LOG_INFO("成功加载配置文件: {}", filePath);
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("加载配置文件异常: {}", e.what());
        return false;
    }
}

// 加载默认配置
bool Config::loadDefault() {
    // 首先尝试加载配置文件
    const std::vector<std::string> configPaths = {
        "config.json",
        "../config.json",
        "/etc/llama_chat/config.json",
        std::string(getenv("HOME") ? getenv("HOME") : "") + "/.config/llama_chat/config.json"
    };
    
    for (const auto& path : configPaths) {
        if (loadFromFile(path)) {
            return true;
        }
    }
    
    // 如果没有找到配置文件，使用默认值并扫描模型目录
    LOG_WARN("未找到配置文件，使用默认配置");
    resetToDefaults();
    scanModelDirectory();
    
    initialized_ = true;
    return true;
}

// 保存配置到文件
bool Config::saveToFile(const std::string& filePath) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // 创建JSON对象
        json configJson;
        
        // 日志配置
        configJson["logging"]["level"] = loggingConfig_.level;
        configJson["logging"]["file"] = loggingConfig_.file;
        configJson["logging"]["console_level"] = loggingConfig_.consoleLevel;
        configJson["logging"]["max_size"] = loggingConfig_.maxSize;
        configJson["logging"]["max_files"] = loggingConfig_.maxFiles;
        configJson["logging"]["color_output"] = loggingConfig_.colorOutput;
        
        // 模型配置
        configJson["models"]["directory"] = modelsConfig_.directory;
        configJson["models"]["default"] = modelsConfig_.defaultModel;
        configJson["models"]["available"] = modelsConfig_.available;
        
        // 模型名称映射
        json nameMap;
        for (const auto& pair : modelsConfig_.nameMap) {
            nameMap[pair.first] = pair.second;
        }
        configJson["models"]["name_map"] = nameMap;
        
        // 生成配置
        configJson["generation"]["context_size"] = generationConfig_.contextSize;
        configJson["generation"]["threads"] = generationConfig_.threads;
        configJson["generation"]["gpu_layers"] = generationConfig_.gpuLayers;
        configJson["generation"]["temperature"] = generationConfig_.temperature;
        configJson["generation"]["top_p"] = generationConfig_.topP;
        configJson["generation"]["top_k"] = generationConfig_.topK;
        configJson["generation"]["repeat_penalty"] = generationConfig_.repeatPenalty;
        configJson["generation"]["max_tokens"] = generationConfig_.maxTokens;
        configJson["generation"]["stream_output"] = generationConfig_.streamOutput;
        configJson["generation"]["stop_sequences"] = generationConfig_.stopSequences;
        configJson["generation"]["system_prompt"] = generationConfig_.systemPrompt;
        
        // 应用配置
        configJson["app"]["history_dir"] = appConfig_.historyDir;
        configJson["app"]["save_history"] = appConfig_.saveHistory;
        configJson["app"]["max_history_files"] = appConfig_.maxHistoryFiles;
        configJson["app"]["auto_save_interval"] = appConfig_.autoSaveInterval;
        configJson["app"]["welcome_message"] = appConfig_.welcomeMessage;
        configJson["app"]["prompt_prefix"] = appConfig_.promptPrefix;
        configJson["app"]["response_prefix"] = appConfig_.responsePrefix;
        configJson["app"]["locale"] = appConfig_.locale;
        
        // 高级配置
        configJson["advanced"]["kv_cache_type"] = advancedConfig_.kvCacheType;
        configJson["advanced"]["batch_size"] = advancedConfig_.batchSize;
        configJson["advanced"]["chunk_size"] = advancedConfig_.chunkSize;
        configJson["advanced"]["dynamic_prompt_handling"] = advancedConfig_.dynamicPromptHandling;
        configJson["advanced"]["enable_function_calling"] = advancedConfig_.enableFunctionCalling;
        configJson["advanced"]["function_calling_format"] = advancedConfig_.functionCallingFormat;
        configJson["advanced"]["memory_limit_mb"] = advancedConfig_.memoryLimitMB;
        configJson["advanced"]["rope_freq_base"] = advancedConfig_.ropeFreqBase;
        configJson["advanced"]["rope_freq_scale"] = advancedConfig_.ropeFreqScale;
        
        // 服务器配置
        configJson["server"]["enabled"] = serverConfig_.enabled;
        configJson["server"]["host"] = serverConfig_.host;
        configJson["server"]["port"] = serverConfig_.port;
        configJson["server"]["cors_origins"] = serverConfig_.corsOrigins;
        configJson["server"]["auth_enabled"] = serverConfig_.authEnabled;
        configJson["server"]["auth_token"] = serverConfig_.authToken;
        configJson["server"]["request_timeout"] = serverConfig_.requestTimeout;
        configJson["server"]["max_request_tokens"] = serverConfig_.maxRequestTokens;
        
        // 数据库配置
        configJson["database"]["host"] = databaseConfig_.host;
        configJson["database"]["port"] = databaseConfig_.port;
        configJson["database"]["user"] = databaseConfig_.user;
        configJson["database"]["password"] = databaseConfig_.password;
        configJson["database"]["database"] = databaseConfig_.database;
        configJson["database"]["charset"] = databaseConfig_.charset;
        configJson["database"]["connect_timeout"] = databaseConfig_.connectTimeout;
        configJson["database"]["read_timeout"] = databaseConfig_.readTimeout;
        configJson["database"]["write_timeout"] = databaseConfig_.writeTimeout;
        configJson["database"]["max_connections"] = databaseConfig_.maxConnections;
        configJson["database"]["auto_reconnect"] = databaseConfig_.autoReconnect;
        configJson["database"]["connection_string"] = databaseConfig_.connectionString;
        
        // 确保目录存在
        std::filesystem::path path(filePath);
        std::filesystem::create_directories(path.parent_path());
        
        // 写入文件
        std::ofstream file(filePath);
        if (!file.is_open()) {
            LOG_ERROR("无法创建配置文件: {}", filePath);
            return false;
        }
        
        file << configJson.dump(4); // 使用4个空格缩进
        LOG_INFO("成功保存配置到文件: {}", filePath);
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("保存配置文件异常: {}", e.what());
        return false;
    }
}

// 扫描模型目录
void Config::scanModelDirectory() {
    try {
        std::string dir = modelsConfig_.directory;
        if (dir.empty()) {
            return;
        }
        
        // 检查目录是否存在
        if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
            LOG_WARN("模型目录不存在: {}", dir);
            return;
        }
        
        // 扫描目录
        modelsConfig_.available.clear();
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                if (ext == ".bin" || ext == ".gguf" || ext == ".ggml") {
                    modelsConfig_.available.push_back(entry.path().string());
                    
                    // 如果没有默认模型，设置第一个找到的模型为默认
                    if (modelsConfig_.defaultModel.empty()) {
                        modelsConfig_.defaultModel = entry.path().string();
                    }
                    
                    LOG_INFO("发现模型: {}", entry.path().string());
                }
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("扫描模型目录异常: {}", e.what());
    }
}

// 日志配置相关的getter方法
const LoggingConfig& Config::getLoggingConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return loggingConfig_;
}

std::string Config::getLogLevel() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return loggingConfig_.level;
}

std::string Config::getLogFilePath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return loggingConfig_.file;
}

std::string Config::getConsoleLogLevel() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return loggingConfig_.consoleLevel;
}

size_t Config::getLogMaxSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return loggingConfig_.maxSize;
}

int Config::getLogMaxFiles() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return loggingConfig_.maxFiles;
}

bool Config::getLogColorOutput() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return loggingConfig_.colorOutput;
}

// 模型配置相关的getter方法
const ModelsConfig& Config::getModelsConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return modelsConfig_;
}

std::string Config::getModelDir() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return modelsConfig_.directory;
}

std::string Config::getDefaultModelPath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return modelsConfig_.defaultModel;
}

std::vector<std::string> Config::getAvailableModels() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return modelsConfig_.available;
}

std::unordered_map<std::string, std::string> Config::getModelNameMap() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return modelsConfig_.nameMap;
}

// 生成配置相关的getter方法
const GenerationConfig& Config::getGenerationConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return generationConfig_;
}

int Config::getContextSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return generationConfig_.contextSize;
}

int Config::getThreads() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return generationConfig_.threads;
}

int Config::getGpuLayers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return generationConfig_.gpuLayers;
}

float Config::getTemperature() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return generationConfig_.temperature;
}

float Config::getTopP() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return generationConfig_.topP;
}

int Config::getTopK() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return generationConfig_.topK;
}

float Config::getRepeatPenalty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return generationConfig_.repeatPenalty;
}

int Config::getMaxTokens() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return generationConfig_.maxTokens;
}

bool Config::getStreamOutput() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return generationConfig_.streamOutput;
}

std::vector<std::string> Config::getStopSequences() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return generationConfig_.stopSequences;
}

std::string Config::getSystemPrompt() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return generationConfig_.systemPrompt;
}

// 应用配置相关的getter方法
const AppConfig& Config::getAppConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return appConfig_;
}

std::string Config::getHistoryDir() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return appConfig_.historyDir;
}

bool Config::getSaveHistory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return appConfig_.saveHistory;
}

int Config::getMaxHistoryFiles() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return appConfig_.maxHistoryFiles;
}

int Config::getAutoSaveInterval() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return appConfig_.autoSaveInterval;
}

std::string Config::getWelcomeMessage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return appConfig_.welcomeMessage;
}

std::string Config::getPromptPrefix() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return appConfig_.promptPrefix;
}

std::string Config::getResponsePrefix() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return appConfig_.responsePrefix;
}

std::string Config::getLocale() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return appConfig_.locale;
}

// 高级配置相关的getter方法
const AdvancedConfig& Config::getAdvancedConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return advancedConfig_;
}

std::string Config::getKvCacheType() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return advancedConfig_.kvCacheType;
}

int Config::getBatchSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return advancedConfig_.batchSize;
}

int Config::getChunkSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return advancedConfig_.chunkSize;
}

bool Config::getDynamicPromptHandling() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return advancedConfig_.dynamicPromptHandling;
}

bool Config::getEnableFunctionCalling() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return advancedConfig_.enableFunctionCalling;
}

std::string Config::getFunctionCallingFormat() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return advancedConfig_.functionCallingFormat;
}

int Config::getMemoryLimitMB() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return advancedConfig_.memoryLimitMB;
}

float Config::getRopeFreqBase() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return advancedConfig_.ropeFreqBase;
}

float Config::getRopeFreqScale() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return advancedConfig_.ropeFreqScale;
}

// 服务器配置相关的getter方法
const ServerConfig& Config::getServerConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return serverConfig_;
}

bool Config::getServerEnabled() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return serverConfig_.enabled;
}

std::string Config::getServerHost() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return serverConfig_.host;
}

int Config::getServerPort() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return serverConfig_.port;
}

std::vector<std::string> Config::getCorsOrigins() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return serverConfig_.corsOrigins;
}

bool Config::getAuthEnabled() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return serverConfig_.authEnabled;
}

std::string Config::getAuthToken() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return serverConfig_.authToken;
}

int Config::getRequestTimeout() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return serverConfig_.requestTimeout;
}

int Config::getMaxRequestTokens() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return serverConfig_.maxRequestTokens;
}

// 数据库配置相关的getter方法
const DatabaseConfig& Config::getDatabaseConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return databaseConfig_;
}

std::string Config::getDatabaseHost() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return databaseConfig_.host;
}

int Config::getDatabasePort() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return databaseConfig_.port;
}

std::string Config::getDatabaseUser() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return databaseConfig_.user;
}

std::string Config::getDatabasePassword() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return databaseConfig_.password;
}

std::string Config::getDatabaseName() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return databaseConfig_.database;
}

std::string Config::getDatabaseCharset() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return databaseConfig_.charset;
}

std::string Config::getDatabaseConnectionString() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return databaseConfig_.connectionString;
}

int Config::getDatabaseConnectTimeout() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return databaseConfig_.connectTimeout;
}

int Config::getDatabaseReadTimeout() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return databaseConfig_.readTimeout;
}

int Config::getDatabaseWriteTimeout() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return databaseConfig_.writeTimeout;
}

int Config::getDatabaseMaxConnections() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return databaseConfig_.maxConnections;
}

bool Config::getDatabaseAutoReconnect() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return databaseConfig_.autoReconnect;
}

// 日志初始化
void initLogging() {
    // 获取日志配置
    auto& config = Config::getInstance();
    std::string logLevel = config.getLogLevel();
    std::string logFilePath = config.getLogFilePath();
    std::string consoleLevel = config.getConsoleLogLevel();
    bool colorOutput = config.getLogColorOutput();
    
    // 确保日志目录存在
    std::filesystem::path path(logFilePath);
    std::filesystem::create_directories(path.parent_path());
    
    // 这里应该根据实际使用的日志库进行初始化
    // 例如，使用spdlog：
    // auto fileLogger = spdlog::basic_logger_mt("file_logger", logFilePath);
    // auto consoleLogger = spdlog::stdout_color_mt("console");
    // spdlog::set_default_logger(fileLogger);
    
    LOG_INFO("日志系统初始化完成");
}

} // namespace common
} // namespace kb 