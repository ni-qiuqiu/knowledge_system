/**
 * @file config_test.cpp
 * @brief 配置系统测试程序
 */

#include "common/config.h"
#include "common/logging.h"
#include <iostream>
#include <iomanip>
#include <locale>
#include <vector>
#include <string>

/**
 * 打印配置信息
 */
void printConfig() {
    auto& config = kb::common::Config::getInstance();
    
    // 设置输出格式
    std::cout << std::boolalpha;
    
    // 设置本地化
    try {
        std::locale::global(std::locale(config.getLocale()));
        std::cout.imbue(std::locale());
    } catch (const std::exception& e) {
        std::cerr << "警告: 无法设置区域设置 " << config.getLocale() << ": " << e.what() << std::endl;
        std::cerr << "尝试设置为 C.UTF-8" << std::endl;
        try {
            std::locale::global(std::locale("C.UTF-8"));
            std::cout.imbue(std::locale());
        } catch (...) {
            std::cerr << "警告: 无法设置区域设置为 C.UTF-8，使用默认区域设置" << std::endl;
        }
    }

    // 打印标题
    std::cout << "\n===================================\n";
    std::cout << "        LLama聊天配置信息\n";
    std::cout << "===================================\n\n";

    // 日志配置
    std::cout << "【日志配置】\n";
    std::cout << "  日志级别: " << config.getLogLevel() << "\n";
    std::cout << "  日志文件: " << config.getLogFilePath() << "\n";
    std::cout << "  控制台日志级别: " << config.getConsoleLogLevel() << "\n";
    std::cout << "  日志文件最大大小: " << (config.getLogMaxSize() / 1024 / 1024) << " MB\n";
    std::cout << "  最大日志文件数: " << config.getLogMaxFiles() << "\n";
    std::cout << "  彩色输出: " << config.getLogColorOutput() << "\n\n";

    // 模型配置
    std::cout << "【模型配置】\n";
    std::cout << "  模型目录: " << config.getModelDir() << "\n";
    std::cout << "  默认模型: " << config.getDefaultModelPath() << "\n";
    
    std::cout << "  可用模型列表:\n";
    for (const auto& model : config.getAvailableModels()) {
        std::cout << "    - " << model << "\n";
    }
    
    std::cout << "  模型名称映射:\n";
    for (const auto& [name, path] : config.getModelNameMap()) {
        std::cout << "    - " << name << " => " << path << "\n";
    }
    std::cout << "\n";

    // 生成配置
    std::cout << "【生成配置】\n";
    std::cout << "  上下文大小: " << config.getContextSize() << "\n";
    std::cout << "  线程数: " << config.getThreads() << "\n";
    std::cout << "  GPU层数: " << config.getGpuLayers() << "\n";
    std::cout << "  温度: " << config.getTemperature() << "\n";
    std::cout << "  Top-P: " << config.getTopP() << "\n";
    std::cout << "  Top-K: " << config.getTopK() << "\n";
    std::cout << "  重复惩罚: " << config.getRepeatPenalty() << "\n";
    std::cout << "  最大生成令牌数: " << config.getMaxTokens() << "\n";
    std::cout << "  流式输出: " << config.getStreamOutput() << "\n";
    
    std::cout << "  停止序列:\n";
    for (const auto& stop : config.getStopSequences()) {
        std::cout << "    - \"" << stop << "\"\n";
    }
    
    std::cout << "  系统提示词: " << config.getSystemPrompt() << "\n\n";

    // 应用配置
    std::cout << "【应用配置】\n";
    std::cout << "  历史目录: " << config.getHistoryDir() << "\n";
    std::cout << "  保存历史: " << config.getSaveHistory() << "\n";
    std::cout << "  最大历史文件数: " << config.getMaxHistoryFiles() << "\n";
    std::cout << "  自动保存间隔: " << config.getAutoSaveInterval() << " 秒\n";
    std::cout << "  欢迎消息: " << config.getWelcomeMessage() << "\n";
    std::cout << "  提示前缀: \"" << config.getPromptPrefix() << "\"\n";
    std::cout << "  响应前缀: \"" << config.getResponsePrefix() << "\"\n";
    std::cout << "  区域设置: " << config.getLocale() << "\n\n";

    // 高级配置
    std::cout << "【高级配置】\n";
    std::cout << "  KV缓存类型: " << config.getKvCacheType() << "\n";
    std::cout << "  批处理大小: " << config.getBatchSize() << "\n";
    std::cout << "  分块大小: " << config.getChunkSize() << "\n";
    std::cout << "  动态提示词处理: " << config.getDynamicPromptHandling() << "\n";
    std::cout << "  启用函数调用: " << config.getEnableFunctionCalling() << "\n";
    std::cout << "  函数调用格式: " << config.getFunctionCallingFormat() << "\n";
    std::cout << "  内存限制: " << (config.getMemoryLimitMB() == 0 ? "无限制" : std::to_string(config.getMemoryLimitMB()) + " MB") << "\n";
    std::cout << "  RoPE频率基数: " << config.getRopeFreqBase() << "\n";
    std::cout << "  RoPE频率缩放: " << config.getRopeFreqScale() << "\n\n";

    // 服务器配置
    std::cout << "【服务器配置】\n";
    std::cout << "  启用服务器: " << config.getServerEnabled() << "\n";
    std::cout << "  主机: " << config.getServerHost() << "\n";
    std::cout << "  端口: " << config.getServerPort() << "\n";
    
    std::cout << "  CORS起源列表:\n";
    for (const auto& origin : config.getCorsOrigins()) {
        std::cout << "    - " << origin << "\n";
    }
    
    std::cout << "  启用认证: " << config.getAuthEnabled() << "\n";
    std::cout << "  认证令牌: " << (config.getAuthToken().empty() ? "[空]" : "******") << "\n";
    std::cout << "  请求超时: " << config.getRequestTimeout() << " 秒\n";
    std::cout << "  最大请求令牌数: " << config.getMaxRequestTokens() << "\n\n";
}

/**
 * 测试加载配置
 */
bool testLoadConfig(const std::string& configPath = "") {
    auto& config = kb::common::Config::getInstance();
    
    if (configPath.empty()) {
        std::cout << "尝试加载默认配置...\n";
        if (!config.loadDefault()) {
            std::cerr << "无法加载默认配置\n";
            return false;
        }
        std::cout << "成功加载默认配置\n";
    } else {
        std::cout << "尝试从 " << configPath << " 加载配置...\n";
        if (!config.loadFromFile(configPath)) {
            std::cerr << "无法从 " << configPath << " 加载配置\n";
            return false;
        }
        std::cout << "成功从 " << configPath << " 加载配置\n";
    }
    
    return true;
}

/**
 * 测试保存配置
 */
bool testSaveConfig(const std::string& configPath) {
    auto& config = kb::common::Config::getInstance();
    
    std::cout << "尝试保存配置到 " << configPath << "...\n";
    if (!config.saveToFile(configPath)) {
        std::cerr << "无法保存配置到 " << configPath << "\n";
        return false;
    }
    std::cout << "成功保存配置到 " << configPath << "\n";
    
    return true;
}

/**
 * 测试模型目录扫描
 */
void testScanModelDirectory() {
    auto& config = kb::common::Config::getInstance();
    
    std::cout << "扫描模型目录前的可用模型列表:\n";
    for (const auto& model : config.getAvailableModels()) {
        std::cout << "  - " << model << "\n";
    }
    
    std::cout << "扫描模型目录...\n";
    config.scanModelDirectory();
    
    std::cout << "扫描模型目录后的可用模型列表:\n";
    for (const auto& model : config.getAvailableModels()) {
        std::cout << "  - " << model << "\n";
    }
}

/**
 * 主函数
 */
int main(int argc, char* argv[]) {
    std::string configPath = (argc > 1) ? argv[1] : "";
    
    if (!testLoadConfig(configPath)) {
        return 1;
    }
    
    printConfig();
    
    if (argc > 2 && std::string(argv[2]) == "--scan") {
        testScanModelDirectory();
    }
    
    if (argc > 2 && std::string(argv[2]) == "--save") {
        std::string savePath = (argc > 3) ? argv[3] : "config_saved.json";
        if (!testSaveConfig(savePath)) {
            return 1;
        }
    }
    
    return 0;
} 