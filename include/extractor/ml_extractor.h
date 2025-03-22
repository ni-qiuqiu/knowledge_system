#ifndef KB_ML_EXTRACTOR_H
#define KB_ML_EXTRACTOR_H

#include "extractor/extractor_base.h"
#include "preprocessor/chinese_tokenizer.h"
#include "vectorizer/tfidf_vectorizer.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <tuple>

namespace kb {
namespace extractor {

/**
 * @brief 实体识别结果
 */
struct EntityRecognitionResult {
    std::string text;                  ///< 实体文本
    knowledge::EntityType entityType;  ///< 实体类型
    size_t startPos;                   ///< 起始位置
    size_t endPos;                     ///< 结束位置
    float confidence;                  ///< 置信度
    
    /**
     * @brief 构造函数
     * @param text 实体文本
     * @param entityType 实体类型
     * @param startPos 起始位置
     * @param endPos 结束位置
     * @param confidence 置信度
     */
    EntityRecognitionResult(const std::string& text, 
                          knowledge::EntityType entityType,
                          size_t startPos, 
                          size_t endPos, 
                          float confidence)
        : text(text), entityType(entityType), startPos(startPos), 
          endPos(endPos), confidence(confidence) {}
};

/**
 * @brief 关系提取结果
 */
struct RelationExtractionResult {
    std::string text;                   ///< 关系文本
    knowledge::RelationType relationType; ///< 关系类型
    size_t startPos;                    ///< 起始位置
    size_t endPos;                      ///< 结束位置
    float confidence;                   ///< 置信度
    size_t subjectIndex;               ///< 主体实体索引
    size_t objectIndex;                ///< 客体实体索引
    
    /**
     * @brief 构造函数
     * @param text 关系文本
     * @param relationType 关系类型
     * @param startPos 起始位置
     * @param endPos 结束位置
     * @param confidence 置信度
     * @param subjectIndex 主体实体索引
     * @param objectIndex 客体实体索引
     */
    RelationExtractionResult(const std::string& text, 
                           knowledge::RelationType relationType,
                           size_t startPos, 
                           size_t endPos, 
                           float confidence,
                           size_t subjectIndex,
                           size_t objectIndex)
        : text(text), relationType(relationType), startPos(startPos), 
          endPos(endPos), confidence(confidence), 
          subjectIndex(subjectIndex), objectIndex(objectIndex) {}
};

/**
 * @brief 基于机器学习的知识提取器接口
 */
class MLExtractor : public ExtractorBase {
public:
    /**
     * @brief 构造函数
     * @param modelPath 模型路径
     * @param dictPath jieba词典路径
     * @param hmmPath jieba HMM模型路径
     * @param userDictPath 用户词典路径
     * @param stopWordsPath 停用词文件路径
     */
    MLExtractor(const std::string& modelPath = "",
               const std::string& dictPath = "",
               const std::string& hmmPath = "",
               const std::string& userDictPath = "",
               const std::string& idfPath = "",
               const std::string& stopWordsPath = "");
    
    /**
     * @brief 析构函数
     */
    ~MLExtractor() override = default;
    
    /**
     * @brief 获取提取器名称
     * @return 提取器名称
     */
    std::string getName() const override {
        return "MLExtractor";
    }
    
    /**
     * @brief 获取提取器描述
     * @return 提取器描述
     */
    std::string getDescription() const override {
        return "基于机器学习的知识提取器，使用预训练模型提取实体和关系";
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
     * @brief 加载预训练模型
     * @param modelPath 模型路径
     * @return 是否加载成功
     */
    virtual bool loadModel(const std::string& modelPath);
    
    /**
     * @brief 获取上次运行的实体识别结果
     * @return 实体识别结果列表
     */
    const std::vector<EntityRecognitionResult>& getLastEntityResults() const {
        return lastEntityResults_;
    }
    
    /**
     * @brief 获取上次运行的关系提取结果
     * @return 关系提取结果列表
     */
    const std::vector<RelationExtractionResult>& getLastRelationResults() const {
        return lastRelationResults_;
    }
    
    /**
     * @brief 设置实体识别阈值
     * @param threshold 阈值，范围[0, 1]
     */
    void setEntityConfidenceThreshold(float threshold) {
        entityConfidenceThreshold_ = std::max(0.0f, std::min(1.0f, threshold));
    }
    
    /**
     * @brief 设置关系提取阈值
     * @param threshold 阈值，范围[0, 1]
     */
    void setRelationConfidenceThreshold(float threshold) {
        relationConfidenceThreshold_ = std::max(0.0f, std::min(1.0f, threshold));
    }

protected:
    std::string modelPath_;                           ///< 模型路径
    std::unique_ptr<preprocessor::ChineseTokenizer> tokenizer_; ///< 中文分词器
    std::unique_ptr<vectorizer::TfidfVectorizer> vectorizer_;  ///< TF-IDF向量化器
    float entityConfidenceThreshold_;                ///< 实体识别阈值
    float relationConfidenceThreshold_;              ///< 关系提取阈值
    std::vector<EntityRecognitionResult> lastEntityResults_;       ///< 上次实体识别结果
    std::vector<RelationExtractionResult> lastRelationResults_;     ///< 上次关系提取结果
    
    /**
     * @brief 对文本进行预处理
     * @param text 输入文本
     * @return 预处理后的文本
     */
    virtual std::string preprocessText(const std::string& text);
    
    /**
     * @brief 对文本进行分词
     * @param text 输入文本
     * @return 分词结果
     */
    virtual std::vector<std::string> tokenizeText(const std::string& text);
    
    /**
     * @brief 执行命名实体识别
     * @param text 输入文本
     * @param tokens 分词结果
     * @return 实体识别结果列表
     */
    virtual std::vector<EntityRecognitionResult> recognizeEntities(
        const std::string& text,
        const std::vector<std::string>& tokens) = 0;
    
    /**
     * @brief 执行关系提取
     * @param text 输入文本
     * @param tokens 分词结果
     * @param entityResults 实体识别结果
     * @return 关系提取结果列表
     */
    virtual std::vector<RelationExtractionResult> extractRelationsFromEntities(
        const std::string& text,
        const std::vector<std::string>& tokens,
        const std::vector<EntityRecognitionResult>& entityResults) = 0;
};

} // namespace extractor
} // namespace kb

#endif // KB_ML_EXTRACTOR_H 