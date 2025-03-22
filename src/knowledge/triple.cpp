#include "knowledge/triple.h"
#include "common/logging.h"
#include <sstream>

namespace kb {
namespace knowledge {

// 初始化静态空字符串
const std::string Triple::EMPTY_STRING = "";

// 构造函数
Triple::Triple(EntityPtr subject, RelationPtr relation, EntityPtr object, float confidence)
    : subject_(subject), relation_(relation), object_(object), confidence_(confidence) {
    
    if (!subject || !relation || !object) {
        LOG_WARN("创建三元组时提供了空指针");
    }
}

// 获取主体实体
EntityPtr Triple::getSubject() const {
    return subject_;
}

// 获取关系
RelationPtr Triple::getRelation() const {
    return relation_;
}

// 获取客体实体
EntityPtr Triple::getObject() const {
    return object_;
}

// 获取置信度
float Triple::getConfidence() const {
    return confidence_;
}

// 设置置信度
void Triple::setConfidence(float confidence) {
    confidence_ = std::max(0.0f, std::min(1.0f, confidence)); // 确保置信度在[0,1]范围内
}

// 获取三元组ID
std::string Triple::getId() const {
    return generateId();
}

// 添加属性
void Triple::addProperty(const std::string& key, const std::string& value) {
    if (!key.empty()) {
        properties_[key] = value;
    }
}

// 获取属性
const std::string& Triple::getProperty(const std::string& key) const {
    auto it = properties_.find(key);
    if (it != properties_.end()) {
        return it->second;
    }
    return EMPTY_STRING;
}

// 获取所有属性
const std::unordered_map<std::string, std::string>& Triple::getAllProperties() const {
    return properties_;
}

// 判断两个三元组是否相等
bool Triple::equals(const Triple& other) const {
    // 三元组相等意味着主体、关系和客体都相等
    return subject_->equals(*other.subject_) && 
           relation_->equals(*other.relation_) && 
           object_->equals(*other.object_);
}

// 将三元组转换为字符串表示
std::string Triple::toString() const {
    std::ostringstream oss;
    oss << "Triple{subject=" << subject_->getName() 
        << ", relation=" << relation_->getName() 
        << ", object=" << object_->getName()
        << ", confidence=" << confidence_;
    
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

// 创建三元组的共享指针
std::shared_ptr<Triple> Triple::create(EntityPtr subject, 
                                      RelationPtr relation, 
                                      EntityPtr object, 
                                      float confidence) {
    return std::make_shared<Triple>(subject, relation, object, confidence);
}

// 生成三元组ID
std::string Triple::generateId() const {
    if (!subject_ || !relation_ || !object_) {
        return "";
    }
    return subject_->getId() + "_" + relation_->getId() + "_" + object_->getId();
}

} // namespace knowledge
} // namespace kb 