#ifndef KB_RELATION_H
#define KB_RELATION_H

#include <string>
#include <unordered_map>
#include <memory>

namespace kb {
namespace knowledge {

/**
 * @brief 关系类型枚举
 */
enum class RelationType {
    IS_A,           ///< 是一个（继承关系）
    PART_OF,        ///< 部分关系
    LOCATED_IN,     ///< 位于关系
    BORN_IN,        ///< 出生于关系
    WORKS_FOR,      ///< 为...工作
    CREATED_BY,     ///< 被...创建
    HAS_PROPERTY,   ///< 具有属性
    BELONGS_TO,     ///< 属于关系
    RELATED_TO,     ///< 相关关系
    CUSTOM,         ///< 自定义关系
    FLOWS_THROUGH,   ///< 流经关系
    FRIEND_OF,      ///< 朋友关系
    LOCATED_NEAR,   ///< 附近关系
    LOCATED_ON,     ///< 位于关系
    LOCATED_AT,     ///< 位于关系
    CAPITAL_OF,     ///< 首都关系
    UNKNOWN         ///< 未知关系
};

/**
 * @brief 将关系类型转换为字符串
 * @param type 关系类型
 * @return 关系类型字符串
 */
std::string relationTypeToString(RelationType type);

/**
 * @brief 将字符串转换为关系类型
 * @param typeStr 关系类型字符串
 * @return 关系类型
 */
RelationType stringToRelationType(const std::string& typeStr);

/**
 * @brief 关系类，表示两个实体之间的联系
 */
class Relation {
public:
    /**
     * @brief 构造函数
     * @param id 关系ID
     * @param name 关系名称
     * @param type 关系类型
     */
    Relation(const std::string& id, 
             const std::string& name,
             RelationType type = RelationType::CUSTOM);

    // /**
    //  * @brief 构造函数
    //  * @param id 关系ID
    //  * @param name 关系名称
    //  * @param type 关系类型
    //  * @param inverseName 反向关系名称
    //  * @param symmetric 是否为对称关系
    //  */
    // Relation(const std::string& id, 
    //          const std::string& name,
    //          RelationType type = RelationType::CUSTOM,
    //          const std::string& inverseName = "",
    //          bool symmetric = false);

    /**
     * @brief 析构函数
     */
    ~Relation() = default;
    
    /**
     * @brief 获取关系ID
     * @return 关系ID
     */
    const std::string& getId() const;
    
    /**
     * @brief 获取关系名称
     * @return 关系名称
     */
    const std::string& getName() const;
    
    /**
     * @brief 设置关系名称
     * @param name 新名称
     */
    void setName(const std::string& name);
    
    /**
     * @brief 获取关系类型
     * @return 关系类型
     */
    RelationType getType() const;
    
    /**
     * @brief 设置关系类型
     * @param type 新类型
     */
    void setType(RelationType type);
    
    /**
     * @brief 设置关系的反向关系名称
     * @param inverseName 反向关系名称
     */
    void setInverseName(const std::string& inverseName);
    
    /**
     * @brief 获取关系的反向关系名称
     * @return 反向关系名称
     */
    const std::string& getInverseName() const;
    
    /**
     * @brief 获取关系是否为对称关系
     * @return 是否为对称关系
     */
    bool isSymmetric() const;
    
    /**
     * @brief 设置关系是否为对称关系
     * @param symmetric 是否为对称关系
     */
    void setSymmetric(bool symmetric);
    
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
     * @brief 判断两个关系是否相等
     * @param other 另一个关系
     * @return 是否相等
     */
    bool equals(const Relation& other) const;
    
    /**
     * @brief 将关系转换为字符串表示
     * @return 关系的字符串表示
     */
    std::string toString() const;
    
    /**
     * @brief 创建关系的共享指针
     * @param id 关系ID
     * @param name 关系名称
     * @param type 关系类型
     * @return 关系的共享指针
     */
    static std::shared_ptr<Relation> create(const std::string& id, 
                                           const std::string& name,
                                           RelationType type = RelationType::CUSTOM);

private:
    std::string id_;                      ///< 关系ID
    std::string name_;                    ///< 关系名称
    RelationType type_;                   ///< 关系类型
    std::string inverseName_;             ///< 反向关系名称
    bool symmetric_;                      ///< 是否为对称关系
    std::unordered_map<std::string, std::string> properties_; ///< 关系属性
    
    // 为空属性值返回静态空字符串
    static const std::string EMPTY_STRING;
};

// 定义关系的共享指针类型
using RelationPtr = std::shared_ptr<Relation>;

} // namespace knowledge
} // namespace kb

#endif // KB_RELATION_H 