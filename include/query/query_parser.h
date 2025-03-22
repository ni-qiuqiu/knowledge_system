/**
 * @file query_parser.h
 * @brief 查询解析器，负责解析用户输入的自然语言查询
 */

#ifndef KB_QUERY_PARSER_H
#define KB_QUERY_PARSER_H

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "knowledge/entity.h"
#include "knowledge/relation.h"

namespace kb {
namespace query {

/**
 * @brief 查询类型枚举
 */
enum class QueryType {
    FACT,            ///< 事实查询（例如：北京的面积是多少？）
    ENTITY,          ///< 实体查询（例如：鲁迅是谁？）
    RELATION,        ///< 关系查询（例如：李白和杜甫是什么关系？）
    ATTRIBUTE,       ///< 属性查询（例如：长江的长度是多少？）
    LIST,            ///< 列表查询（例如：中国有哪些省份？）
    COMPARISON,      ///< 比较查询（例如：长江和黄河哪个更长？）
    DEFINITION,      ///< 定义查询（例如：什么是人工智能？）
    OPEN,            ///< 开放式查询（例如：谈谈对人工智能的看法）
    UNKNOWN          ///< 未知类型
};

/**
 * @brief 查询意图结构
 */
struct QueryIntent {
    QueryType type;                                      ///< 查询类型
    std::vector<std::string> entities;                   ///< 查询中提到的实体
    std::vector<std::string> relations;                  ///< 查询中提到的关系
    std::vector<std::string> attributes;                 ///< 查询中提到的属性
    std::unordered_map<std::string, std::string> params; ///< 其他参数
    
    // 构造函数
    QueryIntent() : type(QueryType::UNKNOWN) {}
};

/**
 * @brief 查询解析结果结构
 */
struct QueryParseResult {
    std::string originalQuery;                           ///< 原始查询文本
    std::string normalizedQuery;                         ///< 规范化后的查询
    QueryIntent intent;                                  ///< 查询意图
    float confidence;                                    ///< 解析置信度
    std::vector<knowledge::EntityPtr> linkedEntities;    ///< 链接到知识库的实体
    std::vector<knowledge::RelationPtr> linkedRelations; ///< 链接到知识库的关系
    
    // 构造函数
    QueryParseResult() : confidence(0.0f) {}
};

/**
 * @brief 查询解析器类
 */
class QueryParser {
public:
    /**
     * @brief 构造函数
     */
    QueryParser();
    
    /**
     * @brief 析构函数
     */
    virtual ~QueryParser();
    
    /**
     * @brief 解析查询
     * @param query 用户查询文本
     * @return 查询解析结果
     */
    virtual QueryParseResult parse(const std::string& query);
    
    /**
     * @brief 设置解析选项
     * @param key 选项名
     * @param value 选项值
     */
    void setOption(const std::string& key, const std::string& value);
    
    /**
     * @brief 获取解析选项
     * @param key 选项名
     * @return 选项值
     */
    std::string getOption(const std::string& key) const;

private:
    /**
     * @brief 预处理查询文本
     * @param query 原始查询
     * @return 预处理后的查询
     */
    std::string preprocessQuery(const std::string& query);
    
    /**
     * @brief 识别查询类型
     * @param query 预处理后的查询
     * @return 查询类型
     */
    QueryType recognizeQueryType(const std::string& query);
    
    /**
     * @brief 提取查询意图
     * @param query 预处理后的查询
     * @param type 查询类型
     * @return 查询意图
     */
    QueryIntent extractIntent(const std::string& query, QueryType type);
    
    /**
     * @brief 实体链接
     * @param intent 查询意图
     * @return 链接到知识库的实体列表
     */
    std::vector<knowledge::EntityPtr> linkEntities(const QueryIntent& intent);
    
    /**
     * @brief 关系链接
     * @param intent 查询意图
     * @return 链接到知识库的关系列表
     */
    std::vector<knowledge::RelationPtr> linkRelations(const QueryIntent& intent);

    /**
     * @brief 初始化查询类型模式
     */
    void initQueryTypePatterns();

    /**
     * @brief 提取实体查询意图
     * @param query 查询文本
     * @param intent 查询意图
     */
    void extractEntityIntent(const std::string& query, QueryIntent& intent);

    /**
     * @brief 提取关系查询意图
     * @param query 查询文本
     * @param intent 查询意图
     */
    void extractRelationIntent(const std::string& query, QueryIntent& intent);

    /**
     * @brief 提取属性查询意图
     * @param query 查询文本
     * @param intent 查询意图
     */
    void extractAttributeIntent(const std::string& query, QueryIntent& intent);

    /**
     * @brief 提取列表查询意图
     * @param query 查询文本
     * @param intent 查询意图
     */
    void extractListIntent(const std::string& query, QueryIntent& intent);

    /**
     * @brief 提取比较查询意图
     * @param query 查询文本
     * @param intent 查询意图
     */
    void extractComparisonIntent(const std::string& query, QueryIntent& intent);

    /**
     * @brief 提取定义查询意图
     * @param query 查询文本
     * @param intent 查询意图
     */
    void extractDefinitionIntent(const std::string& query, QueryIntent& intent);

    /**
     * @brief 提取事实查询意图
     * @param query 查询文本
     * @param intent 查询意图
     */
    void extractFactIntent(const std::string& query, QueryIntent& intent);

    /**
     * @brief 提取开放式查询意图
     * @param query 查询文本
     * @param intent 查询意图
     */
    void extractOpenIntent(const std::string& query, QueryIntent& intent);

    /**
     * @brief 提取通用查询意图
     * @param query 查询文本
     * @param intent 查询意图
     */
    void extractGenericIntent(const std::string& query, QueryIntent& intent);

    /**
     * @brief 计算查询解析置信度
     * @param result 查询解析结果
     * @return 置信度分数
     */
    float calculateConfidence(const QueryParseResult& result);

    /**
     * @brief 判断是否为停用词
     * @param word 待判断的词
     * @return 是否为停用词
     */
    bool isStopWord(const std::string& word);

    /**
     * @brief 获取查询类型名称
     * @param type 查询类型
     * @return 类型名称
     */
    std::string getQueryTypeName(QueryType type);
    
    // 成员变量
    std::unordered_map<std::string, std::string> options_; ///< 解析选项
    std::unordered_map<std::string, QueryType> queryTypePatterns_; ///< 查询类型模式
};

} // namespace query
} // namespace kb

#endif // KB_QUERY_PARSER_H 