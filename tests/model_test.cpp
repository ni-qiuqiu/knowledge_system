/**
 * @file model_test.cpp
 * @brief 学习引擎测试程序
 */

#include "engine/model_interface.h"
#include "engine/llama_model.h"
#include "common/logging.h"
#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <chrono>
#include <iomanip>
#include <sstream>

using namespace kb::engine;

/**
 * @brief 测试模型初始化
 * @param modelPath 模型路径
 * @return 模型接口
 */
ModelInterfacePtr testModelInit(const std::string& modelPath) {
    LOG_INFO("测试模型初始化，模型路径：{}", modelPath);
    
    // 从工厂创建模型

    // auto model = ModelFactory::getInstance().createModel(
    //     ModelType::TEXT_GENERATION,
    //     modelPath
    // );
    //创建llama模型
    auto model = LlamaModelFactory::getInstance().createModel(
        ModelType::TEXT_GENERATION,
        modelPath
    );
    if (!model) {
        LOG_ERROR("创建模型失败");
        return nullptr;
    }
   
    LOG_INFO("模型创建成功，名称：{}，版本：{}", 
             model->getModelName(), model->getModelVersion());
    
    return model;
}

/**
 * @brief 测试文本生成
 * @param model 模型接口
 * @param prompt 提示词
 */
void testGeneration(ModelInterfacePtr model, const std::string& prompt) {
    if (!model) {
        LOG_ERROR("模型为空");
        return;
    }
    
    LOG_INFO("测试文本生成，提示词：{}", prompt);
    
    try {
        // 设置UTF-8本地化
        std::locale::global(std::locale("en_US.UTF-8"));
        std::cout.imbue(std::locale("en_US.UTF-8"));
        
        // 设置生成选项
        GenerationOptions options;
        options.maxTokens = 128;
        options.temperature = 0.7f;
        options.topP = 0.9f;
        options.topK = 40;
        options.repeatPenalty = 1.1f;
        options.stopSequences = {"\n\n", "###"};
        
        // 生成文本
        LOG_INFO("开始生成文本...");
        auto result = model->generate(prompt, options);
        LOG_INFO("生成完成，耗时：{} 毫秒，生成词元数：{}", 
                result.generationTimeMs, result.tokensGenerated);
                
        // 输出生成结果
        std::cout << "\n====== 生成结果 ======\n" << std::endl;
        std::cout << result.text << std::endl;
        std::cout << "\n=====================\n" << std::endl;
        
        // 调试信息: 打印前10个词元的详细信息
        std::cout << "词元详情 (最多10个):" << std::endl;
        for (size_t i = 0; i < std::min(result.tokens.size(), size_t(10)); i++) {
            const auto& token = result.tokens[i];
            
            // 基本信息
            std::cout << "词元[" << i << "]: ID=" << token.id 
                    << ", 文本=\"" << token.text << "\"" 
                    << ", 字节长度=" << token.text.length()
                    << std::endl;
            
            // 字节级分析
            std::cout << "  字节: ";
            for (size_t j = 0; j < token.text.length(); j++) {
                unsigned char byte = static_cast<unsigned char>(token.text[j]);
                std::cout << std::hex << std::setw(2) << std::setfill('0') 
                        << static_cast<int>(byte) << " ";
            }
            std::cout << std::dec << std::endl;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("生成过程发生异常: {}", e.what());
    }
}

/**
 * @brief 测试流式生成
 * @param model 模型接口
 * @param prompt 提示词
 */
void testStreamGeneration(ModelInterfacePtr model, const std::string& prompt) {
    if (!model) {
        LOG_ERROR("模型为空");
        return;
    }
    
    LOG_INFO("测试流式生成，提示词：{}", prompt);
    
    try {
        // 设置UTF-8本地化
        std::locale::global(std::locale("en_US.UTF-8"));
        std::cout.imbue(std::locale("en_US.UTF-8"));
        
        // 设置生成选项
        GenerationOptions options;
        options.maxTokens = 128;
        options.temperature = 0.7f;
        options.topP = 0.9f;
        options.topK = 40;
        options.repeatPenalty = 1.1f;
        options.stopSequences = {"\n\n", "###"};
        options.streamOutput = true;
        
        // 累计收到的所有文本
        std::string accumulated_text;
        
        // 实现回调函数
        StreamCallback callback = [&accumulated_text](const std::string& text, bool isLast) {
            // 检查并打印每个词元的字节内容（调试用）
            // if (text.length() > 0) {
            //     std::stringstream ss;
            //     ss << "[字节: ";
            //     for (size_t i = 0; i < text.length(); i++) {
            //         unsigned char byte = static_cast<unsigned char>(text[i]);
            //         ss << std::hex << std::setw(2) << std::setfill('0') 
            //           << static_cast<int>(byte) << " ";
            //     }
            //     ss << "]";
            //    // LOG_INFO("流式词元: \"{}\" {}", text, ss.str());
            // }
            
            // 累计文本
            accumulated_text += text;
            
            // 输出到控制台
            std::cout << text << std::flush;
            if (isLast) {
                std::cout << std::endl;
            }
        };
        
        // 流式生成
        LOG_INFO("开始流式生成...");
        auto result = model->generateStream(prompt, callback, options);
        
        LOG_INFO("流式生成完成，耗时：{} 毫秒，生成词元数：{}", 
                result.generationTimeMs, result.tokensGenerated);
                
        // 验证流式累计的文本与最终结果是否一致
        if (accumulated_text == result.text) {
            LOG_INFO("流式累计文本与最终结果一致");
        } else {
            LOG_WARN("流式累计文本与最终结果不一致");
            LOG_INFO("累计文本长度: {}, 最终结果长度: {}", 
                    accumulated_text.length(), result.text.length());
        }
    } catch (const std::exception& e) {
        LOG_ERROR("流式生成过程发生异常: {}", e.what());
    }
}

/**
 * @brief 测试异步生成
 * @param model 模型接口
 * @param prompt 提示词
 */
void testAsyncGeneration(ModelInterfacePtr model, const std::string& prompt) {
    if (!model) {
        LOG_ERROR("模型为空");
        return;
    }
    
    LOG_INFO("测试异步生成，提示词：{}", prompt);
    
    // 设置生成选项
    GenerationOptions options;
    options.maxTokens = 128;
    options.temperature = 0.7f;
    options.topP = 0.9f;
    options.topK = 40;
    options.repeatPenalty = 1.1f;
    
    // 异步生成
    auto future = model->generateAsync(prompt, options);
    
    LOG_INFO("等待异步生成完成...");
    
    // 模拟其他工作
    for (int i = 0; i < 5; i++) {
        LOG_INFO("执行其他工作 {}/5", i + 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // 获取结果
    auto result = future.get();
    
    LOG_INFO("异步生成完成，耗时：{} 毫秒，生成词元数：{}", 
             result.generationTimeMs, result.tokensGenerated);
    LOG_INFO("生成结果：\n{}", result.text);
}

/**
 * @brief 测试获取嵌入向量
 * @param model 模型接口
 * @param text 文本
 */
void testEmbedding(ModelInterfacePtr model, const std::string& text) {
    if (!model) {
        LOG_ERROR("模型为空");
        return;
    }
    
    LOG_INFO("测试获取嵌入向量，文本：{}", text);
    
    // 获取嵌入向量
    auto embedding = model->getEmbedding(text);
    
    LOG_INFO("嵌入向量维度：{}", embedding.size());
    
    // 打印部分向量值
    if (!embedding.empty()) {
        std::stringstream ss;
        ss << "向量前5个值：[";
        for (size_t i = 0; i < std::min(size_t(5), embedding.size()); ++i) {
            if (i > 0) ss << ", ";
            ss << std::fixed << std::setprecision(4) << embedding[i];
        }
        ss << "]";
        LOG_INFO("{}", ss.str());
    }
}

/**
 * @brief 测试词元化
 * @param model 模型接口
 * @param text 文本
 */
void testTokenize(ModelInterfacePtr model, const std::string& text) {
    if (!model) {
        LOG_ERROR("模型为空");
        return;
    }
    
    LOG_INFO("测试词元化，文本：{}", text);
    
    // 词元化
    auto tokens = model->tokenize(text);
    
    LOG_INFO("词元数量：{}", tokens.size());
    
    // 打印词元
    for (size_t i = 0; i < std::min(tokens.size(), size_t(10)); i++) {
        std::stringstream ss;
        ss << "词元[" << i << "]: ID=" << tokens[i].id 
           << ", 文本=\"" << tokens[i].text 
           << "\", 概率=" << std::fixed << std::setprecision(4) << tokens[i].probability 
           << ", 特殊=" << tokens[i].isSpecial;
        LOG_INFO("{}", ss.str());
    }
    
    // 测试去词元化
    auto detokenized = model->detokenize(tokens);
    LOG_INFO("去词元化结果：{}", detokenized);
}

/**
 * @brief 主函数
 * @param argc 参数数量
 * @param argv 参数列表
 * @return 程序退出码
 */
int main(int argc, char** argv) {
    // 初始化日志
    //kb::common::initLogging();
    
    LOG_INFO("学习引擎测试程序启动");
    
    // 模型路径
    std::string modelPath = "/media/qiu/新加卷/Ollama/DeepSeek_Mod/DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf";
    if (argc > 1) {
        modelPath = argv[1];
    }
    
    // 初始化模型
    auto model = testModelInit(modelPath);
    if (!model) {
        LOG_ERROR("模型初始化失败，退出测试");
        return 1;
    }
    
    // 测试文本生成
    testGeneration(model, "你好，请简单介绍一下知识图谱");
    
    // 测试流式生成
    testStreamGeneration(model, "请列出三种常见的知识表示方法");
    
    // 测试异步生成
    testAsyncGeneration(model, "简述向量数据库的工作原理");
    
    // 测试嵌入向量
    testEmbedding(model, "知识图谱是知识表示的一种重要方式");
    
    // 测试词元化
    testTokenize(model, "自然语言处理是人工智能的重要分支");
    
    LOG_INFO("学习引擎测试程序结束");
    return 0;
} 