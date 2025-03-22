#include "knowledge/entity.h"
#include "common/logging.h"
#include <sstream>

namespace kb {
namespace knowledge {

// 初始化静态空字符串
const std::string Entity::EMPTY_STRING = "";

// 实体类型转换为字符串
std::string entityTypeToString(EntityType type) {
    switch (type) {
        case EntityType::PERSON:
            return "PERSON";
        case EntityType::ORGANIZATION:
            return "ORGANIZATION";
        case EntityType::LOCATION:
            return "LOCATION";
        case EntityType::TIME:
            return "TIME";
        case EntityType::EVENT:
            return "EVENT";
        case EntityType::CONCEPT:
            return "CONCEPT";
        case EntityType::UNKNOWN:
        default:
            return "UNKNOWN";
    }
}

// 字符串转换为实体类型
EntityType stringToEntityType(const std::string& typeStr) {
    if (typeStr == "PERSON") {
        return EntityType::PERSON;
    } else if (typeStr == "ORGANIZATION") {
        return EntityType::ORGANIZATION;
    } else if (typeStr == "LOCATION") {
        return EntityType::LOCATION;
    } else if (typeStr == "TIME") {
        return EntityType::TIME;
    } else if (typeStr == "EVENT") {
        return EntityType::EVENT;
    } else if (typeStr == "CONCEPT") {
        return EntityType::CONCEPT;
    } else {
        return EntityType::UNKNOWN;
    }
}

// 构造函数
Entity::Entity(const std::string& id, const std::string& name, EntityType type)
    : id_(id), name_(name), type_(type) {
    
    if (id_.empty()) {
        LOG_WARN("创建实体时未提供ID");
    }
}

// 获取实体ID
const std::string& Entity::getId() const {
    return id_;
}

// 获取实体名称
const std::string& Entity::getName() const {
    return name_;
}

// 设置实体名称
void Entity::setName(const std::string& name) {
    name_ = name;
}

// 获取实体类型
EntityType Entity::getType() const {
    return type_;
}

// 设置实体类型
void Entity::setType(EntityType type) {
    type_ = type;
}

// 获取实体向量表示
const Eigen::VectorXf& Entity::getVector() const {
    return vector_;
}

// 设置实体向量表示
void Entity::setVector(const Eigen::VectorXf& vector) {
    vector_ = vector;
}

// 添加属性
void Entity::addProperty(const std::string& key, const std::string& value) {
    if (!key.empty()) {
        properties_[key] = value;
    }
}

// 获取属性
const std::string& Entity::getProperty(const std::string& key) const {
    auto it = properties_.find(key);
    if (it != properties_.end()) {
        return it->second;
    }
    return EMPTY_STRING;
}

// 获取所有属性
const std::unordered_map<std::string, std::string>& Entity::getAllProperties() const {
    return properties_;
}

// 判断两个实体是否相等
bool Entity::equals(const Entity& other) const {
    return id_ == other.id_;
}

// 将实体转换为字符串表示
std::string Entity::toString() const {
    std::ostringstream oss;
    oss << "Entity{id=" << id_ 
        << ", name=" << name_ 
        << ", type=" << entityTypeToString(type_);
    
    if (!properties_.empty()) {
        oss << ", properties={";
        bool first = true;
        for (const auto& pair : properties_) {
            if (!first) {
                oss << ", ";
            }
            oss << pair.first << "=" << pair.second;
            first = false;
        }
        oss << "}";
    }
    
    oss << "}";
    return oss.str();
}

// 创建实体的共享指针
std::shared_ptr<Entity> Entity::create(const std::string& id, 
                                      const std::string& name, 
                                      EntityType type) {
    return std::make_shared<Entity>(id, name, type);
}

} // namespace knowledge
} // namespace kb 