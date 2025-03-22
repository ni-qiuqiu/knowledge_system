#ifndef KB_ENTITY_H
#define KB_ENTITY_H

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <Eigen/Dense>

namespace kb {
namespace knowledge {

/**
 * @brief 实体链接结构体，用于存储文本中提及的实体与知识库中实体的链接关系
 */
struct EntityLink {
    std::string mentionText;    ///< 文本中提及的实体文本
    std::string entityId;       ///< 链接到的知识库实体ID
    float confidence;           ///< 链接的置信度
    
    // 构造函数
    EntityLink() : confidence(0.0f) {}
};

/**
 * @brief 实体类型枚举
 */
enum class EntityType {
    PERSON,         ///< 人物
    ORGANIZATION,   ///< 组织
    LOCATION,       ///< 地点
    TIME,           ///< 时间
    EVENT,          ///< 事件
    CONCEPT,        ///< 概念
    PRODUCT,        ///< 产品
    RIVER,          ///< 河流
    MOUNTAIN,       ///< 山脉
    UNKNOWN         ///< 未知类型
};

/**
 * @brief 将实体类型转换为字符串
 * @param type 实体类型
 * @return 实体类型字符串
 */
std::string entityTypeToString(EntityType type);

/**
 * @brief 将字符串转换为实体类型
 * @param typeStr 实体类型字符串
 * @return 实体类型
 */
EntityType stringToEntityType(const std::string& typeStr);

/**
 * @brief 实体类，表示知识图谱中的节点
 */
class Entity {
public:
    /**
     * @brief 构造函数
     * @param id 实体ID
     * @param name 实体名称
     * @param type 实体类型
     */
    Entity(const std::string& id, 
           const std::string& name, 
           EntityType type = EntityType::UNKNOWN);
    
    /**
     * @brief 析构函数
     */
    ~Entity() = default;
    
    /**
     * @brief 获取实体ID
     * @return 实体ID
     */
    const std::string& getId() const;
    
    /**
     * @brief 获取实体名称
     * @return 实体名称
     */
    const std::string& getName() const;
    
    /**
     * @brief 设置实体名称
     * @param name 新名称
     */
    void setName(const std::string& name);
    
    /**
     * @brief 获取实体类型
     * @return 实体类型
     */
    EntityType getType() const;
    
    /**
     * @brief 设置实体类型
     * @param type 新类型
     */
    void setType(EntityType type);
    
    /**
     * @brief 获取实体向量表示
     * @return 实体向量
     */
    const Eigen::VectorXf& getVector() const;
    
    /**
     * @brief 设置实体向量表示
     * @param vector 新的向量表示
     */
    void setVector(const Eigen::VectorXf& vector);
    
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
     * @brief 判断两个实体是否相等
     * @param other 另一个实体
     * @return 是否相等
     */
    bool equals(const Entity& other) const;
    
    /**
     * @brief 将实体转换为字符串表示
     * @return 实体的字符串表示
     */
    std::string toString() const;
    
    /**
     * @brief 创建实体的共享指针
     * @param id 实体ID
     * @param name 实体名称
     * @param type 实体类型
     * @return 实体的共享指针
     */
    static std::shared_ptr<Entity> create(const std::string& id, 
                                         const std::string& name, 
                                         EntityType type = EntityType::UNKNOWN);

private:
    std::string id_;                      ///< 实体ID
    std::string name_;                    ///< 实体名称
    EntityType type_;                     ///< 实体类型
    Eigen::VectorXf vector_;              ///< 实体向量表示
    std::unordered_map<std::string, std::string> properties_; ///< 实体属性
    
    // 为空属性值返回静态空字符串
    static const std::string EMPTY_STRING;
};

// 定义实体的共享指针类型
using EntityPtr = std::shared_ptr<Entity>;

} // namespace knowledge
} // namespace kb

#endif // KB_ENTITY_H 