#ifndef KB_EXTRACTOR_BASE_H
#define KB_EXTRACTOR_BASE_H

#include "knowledge/entity.h"
#include "knowledge/relation.h"
#include "knowledge/triple.h"
#include "knowledge/knowledge_graph.h"
#include <string>
#include <vector>
#include <memory>

namespace kb {
namespace extractor {

/**
 * @brief 知识提取器基类，定义提取知识的公共接口
 */
class ExtractorBase {
public:
    /**
     * @brief 默认构造函数
     */
    ExtractorBase() = default;
    
    /**
     * @brief 虚析构函数
     */
    virtual ~ExtractorBase() = default;
    
    /**
     * @brief 从文本中提取实体
     * @param text 输入文本
     * @return 提取的实体列表
     */
    virtual std::vector<knowledge::EntityPtr> extractEntities(const std::string& text) = 0;
    
    /**
     * @brief 从文本中提取关系
     * @param text 输入文本
     * @param entities 可选参数，已提取的实体列表
     * @return 提取的关系列表
     */
    virtual std::vector<knowledge::RelationPtr> extractRelations(
        const std::string& text,
        const std::vector<knowledge::EntityPtr>& entities = {}) = 0;
    
    /**
     * @brief 从文本中提取三元组
     * @param text 输入文本
     * @return 提取的三元组列表
     */
    virtual std::vector<knowledge::TriplePtr> extractTriples(const std::string& text) = 0;
    
    /**
     * @brief 从文本中构建知识图谱
     * @param text 输入文本
     * @param graph 可选参数，现有知识图谱
     * @return 构建的知识图谱
     */
    virtual knowledge::KnowledgeGraphPtr buildKnowledgeGraph(
        const std::string& text,
        knowledge::KnowledgeGraphPtr graph = nullptr) = 0;
    
    /**
     * @brief 获取提取器的名称
     * @return 提取器名称
     */
    virtual std::string getName() const = 0;
    
    /**
     * @brief 获取提取器的描述
     * @return 提取器描述
     */
    virtual std::string getDescription() const {
        return "基础知识提取器";
    }
    
    /**
     * @brief 设置提取器的配置参数
     * @param key 参数名
     * @param value 参数值
     */
    virtual void setParameter(const std::string& key, const std::string& value) {
        parameters_[key] = value;
    }
    
    /**
     * @brief 获取提取器的配置参数
     * @param key 参数名
     * @param defaultValue 默认值
     * @return 参数值
     */
    virtual std::string getParameter(const std::string& key, const std::string& defaultValue = "") const {
        auto it = parameters_.find(key);
        if (it != parameters_.end()) {
            return it->second;
        }
        return defaultValue;
    }

protected:
    std::unordered_map<std::string, std::string> parameters_; ///< 提取器参数
};

// 定义提取器的共享指针类型
using ExtractorPtr = std::shared_ptr<ExtractorBase>;

} // namespace extractor
} // namespace kb

#endif // KB_EXTRACTOR_BASE_H 