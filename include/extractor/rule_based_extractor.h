#ifndef KB_RULE_BASED_EXTRACTOR_H
#define KB_RULE_BASED_EXTRACTOR_H

#include "extractor/extractor_base.h"
#include "preprocessor/chinese_tokenizer.h"
#include <regex>
#include <unordered_map>
#include <functional>

namespace kb {
namespace extractor {

/**
 * @brief 实体识别规则
 */
struct EntityRecognitionRule {
    std::string name;                    ///< 规则名称
    knowledge::EntityType entityType;    ///< 实体类型
    std::regex pattern;                  ///< 匹配模式
    
    /**
     * @brief 构造函数
     * @param name 规则名称
     * @param entityType 实体类型
     * @param pattern 正则表达式模式
     */
    EntityRecognitionRule(const std::string& name, 
                         knowledge::EntityType entityType, 
                         const std::string& pattern) 
        : name(name), entityType(entityType), pattern(pattern) {}
};

/**
 * @brief 关系识别规则
 */
struct RelationRecognitionRule {
    std::string name;                    ///< 规则名称
    knowledge::RelationType relationType; ///< 关系类型
    std::regex pattern;                  ///< 匹配模式
    std::string relationName;            ///< 关系名称
    
    /**
     * @brief 构造函数
     * @param name 规则名称
     * @param relationType 关系类型
     * @param pattern 正则表达式模式
     * @param relationName 关系名称
     */
    RelationRecognitionRule(const std::string& name, 
                           knowledge::RelationType relationType,
                           const std::string& pattern,
                           const std::string& relationName = "") 
        : name(name), relationType(relationType), pattern(pattern), relationName(relationName) {}
};

/**
 * @brief 基于规则的知识提取器
 */
class RuleBasedExtractor : public ExtractorBase {
public:
    /**
     * @brief 构造函数
     * @param dictPath jieba词典路径
     * @param hmmPath jieba HMM模型路径
     * @param userDictPath 用户词典路径
     * @param idfPath IDF文件路径
     * @param stopWordsPath 停用词文件路径
     */
    RuleBasedExtractor(const std::string& dictPath = "",
                      const std::string& hmmPath = "",
                      const std::string& userDictPath = "",
                      const std::string& idfPath = "",
                      const std::string& stopWordsPath = "");
    
    /**
     * @brief 析构函数
     */
    ~RuleBasedExtractor() override = default;
    
    /**
     * @brief 获取提取器名称
     * @return 提取器名称
     */
    std::string getName() const override {
        return "RuleBasedExtractor";
    }
    
    /**
     * @brief 获取提取器描述
     * @return 提取器描述
     */
    std::string getDescription() const override {
        return "基于规则的知识提取器，使用正则表达式和模式匹配提取实体和关系";
    }
    
    /**
     * @brief 从文本中提取实体
     * @param text 输入文本
     * @return 提取的实体列表
     */
    std::vector<knowledge::EntityPtr> extractEntities(const std::string& text) override;
    
    /**
     * @brief 从文本中提取关系
     * @param text 输入文本
     * @param entities 已提取的实体列表
     * @return 提取的关系列表
     */
    std::vector<knowledge::RelationPtr> extractRelations(
        const std::string& text,
        const std::vector<knowledge::EntityPtr>& entities = {}) override;
    
    /**
     * @brief 从文本中提取三元组
     * @param text 输入文本
     * @return 提取的三元组列表
     */
    std::vector<knowledge::TriplePtr> extractTriples(const std::string& text) override;
    
    /**
     * @brief 从文本中构建知识图谱
     * @param text 输入文本
     * @param graph 可选参数，现有知识图谱
     * @return 构建的知识图谱
     */
    knowledge::KnowledgeGraphPtr buildKnowledgeGraph(
        const std::string& text,
        knowledge::KnowledgeGraphPtr graph = nullptr) override;
    
    /**
     * @brief 添加实体识别规则
     * @param rule 实体识别规则
     */
    void addEntityRule(const EntityRecognitionRule& rule);
    
    /**
     * @brief 添加关系识别规则
     * @param rule 关系识别规则
     */
    void addRelationRule(const RelationRecognitionRule& rule);
    
    /**
     * @brief 添加实体类型映射函数
     * @param typeName 类型名称
     * @param func 映射函数，接收文本片段，返回实体类型
     */
    void addEntityTypeMapper(const std::string& typeName, 
                            std::function<knowledge::EntityType(const std::string&)> func);
    
    /**
     * @brief 添加关系类型映射函数
     * @param typeName 类型名称
     * @param func 映射函数，接收文本片段，返回关系类型
     */
    void addRelationTypeMapper(const std::string& typeName, 
                              std::function<knowledge::RelationType(const std::string&)> func);
    
    /**
     * @brief 设置是否使用中文分词
     * @param use 是否使用
     */
    void setUseChineseTokenizer(bool use) {
        useChineseTokenizer_ = use;
    }
    
    /**
     * @brief 获取是否使用中文分词
     * @return 是否使用
     */
    bool getUseChineseTokenizer() const {
        return useChineseTokenizer_;
    }
    
    /**
     * @brief 添加预定义的规则集
     * @param rulesetName 规则集名称
     */
    void addPredefinedRuleset(const std::string& rulesetName);

private:
    std::vector<EntityRecognitionRule> entityRules_;     ///< 实体识别规则
    std::vector<RelationRecognitionRule> relationRules_; ///< 关系识别规则
    std::unique_ptr<preprocessor::ChineseTokenizer> tokenizer_; ///< 中文分词器
    bool useChineseTokenizer_;           ///< 是否使用中文分词
    
    // 类型映射函数
    std::unordered_map<std::string, std::function<knowledge::EntityType(const std::string&)>> entityTypeMappers_;
    std::unordered_map<std::string, std::function<knowledge::RelationType(const std::string&)>> relationTypeMappers_;
    
    /**
     * @brief 添加中文人名识别规则
     */
    void addChinesePersonNameRules();
    
    /**
     * @brief 添加中文组织机构识别规则
     */
    void addChineseOrganizationRules();
    
    /**
     * @brief 添加中文地点识别规则
     */
    void addChineseLocationRules();
    
    /**
     * @brief 添加中文时间识别规则
     */
    void addChineseTimeRules();
    
    /**
     * @brief 添加中文事件识别规则
     */
    void addChineseEventRules();
    
    /**
     * @brief 添加中文常见关系识别规则
     */
    void addChineseRelationRules();
    
    /**
     * @brief 从分词结果中识别实体
     * @param tokens 分词结果
     * @return 识别的实体列表
     */
    std::vector<knowledge::EntityPtr> recognizeEntitiesFromTokens(
        const std::vector<std::string>& tokens);
        
    /**
     * @brief 关联实体和关系，生成三元组
     * @param entities 实体列表
     * @param relations 关系列表
     * @param text 原始文本
     * @return 生成的三元组列表
     */
    std::vector<knowledge::TriplePtr> associateEntitiesAndRelations(
        const std::vector<knowledge::EntityPtr>& entities,
        const std::vector<knowledge::RelationPtr>& relations,
        const std::string& text);
        
    /**
     * @brief 使用启发式规则寻找主体和客体
     * @param relation 关系
     * @param entities 候选实体列表
     * @param text 原始文本
     * @return 主体和客体的实体对
     */
    std::pair<knowledge::EntityPtr, knowledge::EntityPtr> findSubjectAndObject(
        const knowledge::RelationPtr& relation,
        const std::vector<knowledge::EntityPtr>& entities,
        const std::string& text);
};

} // namespace extractor
} // namespace kb

#endif // KB_RULE_BASED_EXTRACTOR_H 