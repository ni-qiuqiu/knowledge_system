#ifndef KB_TFIDF_VECTORIZER_H
#define KB_TFIDF_VECTORIZER_H

#include "vectorizer/vectorizer.h"
#include <unordered_map>
#include <set>
#include <Eigen/Sparse>

namespace kb {
namespace vectorizer {

/**
 * @brief TF-IDF向量化器实现
 */
class TfidfVectorizer : public Vectorizer {
public:
    /**
     * @brief 构造函数
     * @param minDf 最小文档频率（过滤低频词）
     * @param maxDf 最大文档频率（过滤高频词）
     * @param maxFeatures 最大特征数
     * @param normalization 归一化方法 ("l1", "l2", "none")
     * @param useBinary 是否使用二元计数
     * @param smooth 平滑参数
     */
    TfidfVectorizer(float minDf = 0.0, 
                   float maxDf = 1.0, 
                   size_t maxFeatures = 0,
                   const std::string& normalization = "l2",
                   bool useBinary = false,
                   float smooth = 1.0);
    
    /**
     * @brief 析构函数
     */
    ~TfidfVectorizer() override = default;
    
    /**
     * @brief 拟合向量化模型
     * @param documents 文档集合
     */
    void fit(const std::vector<std::string>& documents) override;
    
    /**
     * @brief 将文档转换为向量
     * @param document 单个文档
     * @return 文档的TF-IDF向量
     */
    Eigen::VectorXf transform(const std::string& document) const override;
    
    /**
     * @brief 将多个文档转换为向量矩阵
     * @param documents 文档集合
     * @return 文档的TF-IDF矩阵
     */
    Eigen::MatrixXf transform(const std::vector<std::string>& documents) const override;
    
    /**
     * @brief 获取词汇表大小
     * @return 词汇表大小
     */
    size_t getVocabularySize() const override;
    
    /**
     * @brief 获取特征词列表
     * @return 特征词列表
     */
    std::vector<std::string> getFeatureNames() const override;
    
    /**
     * @brief 保存模型到文件
     * @param filepath 文件路径
     * @return 是否成功
     */
    bool save(const std::string& filepath) const override;
    
    /**
     * @brief 从文件加载模型
     * @param filepath 文件路径
     * @return 是否成功
     */
    bool load(const std::string& filepath) override;
    
    /**
     * @brief 设置自定义分词器
     * @param tokenizer 分词函数
     */
    void setTokenizer(std::function<std::vector<std::string>(const std::string&)> tokenizer);
    
    /**
     * @brief 设置停用词
     * @param stopWords 停用词集合
     */
    void setStopWords(const std::set<std::string>& stopWords);

private:
    // 参数设置
    float minDf_;             ///< 最小文档频率
    float maxDf_;             ///< 最大文档频率
    size_t maxFeatures_;      ///< 最大特征数
    std::string normalization_; ///< 归一化方法
    bool useBinary_;          ///< 是否使用二元计数
    float smooth_;            ///< 平滑参数
    
    // 词汇表管理
    std::unordered_map<std::string, size_t> vocabulary_; ///< 词汇映射到索引
    std::vector<std::string> featureNames_;             ///< 索引映射到词汇
    
    // IDF值
    Eigen::VectorXf idf_;     ///< 逆文档频率向量
    
    // 文档统计
    size_t numDocuments_;     ///< 文档总数
    
    // 分词器
    std::function<std::vector<std::string>(const std::string&)> tokenizer_; ///< 分词函数
    
    // 停用词
    std::set<std::string> stopWords_; ///< 停用词集合
    
    /**
     * @brief 对文档集合进行分词
     * @param documents 文档集合
     * @return 分词后的文档集合
     */
    std::vector<std::vector<std::string>> tokenizeDocuments(const std::vector<std::string>& documents) const;
    
    /**
     * @brief 计算词频矩阵
     * @param tokenizedDocs 分词后的文档集合
     * @return 词频矩阵（稀疏矩阵）
     */
    Eigen::SparseMatrix<float> calculateTf(const std::vector<std::vector<std::string>>& tokenizedDocs) const;
    
    /**
     * @brief 计算IDF向量
     * @param tfMatrix 词频矩阵
     * @return IDF向量
     */
    Eigen::VectorXf calculateIdf(const Eigen::SparseMatrix<float>& tfMatrix);
    
    /**
     * @brief 应用归一化
     * @param matrix 需要归一化的矩阵
     * @return 归一化后的矩阵
     */
    Eigen::MatrixXf applyNormalization(const Eigen::MatrixXf& matrix) const;
    
    /**
     * @brief 从分词结果构建词汇表
     * @param tokenizedDocs 分词后的文档集合
     */
    void buildVocabulary(const std::vector<std::vector<std::string>>& tokenizedDocs);
    
    /**
     * @brief 默认分词器
     * @param document 文档
     * @return 分词结果
     */
    static std::vector<std::string> defaultTokenizer(const std::string& document);
};

} // namespace vectorizer
} // namespace kb

#endif // KB_TFIDF_VECTORIZER_H 