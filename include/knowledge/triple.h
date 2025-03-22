#ifndef KB_TRIPLE_H
#define KB_TRIPLE_H

#include "knowledge/entity.h"
#include "knowledge/relation.h"
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>

namespace kb {
namespace knowledge {

/**
 * @brief 三元组类，表示"主体-关系-客体"的知识结构
 */
class Triple {
public:
    /**
     * @brief 构造函数
     * @param subject 主体实体
     * @param relation 关系
     * @param object 客体实体
     * @param confidence 置信度
     */
    Triple(EntityPtr subject, 
           RelationPtr relation, 
           EntityPtr object, 
           float confidence = 1.0);
    
    /**
     * @brief 析构函数
     */
    ~Triple() = default;
    
    /**
     * @brief 获取主体实体
     * @return 主体实体
     */
    EntityPtr getSubject() const;
    
    /**
     * @brief 获取关系
     * @return 关系
     */
    RelationPtr getRelation() const;
    
    /**
     * @brief 获取客体实体
     * @return 客体实体
     */
    EntityPtr getObject() const;
    
    /**
     * @brief 获取置信度
     * @return 置信度
     */
    float getConfidence() const;
    
    /**
     * @brief 设置置信度
     * @param confidence 新的置信度
     */
    void setConfidence(float confidence);
    
    /**
     * @brief 获取三元组ID
     * @return 三元组ID
     */
    std::string getId() const;
    
    /**
     * @brief 添加属性
     * @param key 属性名
     * @param value 属性值
     */
    void addProperty(const std::string& key, const std::string& value);
    
    /**
     * @brief 获取属性
     * @param key 属性名
     * @return 属性值，如果不存在返回空字符串
     */
    const std::string& getProperty(const std::string& key) const;
    
    /**
     * @brief 获取所有属性
     * @return 所有属性的映射
     */
    const std::unordered_map<std::string, std::string>& getAllProperties() const;
    
    /**
     * @brief 判断两个三元组是否相等
     * @param other 另一个三元组
     * @return 是否相等
     */
    bool equals(const Triple& other) const;
    
    /**
     * @brief 将三元组转换为字符串表示
     * @return 三元组的字符串表示
     */
    std::string toString() const;
    
    /**
     * @brief 创建三元组的共享指针
     * @param subject 主体实体
     * @param relation 关系
     * @param object 客体实体
     * @param confidence 置信度
     * @return 三元组的共享指针
     */
    static std::shared_ptr<Triple> create(EntityPtr subject, 
                                         RelationPtr relation, 
                                         EntityPtr object, 
                                         float confidence = 1.0);

private:
    EntityPtr subject_;                                 ///< 主体实体
    RelationPtr relation_;                              ///< 关系
    EntityPtr object_;                                  ///< 客体实体
    float confidence_;                                  ///< 置信度 (0-1)
    std::unordered_map<std::string, std::string> properties_; ///< 三元组属性
    
    /**
     * @brief 生成三元组ID
     * @return 三元组ID
     */
    std::string generateId() const;
    
    // 为空属性值返回静态空字符串
    static const std::string EMPTY_STRING;
};

// 定义三元组的共享指针类型
using TriplePtr = std::shared_ptr<Triple>;

// 定义三元组集合类型
using TripleSet = std::vector<TriplePtr>;

} // namespace knowledge
} // namespace kb

#endif // KB_TRIPLE_H 