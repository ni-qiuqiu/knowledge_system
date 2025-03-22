/**
 * @file answer_generator.cpp
 * @brief 回答生成器实现
 */

#include "query/answer_generator.h"
#include "common/logging.h"
#include <algorithm>
#include <sstream>
#include <chrono>
#include <thread>
#include <regex>

namespace kb {
namespace query {

// 声明获取查询类型名称的辅助函数
std::string getQueryTypeName(QueryType type);

// 构造函数
AnswerGenerator::AnswerGenerator(std::shared_ptr<engine::ModelInterface> model)
    : model_(model) {
    
    if (!model_) {
        LOG_ERROR("语言模型指针为空，回答生成器初始化失败");
        throw std::invalid_argument("语言模型指针不能为空");
    }
    
    LOG_INFO("回答生成器初始化完成");
}

// 析构函数
AnswerGenerator::~AnswerGenerator() {
    LOG_INFO("回答生成器释放资源");
}

// 生成回答
AnswerGenerationResult AnswerGenerator::generateAnswer(
    const QueryParseResult& parseResult,
    const SearchResult& searchResult,
    const AnswerGenerationOptions& options) {
    
    LOG_INFO("开始生成回答，查询类型: {}", getQueryTypeName(parseResult.intent.type));
    
    AnswerGenerationResult result;
    
    // 记录开始时间
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 根据查询类型和搜索结果的特点，决定是直接构建回答还是使用模型生成
    if (!options.useModelGeneration ||
        parseResult.intent.type == QueryType::FACT ||
        parseResult.intent.type == QueryType::ATTRIBUTE ||
        (searchResult.items.size() == 1 && searchResult.items[0].relevanceScore > 0.9f)) {
        
        // 直接从搜索结果构建回答
        result = buildDirectAnswer(parseResult, searchResult, options);
        result.isDirectAnswer = true;
        result.isModelGenerated = false;
        
        LOG_INFO("直接从搜索结果构建回答，置信度: {:.2f}", result.confidence);
    }
    else {
        // 使用语言模型生成回答
        if (model_) {
            // 构建提示词
            std::string prompt = buildPrompt(parseResult, searchResult, options);
            
            // 生成参数
            engine::GenerationOptions genOptions;
            genOptions.maxTokens = options.maxLength;
            genOptions.temperature = 0.7f;
            genOptions.topP = 0.9f;
            
            // 调用模型生成
            engine::GenerationResult genResult = model_->generate(prompt, genOptions);
            
            // 后处理回答
            result.answer = postprocessAnswer(genResult.text, parseResult, options);
            result.isModelGenerated = true;
            
            // 计算置信度
            result.confidence = calculateConfidence(result.answer, searchResult);
            
            LOG_INFO("使用语言模型生成回答，置信度: {:.2f}", result.confidence);
        }
        else {
            LOG_ERROR("语言模型不可用，回退到直接回答");
            result = buildDirectAnswer(parseResult, searchResult, options);
            result.isDirectAnswer = true;
            result.isModelGenerated = false;
        }
    }
    
    // 添加来源信息
    if (options.includeSourceInfo) {
        for (const auto& item : searchResult.items) {
            if (item.triple) {
                std::string source = "Triple: " + item.triple->getSubject()->getName() +
                                    " - " + item.triple->getRelation()->getName() +
                                    " - " + item.triple->getObject()->getName();
                result.sources.push_back(source);
            }
        }
    }
    
    // 记录结束时间
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    LOG_INFO("回答生成完成，耗时: {:.2f}ms", static_cast<float>(duration.count()));
    
    return result;
}

// 流式生成回答
void AnswerGenerator::generateAnswerStream(
    const QueryParseResult& parseResult,
    const SearchResult& searchResult,
    StreamAnswerCallback callback,
    const AnswerGenerationOptions& options) {
    
    LOG_INFO("开始流式生成回答，查询类型: {}", getQueryTypeName(parseResult.intent.type));
    
    // 如果不使用模型生成，或者特定查询类型适合直接回答，则不使用流式生成
    if (!options.useModelGeneration ||
        parseResult.intent.type == QueryType::FACT ||
        parseResult.intent.type == QueryType::ATTRIBUTE ||
        (searchResult.items.size() == 1 && searchResult.items[0].relevanceScore > 0.9f)) {
        
        // 直接从搜索结果构建回答
        AnswerGenerationResult result = buildDirectAnswer(parseResult, searchResult, options);
        
        // 模拟流式生成，分段返回
        size_t totalLength = result.answer.length();
        size_t chunkSize = 5; // 每次返回的字符数
        
        for (size_t i = 0; i < totalLength; i += chunkSize) {
            size_t end = std::min(i + chunkSize, totalLength);
            std::string chunk = result.answer.substr(i, end - i);
            callback(chunk, (end == totalLength));
            
            // 模拟生成延迟
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        LOG_INFO("直接回答流式生成完成");
    }
    else {
        // 使用语言模型流式生成
        if (model_) {
            // 构建提示词
            std::string prompt = buildPrompt(parseResult, searchResult, options);
            
            // 生成参数
            engine::GenerationOptions genOptions;
            genOptions.maxTokens = options.maxLength;
            genOptions.temperature = 0.7f;
            genOptions.topP = 0.9f;
            genOptions.streamOutput = true;
            
            // 流式回调
            engine::StreamCallback streamCb = [&](const std::string& token, bool isFinished) {
                std::string processedToken = token;
                
                // 对截断的 UTF-8 字符进行处理
                // 这里简单处理，实际应用中可能需要更复杂的处理
                
                callback(processedToken, isFinished);
            };
            
            // 调用模型流式生成
            model_->generateStream(prompt, streamCb, genOptions);
            
            LOG_INFO("语言模型流式生成完成");
        }
        else {
            LOG_ERROR("语言模型不可用，回退到非流式直接回答");
            
            // 直接从搜索结果构建回答
            AnswerGenerationResult result = buildDirectAnswer(parseResult, searchResult, options);
            
            // 一次性返回
            callback(result.answer, true);
        }
    }
}

// 设置语言模型
void AnswerGenerator::setModel(std::shared_ptr<engine::ModelInterface> model) {
    model_ = model;
    LOG_INFO("更新回答生成器的语言模型");
}

// 获取当前语言模型
std::shared_ptr<engine::ModelInterface> AnswerGenerator::getModel() const {
    return model_;
}

// 构建提示词
std::string AnswerGenerator::buildPrompt(
    const QueryParseResult& parseResult,
    const SearchResult& searchResult,
    const AnswerGenerationOptions& options) {
    
    std::stringstream prompt;
    
    // 构建系统提示
    prompt << "你是一个基于知识图谱的问答助手。请根据提供的知识回答用户的问题。";
    prompt << "请简洁、准确地回答，只使用提供的知识。";
    prompt << "如果没有足够的信息，请诚实地说你不知道。\n\n";
    
    // 添加用户查询
    prompt << "用户问题: " << parseResult.originalQuery << "\n\n";
    
    // 添加相关知识
    prompt << "相关知识:\n";
    
    for (const auto& item : searchResult.items) {
        if (item.triple && item.relevanceScore >= options.minConfidence) {
            prompt << "- " << item.triple->getSubject()->getName() << " "
                   << item.triple->getRelation()->getName() << " "
                   << item.triple->getObject()->getName();
            
            // 添加置信度信息
            if (options.includeConfidenceInfo) {
                prompt << " (置信度: " << item.triple->getConfidence() << ")";
            }
            
            prompt << "\n";
        }
    }
    prompt << "\n";
    
    // 添加回答要求
    prompt << "请根据以上信息回答用户问题。";
    if (options.language == "zh") {
        prompt << "请用中文回答。";
    } else if (options.language == "en") {
        prompt << "Please answer in English.";
    }
    
    LOG_DEBUG("构建提示词完成，长度: {}", prompt.str().length());
    
    return prompt.str();
}

// 直接构建回答
AnswerGenerationResult AnswerGenerator::buildDirectAnswer(
    const QueryParseResult& parseResult,
    const SearchResult& searchResult,
    const AnswerGenerationOptions& options) {
    
    AnswerGenerationResult result;
    std::stringstream answer;
    
    // 根据查询类型构建回答
    switch (parseResult.intent.type) {
        case QueryType::FACT: {
            // 事实查询，判断是否为真
            if (!searchResult.items.empty() && searchResult.items[0].relevanceScore > 0.8f) {
                answer << "是的，";
                
                // 添加具体事实
                const auto& item = searchResult.items[0];
                answer << item.triple->getSubject()->getName() << " "
                       << item.triple->getRelation()->getName() << " "
                       << item.triple->getObject()->getName() << "。";
                
                result.confidence = item.triple->getConfidence();
            }
            else {
                answer << "根据知识库中的信息，无法确认这一事实。";
                result.confidence = 0.3f;
            }
            break;
        }
        
        case QueryType::ENTITY: {
            // 实体查询，返回实体信息
            if (!searchResult.items.empty()) {
                // 获取实体名称
                std::string entityName;
                if (!parseResult.intent.entities.empty()) {
                    entityName = parseResult.intent.entities[0];
                }
                else if (!searchResult.items.empty()) {
                    entityName = searchResult.items[0].triple->getSubject()->getName();
                }
                
                answer << entityName << " 是";
                
                // 收集实体的关键属性
                bool hasDefinition = false;
                std::vector<std::string> properties;
                
                for (const auto& item : searchResult.items) {
                    const auto& triple = item.triple;
                    
                    // 查找定义或类型关系
                    if (triple->getRelation()->getType() == knowledge::RelationType::IS_A ||
                        triple->getRelation()->getName().find("定义") != std::string::npos ||
                        triple->getRelation()->getName().find("类型") != std::string::npos) {
                        
                        answer << " " << triple->getObject()->getName();
                        hasDefinition = true;
                    }
                    // 收集其他重要属性
                    else if (triple->getRelation()->getType() == knowledge::RelationType::HAS_PROPERTY) {
                        properties.push_back(triple->getRelation()->getName() + " " + 
                                           triple->getObject()->getName());
                    }
                }
                
                if (!hasDefinition) {
                    answer << " 一个实体";
                }
                
                answer << "。";
                
                // 添加属性信息
                if (!properties.empty()) {
                    answer << " " << entityName << " 的特点包括：";
                    for (size_t i = 0; i < properties.size() && i < 3; ++i) {
                        if (i > 0) {
                            answer << "，";
                        }
                        answer << properties[i];
                    }
                    answer << "。";
                }
                
                result.confidence = 0.8f;
            }
            else {
                answer << "很抱歉，我没有找到关于这个实体的信息。";
                result.confidence = 0.2f;
            }
            break;
        }
        
        case QueryType::ATTRIBUTE: {
            // 属性查询，返回具体属性值
            if (!searchResult.items.empty() && !parseResult.intent.attributes.empty()) {
                const std::string& attrName = parseResult.intent.attributes[0];
                
                // 查找匹配的属性
                for (const auto& item : searchResult.items) {
                    const auto& triple = item.triple;
                    
                    if (triple->getRelation()->getName().find(attrName) != std::string::npos) {
                        // 找到匹配的属性
                        answer << triple->getSubject()->getName() << " 的 " 
                               << triple->getRelation()->getName() << " 是 "
                               << triple->getObject()->getName() << "。";
                        
                        result.confidence = item.triple->getConfidence();
                        break;
                    }
                }
                
                if (answer.str().empty()) {
                    answer << "很抱歉，我没有找到关于这个属性的信息。";
                    result.confidence = 0.2f;
                }
            }
            else {
                answer << "很抱歉，我无法回答这个问题，因为没有足够的信息或者查询不明确。";
                result.confidence = 0.1f;
            }
            break;
        }
        
        case QueryType::LIST: {
            // 列表查询，返回列表项
            if (!searchResult.items.empty() && parseResult.intent.params.find("list_type") != parseResult.intent.params.end()) {
                const std::string& listType = parseResult.intent.params.at("list_type");
                std::string entityName;
                
                if (!parseResult.intent.entities.empty()) {
                    entityName = parseResult.intent.entities[0];
                }
                
                answer << entityName << " 的 " << listType << " 包括：";
                
                std::vector<std::string> items;
                for (const auto& item : searchResult.items) {
                    const auto& triple = item.triple;
                    
                    if (triple->getRelation()->getName().find(listType) != std::string::npos) {
                        items.push_back(triple->getObject()->getName());
                    }
                }
                
                if (!items.empty()) {
                    for (size_t i = 0; i < items.size(); ++i) {
                        if (i > 0) {
                            answer << "，";
                        }
                        answer << items[i];
                    }
                    answer << "。";
                    result.confidence = 0.7f;
                }
                else {
                    answer << "未找到相关信息。";
                    result.confidence = 0.3f;
                }
            }
            else {
                answer << "很抱歉，我无法提供这个列表信息，因为没有足够的数据。";
                result.confidence = 0.2f;
            }
            break;
        }
        
        default: {
            // 其他类型的查询，简单整合搜索结果
            if (!searchResult.items.empty()) {
                answer << "根据知识库中的信息：\n";
                
                for (size_t i = 0; i < searchResult.items.size() && i < 3; ++i) {
                    const auto& item = searchResult.items[i];
                    
                    answer << "- " << item.triple->getSubject()->getName() << " "
                           << item.triple->getRelation()->getName() << " "
                           << item.triple->getObject()->getName() << "\n";
                }
                
                result.confidence = 0.5f;
            }
            else {
                answer << "很抱歉，我没有找到相关信息来回答这个问题。";
                result.confidence = 0.1f;
            }
            break;
        }
    }
    
    result.answer = answer.str();
    
    // 后处理
    result.answer = postprocessAnswer(result.answer, parseResult, options);
    
    return result;
}

// 后处理生成的回答
std::string AnswerGenerator::postprocessAnswer(
    const std::string& answer,
    const QueryParseResult& parseResult,
    const AnswerGenerationOptions& options) {
    
    std::string processed = answer;
    
    // 移除多余空格
    processed = std::regex_replace(processed, std::regex("\\s+"), " ");
    
    // 修复常见的标点符号错误
    processed = std::regex_replace(processed, std::regex("([。！？，：；]) "), "$1");
    
    // 确保回答以句号结尾
    if (!processed.empty() && processed.back() != '。' && processed.back() != '!' && 
        processed.back() != '?' && processed.back() != '！' && processed.back() != '？') {
        processed += "。";
    }
    
    // 如果超过最大长度，进行截断
    if (processed.length() > options.maxLength) {
        processed = processed.substr(0, options.maxLength);
        
        // 尝试在句子末尾截断
        size_t lastSentence = processed.find_last_of("。！？!?");
        if (lastSentence != std::string::npos && lastSentence > options.maxLength * 0.8) {
            processed = processed.substr(0, lastSentence + 1);
        }
    }
    
    return processed;
}

// 计算回答的置信度
float AnswerGenerator::calculateConfidence(
    const std::string& answer,
    const SearchResult& searchResult) {
    
    float confidence = 0.0f;
    
    // 基础置信度
    confidence = 0.5f;
    
    // 根据搜索结果数量调整置信度
    if (!searchResult.items.empty()) {
        confidence += 0.2f * std::min(1.0f, searchResult.items.size() / 5.0f);
    }
    
    // 根据搜索结果的平均相关性调整置信度
    float avgRelevance = 0.0f;
    for (const auto& item : searchResult.items) {
        avgRelevance += item.relevanceScore;
    }
    
    if (!searchResult.items.empty()) {
        avgRelevance /= searchResult.items.size();
        confidence += 0.3f * avgRelevance;
    }
    
    // 归一化到[0,1]范围
    confidence = std::min(1.0f, std::max(0.0f, confidence));
    
    return confidence;
}

// 获取查询类型名称的辅助函数（与 QueryParser 保持一致）
std::string getQueryTypeName(QueryType type) {
    switch (type) {
        case QueryType::FACT: return "事实查询";
        case QueryType::ENTITY: return "实体查询";
        case QueryType::RELATION: return "关系查询";
        case QueryType::ATTRIBUTE: return "属性查询";
        case QueryType::LIST: return "列表查询";
        case QueryType::COMPARISON: return "比较查询";
        case QueryType::DEFINITION: return "定义查询";
        case QueryType::OPEN: return "开放查询";
        case QueryType::UNKNOWN: return "未知查询";
        default: return "未定义查询";
    }
}

} // namespace query
} // namespace kb 