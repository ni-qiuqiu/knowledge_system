#ifndef KB_EXTRACTOR_MANAGER_H
#define KB_EXTRACTOR_MANAGER_H

#include "extractor/extractor_base.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

namespace kb {
namespace extractor {

/**
 * @brief 知识提取器管理器，负责注册和管理不同类型的提取器
 */
class ExtractorManager {
public:
    /**
     * @brief 获取单例实例
     * @return 单例实例引用
     */
    static ExtractorManager& getInstance();
    
    /**
     * @brief 注册提取器
     * @param name 提取器名称
     * @param extractor 提取器实例
     * @return 是否注册成功
     */
    bool registerExtractor(const std::string& name, ExtractorPtr extractor);
    
    /**
     * @brief 获取提取器
     * @param name 提取器名称
     * @return 提取器实例，如果不存在返回nullptr
     */
    ExtractorPtr getExtractor(const std::string& name) const;
    
    /**
     * @brief 获取所有提取器名称
     * @return 提取器名称列表
     */
    std::vector<std::string> getRegisteredExtractorNames() const;
    
    /**
     * @brief 从文本中使用指定提取器提取实体
     * @param name 提取器名称
     * @param text 输入文本
     * @return 提取的实体列表
     */
    std::vector<knowledge::EntityPtr> extractEntities(
        const std::string& name, const std::string& text);
    
    /**
     * @brief 从文本中使用指定提取器提取关系
     * @param name 提取器名称
     * @param text 输入文本
     * @param entities 已提取的实体列表
     * @return 提取的关系列表
     */
    std::vector<knowledge::RelationPtr> extractRelations(
        const std::string& name, 
        const std::string& text,
        const std::vector<knowledge::EntityPtr>& entities = {});
    
    /**
     * @brief 从文本中使用指定提取器提取三元组
     * @param name 提取器名称
     * @param text 输入文本
     * @return 提取的三元组列表
     */
    std::vector<knowledge::TriplePtr> extractTriples(
        const std::string& name, const std::string& text);
    
    /**
     * @brief 从文本中使用指定提取器构建知识图谱
     * @param name 提取器名称
     * @param text 输入文本
     * @param graph 可选参数，现有知识图谱
     * @return 构建的知识图谱
     */
    knowledge::KnowledgeGraphPtr buildKnowledgeGraph(
        const std::string& name,
        const std::string& text,
        knowledge::KnowledgeGraphPtr graph = nullptr);
    
    /**
     * @brief 设置默认提取器
     * @param name 提取器名称
     * @return 是否设置成功
     */
    bool setDefaultExtractor(const std::string& name);
    
    /**
     * @brief 获取默认提取器名称
     * @return 默认提取器名称
     */
    const std::string& getDefaultExtractorName() const;
    
    /**
     * @brief 获取默认提取器
     * @return 默认提取器实例，如果不存在返回nullptr
     */
    ExtractorPtr getDefaultExtractor() const;
    
    /**
     * @brief 注册常见内置提取器
     */
    void registerBuiltinExtractors();

private:
    /**
     * @brief 构造函数（私有）
     */
    ExtractorManager();
    
    /**
     * @brief 析构函数（私有）
     */
    ~ExtractorManager() = default;
    
    /**
     * @brief 拷贝构造函数（私有，禁止使用）
     */
    ExtractorManager(const ExtractorManager&) = delete;
    
    /**
     * @brief 赋值运算符（私有，禁止使用）
     */
    ExtractorManager& operator=(const ExtractorManager&) = delete;
    
    std::unordered_map<std::string, ExtractorPtr> extractors_; ///< 已注册的提取器
    std::string defaultExtractorName_;                    ///< 默认提取器名称
};

} // namespace extractor
} // namespace kb

#endif // KB_EXTRACTOR_MANAGER_H 