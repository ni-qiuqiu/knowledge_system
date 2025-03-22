#include "preprocessor/text_cleaner.h"
#include "common/logging.h"
#include "common/utils.h"

#include <boost/algorithm/string.hpp>

namespace kb {
namespace preprocessor {

RegexReplacementRule::RegexReplacementRule(const std::string& pattern, const std::string& replacement)
    : pattern_(pattern, std::regex::optimize | std::regex::ECMAScript), replacement_(replacement) {
}

std::string RegexReplacementRule::apply(const std::string& text) const {
    return std::regex_replace(text, pattern_, replacement_);
}

FunctionRule::FunctionRule(std::function<std::string(const std::string&)> func)
    : func_(std::move(func)) {
}

std::string FunctionRule::apply(const std::string& text) const {
    return func_(text);
}

TextCleaner::TextCleaner() {
    // 添加默认的清洗规则
    
    // 移除HTML标签
    addRegexRule("<[^>]*>", " ");
    
    // 规范化空白字符 (多个空白替换为单个空格)
    addRegexRule("\\s+", " ");
    
    // 移除文档头部格式信息 (常见于PDF转换)
    addRegexRule("^\\s*(?:Page|Document)\\s+\\d+.*?(?:\\n|\\r\\n?)", "");
    
    // 移除特殊控制字符
    addRegexRule("[\\x00-\\x08\\x0B\\x0C\\x0E-\\x1F\\x7F]", "");
    
    // 处理常见HTML实体
    addFunctionRule([](const std::string& text) {
        std::string result = text;
        std::vector<std::pair<std::string, std::string>> entities = {
            {"&nbsp;", " "}, {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"},
            {"&quot;", "\""}, {"&apos;", "'"}, {"&#39;", "'"}
        };
        
        for (const auto& entity : entities) {
            boost::replace_all(result, entity.first, entity.second);
        }
        
        return result;
    });
    
    // 合并多个换行
    addRegexRule("(\\r\\n|\\n|\\r){3,}", "\n\n");
}

std::string TextCleaner::clean(const std::string& text) const {
    LOG_DEBUG("开始清洗文本...");
    
    std::string result = text;
    
    // 依次应用所有清洗规则
    for (const auto& rule : rules_) {
        result = rule->apply(result);
    }
    
    // 最后进行修剪，去除首尾空白
    result = kb::utils::trim(result);
    
    LOG_DEBUG("文本清洗完成");
    return result;
}

void TextCleaner::addRule(std::unique_ptr<CleaningRule> rule) {
    rules_.push_back(std::move(rule));
}

void TextCleaner::addRegexRule(const std::string& pattern, const std::string& replacement) {
    rules_.push_back(std::make_unique<RegexReplacementRule>(pattern, replacement));
}

void TextCleaner::addFunctionRule(std::function<std::string(const std::string&)> func) {
    rules_.push_back(std::make_unique<FunctionRule>(std::move(func)));
}

} // namespace preprocessor
} // namespace kb