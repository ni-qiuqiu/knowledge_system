#ifndef KB_VECTORIZER_H
#define KB_VECTORIZER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <Eigen/Dense>

namespace kb {
namespace vectorizer {

/**
 * @brief 向量化器基类，定义文本向量化的通用接口
 */
class Vectorizer {
public:
    /**
     * @brief 默认构造函数
     */
    Vectorizer() = default;
    
    /**
     * @brief 虚析构函数
     */
    virtual ~Vectorizer() = default;
    
    /**
     * @brief 拟合向量化模型
     * @param documents 文档集合，每个文档是一个字符串
     */
    virtual void fit(const std::vector<std::string>& documents) = 0;
    
    /**
     * @brief 将文档转换为向量
     * @param document 单个文档
     * @return 文档的向量表示
     */
    virtual Eigen::VectorXf transform(const std::string& document) const = 0;
    
    /**
     * @brief 将多个文档转换为向量矩阵
     * @param documents 文档集合
     * @return 文档向量矩阵，每行对应一个文档的向量
     */
    virtual Eigen::MatrixXf transform(const std::vector<std::string>& documents) const = 0;
    
    /**
     * @brief 拟合向量化模型并转换文档
     * @param documents 文档集合
     * @return 文档向量矩阵
     */
    virtual Eigen::MatrixXf fitTransform(const std::vector<std::string>& documents);
    
    /**
     * @brief 获取词汇表大小
     * @return 词汇表大小
     */
    virtual size_t getVocabularySize() const = 0;
    
    /**
     * @brief 获取特征词列表
     * @return 特征词列表
     */
    virtual std::vector<std::string> getFeatureNames() const = 0;
    
    /**
     * @brief 保存模型到文件
     * @param filepath 文件路径
     * @return 是否成功
     */
    virtual bool save(const std::string& filepath) const = 0;
    
    /**
     * @brief 从文件加载模型
     * @param filepath 文件路径
     * @return 是否成功
     */
    virtual bool load(const std::string& filepath) = 0;
};

/**
 * @brief 创建向量化器工厂函数
 * @param type 向量化器类型 ("tfidf", "bow", "word2vec", "fasttext" 等)
 * @param params 向量化器参数
 * @return 向量化器指针
 */
std::unique_ptr<Vectorizer> createVectorizer(
    const std::string& type, 
    const std::unordered_map<std::string, std::string>& params = {});

} // namespace vectorizer
} // namespace kb

#endif // KB_VECTORIZER_H 