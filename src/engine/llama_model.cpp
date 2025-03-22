/**
 * @file llama_model.cpp
 * @brief llama.cpp模型适配器实现
 */

#include "engine/llama_model.h"
#include "common/logging.h"
#include "common/config.h"
#include <chrono>
#include <random>
#include <algorithm>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <cassert>
#include <cstring>
#include <cmath>
#include <cfloat>
#include <cstddef>



// 检查是否使用模拟实现
#ifndef LLAMA_CPP_MOCK

// 包含llama.cpp头文件
#include "llama.h"

#else

// 模拟llama.cpp的结构，用于没有llama.cpp库的情况
struct llama_context {};
struct llama_model {};
struct llama_token_data {
    int id;
    float logit;
    float p;
};
typedef int llama_token;
// 模拟的llama.cpp函数
inline llama_model* llama_load_model_from_file(const char*, const struct llama_model_params) { return nullptr; }
inline llama_context* llama_new_context_with_model(llama_model*, const struct llama_context_params) { return nullptr; }
inline void llama_free(llama_context*) {}
inline void llama_free_model(llama_model*) {}
inline int llama_tokenize(const struct llama_vocab*, const char*, int, llama_token*, int, bool, bool) { return 0; }
inline int llama_n_vocab(const struct llama_vocab*) { return 0; }
inline float* llama_get_logits(struct llama_context*) { return nullptr; }
inline int llama_token_to_piece(const struct llama_vocab*, llama_token, char*, int, int, bool) { return 0; }
inline struct llama_batch llama_batch_init(int32_t, int32_t, int32_t) { return {}; }
inline int llama_decode(struct llama_context*, struct llama_batch) { return 0; }
inline int llama_n_ctx(const struct llama_context*) { return 0; }
inline const struct llama_vocab* llama_model_get_vocab(const struct llama_model*) { return nullptr; }
#endif

namespace kb {
namespace engine {

// LlamaModel实现
LlamaModel::LlamaModel() 
    : model_(nullptr), context_(nullptr), vocab_(nullptr), initialized_(false), shouldStop_(false) {
    LOG_INFO("LlamaModel创建");
    
    // 启动工作线程
    workerThread_ = std::thread([this]() {
        LOG_INFO("LlamaModel工作线程启动");
        
        std::unique_lock<std::mutex> lock(mutex_);
        while (!shouldStop_) {
            if (taskQueue_.empty()) {
                // 等待任务
                condition_.wait(lock, [this]() { 
                    return !taskQueue_.empty() || shouldStop_; 
                });
                
                if (shouldStop_) {
                    break;
                }
            }
            
            // 取出任务执行
            auto task = std::move(taskQueue_.front());
            taskQueue_.pop();
            
            lock.unlock();
            task();
            lock.lock();
        }
        
        LOG_INFO("LlamaModel工作线程退出");
    });
}

LlamaModel::~LlamaModel() {
    LOG_INFO("LlamaModel销毁");
    
    // 释放资源
    release();
    
    // 停止工作线程
    {
        std::lock_guard<std::mutex> lock(mutex_);
        shouldStop_ = true;
    }
    
    condition_.notify_one();
    
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
}

bool LlamaModel::initialize(const ModelParameters& parameters) {
    LOG_INFO("初始化LlamaModel，模型路径：{}", parameters.modelPath);
    
    // 设置UTF-8本地化
    std::locale::global(std::locale("en_US.UTF-8"));
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 保存参数
    parameters_ = parameters;
    
    if (parameters.modelPath.empty()) {
        LOG_ERROR("模型路径为空");
        return false;
    }
    
    // 检查文件是否存在
    if (!std::filesystem::exists(parameters.modelPath)) {
        LOG_ERROR("模型文件不存在：{}", parameters.modelPath);
        return false;
    }
    
    // 检查文件大小
    auto fileSize = std::filesystem::file_size(parameters.modelPath);
    if (fileSize == 0) {
        LOG_ERROR("模型文件大小为0：{}", parameters.modelPath);
        return false;
    }
    LOG_INFO("模型文件大小：{} MB", fileSize / (1024 * 1024));
    
#ifndef LLAMA_CPP_MOCK
    // 释放旧资源
    //release();
    
    try {
         // load dynamic backends
        ggml_backend_load_all();
        // 加载模型
        llama_model_params model_params = llama_model_default_params();
        model_params.n_gpu_layers = 99;//parameters.useGPU ? parameters.gpuLayers : 0;
        model_params.vocab_only = false;
        
        // 使用新的API加载模型
        model_ = llama_model_load_from_file(parameters.modelPath.c_str(), model_params);
        
        if (!model_) {
            LOG_ERROR("加载模型失败：{}", parameters.modelPath);
            return false;
        }
        LOG_INFO("模型文件加载成功");
        
        // 获取词汇表
        vocab_ = llama_model_get_vocab(model_);
        if (!vocab_) {
            LOG_ERROR("获取词汇表失败");
            llama_model_free(model_);
            model_ = nullptr;
            return false;
        }
        LOG_INFO("词汇表获取成功，词表大小：{}", llama_n_vocab(vocab_));
        
        // 创建上下文
        llama_context_params ctx_params = llama_context_default_params();
        ctx_params.n_ctx = parameters.contextSize;
        ctx_params.n_batch = parameters.contextSize;  // 使用相同的上下文大小
        ctx_params.n_threads = parameters.threads;
        ctx_params.n_threads_batch = parameters.threads;
        //ctx_params.rope_freq_base = 10000.0f;  // 设置RoPE基础频率
        //ctx_params.rope_freq_scale = 1.0f;     // 设置RoPE缩放因子
        
        // 使用新的API创建上下文
        context_ = llama_init_from_model(model_, ctx_params);
        
        if (!context_) {
            LOG_ERROR("创建上下文失败");
            llama_model_free(model_);
            model_ = nullptr;
            return false;
        }
        LOG_INFO("上下文创建成功，上下文大小：{}", llama_n_ctx(context_));
         
        // 创建采样器
        smpl_ = llama_sampler_chain_init(llama_sampler_chain_default_params());
        // 添加min_p采样器，过滤掉概率低于5%的词元
        llama_sampler_chain_add(smpl_, llama_sampler_init_min_p(0.05f, 1));
        // 添加温度采样器
        llama_sampler_chain_add(smpl_, llama_sampler_init_temp(parameters.temperature));
        // 添加随机分布采样器
        llama_sampler_chain_add(smpl_, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));
        
        // 解析模型元数据
        std::filesystem::path modelPath(parameters.modelPath);
        modelName_ = modelPath.stem().string();
        modelVersion_ = "1.0.0"; // 可以从模型中读取，目前简化处理
        
        initialized_ = true;
        LOG_INFO("LlamaModel初始化成功");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("初始化LlamaModel异常：{}", e.what());
        release();
        return false;
    }
#else
    // 模拟实现，直接返回成功
    modelName_ = "MockLlamaModel";
    modelVersion_ = "1.0.0";
    initialized_ = true;
    LOG_INFO("LlamaModel模拟初始化成功");
    return true;
#endif
}

void LlamaModel::release() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return;
    }
    
#ifndef LLAMA_CPP_MOCK
    // 释放llama.cpp资源
    if (context_) {
        llama_free(context_);
        context_ = nullptr;
    }
    LOG_INFO("context_释放成功");
    if (model_) {
        llama_model_free(model_);
        model_ = nullptr;
    }
    LOG_INFO("model_释放成功");
    vocab_ = nullptr; // 词汇表由模型管理，不需要单独释放
#endif
    
    initialized_ = false;
    LOG_INFO("LlamaModel资源已释放");
}

GenerationResult LlamaModel::generate(
    const std::string& prompt, 
    const GenerationOptions& options) {
    
    return generateInternal(prompt, options);
}

GenerationResult LlamaModel::generateStream(
    const std::string& prompt, 
    StreamCallback callback,
    const GenerationOptions& options) {
    
    return generateInternal(prompt, options, callback);
}

std::future<GenerationResult> LlamaModel::generateAsync(
    const std::string& prompt, 
    const GenerationOptions& options) {
    
    auto promise = std::make_shared<std::promise<GenerationResult>>();
    auto future = promise->get_future();
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        taskQueue_.push([this, prompt, options, promise]() {
            try {
                auto result = generateInternal(prompt, options);
                promise->set_value(result);
            } catch (const std::exception& e) {
                LOG_ERROR("异步生成异常：{}", e.what());
                promise->set_exception(std::current_exception());
            }
        });
    }
    
    condition_.notify_one();
    return future;
}

Embedding LlamaModel::getEmbedding(const std::string& text) {
    LOG_INFO("获取文本嵌入：{}", text.substr(0, 30) + (text.length() > 30 ? "..." : ""));
    
    if (!initialized_) {
        LOG_ERROR("模型未初始化");
        return {};
    }
    
#ifndef LLAMA_CPP_MOCK
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // 将文本转换为词元
        std::vector<llama_token> tokens;
        tokens.resize(text.length() + 10);  // 预留空间
        
        int n_tokens = llama_tokenize(
            vocab_, 
            text.c_str(), 
            static_cast<int32_t>(text.length()),
            tokens.data(),
            static_cast<int32_t>(tokens.size()),
            true,  // 添加BOS
            false  // 不添加特殊词元
        );
        
        if (n_tokens <= 0) {
            LOG_ERROR("词元化失败");
            return {};
        }
        
        tokens.resize(n_tokens);
        
        // 运行模型
        llama_batch batch = llama_batch_get_one(tokens.data(), n_tokens);
        
        if (llama_decode(context_, batch) != 0) {
            LOG_ERROR("模型评估失败");
            return {};
        }
        
        // 获取最后一层的嵌入向量
        float* logits = llama_get_logits(context_);
        int n_vocab = llama_vocab_n_tokens(vocab_);
        
        if (!logits || n_vocab <= 0) {
            LOG_ERROR("获取logits失败");
            return {};
        }
        
        // 创建新的向量来存储嵌入
        Embedding embedding;
        embedding.reserve(n_vocab);
        
        // 复制logits到embedding向量
        for (int i = 0; i < n_vocab; ++i) {
            embedding.push_back(logits[i]);
        }
        
        // 归一化
        float sum = 0.0f;
        for (auto& value : embedding) {
            sum += value * value;
        }
        
        if (sum > 0.0f) {
            float norm = std::sqrt(sum);
            for (auto& value : embedding) {
                value /= norm;
            }
        }
        
        return embedding;
    } catch (const std::exception& e) {
        LOG_ERROR("获取嵌入异常：{}", e.what());
        return {};
    }
#else
    // 模拟实现
    Embedding mockEmbedding(384);
    std::mt19937 rng(std::hash<std::string>{}(text));
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    
    for (auto& value : mockEmbedding) {
        value = dist(rng);
    }
    
    // 归一化
    float sum = 0.0f;
    for (auto& value : mockEmbedding) {
        sum += value * value;
    }
    
    if (sum > 0.0f) {
        float norm = std::sqrt(sum);
        for (auto& value : mockEmbedding) {
            value /= norm;
        }
    }
    
    return mockEmbedding;
#endif
}

std::vector<Embedding> LlamaModel::getEmbeddings(const std::vector<std::string>& texts) {
    LOG_INFO("批量获取嵌入，数量：{}", texts.size());
    
    std::vector<Embedding> embeddings;
    embeddings.reserve(texts.size());
    
    for (const auto& text : texts) {
        embeddings.push_back(getEmbedding(text));
    }
    
    return embeddings;
}

std::vector<Token> LlamaModel::tokenize(const std::string& text) {
    LOG_INFO("词元化文本：{}", text.substr(0, 30) + (text.length() > 30 ? "..." : ""));
    
    if (!initialized_) {
        LOG_ERROR("模型未初始化");
        return {};
    }
    
#ifndef LLAMA_CPP_MOCK
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // 将文本转换为词元
        std::vector<llama_token> tokens;
        tokens.resize(text.length() + 10);  // 预留空间
        
        int n_tokens = llama_tokenize(
            vocab_, 
            text.c_str(), 
            text.length(),
            tokens.data(),
            tokens.size(),
            false,  // 不添加BOS
            false   // 不添加特殊词元
        );
        
        if (n_tokens <= 0) {
            LOG_ERROR("词元化失败");
            return {};
        }
        
        tokens.resize(n_tokens);
        
        // 构建Token对象
        std::vector<Token> result;
        result.reserve(tokens.size());
        
        for (const auto& token_id : tokens) {
            Token token;
            token.id = token_id;
            
            // 获取词元文本
            //std::string tokenText = llama_vocab_get_text(vocab_, token_id);
            char buf[256];
            int n = llama_token_to_piece(vocab_, token_id, buf, sizeof(buf), 0, true);
            if (n < 0) {
                LOG_ERROR("转换词元失败");
                continue;
            }
            std::string tokenText(buf, n);
            if (!tokenText.empty()) {
                token.text = tokenText;
            }
            
            token.probability = 1.0f;  // 这里简化处理
            token.isSpecial = token_id < 0;  // 特殊词元通常是负数
            
            result.push_back(token);
        }
        
        return result;
    } catch (const std::exception& e) {
        LOG_ERROR("词元化异常：{}", e.what());
        return {};
    }
#else
    // 模拟实现
    std::vector<Token> mockTokens;
    std::istringstream iss(text);
    std::string word;
    int id = 1;
    
    while (iss >> word) {
        Token token;
        token.id = id++;
        token.text = word;
        token.probability = 1.0f;
        token.isSpecial = false;
        mockTokens.push_back(token);
    }
    
    return mockTokens;
#endif
}

std::string LlamaModel::detokenize(const std::vector<Token>& tokens) {
    LOG_INFO("去词元化，词元数量：{}", tokens.size());
    
    if (!initialized_) {
        LOG_ERROR("模型未初始化");
        return "";
    }
    
#ifndef LLAMA_CPP_MOCK
    std::string result;
    result.reserve(tokens.size() * 4); // 每个词元平均4个字节
    
    for (const auto& token : tokens) {
        if (!token.text.empty()) {
            result += token.text;
        }
    }
    
    return result;
#else
    // 模拟实现
    std::string result;
    
    for (const auto& token : tokens) {
        result += token.text + " ";
    }
    
    if (!result.empty()) {
        result.pop_back();  // 移除最后的空格
    }
    
    return result;
#endif
}

size_t LlamaModel::getTokenCount(const std::string& text) {
    LOG_INFO("计算词元数量：{}", text.substr(0, 30) + (text.length() > 30 ? "..." : ""));
    
    if (!initialized_) {
        LOG_ERROR("模型未初始化");
        return 0;
    }
    
#ifndef LLAMA_CPP_MOCK
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // 将文本转换为词元
        std::vector<llama_token> tokens;
        tokens.resize(text.length() + 10);  // 预留空间
        
        int n_tokens = llama_tokenize(
            vocab_, 
            text.c_str(), 
            static_cast<int32_t>(text.length()),
            tokens.data(),
            static_cast<int32_t>(tokens.size()),
            false,  // 不添加BOS
            false   // 不添加特殊词元
        );
        
        return n_tokens > 0 ? n_tokens : 0;
    } catch (const std::exception& e) {
        LOG_ERROR("计算词元数量异常：{}", e.what());
        return 0;
    }
#else
    // 模拟实现：简单地按空格分词
    std::istringstream iss(text);
    std::string word;
    size_t count = 0;
    
    while (iss >> word) {
        count++;
    }
    
    return count;
#endif
}

ModelType LlamaModel::getModelType() const {
    // 目前只支持文本生成
    return ModelType::TEXT_GENERATION;
}

ModelParameters LlamaModel::getParameters() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return parameters_;
}

std::string LlamaModel::getModelName() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return modelName_;
}

std::string LlamaModel::getModelVersion() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return modelVersion_;
}

bool LlamaModel::isInitialized() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return initialized_;
}

int LlamaModel::sampleToken(
    const std::vector<llama_token_data>& candidates,
    const GenerationOptions& options) const {
    
    if (candidates.empty()) {
        LOG_ERROR("候选词元为空");
        return -1;
    }
    
    // 如果温度接近零，直接选择概率最高的词元
    if (options.temperature < 1e-5) {
        return candidates[0].id;
    }
    
    // 计算 top_k 和 top_p 采样
    std::vector<llama_token_data> sorted_candidates = candidates;
    size_t topK = std::min(options.topK, sorted_candidates.size());
    
    if (topK > 0 && topK < sorted_candidates.size()) {
        std::partial_sort(
            sorted_candidates.begin(),
            sorted_candidates.begin() + topK,
            sorted_candidates.end(),
            [](const llama_token_data& a, const llama_token_data& b) {
                return a.p > b.p;
            }
        );
        sorted_candidates.resize(topK);
    }
    
    // top_p 采样
    if (options.topP > 0.0f && options.topP < 1.0f) {
        std::sort(
            sorted_candidates.begin(),
            sorted_candidates.end(),
            [](const llama_token_data& a, const llama_token_data& b) {
                return a.p > b.p;
            }
        );
        
        float cumsum = 0.0f;
        size_t i = 0;
        while (i < sorted_candidates.size()) {
            cumsum += sorted_candidates[i].p;
            if (cumsum >= options.topP) {
                i++;
                break;
            }
            i++;
        }
        
        sorted_candidates.resize(i);
    }
    
    // 应用温度
    if (options.temperature > 0.0f && options.temperature != 1.0f) {
        float sum = 0.0f;
        for (auto& candidate : sorted_candidates) {
            candidate.p = expf(logf(candidate.p) / options.temperature);
            sum += candidate.p;
        }
        
        // 重新归一化概率
        if (sum > 0.0f) {
            for (auto& candidate : sorted_candidates) {
                candidate.p /= sum;
            }
        }
    }
    
    // 抽样
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    float r = dist(gen);
    
    float cdf = 0.0f;
    for (const auto& candidate : sorted_candidates) {
        cdf += candidate.p;
        if (r < cdf) {
            return candidate.id;
        }
    }
    
    // 默认返回第一个
    return sorted_candidates[0].id;
}

bool LlamaModel::shouldStop(
    const std::string& text,
    const std::vector<std::string>& stopSequences) const {
    
    if (stopSequences.empty()) {
        return false;
    }
    
    for (const auto& stopSeq : stopSequences) {
        if (text.length() >= stopSeq.length()) {
            size_t pos = text.length() - stopSeq.length();
            if (text.substr(pos) == stopSeq) {
                return true;
            }
        }
    }
    
    return false;
}

GenerationResult LlamaModel::generateInternal(
    const std::string& prompt, 
    const GenerationOptions& options,
    StreamCallback callback) {
    
    LOG_INFO("生成文本，提示词：{}", prompt.substr(0, 30) + (prompt.length() > 30 ? "..." : ""));
    
    GenerationResult result;
    result.text = "";
    result.tokensGenerated = 0;
    result.totalProbability = 1.0f;
    result.truncated = false;
    result.finishReason = "completed";
    
    if (!initialized_) {
        LOG_ERROR("模型未初始化");
        result.finishReason = "error";
        return result;
    }
    
    // 记录开始时间
    auto startTime = std::chrono::high_resolution_clock::now();
    
#ifndef LLAMA_CPP_MOCK
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // 检查是否是首次生成
        const bool is_first = llama_kv_self_used_cells(context_) == 0;
        
        // 获取chat模板
        const char* tmpl = llama_model_chat_template(model_, nullptr);
        if (!tmpl) {
            LOG_ERROR("获取chat模板失败");
            result.finishReason = "error";
            return result;
        }
        
        // 准备消息列表
        std::vector<llama_chat_message> messages;
        messages.push_back({"user", strdup(prompt.c_str())});
        
        // 准备格式化缓冲区
        std::vector<char> formatted(llama_n_ctx(context_));
        
        // 应用chat模板
        int new_len = llama_chat_apply_template(
            tmpl,
            messages.data(),
            messages.size(),
            true,
            formatted.data(),
            formatted.size()
        );
        
        // 如果缓冲区不足，重新分配
        if (new_len > (int)formatted.size()) {
            formatted.resize(new_len);
            new_len = llama_chat_apply_template(
                tmpl,
                messages.data(),
                messages.size(),
                true,
                formatted.data(),
                formatted.size()
            );
        }
        
        if (new_len < 0) {
            LOG_ERROR("应用chat模板失败");
            result.finishReason = "error";
            return result;
        }
        
        // 获取实际的提示词
        std::string actual_prompt(formatted.begin(), formatted.begin() + new_len);
        
        // 词元化提示词
        const int n_prompt_tokens = -llama_tokenize(
            vocab_,
            actual_prompt.c_str(),
            actual_prompt.size(),
            NULL,
            0,
            is_first,
            true
        );
        
        if (n_prompt_tokens <= 0) {
            LOG_ERROR("词元化失败");
            result.finishReason = "error";
            return result;
        }
        
        std::vector<llama_token> prompt_tokens(n_prompt_tokens);
        if (llama_tokenize(
            vocab_,
            actual_prompt.c_str(),
            actual_prompt.size(),
            prompt_tokens.data(),
            prompt_tokens.size(),
            is_first,
            true
        ) < 0) {
            LOG_ERROR("词元化失败");
            result.finishReason = "error";
            return result;
        }
        
        // 检查上下文容量
        int n_ctx = llama_n_ctx(context_);
        int n_ctx_used = llama_kv_self_used_cells(context_);
        if (n_ctx_used + prompt_tokens.size() > n_ctx) {
            LOG_WARN("上下文空间不足");
            result.truncated = true;
            result.finishReason = "length";
            return result;
        }
        
        // 准备batch并处理提示词
        llama_batch batch = llama_batch_get_one(prompt_tokens.data(), prompt_tokens.size());
        if (llama_decode(context_, batch) != 0) {
            LOG_ERROR("处理提示词失败");
            result.finishReason = "error";
            return result;
        }
        
        // 生成响应
        std::string response;
        llama_token new_token_id;
        size_t tokens_generated = 0;
        
        //while (tokens_generated < options.maxTokens) 
        while(1)
        {
            // 检查上下文容量
            n_ctx_used = llama_kv_self_used_cells(context_);
            if (n_ctx_used + 1 > n_ctx) {
                LOG_WARN("生成过程中上下文空间不足");
                result.truncated = true;
                result.finishReason = "length";
                break;
            }
            
            // 采样下一个词元
            new_token_id = llama_sampler_sample(smpl_, context_, -1);
            
            // 检查是否到达生成结束
            if (llama_vocab_is_eog(vocab_, new_token_id)) {
                LOG_INFO("遇到结束标记");
                result.finishReason = "stop";
                break;
            }
            
            // 转换词元为文本
            char buf[256];
            int n = llama_token_to_piece(vocab_, new_token_id, buf, sizeof(buf), 0, true);
            if (n < 0) {
                LOG_ERROR("转换词元失败");
                result.finishReason = "error";
                break;
            }
            
            std::string piece(buf, n);
            
            // 流式输出
            if (callback && options.streamOutput) {
                callback(piece, false);
            }
            
            // 添加到响应中
            response += piece;
            tokens_generated++;
            
            // 检查停止条件
            // if (shouldStop(response, options.stopSequences)) {
            //     LOG_INFO("触发停止序列");
            //     result.finishReason = "stop";
            //     break;
            // }
            
            // 处理当前词元
            batch = llama_batch_get_one(&new_token_id, 1);
            if (llama_decode(context_, batch) != 0) {
                LOG_ERROR("处理生成的词元失败");
                result.finishReason = "error";
                break;
            }
        }
        
        // 保存结果
        result.text = response;
        result.tokensGenerated = tokens_generated;
        
        // 添加助手回复到消息列表
        messages.push_back({"assistant", strdup(response.c_str())});
        
        // 清理消息内存
        for (auto& msg : messages) {
            free(const_cast<char*>(msg.content));
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("生成过程发生异常: {}", e.what());
        result.finishReason = "error";
    }
    
#else
    // 模拟实现
    result.text = "这是一个模拟的文本生成结果，基于提示词：" + prompt.substr(0, 30);
    result.tokensGenerated = 20;
    
    if (callback && options.streamOutput) {
        std::string mockText = result.text;
        for (size_t i = 0; i < mockText.length(); i += 5) {
            size_t len = std::min(5UL, mockText.length() - i);
            std::string chunk = mockText.substr(i, len);
            callback(chunk, i + len >= mockText.length());
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
#endif
    
    // 计算生成时间
    auto endTime = std::chrono::high_resolution_clock::now();
    result.generationTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime).count();
    
    LOG_INFO("生成文本完成，生成了 {} 个词元，耗时 {} 毫秒", 
             result.tokensGenerated, result.generationTimeMs);
    
    return result;
}

// LlamaModelFactory实现
LlamaModelFactory& LlamaModelFactory::getInstance() {
    static LlamaModelFactory instance;
    return instance;
}

LlamaModelFactory::LlamaModelFactory() {
    LOG_INFO("LlamaModelFactory初始化");
    
    // 扫描模型目录
    std::string modelDir = kb::common::Config::getInstance().getModelDir();
    if (!modelDir.empty()) {
        scanModelDirectory(modelDir);
    }
}

ModelInterfacePtr LlamaModelFactory::createModel(
    ModelType modelType,
    const std::string& modelPath,
    const ModelParameters& parameters) {
    
    LOG_INFO("创建LlamaModel，类型：{}，路径：{}", 
             static_cast<int>(modelType), modelPath);
    
    if (modelType != ModelType::TEXT_GENERATION && 
        modelType != ModelType::TEXT_EMBEDDING) {
        LOG_ERROR("不支持的模型类型：{}", static_cast<int>(modelType));
        return nullptr;
    }
    
    try {
        auto model = std::make_shared<LlamaModel>();
        
        // 如果提供了模型路径，立即初始化
        if (!modelPath.empty()) {
            ModelParameters params = parameters;
            params.modelPath = modelPath;
            
            if (!model->initialize(params)) {
                LOG_ERROR("初始化LlamaModel失败");
                return nullptr;
            }
        }
        
        return model;
    } catch (const std::exception& e) {
        LOG_ERROR("创建LlamaModel异常：{}", e.what());
        return nullptr;
    }
}

std::vector<std::string> LlamaModelFactory::getAvailableModels() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return availableModels_;
}

void LlamaModelFactory::scanModelDirectory(const std::string& directory) {
    LOG_INFO("扫描模型目录：{}", directory);
    
    std::lock_guard<std::mutex> lock(mutex_);
    availableModels_.clear();
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                if (ext == ".bin" || ext == ".gguf" || ext == ".ggml") {
                    availableModels_.push_back(entry.path().string());
                    LOG_INFO("发现模型：{}", entry.path().string());
                }
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("扫描模型目录异常：{}", e.what());
    }
}

} // namespace engine
} // namespace kb 