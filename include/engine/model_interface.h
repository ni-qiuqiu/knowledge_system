/**
 * @file model_interface.h
 * @brief 学习引擎模型接口
 */

#ifndef KB_MODEL_INTERFACE_H
#define KB_MODEL_INTERFACE_H

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <future>
#include <optional>

namespace kb {
namespace engine {

/**
 * @brief 模型参数
 */
struct ModelParameters {
    std::string modelPath;          ///< 模型路径
    size_t contextSize = 4096;      ///< 上下文大小
    size_t batchSize = 512;         ///< 批处理大小
    float temperature = 0.7f;       ///< 温度参数
    size_t topK = 40;               ///< Top-K采样参数
    float topP = 0.9f;              ///< Top-P采样参数
    float repeatPenalty = 1.1f;     ///< 重复惩罚
    int threads = 4;                ///< 线程数
    bool useGPU = false;            ///< 是否使用GPU
    int gpuLayers = 0;              ///< GPU层数
    std::unordered_map<std::string, std::string> extraParams; ///< 额外参数
};

/**
 * @brief 生成选项
 */
struct GenerationOptions {
    size_t maxTokens = 4096;         ///< 最大生成token数
    float temperature = 0.7f;       ///< 温度参数
    size_t topK = 40;               ///< Top-K采样参数
    float topP = 0.9f;              ///< Top-P采样参数
    float minP = 0.05f;             ///< Min-P采样参数
    float repeatPenalty = 1.1f;     ///< 重复惩罚
    std::vector<std::string> stopSequences; ///< 停止序列
    bool streamOutput = false;      ///< 是否流式输出
    std::unordered_map<std::string, std::string> extraOptions; ///< 额外选项
};

/**
 * @brief 词元（Token）
 */
struct Token {
    int id;                    ///< 词元ID
    std::string text;          ///< 词元文本
    float probability;         ///< 词元概率
    bool isSpecial;            ///< 是否特殊词元
};

/**
 * @brief 生成结果
 */
struct GenerationResult {
    std::string text;           ///< 生成的文本
    std::vector<Token> tokens;  ///< 生成的词元列表
    float totalProbability;     ///< 总概率
    size_t tokensGenerated;     ///< 生成的词元数量
    double generationTimeMs;    ///< 生成时间（毫秒）
    bool truncated;             ///< 是否被截断
    std::string finishReason;   ///< 结束原因
};

/**
 * @brief 流式输出回调
 */
using StreamCallback = std::function<void(const std::string&, bool)>;

/**
 * @brief 嵌入向量
 */
using Embedding = std::vector<float>;

/**
 * @brief 模型类型
 */
enum class ModelType {
    TEXT_GENERATION,     ///< 文本生成
    TEXT_EMBEDDING,      ///< 文本嵌入
    MULTIMODAL           ///< 多模态
};

/**
 * @brief 模型接口
 */
class ModelInterface {
public:
    virtual ~ModelInterface() = default;
    
    /**
     * @brief 初始化模型
     * @param parameters 模型参数
     * @return 是否成功
     */
    virtual bool initialize(const ModelParameters& parameters) = 0;
    
    /**
     * @brief 释放模型资源
     */
    virtual void release() = 0;
    
    /**
     * @brief 生成文本
     * @param prompt 提示词
     * @param options 生成选项
     * @return 生成结果
     */
    virtual GenerationResult generate(
        const std::string& prompt, 
        const GenerationOptions& options = {}) = 0;
    
    /**
     * @brief 流式生成文本
     * @param prompt 提示词
     * @param callback 回调函数
     * @param options 生成选项
     * @return 生成结果
     */
    virtual GenerationResult generateStream(
        const std::string& prompt, 
        StreamCallback callback,
        const GenerationOptions& options = {}) = 0;
    
    /**
     * @brief 异步生成文本
     * @param prompt 提示词
     * @param options 生成选项
     * @return 生成结果的future
     */
    virtual std::future<GenerationResult> generateAsync(
        const std::string& prompt, 
        const GenerationOptions& options = {}) = 0;
    
    /**
     * @brief 获取文本嵌入向量
     * @param text 文本
     * @return 嵌入向量
     */
    virtual Embedding getEmbedding(const std::string& text) = 0;
    
    /**
     * @brief 批量获取文本嵌入向量
     * @param texts 文本列表
     * @return 嵌入向量列表
     */
    virtual std::vector<Embedding> getEmbeddings(const std::vector<std::string>& texts) = 0;
    
    /**
     * @brief 词元化
     * @param text 文本
     * @return 词元列表
     */
    virtual std::vector<Token> tokenize(const std::string& text) = 0;
    
    /**
     * @brief 去词元化
     * @param tokens 词元列表
     * @return 文本
     */
    virtual std::string detokenize(const std::vector<Token>& tokens) = 0;
    
    /**
     * @brief 获取词元数量
     * @param text 文本
     * @return 词元数量
     */
    virtual size_t getTokenCount(const std::string& text) = 0;
    
    /**
     * @brief 获取模型类型
     * @return 模型类型
     */
    virtual ModelType getModelType() const = 0;
    
    /**
     * @brief 获取模型参数
     * @return 模型参数
     */
    virtual ModelParameters getParameters() const = 0;
    
    /**
     * @brief 获取模型名称
     * @return 模型名称
     */
    virtual std::string getModelName() const = 0;
    
    /**
     * @brief 获取模型版本
     * @return 模型版本
     */
    virtual std::string getModelVersion() const = 0;
    
    /**
     * @brief 是否已初始化
     * @return 是否已初始化
     */
    virtual bool isInitialized() const = 0;
};

using ModelInterfacePtr = std::shared_ptr<ModelInterface>;

/**
 * @brief 模型工厂接口
 */
class ModelFactory {
public:
    virtual ~ModelFactory() = default;
    
    /**
     * @brief 创建模型
     * @param modelType 模型类型
     * @param modelPath 模型路径
     * @param parameters 模型参数
     * @return 模型接口
     */
    virtual ModelInterfacePtr createModel(
        ModelType modelType,
        const std::string& modelPath,
        const ModelParameters& parameters = {}) = 0;
    
    /**
     * @brief 获取可用模型列表
     * @return 模型路径列表
     */
    virtual std::vector<std::string> getAvailableModels() const = 0;
    
    /**
     * @brief 获取单例实例
     * @return 模型工厂实例
     */
    static ModelFactory& getInstance();
};

} // namespace engine
} // namespace kb

#endif // KB_MODEL_INTERFACE_H 