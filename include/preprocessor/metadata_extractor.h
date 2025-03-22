/**
 * @file metadata_extractor.h
 * @brief 元数据提取器，定义文档元数据提取功能
 */

#ifndef KB_METADATA_EXTRACTOR_H
#define KB_METADATA_EXTRACTOR_H

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

namespace kb {
namespace preprocessor {

/**
 * @brief 文档类型枚举
 */
enum class DocumentType {
    UNKNOWN,
    TEXT,
    PDF,
    WORD,
    HTML,
    MARKDOWN,
    JSON,
    XML,
    CSV
};

/**
 * @brief 元数据提取器接口
 */
class MetadataExtractor {
public:
    /**
     * @brief 析构函数
     */
    virtual ~MetadataExtractor() = default;
    
    /**
     * @brief 从文件路径提取元数据
     * @param filePath 文件路径
     * @return 元数据映射表
     */
    virtual std::unordered_map<std::string, std::string> extract(
        const std::string& filePath) const = 0;
    
    /**
     * @brief 从文件内容提取元数据
     * @param content 文件内容
     * @param fileName 文件名（可选）
     * @return 元数据映射表
     */
    virtual std::unordered_map<std::string, std::string> extractFromContent(
        const std::string& content,
        const std::string& fileName = "") const = 0;
    
    /**
     * @brief 检测文档类型
     * @param filePath 文件路径
     * @return 文档类型
     */
    static DocumentType detectDocumentType(const std::string& filePath);
    
    /**
     * @brief 检测文档类型（从内容）
     * @param content 文件内容
     * @param fileName 文件名（可选）
     * @return 文档类型
     */
    static DocumentType detectDocumentTypeFromContent(
        const std::string& content,
        const std::string& fileName = "");
};

/**
 * @brief 基本元数据提取器
 */
class BasicMetadataExtractor : public MetadataExtractor {
public:
    /**
     * @brief 从文件路径提取元数据
     * @param filePath 文件路径
     * @return 元数据映射表
     */
    std::unordered_map<std::string, std::string> extract(
        const std::string& filePath) const override;
    
    /**
     * @brief 从文件内容提取元数据
     * @param content 文件内容
     * @param fileName 文件名（可选）
     * @return 元数据映射表
     */
    std::unordered_map<std::string, std::string> extractFromContent(
        const std::string& content,
        const std::string& fileName = "") const override;
    
private:
    /**
     * @brief 提取文件系统元数据
     * @param filePath 文件路径
     * @return 元数据映射表
     */
    std::unordered_map<std::string, std::string> extractFileSystemMetadata(
        const std::string& filePath) const;
    
    /**
     * @brief 提取文本内容相关元数据
     * @param content 文件内容
     * @return 元数据映射表
     */
    std::unordered_map<std::string, std::string> extractContentMetadata(
        const std::string& content) const;
};

/**
 * @brief PDF元数据提取器
 */
class PDFMetadataExtractor : public MetadataExtractor {
public:
    /**
     * @brief 从文件路径提取元数据
     * @param filePath 文件路径
     * @return 元数据映射表
     */
    std::unordered_map<std::string, std::string> extract(
        const std::string& filePath) const override;
    
    /**
     * @brief 从文件内容提取元数据
     * @param content 文件内容
     * @param fileName 文件名（可选）
     * @return 元数据映射表
     */
    std::unordered_map<std::string, std::string> extractFromContent(
        const std::string& content,
        const std::string& fileName = "") const override;
};

/**
 * @brief Word文档元数据提取器
 */
class WordMetadataExtractor : public MetadataExtractor {
public:
    /**
     * @brief 从文件路径提取元数据
     * @param filePath 文件路径
     * @return 元数据映射表
     */
    std::unordered_map<std::string, std::string> extract(
        const std::string& filePath) const override;
    
    /**
     * @brief 从文件内容提取元数据
     * @param content 文件内容
     * @param fileName 文件名（可选）
     * @return 元数据映射表
     */
    std::unordered_map<std::string, std::string> extractFromContent(
        const std::string& content,
        const std::string& fileName = "") const override;
};

/**
 * @brief HTML元数据提取器
 */
class HTMLMetadataExtractor : public MetadataExtractor {
public:
    /**
     * @brief 从文件路径提取元数据
     * @param filePath 文件路径
     * @return 元数据映射表
     */
    std::unordered_map<std::string, std::string> extract(
        const std::string& filePath) const override;
    
    /**
     * @brief 从文件内容提取元数据
     * @param content 文件内容
     * @param fileName 文件名（可选）
     * @return 元数据映射表
     */
    std::unordered_map<std::string, std::string> extractFromContent(
        const std::string& content,
        const std::string& fileName = "") const override;
};

/**
 * @brief 元数据提取器工厂
 */
class MetadataExtractorFactory {
public:
    /**
     * @brief 创建元数据提取器
     * @param type 文档类型
     * @return 元数据提取器指针
     */
    static std::unique_ptr<MetadataExtractor> createExtractor(DocumentType type);
    
    /**
     * @brief 创建元数据提取器（基于文件路径）
     * @param filePath 文件路径
     * @return 元数据提取器指针
     */
    static std::unique_ptr<MetadataExtractor> createExtractor(const std::string& filePath);
    
    /**
     * @brief 提取元数据
     * @param filePath 文件路径
     * @return 元数据映射表
     */
    static std::unordered_map<std::string, std::string> extractMetadata(
        const std::string& filePath);
    
    /**
     * @brief 提取元数据（从内容）
     * @param content 文件内容
     * @param type 文档类型
     * @param fileName 文件名（可选）
     * @return 元数据映射表
     */
    static std::unordered_map<std::string, std::string> extractMetadataFromContent(
        const std::string& content,
        DocumentType type,
        const std::string& fileName = "");
};

} // namespace preprocessor
} // namespace kb

#endif // KB_METADATA_EXTRACTOR_H 