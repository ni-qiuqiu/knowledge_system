/**
 * @file llama_model.h
 * @brief llama.cpp模型适配器
 */

#ifndef KB_LLAMA_MODEL_H
#define KB_LLAMA_MODEL_H

#include "engine/model_interface.h"
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>
#include <atomic>

// 前向声明llama.cpp的类型，避免包含头文件
struct llama_context;
struct llama_model;
struct llama_token_data;
struct llama_vocab;
struct llama_sampler;

namespace kb {
namespace engine {

/**
 * @brief llama模型适配器
 */
class LlamaModel : public ModelInterface {
public:
    /**
     * @brief 构造函数
     */
    LlamaModel();
    
    /**
     * @brief 析构函数
     */
    ~LlamaModel() override;
    
    /**
     * @brief 初始化模型
     * @param parameters 模型参数
     * @return 是否成功
     */
    bool initialize(const ModelParameters& parameters) override;
    
    /**
     * @brief 释放模型资源
     */
    void release() override;
    
    /**
     * @brief 生成文本
     * @param prompt 提示词
     * @param options 生成选项
     * @return 生成结果
     */
    GenerationResult generate(
        const std::string& prompt, 
        const GenerationOptions& options = {}) override;
    
    /**
     * @brief 流式生成文本
     * @param prompt 提示词
     * @param callback 回调函数
     * @param options 生成选项
     * @return 生成结果
     */
    GenerationResult generateStream(
        const std::string& prompt, 
        StreamCallback callback,
        const GenerationOptions& options = {}) override;
    
    /**
     * @brief 异步生成文本
     * @param prompt 提示词
     * @param options 生成选项
     * @return 生成结果的future
     */
    std::future<GenerationResult> generateAsync(
        const std::string& prompt, 
        const GenerationOptions& options = {}) override;
    
    /**
     * @brief 获取文本嵌入向量
     * @param text 文本
     * @return 嵌入向量
     */
    Embedding getEmbedding(const std::string& text) override;
    
    /**
     * @brief 批量获取文本嵌入向量
     * @param texts 文本列表
     * @return 嵌入向量列表
     */
    std::vector<Embedding> getEmbeddings(const std::vector<std::string>& texts) override;
    
    /**
     * @brief 词元化
     * @param text 文本
     * @return 词元列表
     */
    std::vector<Token> tokenize(const std::string& text) override;
    
    /**
     * @brief 去词元化
     * @param tokens 词元列表
     * @return 文本
     */
    std::string detokenize(const std::vector<Token>& tokens) override;
    
    /**
     * @brief 获取词元数量
     * @param text 文本
     * @return 词元数量
     */
    size_t getTokenCount(const std::string& text) override;
    
    /**
     * @brief 获取模型类型
     * @return 模型类型
     */
    ModelType getModelType() const override;
    
    /**
     * @brief 获取模型参数
     * @return 模型参数
     */
    ModelParameters getParameters() const override;
    
    /**
     * @brief 获取模型名称
     * @return 模型名称
     */
    std::string getModelName() const override;
    
    /**
     * @brief 获取模型版本
     * @return 模型版本
     */
    std::string getModelVersion() const override;
    
    /**
     * @brief 是否已初始化
     * @return 是否已初始化
     */
    bool isInitialized() const override;
    
private:
    /**
     * @brief 采样函数
     * @param candidates 候选词元
     * @param options 生成选项
     * @return 选中的词元
     */
    int sampleToken(
        const std::vector<llama_token_data>& candidates,
        const GenerationOptions& options) const;
    
    /**
     * @brief 处理停止序列
     * @param text 当前文本
     * @param stopSequences 停止序列
     * @return 是否需要停止
     */
    bool shouldStop(
        const std::string& text,
        const std::vector<std::string>& stopSequences) const;
    
    /**
     * @brief 实际生成文本实现
     * @param prompt 提示词
     * @param options 生成选项
     * @param callback 回调函数
     * @return 生成结果
     */
    GenerationResult generateInternal(
        const std::string& prompt, 
        const GenerationOptions& options,
        StreamCallback callback = nullptr);
    
    // llama.cpp模型相关
    llama_model* model_;           ///< llama模型
    llama_context* context_;       ///< llama上下文
    const llama_vocab* vocab_;     ///< llama词汇表
    llama_sampler* smpl_;          ///< llama采样器
    ModelParameters parameters_;   ///< 模型参数
    std::string modelName_;        ///< 模型名称
    std::string modelVersion_;     ///< 模型版本
    bool initialized_;             ///< 是否已初始化
    
    // 并发控制
    mutable std::mutex mutex_;            ///< 互斥锁
    std::queue<std::function<void()>> taskQueue_;  ///< 任务队列
    std::thread workerThread_;            ///< 工作线程
    std::condition_variable condition_;   ///< 条件变量
    std::atomic<bool> shouldStop_;        ///< 是否应该停止
};

/**
 * @brief llama模型工厂
 */
class LlamaModelFactory : public ModelFactory {
public:
    /**
     * @brief 获取单例实例
     * @return 模型工厂实例
     */
    static LlamaModelFactory& getInstance();
    
    /**
     * @brief 创建模型
     * @param modelType 模型类型
     * @param modelPath 模型路径
     * @param parameters 模型参数
     * @return 模型接口
     */
    ModelInterfacePtr createModel(
        ModelType modelType,
        const std::string& modelPath,
        const ModelParameters& parameters = {}) override;
    
    /**
     * @brief 获取可用模型列表
     * @return 模型路径列表
     */
    std::vector<std::string> getAvailableModels() const override;
    
private:
    /**
     * @brief 构造函数
     */
    LlamaModelFactory();
    
    /**
     * @brief 扫描模型目录
     * @param directory 目录路径
     */
    void scanModelDirectory(const std::string& directory);
    
    std::vector<std::string> availableModels_;  ///< 可用模型列表
    mutable std::mutex mutex_;                  ///< 互斥锁
};

} // namespace engine
} // namespace kb

#endif // KB_LLAMA_MODEL_H 