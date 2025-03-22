/**
 * @file engine_integration.cpp
 * @brief 学习引擎集成示例
 */

#include "engine/model_interface.h"
#include "common/logging.h"
#include "common/config.h"
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <cmath>
namespace kb {
namespace engine {

/**
 * @brief 学习引擎集成类
 */
class LearningEngine {
public:
    /**
     * @brief 获取单例实例
     * @return 学习引擎实例
     */
    static LearningEngine& getInstance() {
        static LearningEngine instance;
        return instance;
    }
    
    /**
     * @brief 初始化学习引擎
     * @return 是否成功
     */
    bool initialize() {
        LOG_INFO("初始化学习引擎");
        
        // 从配置获取模型路径
        std::string modelPath = common::Config::getInstance().getDefaultModelPath();
        LOG_INFO("使用模型：{}", modelPath);
        
        // 创建模型
        model_ = ModelFactory::getInstance().createModel(
            ModelType::TEXT_GENERATION,
            modelPath
        );
        
        if (!model_ || !model_->isInitialized()) {
            LOG_ERROR("初始化模型失败");
            return false;
        }
        
        LOG_INFO("学习引擎初始化成功，模型名称：{}", model_->getModelName());
        initialized_ = true;
        return true;
    }
    
    /**
     * @brief 释放资源
     */
    void release() {
        if (model_) {
            model_->release();
            model_.reset();
        }
        initialized_ = false;
        LOG_INFO("学习引擎资源已释放");
    }
    
    /**
     * @brief 生成回答
     * @param question 问题
     * @param context 上下文信息
     * @return 生成的回答
     */
    std::string generateAnswer(
        const std::string& question,
        const std::vector<std::string>& context) {
        
        if (!initialized_ || !model_) {
            LOG_ERROR("学习引擎未初始化");
            return "系统错误：学习引擎未初始化";
        }
        
        LOG_INFO("生成答案，问题：{}", question);
        
        // 构建提示词
        std::string prompt = buildPrompt(question, context);
        
        // 设置生成选项
        GenerationOptions options;
        options.maxTokens = 1024;
        options.temperature = 0.7f;
        options.topP = 0.9f;
        options.topK = 40;
        options.repeatPenalty = 1.1f;
        options.stopSequences = {"\n\n", "###", "用户:"};
        
        // 生成回答
        auto result = model_->generate(prompt, options);
        
        LOG_INFO("生成完成，生成 {} 个词元，耗时 {} 毫秒", 
                 result.tokensGenerated, result.generationTimeMs);
        
        return result.text;
    }
    
    /**
     * @brief 生成回答（流式）
     * @param question 问题
     * @param context 上下文信息
     * @param callback 回调函数
     */
    void generateAnswerStream(
        const std::string& question,
        const std::vector<std::string>& context,
        StreamCallback callback) {
        
        if (!initialized_ || !model_) {
            LOG_ERROR("学习引擎未初始化");
            callback("系统错误：学习引擎未初始化", true);
            return;
        }
        
        LOG_INFO("流式生成答案，问题：{}", question);
        
        // 构建提示词
        std::string prompt = buildPrompt(question, context);
        
        // 设置生成选项
        GenerationOptions options;
        options.maxTokens = 1024;
        options.temperature = 0.7f;
        options.topP = 0.9f;
        options.topK = 40;
        options.repeatPenalty = 1.1f;
        options.stopSequences = {"\n\n", "###", "用户:"};
        options.streamOutput = true;
        
        // 流式生成回答
        auto result = model_->generateStream(prompt, callback, options);
        
        LOG_INFO("流式生成完成，生成 {} 个词元，耗时 {} 毫秒", 
                 result.tokensGenerated, result.generationTimeMs);
    }
    
    /**
     * @brief 获取文本嵌入向量
     * @param text 文本
     * @return 嵌入向量
     */
    Embedding getEmbedding(const std::string& text) {
        if (!initialized_ || !model_) {
            LOG_ERROR("学习引擎未初始化");
            return {};
        }
        
        return model_->getEmbedding(text);
    }
    
    /**
     * @brief 计算相似度
     * @param text1 文本1
     * @param text2 文本2
     * @return 相似度得分
     */
    float computeSimilarity(const std::string& text1, const std::string& text2) {
        auto embedding1 = getEmbedding(text1);
        auto embedding2 = getEmbedding(text2);
        
        if (embedding1.empty() || embedding2.empty()) {
            return 0.0f;
        }
        
        return computeCosineSimilarity(embedding1, embedding2);
    }
    
    /**
     * @brief 是否已初始化
     * @return 是否已初始化
     */
    bool isInitialized() const {
        return initialized_;
    }
    
private:
    /**
     * @brief 构造函数
     */
    LearningEngine() : initialized_(false) {
        LOG_INFO("创建学习引擎");
    }
    
    /**
     * @brief 析构函数
     */
    ~LearningEngine() {
        release();
    }
    
    /**
     * @brief 构建提示词
     * @param question 问题
     * @param context 上下文信息
     * @return 提示词
     */
    std::string buildPrompt(
        const std::string& question,
        const std::vector<std::string>& context) {
        
        std::string prompt = "你是一个知识库助手，请根据以下信息回答用户的问题。\n\n";
        
        // 添加上下文信息
        if (!context.empty()) {
            prompt += "相关信息：\n";
            for (size_t i = 0; i < context.size(); i++) {
                prompt += "[" + std::to_string(i + 1) + "] " + context[i] + "\n";
            }
            prompt += "\n";
        }
        
        prompt += "用户: " + question + "\n";
        prompt += "助手: ";
        
        return prompt;
    }
    
    /**
     * @brief 计算余弦相似度
     * @param v1 向量1
     * @param v2 向量2
     * @return 余弦相似度
     */
    float computeCosineSimilarity(const Embedding& v1, const Embedding& v2) {
        if (v1.size() != v2.size() || v1.empty()) {
            return 0.0f;
        }
        
        float dot = 0.0f;
        float norm1 = 0.0f;
        float norm2 = 0.0f;
        
        for (size_t i = 0; i < v1.size(); i++) {
            dot += v1[i] * v2[i];
            norm1 += v1[i] * v1[i];
            norm2 += v2[i] * v2[i];
        }
        
        if (norm1 <= 0.0f || norm2 <= 0.0f) {
            return 0.0f;
        }
        
        return dot / (std::sqrt(norm1) * std::sqrt(norm2));
    }
    
    ModelInterfacePtr model_; ///< 模型接口
    bool initialized_;        ///< 是否已初始化
};

} // namespace engine
} // namespace kb 