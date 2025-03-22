/**
 * @file query_parser.cpp
 * @brief 查询解析器实现
 */

#include "query/query_parser.h"
#include "common/logging.h"
#include <algorithm>
#include <regex>
#include <cctype>
#include <unordered_set>

namespace kb {
namespace query {

// 构造函数
QueryParser::QueryParser() {
    LOG_INFO("初始化查询解析器");
    
    // 初始化查询类型模式
    initQueryTypePatterns();
}

// 析构函数
QueryParser::~QueryParser() {
    LOG_INFO("查询解析器释放资源");
}

// 解析查询
QueryParseResult QueryParser::parse(const std::string& query) {
    LOG_INFO("开始解析查询: {}", query);
    
    QueryParseResult result;
    result.originalQuery = query;
    
    // 预处理查询
    result.normalizedQuery = preprocessQuery(query);
    
    // 识别查询类型
    QueryType queryType = recognizeQueryType(result.normalizedQuery);
    
    // 提取查询意图
    result.intent = extractIntent(result.normalizedQuery, queryType);
    
    // 实体链接
    result.linkedEntities = linkEntities(result.intent);
    
    // 关系链接
    result.linkedRelations = linkRelations(result.intent);
    
    // 计算置信度（简单实现）
    result.confidence = calculateConfidence(result);
    
    LOG_INFO("查询解析完成，类型: {}，置信度: {}", 
              getQueryTypeName(result.intent.type), result.confidence);
    
    return result;
}

// 设置解析选项
void QueryParser::setOption(const std::string& key, const std::string& value) {
    options_[key] = value;
    LOG_DEBUG("设置查询解析选项: {} = {}", key, value);
}

// 获取解析选项
std::string QueryParser::getOption(const std::string& key) const {
    auto it = options_.find(key);
    if (it != options_.end()) {
        return it->second;
    }
    return "";
}

// 预处理查询文本
std::string QueryParser::preprocessQuery(const std::string& query) {
    std::string processed = query;
    
    // 转换为小写（仅对英文部分）
    std::string result;
    result.reserve(processed.size());
    
    for (char c : processed) {
        if (c >= 'A' && c <= 'Z') {
            result.push_back(std::tolower(c));
        } else {
            result.push_back(c);
        }
    }
    processed = result;
    
    // 移除多余空格
    processed = std::regex_replace(processed, std::regex("\\s+"), " ");
    
    // 移除特殊字符
    processed = std::regex_replace(processed, std::regex("[\\p{P}&&[^?！\\u3002]]"), " ");
    
    // 移除头尾空格
    processed = std::regex_replace(processed, std::regex("^\\s+|\\s+$"), "");
    
    LOG_DEBUG("预处理后的查询: {}", processed);
    return processed;
}

// 初始化查询类型模式
void QueryParser::initQueryTypePatterns() {
    // 实体查询模式
    queryTypePatterns_["是谁"] = QueryType::ENTITY;
    queryTypePatterns_["是什么人"] = QueryType::ENTITY;
    queryTypePatterns_["是哪位"] = QueryType::ENTITY;
    
    // 关系查询模式
    queryTypePatterns_["什么关系"] = QueryType::RELATION;
    queryTypePatterns_["有何关系"] = QueryType::RELATION;
    queryTypePatterns_["如何联系"] = QueryType::RELATION;
    
    // 属性查询模式
    queryTypePatterns_["有多少"] = QueryType::ATTRIBUTE;
    queryTypePatterns_["多大"] = QueryType::ATTRIBUTE;
    queryTypePatterns_["多高"] = QueryType::ATTRIBUTE;
    queryTypePatterns_["在哪里"] = QueryType::ATTRIBUTE;
    
    // 列表查询模式
    queryTypePatterns_["有哪些"] = QueryType::LIST;
    queryTypePatterns_["列举"] = QueryType::LIST;
    queryTypePatterns_["包括哪些"] = QueryType::LIST;
    
    // 比较查询模式
    queryTypePatterns_["相比"] = QueryType::COMPARISON;
    queryTypePatterns_["比较"] = QueryType::COMPARISON;
    queryTypePatterns_["哪个更"] = QueryType::COMPARISON;
    
    // 定义查询模式
    queryTypePatterns_["什么是"] = QueryType::DEFINITION;
    queryTypePatterns_["定义"] = QueryType::DEFINITION;
    queryTypePatterns_["解释"] = QueryType::DEFINITION;
    
    // 事实查询模式
    queryTypePatterns_["是不是"] = QueryType::FACT;
    queryTypePatterns_["是否"] = QueryType::FACT;
    queryTypePatterns_["对不对"] = QueryType::FACT;
}

// 识别查询类型
QueryType QueryParser::recognizeQueryType(const std::string& query) {
    // 默认为未知类型
    QueryType type = QueryType::UNKNOWN;
    
    // 遍历所有模式，寻找匹配
    for (const auto& pattern : queryTypePatterns_) {
        if (query.find(pattern.first) != std::string::npos) {
            type = pattern.second;
            LOG_DEBUG("识别查询类型: {}", getQueryTypeName(type));
            return type;
        }
    }
    
    // 未匹配到特定模式，进行启发式判断
    if (query.find("?") != std::string::npos || 
        query.find("？") != std::string::npos) {
        // 包含问号，可能是一般性问题
        type = QueryType::OPEN;
    } else if (std::regex_search(query, std::regex("为什么|怎么样|如何"))) {
        // 可能是开放式问题
        type = QueryType::OPEN;
    }
    
    LOG_DEBUG("识别查询类型: {}", getQueryTypeName(type));
    return type;
}

// 提取查询意图
QueryIntent QueryParser::extractIntent(const std::string& query, QueryType type) {
    QueryIntent intent;
    intent.type = type;
    
    // 根据查询类型提取意图
    switch (type) {
        case QueryType::ENTITY:
            extractEntityIntent(query, intent);
            break;
        case QueryType::RELATION:
            extractRelationIntent(query, intent);
            break;
        case QueryType::ATTRIBUTE:
            extractAttributeIntent(query, intent);
            break;
        case QueryType::LIST:
            extractListIntent(query, intent);
            break;
        case QueryType::COMPARISON:
            extractComparisonIntent(query, intent);
            break;
        case QueryType::DEFINITION:
            extractDefinitionIntent(query, intent);
            break;
        case QueryType::FACT:
            extractFactIntent(query, intent);
            break;
        case QueryType::OPEN:
            extractOpenIntent(query, intent);
            break;
        default:
            // 未知类型，尝试通用提取
            extractGenericIntent(query, intent);
            break;
    }
    
    LOG_DEBUG("提取查询意图，包含 {} 个实体, {} 个关系",
              intent.entities.size(), intent.relations.size());
    
    return intent;
}

// 提取实体意图
void QueryParser::extractEntityIntent(const std::string& query, QueryIntent& intent) {
    // 基本实现：寻找"XX是谁"格式
    std::regex entityPattern("(.*?)是谁");
    std::smatch match;
    
    if (std::regex_search(query, match, entityPattern) && match.size() > 1) {
        intent.entities.push_back(match[1].str());
    }
}

// 提取关系意图
void QueryParser::extractRelationIntent(const std::string& query, QueryIntent& intent) {
    // 基本实现：寻找"XX和YY是什么关系"格式
    std::regex relationPattern("(.*?)和(.*?)(?:是什么|有什么|有何)关系");
    std::smatch match;
    
    if (std::regex_search(query, match, relationPattern) && match.size() > 2) {
        intent.entities.push_back(match[1].str());
        intent.entities.push_back(match[2].str());
        intent.relations.push_back("关系");
    }
}

// 提取属性意图
void QueryParser::extractAttributeIntent(const std::string& query, QueryIntent& intent) {
    // 基本实现：寻找"XX的YY是什么"格式
    std::regex attributePattern("(.*?)的(.*?)是(?:什么|多少|多大|多高|哪里)");
    std::smatch match;
    
    if (std::regex_search(query, match, attributePattern) && match.size() > 2) {
        intent.entities.push_back(match[1].str());
        intent.attributes.push_back(match[2].str());
    }
}

// 提取列表意图
void QueryParser::extractListIntent(const std::string& query, QueryIntent& intent) {
    // 基本实现：寻找"XX有哪些YY"格式
    std::regex listPattern("(.*?)有哪些(.*?)");
    std::smatch match;
    
    if (std::regex_search(query, match, listPattern) && match.size() > 2) {
        intent.entities.push_back(match[1].str());
        intent.attributes.push_back(match[2].str());
        intent.params["list_type"] = match[2].str();
    }
}

// 提取比较意图
void QueryParser::extractComparisonIntent(const std::string& query, QueryIntent& intent) {
    // 基本实现：寻找"XX和YY哪个更ZZ"格式
    std::regex comparisonPattern("(.*?)和(.*?)(?:相比|比较|哪个更)(.*?)");
    std::smatch match;
    
    if (std::regex_search(query, match, comparisonPattern) && match.size() > 3) {
        intent.entities.push_back(match[1].str());
        intent.entities.push_back(match[2].str());
        intent.attributes.push_back(match[3].str());
        intent.params["comparison_aspect"] = match[3].str();
    }
}

// 提取定义意图
void QueryParser::extractDefinitionIntent(const std::string& query, QueryIntent& intent) {
    // 基本实现：寻找"什么是XX"格式
    std::regex definitionPattern("(?:什么是|定义|解释)(.*?)");
    std::smatch match;
    
    if (std::regex_search(query, match, definitionPattern) && match.size() > 1) {
        intent.entities.push_back(match[1].str());
        intent.params["definition_type"] = "general";
    }
}

// 提取事实意图
void QueryParser::extractFactIntent(const std::string& query, QueryIntent& intent) {
    // 基本实现：寻找"XX是不是YY"格式
    std::regex factPattern("(.*?)(?:是不是|是否)(.*?)");
    std::smatch match;
    
    if (std::regex_search(query, match, factPattern) && match.size() > 2) {
        intent.entities.push_back(match[1].str());
        intent.entities.push_back(match[2].str());
        intent.params["fact_check"] = "true";
    }
}

// 提取开放意图
void QueryParser::extractOpenIntent(const std::string& query, QueryIntent& intent) {
    // 提取所有可能的实体词（简单实现）
    std::vector<std::string> words;
    std::istringstream iss(query);
    std::string word;
    
    while (iss >> word) {
        if (word.length() > 1 && !isStopWord(word)) {
            words.push_back(word);
        }
    }
    
    // 排序并去重
    std::sort(words.begin(), words.end());
    words.erase(std::unique(words.begin(), words.end()), words.end());
    
    // 添加到实体列表
    for (const auto& w : words) {
        intent.entities.push_back(w);
    }
    
    intent.params["open_query"] = "true";
}

// 提取通用意图
void QueryParser::extractGenericIntent(const std::string& query, QueryIntent& intent) {
    // 简单提取所有可能的实体词
    extractOpenIntent(query, intent);
    intent.params["generic_query"] = "true";
}

// 实体链接
std::vector<knowledge::EntityPtr> QueryParser::linkEntities(const QueryIntent& intent) {
    // 实体链接简单实现
    std::vector<knowledge::EntityPtr> linkedEntities;
    
    // TODO: 从知识图谱中查找匹配的实体，这里只做简单示例
    for (const auto& entityName : intent.entities) {
        auto entity = knowledge::Entity::create(
            "temp_" + entityName,
            entityName,
            knowledge::EntityType::UNKNOWN
        );
        linkedEntities.push_back(entity);
    }
    
    LOG_DEBUG("实体链接完成，找到 {} 个匹配实体", linkedEntities.size());
    return linkedEntities;
}

// 关系链接
std::vector<knowledge::RelationPtr> QueryParser::linkRelations(const QueryIntent& intent) {
    // 关系链接简单实现
    std::vector<knowledge::RelationPtr> linkedRelations;
    
    // TODO: 从知识图谱中查找匹配的关系，这里只做简单示例
    for (const auto& relationName : intent.relations) {
        auto relation = knowledge::Relation::create(
            "temp_" + relationName,
            relationName,
            knowledge::RelationType::CUSTOM
        );
        linkedRelations.push_back(relation);
    }
    
    LOG_DEBUG("关系链接完成，找到 {} 个匹配关系", linkedRelations.size());
    return linkedRelations;
}

// 计算解析结果的置信度
float QueryParser::calculateConfidence(const QueryParseResult& result) {
    float confidence = 0.0f;
    
    // 简单置信度计算
    if (result.intent.type != QueryType::UNKNOWN) {
        confidence += 0.3f;
    }
    
    if (!result.intent.entities.empty()) {
        confidence += 0.3f * std::min(1.0f, result.intent.entities.size() / 2.0f);
    }
    
    if (!result.intent.relations.empty()) {
        confidence += 0.2f;
    }
    
    if (!result.intent.attributes.empty()) {
        confidence += 0.2f;
    }
    
    return std::min(1.0f, confidence);
}

// 判断是否为停用词（简单实现）
bool QueryParser::isStopWord(const std::string& word) {
    static const std::unordered_set<std::string> stopWords = {
        "的", "了", "在", "是", "我", "有", "和", "与", "这", "那", "你", "会", "要",
        "就", "都", "而", "及", "上", "也", "很", "但", "到", "对", "可以", "去", "把"
    };
    
    return stopWords.find(word) != stopWords.end();
}

// 获取查询类型名称
std::string QueryParser::getQueryTypeName(QueryType type) {
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