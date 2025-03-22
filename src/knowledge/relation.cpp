#include "knowledge/relation.h"
#include "common/logging.h"
#include <sstream>

namespace kb {
namespace knowledge {

// 初始化静态空字符串
const std::string Relation::EMPTY_STRING = "";

// 关系类型转换为字符串
std::string relationTypeToString(RelationType type) {
    switch (type) {
        case RelationType::IS_A:
            return "IS_A";
        case RelationType::PART_OF:
            return "PART_OF";
        case RelationType::LOCATED_IN:
            return "LOCATED_IN";
        case RelationType::BORN_IN:
            return "BORN_IN";
        case RelationType::WORKS_FOR:
            return "WORKS_FOR";
        case RelationType::CREATED_BY:
            return "CREATED_BY";
        case RelationType::HAS_PROPERTY:
            return "HAS_PROPERTY";
        case RelationType::BELONGS_TO:
            return "BELONGS_TO";
        case RelationType::RELATED_TO:
            return "RELATED_TO";
        case RelationType::CUSTOM:
        default:
            return "CUSTOM";
    }
}

// 字符串转换为关系类型
RelationType stringToRelationType(const std::string& typeStr) {
    if (typeStr == "IS_A") {
        return RelationType::IS_A;
    } else if (typeStr == "PART_OF") {
        return RelationType::PART_OF;
    } else if (typeStr == "LOCATED_IN") {
        return RelationType::LOCATED_IN;
    } else if (typeStr == "BORN_IN") {
        return RelationType::BORN_IN;
    } else if (typeStr == "WORKS_FOR") {
        return RelationType::WORKS_FOR;
    } else if (typeStr == "CREATED_BY") {
        return RelationType::CREATED_BY;
    } else if (typeStr == "HAS_PROPERTY") {
        return RelationType::HAS_PROPERTY;
    } else if (typeStr == "BELONGS_TO") {
        return RelationType::BELONGS_TO;
    } else if (typeStr == "RELATED_TO") {
        return RelationType::RELATED_TO;
    } else {
        return RelationType::CUSTOM;
    }
}

// 构造函数
Relation::Relation(const std::string& id, const std::string& name, RelationType type)
    : id_(id), name_(name), type_(type), symmetric_(false) {
    
    if (id_.empty()) {
        LOG_WARN("创建关系时未提供ID");
    }
    
    // 为常见关系类型设置反向关系名称
    switch (type) {
        case RelationType::IS_A:
            inverseName_ = "是...的一种";
            break;
        case RelationType::PART_OF:
            inverseName_ = "包含";
            break;
        case RelationType::LOCATED_IN:
            inverseName_ = "位置包含";
            break;
        case RelationType::BORN_IN:
            inverseName_ = "是...的出生地";
            break;
        case RelationType::WORKS_FOR:
            inverseName_ = "雇佣";
            break;
        case RelationType::CREATED_BY:
            inverseName_ = "创建了";
            break;
        case RelationType::HAS_PROPERTY:
            inverseName_ = "是...的属性";
            break;
        case RelationType::BELONGS_TO:
            inverseName_ = "拥有";
            break;
        case RelationType::RELATED_TO:
            inverseName_ = "相关";
            symmetric_ = true; // 相关关系是对称的
            break;
        default:
            inverseName_ = "";
            break;
    }
}

// Relation::Relation(const std::string& id, 
//              const std::string& name,
//              RelationType type,
//              const std::string& inverseName,
//              bool symmetric)
//     : id_(id), name_(name), type_(type), inverseName_(inverseName), symmetric_(symmetric) {
// }


// 获取关系ID
const std::string& Relation::getId() const {
    return id_;
}

// 获取关系名称
const std::string& Relation::getName() const {
    return name_;
}

// 设置关系名称
void Relation::setName(const std::string& name) {
    name_ = name;
}

// 获取关系类型
RelationType Relation::getType() const {
    return type_;
}

// 设置关系类型
void Relation::setType(RelationType type) {
    type_ = type;
}

// 设置关系的反向关系名称
void Relation::setInverseName(const std::string& inverseName) {
    inverseName_ = inverseName;
}

// 获取关系的反向关系名称
const std::string& Relation::getInverseName() const {
    return inverseName_;
}

// 获取关系是否为对称关系
bool Relation::isSymmetric() const {
    return symmetric_;
}

// 设置关系是否为对称关系
void Relation::setSymmetric(bool symmetric) {
    symmetric_ = symmetric;
}

// 添加属性
void Relation::addProperty(const std::string& key, const std::string& value) {
    if (!key.empty()) {
        properties_[key] = value;
    }
}

// 获取属性
const std::string& Relation::getProperty(const std::string& key) const {
    auto it = properties_.find(key);
    if (it != properties_.end()) {
        return it->second;
    }
    return EMPTY_STRING;
}

// 获取所有属性
const std::unordered_map<std::string, std::string>& Relation::getAllProperties() const {
    return properties_;
}

// 判断两个关系是否相等
bool Relation::equals(const Relation& other) const {
    return id_ == other.id_;
}

// 将关系转换为字符串表示
std::string Relation::toString() const {
    std::ostringstream oss;
    oss << "Relation{id=" << id_ 
        << ", name=" << name_ 
        << ", type=" << relationTypeToString(type_)
        << ", inverseName=" << inverseName_
        << ", symmetric=" << (symmetric_ ? "true" : "false");
    
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

// 创建关系的共享指针
std::shared_ptr<Relation> Relation::create(const std::string& id, 
                                         const std::string& name,
                                         RelationType type) {
    return std::make_shared<Relation>(id, name, type);
}

} // namespace knowledge
} // namespace kb 