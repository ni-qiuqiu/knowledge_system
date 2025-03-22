/**
 * @file main.cpp
 * @brief 项目主入口文件
 */

#include "engine/model_interface.h"
#include "engine/llama_model.h"
#include "common/logging.h"
#include "common/config.h"
#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <locale>
#include <signal.h>
#include <getopt.h>

using namespace kb::engine;

// 全局变量
bool g_running = true;
ModelInterfacePtr g_model = nullptr;

// 命令行参数结构
struct CommandLineArgs {
    std::string modelPath;      // 模型路径
    int gpuLayers = 0;          // GPU层数
    int contextSize = 2048;     // 上下文大小
    int threads = 4;            // 线程数
    float temperature = 0.7f;   // 温度
    float topP = 0.9f;          // Top-P
    int topK = 40;              // Top-K
    float repeatPenalty = 1.1f; // 重复惩罚
    int maxTokens = 2048;       // 最大生成词元数
    bool streamOutput = true;   // 是否流式输出
    bool interactiveMode = true;// 是否交互模式
    std::string promptFile;     // 提示词文件
    std::string outputFile;     // 输出文件
};

// 信号处理函数
void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\n收到退出信号，正在停止程序..." << std::endl;
        g_running = false;
    }
}

// 打印使用帮助
void printUsage(const char* programName) {
    std::cout << "用法: " << programName << " [选项]" << std::endl;
    std::cout << "选项:" << std::endl;
    std::cout << "  -m, --model PATH         指定模型路径" << std::endl;
    std::cout << "  -g, --gpu-layers N       指定GPU层数 (默认: 0, 使用CPU)" << std::endl;
    std::cout << "  -c, --context-size N     指定上下文大小 (默认: 2048)" << std::endl;
    std::cout << "  -t, --threads N          指定线程数 (默认: 4)" << std::endl;
    std::cout << "  --temp FLOAT             指定生成温度 (默认: 0.7)" << std::endl;
    std::cout << "  --top-p FLOAT            指定Top-P值 (默认: 0.9)" << std::endl;
    std::cout << "  --top-k INT              指定Top-K值 (默认: 40)" << std::endl;
    std::cout << "  --repeat-penalty FLOAT   指定重复惩罚 (默认: 1.1)" << std::endl;
    std::cout << "  --max-tokens INT         指定最大生成词元数 (默认: 2048)" << std::endl;
    std::cout << "  --no-stream              禁用流式输出" << std::endl;
    std::cout << "  --prompt-file PATH       从文件读取提示词" << std::endl;
    std::cout << "  --output-file PATH       将输出写入文件" << std::endl;
    std::cout << "  -h, --help               显示帮助信息" << std::endl;
    std::cout << "示例:" << std::endl;
    std::cout << "  " << programName << " -m /path/to/model.gguf" << std::endl;
    std::cout << "  " << programName << " -m /path/to/model.gguf --gpu-layers 32 --context-size 4096" << std::endl;
}

// 解析命令行参数
CommandLineArgs parseArgs(int argc, char** argv) {
    CommandLineArgs args;
    
    const struct option longOptions[] = {
        {"model",           required_argument, nullptr, 'm'},
        {"gpu-layers",      required_argument, nullptr, 'g'},
        {"context-size",    required_argument, nullptr, 'c'},
        {"threads",         required_argument, nullptr, 't'},
        {"temp",            required_argument, nullptr, 1000},
        {"top-p",           required_argument, nullptr, 1001},
        {"top-k",           required_argument, nullptr, 1002},
        {"repeat-penalty",  required_argument, nullptr, 1003},
        {"max-tokens",      required_argument, nullptr, 1004},
        {"no-stream",       no_argument,       nullptr, 1005},
        {"prompt-file",     required_argument, nullptr, 1006},
        {"output-file",     required_argument, nullptr, 1007},
        {"help",            no_argument,       nullptr, 'h'},
        {nullptr,           0,                 nullptr, 0}
    };
    
    int opt;
    int optionIndex = 0;
    
    // 设置getopt不打印错误信息
    opterr = 0;
    
    while ((opt = getopt_long(argc, argv, "m:g:c:t:h", longOptions, &optionIndex)) != -1) {
        switch (opt) {
            case 'm':
                args.modelPath = optarg;
                break;
            case 'g':
                args.gpuLayers = std::stoi(optarg);
                break;
            case 'c':
                args.contextSize = std::stoi(optarg);
                break;
            case 't':
                args.threads = std::stoi(optarg);
                break;
            case 1000:
                args.temperature = std::stof(optarg);
                break;
            case 1001:
                args.topP = std::stof(optarg);
                break;
            case 1002:
                args.topK = std::stoi(optarg);
                break;
            case 1003:
                args.repeatPenalty = std::stof(optarg);
                break;
            case 1004:
                args.maxTokens = std::stoi(optarg);
                break;
            case 1005:
                args.streamOutput = false;
                break;
            case 1006:
                args.promptFile = optarg;
                args.interactiveMode = false;
                break;
            case 1007:
                args.outputFile = optarg;
                break;
            case 'h':
                printUsage(argv[0]);
                exit(0);
                break;
            case '?':
            default:
                std::cerr << "未知选项: " << argv[optind - 1] << std::endl;
                printUsage(argv[0]);
                exit(1);
                break;
        }
    }
    
    // 检查必需参数
    if (args.modelPath.empty()) {
        // 尝试从配置文件获取默认模型
        std::string defaultModel = kb::common::Config::getInstance().getDefaultModelPath();
        if (!defaultModel.empty()) {
            args.modelPath = defaultModel;
            LOG_INFO("使用配置文件中的默认模型: {}", args.modelPath);
        } else {
            std::cerr << "错误: 必须指定模型路径" << std::endl;
            printUsage(argv[0]);
            exit(1);
        }
    }
    
    return args;
}

// 初始化模型
ModelInterfacePtr initializeModel(const CommandLineArgs& args) {
    LOG_INFO("开始初始化模型: {}", args.modelPath);
    
    // 检查模型文件是否存在
    if (!std::filesystem::exists(args.modelPath)) {
        LOG_ERROR("模型文件不存在: {}", args.modelPath);
        return nullptr;
    }
    
    // 设置模型参数
    ModelParameters parameters;
    parameters.modelPath = args.modelPath;
    parameters.contextSize = args.contextSize;
    parameters.threads = args.threads;
    parameters.useGPU = args.gpuLayers > 0;
    parameters.gpuLayers = args.gpuLayers;
    parameters.temperature = args.temperature;
    
    // 创建模型实例
    auto model = LlamaModelFactory::getInstance().createModel(
        ModelType::TEXT_GENERATION,
        args.modelPath,
        parameters
    );
    
    if (!model) {
        LOG_ERROR("创建模型失败");
        return nullptr;
    }
    
    LOG_INFO("模型初始化成功: {} ({})", model->getModelName(), model->getModelVersion());
    return model;
}

// 读取文件内容
std::string readFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        LOG_ERROR("无法打开文件: {}", filePath);
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// 将文本写入文件
bool writeFile(const std::string& filePath, const std::string& content) {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        LOG_ERROR("无法打开文件进行写入: {}", filePath);
        return false;
    }
    
    file << content;
    return true;
}

// 交互式聊天模式
void interactiveChat(ModelInterfacePtr model, const CommandLineArgs& args) {
    LOG_INFO("启动交互式聊天模式");
    
    // 设置生成选项
    GenerationOptions options;
    options.maxTokens = args.maxTokens;
    options.temperature = args.temperature;
    options.topP = args.topP;
    options.topK = args.topK;
    options.repeatPenalty = args.repeatPenalty;
    options.streamOutput = args.streamOutput;
    options.stopSequences = {"\n\n\n"};
    
    // 欢迎消息
    std::cout << "\n====================================================" << std::endl;
    std::cout << "欢迎使用聊天程序" << std::endl;
    std::cout << "模型: " << model->getModelName() << " (" << model->getModelVersion() << ")" << std::endl;
    std::cout << "输入 'quit' 或 'exit' 退出程序" << std::endl;
    std::cout << "====================================================" << std::endl;
    
    std::string conversation;
    std::string userInput;
    
    // 主聊天循环
    while (g_running) {
        // 输出提示符
        std::cout << "\n用户> ";
        std::getline(std::cin, userInput);
        
        // 检查退出命令
        if (userInput == "quit" || userInput == "exit") {
            break;
        }
        
        // 跳过空输入
        if (userInput.empty()) {
            continue;
        }
        
        try {
            // 准备生成
            std::cout << "\n助手> ";
            
            // 流式生成回调
            StreamCallback callback = [](const std::string& text, bool isLast) {
                std::cout << text << std::flush;
                
                if (isLast) {
                    std::cout << std::endl;
                }
            };
            
            // 生成回复
            GenerationResult result;
            if (args.streamOutput) {
                result = model->generateStream(userInput, callback, options);
            } else {
                result = model->generate(userInput, options);
                std::cout << result.text << std::endl;
            }
            
            // 记录到会话历史
            conversation += "用户: " + userInput + "\n";
            conversation += "助手: " + result.text + "\n\n";
            
            LOG_INFO("生成完成，耗时: {} 毫秒，生成词元数: {}", 
                     result.generationTimeMs, result.tokensGenerated);
            
        } catch (const std::exception& e) {
            LOG_ERROR("生成过程发生异常: {}", e.what());
            std::cout << "抱歉，生成过程中出现错误，请重试。" << std::endl;
        }
    }
    
    // 保存会话历史
    if (!conversation.empty() && !args.outputFile.empty()) {
        LOG_INFO("保存会话记录到文件: {}", args.outputFile);
        writeFile(args.outputFile, conversation);
    }
}

// 批处理模式
void batchProcess(ModelInterfacePtr model, const CommandLineArgs& args) {
    LOG_INFO("启动批处理模式");
    
    // 读取提示词文件
    std::string prompt = readFile(args.promptFile);
    if (prompt.empty()) {
        LOG_ERROR("提示词文件为空或无法读取");
        return;
    }
    
    LOG_INFO("从文件加载提示词: {}", args.promptFile);
    
    // 设置生成选项
    GenerationOptions options;
    options.maxTokens = args.maxTokens;
    options.temperature = args.temperature;
    options.topP = args.topP;
    options.topK = args.topK;
    options.repeatPenalty = args.repeatPenalty;
    options.streamOutput = false;
    
    try {
        LOG_INFO("开始生成...");
        
        // 生成文本
        auto result = model->generate(prompt, options);
        
        LOG_INFO("生成完成，耗时: {} 毫秒，生成词元数: {}", 
                 result.generationTimeMs, result.tokensGenerated);
        
        // 输出结果
        if (!args.outputFile.empty()) {
            LOG_INFO("将结果写入文件: {}", args.outputFile);
            writeFile(args.outputFile, result.text);
        } else {
            std::cout << "\n生成结果:\n" << std::endl;
            std::cout << result.text << std::endl;
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("生成过程发生异常: {}", e.what());
        std::cerr << "生成过程中出现错误: " << e.what() << std::endl;
    }
}

// 主函数
int main(int argc, char** argv) {
    // 设置UTF-8本地化
    std::locale::global(std::locale("en_US.UTF-8"));
    std::cout.imbue(std::locale("en_US.UTF-8"));
    
    // 注册信号处理函数
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    LOG_INFO("程序启动");
    
    // 解析命令行参数
    CommandLineArgs args = parseArgs(argc, argv);
    
    try {
        // 初始化模型
        g_model = initializeModel(args);
        if (!g_model) {
            LOG_ERROR("模型初始化失败，程序退出");
            return 1;
        }
        
        // 根据模式执行不同的操作
        if (args.interactiveMode) {
            interactiveChat(g_model, args);
        } else {
            batchProcess(g_model, args);
        }
        
        // 释放资源
        g_model = nullptr;
        
    } catch (const std::exception& e) {
        LOG_ERROR("程序运行异常: {}", e.what());
        std::cerr << "程序发生错误: " << e.what() << std::endl;
        return 1;
    }
    
    LOG_INFO("程序正常退出");
    return 0;
} 