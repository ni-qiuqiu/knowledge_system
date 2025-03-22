/**
 * @file text_cleaner.h
 * @brief 文本清洗器，定义文本清洗功能
 */

#ifndef KB_TEXT_CLEANER_H
#define KB_TEXT_CLEANER_H

#include <string>
#include <vector>
#include <regex>
#include <memory>
#include <functional>

namespace kb {
namespace preprocessor {

/**
 * @brief 清洗规则接口
 */
class CleaningRule {
public:
    /**
     * @brief 析构函数
     */
    virtual ~CleaningRule() = default;
    
    /**
     * @brief 应用清洗规则
     * @param text 待清洗文本
     * @return 清洗后的文本
     */
    virtual std::string apply(const std::string& text) const = 0;
};

/**
 * @brief 正则表达式替换规则
 */
class RegexReplacementRule : public CleaningRule {
public:
    /**
     * @brief 构造函数
     * @param pattern 正则表达式模式
     * @param replacement 替换字符串
     */
    RegexReplacementRule(const std::string& pattern, const std::string& replacement);
    
    /**
     * @brief 应用正则替换规则
     * @param text 待清洗文本
     * @return 清洗后的文本
     */
    std::string apply(const std::string& text) const override;
    
private:
    std::regex pattern_;
    std::string replacement_;
};

/**
 * @brief 基于函数的清洗规则
 */
class FunctionRule : public CleaningRule {
public:
    /**
     * @brief 构造函数
     * @param func 函数对象，接受string参数，返回string
     */
    explicit FunctionRule(std::function<std::string(const std::string&)> func);
    
    /**
     * @brief 应用函数规则
     * @param text 待清洗文本
     * @return 清洗后的文本
     */
    std::string apply(const std::string& text) const override;
    
private:
    std::function<std::string(const std::string&)> func_;
};

/**
 * @brief 文本清洗器
 */
class TextCleaner {
public:
    /**
     * @brief 构造函数
     */
    TextCleaner();
    
    /**
     * @brief 清洗文本
     * @param text 待清洗文本
     * @return 清洗后的文本
     */
    std::string clean(const std::string& text) const;
    
    /**
     * @brief 添加清洗规则
     * @param rule 清洗规则
     */
    void addRule(std::unique_ptr<CleaningRule> rule);
    
    /**
     * @brief 添加正则表达式替换规则
     * @param pattern 正则表达式模式
     * @param replacement 替换字符串
     */
    void addRegexRule(const std::string& pattern, const std::string& replacement);
    
    /**
     * @brief 添加函数规则
     * @param func 函数对象，接受string参数，返回string
     */
    void addFunctionRule(std::function<std::string(const std::string&)> func);
    
private:
    std::vector<std::unique_ptr<CleaningRule>> rules_;
};

/**
 * @brief 基本文本清洗器
 * 实现基本的文本清洗功能，如去除多余空白、标准化换行符等
 */
class BasicCleaner : public TextCleaner {
public:
    /**
     * @brief 构造函数
     * 初始化基本的清洗规则
     */
    BasicCleaner() {
        // 标准化换行符
        addRegexRule("\\r\\n", "\n");
        
        // 删除连续空白
        addRegexRule("[ \\t]+", " ");
        
        // 删除行首空白
        addRegexRule("\\n[ \\t]+", "\n");
        
        // 删除行尾空白
        addRegexRule("[ \\t]+\\n", "\n");
        
        // 删除连续多个换行符
        addRegexRule("\\n{3,}", "\n\n");
        
        // 删除不可打印字符
        addRegexRule("[\\x00-\\x08\\x0B\\x0C\\x0E-\\x1F\\x7F]", "");
    }
};

} // namespace preprocessor
} // namespace kb

#endif // KB_TEXT_CLEANER_H