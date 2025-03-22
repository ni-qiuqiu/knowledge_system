#include "extractor/extractor_manager.h"
#include "extractor/rule_based_extractor.h"
#include "common/logging.h"

namespace kb {
namespace extractor {

// 获取单例实例
ExtractorManager& ExtractorManager::getInstance() {
    static ExtractorManager instance;
    return instance;
}

// 构造函数
ExtractorManager::ExtractorManager()
    : defaultExtractorName_("rule_based") {
    LOG_INFO("初始化知识提取器管理器");
}

// 注册提取器
bool ExtractorManager::registerExtractor(const std::string& name, ExtractorPtr extractor) {
    if (!extractor) {
        LOG_ERROR("尝试注册空提取器: {}" , name);
        return false;
    }
    
    auto it = extractors_.find(name);
    if (it != extractors_.end()) {
        LOG_WARN("提取器名称已存在，将被替换: {}" , name);
    }
    
    extractors_[name] = extractor;
    LOG_INFO("注册提取器: {}" , name + " (" + extractor->getName() + ")");
    return true;
}

// 获取提取器
ExtractorPtr ExtractorManager::getExtractor(const std::string& name) const {
    auto it = extractors_.find(name);
    if (it != extractors_.end()) {
        return it->second;
    }
    
    LOG_WARN("提取器不存在: {}" , name);
    return nullptr;
}

// 获取所有提取器名称
std::vector<std::string> ExtractorManager::getRegisteredExtractorNames() const {
    std::vector<std::string> names;
    names.reserve(extractors_.size());
    
    for (const auto& pair : extractors_) {
        names.push_back(pair.first);
    }
    
    return names;
}

// 设置默认提取器
bool ExtractorManager::setDefaultExtractor(const std::string& name) {
    auto it = extractors_.find(name);
    if (it == extractors_.end()) {
        LOG_ERROR("无法设置默认提取器，提取器不存在: {}" , name);
        return false;
    }
    
    defaultExtractorName_ = name;
    LOG_INFO("设置默认提取器: {}" , name);
    return true;
}

// 获取默认提取器名称
const std::string& ExtractorManager::getDefaultExtractorName() const {
    return defaultExtractorName_;
}

// 获取默认提取器
ExtractorPtr ExtractorManager::getDefaultExtractor() const {
    return getExtractor(defaultExtractorName_);
}

// 注册常见内置提取器
void ExtractorManager::registerBuiltinExtractors() {
    // 注册基于规则的提取器
    auto ruleBasedExtractor = std::make_shared<RuleBasedExtractor>();
    registerExtractor("rule_based", ruleBasedExtractor);
    
    // 添加预定义规则集
    ruleBasedExtractor->addPredefinedRuleset("chinese_all");
    
    // 设置为默认提取器
    setDefaultExtractor("rule_based");
    
    LOG_INFO("注册内置提取器完成");
}

// 从文本中使用指定提取器提取实体
std::vector<knowledge::EntityPtr> ExtractorManager::extractEntities(
    const std::string& name, const std::string& text) {
    
    auto extractor = getExtractor(name);
    if (!extractor) {
        LOG_ERROR("提取实体失败，提取器不存在: {}" , name);
        return {};
    }
    
    return extractor->extractEntities(text);
}

// 从文本中使用指定提取器提取关系
std::vector<knowledge::RelationPtr> ExtractorManager::extractRelations(
    const std::string& name, 
    const std::string& text,
    const std::vector<knowledge::EntityPtr>& entities) {
    
    auto extractor = getExtractor(name);
    if (!extractor) {
        LOG_ERROR("提取关系失败，提取器不存在: {}" , name);
        return {};
    }
    
    return extractor->extractRelations(text, entities);
}

// 从文本中使用指定提取器提取三元组
std::vector<knowledge::TriplePtr> ExtractorManager::extractTriples(
    const std::string& name, const std::string& text) {
    
    auto extractor = getExtractor(name);
    if (!extractor) {
        LOG_ERROR("提取三元组失败，提取器不存在: {}" , name);
        return {};
    }
    
    return extractor->extractTriples(text);
}

// 从文本中使用指定提取器构建知识图谱
knowledge::KnowledgeGraphPtr ExtractorManager::buildKnowledgeGraph(
    const std::string& name,
    const std::string& text,
    knowledge::KnowledgeGraphPtr graph) {
    
    auto extractor = getExtractor(name);
    if (!extractor) {
        LOG_ERROR("构建知识图谱失败，提取器不存在: {}" , name);
        return graph ? graph : std::make_shared<knowledge::KnowledgeGraph>();
    }
    
    return extractor->buildKnowledgeGraph(text, graph);
}

} // namespace extractor
} // namespace kb 