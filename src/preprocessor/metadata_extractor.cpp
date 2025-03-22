/**
 * @file metadata_extractor.cpp
 * @brief 元数据提取器实现
 */

#include "preprocessor/metadata_extractor.h"
#include "common/logging.h"
#include <filesystem>
#include <fstream>
#include <regex>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace kb {
namespace preprocessor {

namespace fs = std::filesystem;

// ===== MetadataExtractor静态方法实现 =====

DocumentType MetadataExtractor::detectDocumentType(const std::string& filePath) {
    try {
        // 检查文件是否存在
        if (!fs::exists(filePath)) {
            LOG_WARN("文件不存在: {}", filePath);
            return DocumentType::UNKNOWN;
        }
        
        // 根据文件扩展名判断文档类型
        std::string extension = fs::path(filePath).extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
        
        if (extension == ".txt") {
            return DocumentType::TEXT;
        } else if (extension == ".pdf") {
            return DocumentType::PDF;
        } else if (extension == ".doc" || extension == ".docx") {
            return DocumentType::WORD;
        } else if (extension == ".html" || extension == ".htm") {
            return DocumentType::HTML;
        } else if (extension == ".md" || extension == ".markdown") {
            return DocumentType::MARKDOWN;
        } else if (extension == ".json") {
            return DocumentType::JSON;
        } else if (extension == ".xml") {
            return DocumentType::XML;
        } else if (extension == ".csv") {
            return DocumentType::CSV;
        } else {
            // 如果无法通过扩展名判断，尝试通过文件内容判断
            std::ifstream file(filePath, std::ios::binary);
            if (!file) {
                LOG_WARN("无法打开文件: {}", filePath);
                return DocumentType::UNKNOWN;
            }
            
            // 读取文件前1024字节
            char buffer[1024];
            file.read(buffer, sizeof(buffer));
            std::streamsize bytesRead = file.gcount();
            std::string content(buffer, bytesRead);
            
            return detectDocumentTypeFromContent(content, fs::path(filePath).filename().string());
        }
    } catch (const std::exception& e) {
        LOG_ERROR("检测文档类型异常: {}", e.what());
        return DocumentType::UNKNOWN;
    }
}

DocumentType MetadataExtractor::detectDocumentTypeFromContent(
    const std::string& content,
    const std::string& fileName) {
    
    // 如果提供了文件名，先尝试通过文件扩展名判断
    if (!fileName.empty()) {
        std::string extension = fs::path(fileName).extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
        
        if (extension == ".txt") {
            return DocumentType::TEXT;
        } else if (extension == ".pdf") {
            return DocumentType::PDF;
        } else if (extension == ".doc" || extension == ".docx") {
            return DocumentType::WORD;
        } else if (extension == ".html" || extension == ".htm") {
            return DocumentType::HTML;
        } else if (extension == ".md" || extension == ".markdown") {
            return DocumentType::MARKDOWN;
        } else if (extension == ".json") {
            return DocumentType::JSON;
        } else if (extension == ".xml") {
            return DocumentType::XML;
        } else if (extension == ".csv") {
            return DocumentType::CSV;
        }
    }
    
    // 通过内容特征判断文档类型
    if (content.empty()) {
        return DocumentType::UNKNOWN;
    }
    
    // 检查是否是PDF
    if (content.substr(0, 4) == "%PDF") {
        return DocumentType::PDF;
    }
    
    // 检查是否是HTML
    if (content.find("<!DOCTYPE html>") != std::string::npos ||
        content.find("<html") != std::string::npos) {
        return DocumentType::HTML;
    }
    
    // 检查是否是XML
    if (content.find("<?xml") != std::string::npos) {
        return DocumentType::XML;
    }
    
    // 检查是否是JSON
    if ((content.find("{") != std::string::npos && content.find("}") != std::string::npos) ||
        (content.find("[") != std::string::npos && content.find("]") != std::string::npos)) {
        // 简单的启发式检查，可能会有误判
        std::regex jsonRegex(R"(^\s*[\{\[].*[\}\]]\s*$)");
        if (std::regex_search(content, jsonRegex)) {
            return DocumentType::JSON;
        }
    }
    
    // 检查是否是CSV
    if (std::count(content.begin(), content.end(), ',') > 5) {
        // 简单的启发式检查，可能会有误判
        std::regex csvRegex(R"(^[^,\r\n]*(?:,[^,\r\n]*)+(?:\r?\n[^,\r\n]*(?:,[^,\r\n]*)+)*$)");
        if (std::regex_search(content, csvRegex)) {
            return DocumentType::CSV;
        }
    }
    
    // 检查是否是Markdown
    if (content.find("#") != std::string::npos || content.find("```") != std::string::npos) {
        std::regex mdRegex(R"(^# |\n# |\n## |\n### |\n\* |\n- |\n```|\n> )");
        if (std::regex_search(content, mdRegex)) {
            return DocumentType::MARKDOWN;
        }
    }
    
    // 默认为纯文本
    return DocumentType::TEXT;
}

// ===== BasicMetadataExtractor实现 =====

std::unordered_map<std::string, std::string> BasicMetadataExtractor::extract(
    const std::string& filePath) const {
    
    std::unordered_map<std::string, std::string> metadata;
    
    try {
        // 检查文件是否存在
        if (!fs::exists(filePath)) {
            LOG_WARN("文件不存在: {}", filePath);
            return metadata;
        }
        
        // 提取文件系统元数据
        auto fsMetadata = extractFileSystemMetadata(filePath);
        metadata.insert(fsMetadata.begin(), fsMetadata.end());
        
        // 读取文件内容
        std::ifstream file(filePath);
        if (!file) {
            LOG_WARN("无法打开文件: {}", filePath);
            return metadata;
        }
        
        std::string content((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
        
        // 提取内容相关元数据
        auto contentMetadata = extractContentMetadata(content);
        metadata.insert(contentMetadata.begin(), contentMetadata.end());
        
    } catch (const std::exception& e) {
        LOG_ERROR("提取元数据异常: {}", e.what());
    }
    
    return metadata;
}

std::unordered_map<std::string, std::string> BasicMetadataExtractor::extractFromContent(
    const std::string& content,
    const std::string& fileName) const {
    
    std::unordered_map<std::string, std::string> metadata;
    
    try {
        // 添加基本元数据
        if (!fileName.empty()) {
            metadata["file_name"] = fileName;
            
            std::string extension = fs::path(fileName).extension().string();
            if (!extension.empty()) {
                metadata["file_extension"] = extension;
            }
        }
        
        // 检测文档类型
        DocumentType type = MetadataExtractor::detectDocumentTypeFromContent(content, fileName);
        metadata["document_type"] = std::to_string(static_cast<int>(type));
        
        switch (type) {
            case DocumentType::TEXT:
                metadata["document_type_name"] = "TEXT";
                break;
            case DocumentType::PDF:
                metadata["document_type_name"] = "PDF";
                break;
            case DocumentType::WORD:
                metadata["document_type_name"] = "WORD";
                break;
            case DocumentType::HTML:
                metadata["document_type_name"] = "HTML";
                break;
            case DocumentType::MARKDOWN:
                metadata["document_type_name"] = "MARKDOWN";
                break;
            case DocumentType::JSON:
                metadata["document_type_name"] = "JSON";
                break;
            case DocumentType::XML:
                metadata["document_type_name"] = "XML";
                break;
            case DocumentType::CSV:
                metadata["document_type_name"] = "CSV";
                break;
            default:
                metadata["document_type_name"] = "UNKNOWN";
        }
        
        // 添加内容大小
        metadata["content_size"] = std::to_string(content.size());
        
        // 提取内容相关元数据
        auto contentMetadata = extractContentMetadata(content);
        metadata.insert(contentMetadata.begin(), contentMetadata.end());
        
    } catch (const std::exception& e) {
        LOG_ERROR("从内容提取元数据异常: {}", e.what());
    }
    
    return metadata;
}

std::unordered_map<std::string, std::string> BasicMetadataExtractor::extractFileSystemMetadata(
    const std::string& filePath) const {
    
    std::unordered_map<std::string, std::string> metadata;
    
    try {
        fs::path path(filePath);
        
        // 文件名
        metadata["file_name"] = path.filename().string();
        
        // 文件扩展名
        std::string extension = path.extension().string();
        if (!extension.empty()) {
            metadata["file_extension"] = extension;
        }
        
        // 文件大小
        metadata["file_size"] = std::to_string(fs::file_size(path));
        
        // 最后修改时间
        auto lastWriteTime = fs::last_write_time(path);
        auto lastWriteTimeT = std::chrono::system_clock::to_time_t(
            std::chrono::file_clock::to_sys(lastWriteTime));
        std::stringstream ss;
        ss << std::put_time(std::localtime(&lastWriteTimeT), "%Y-%m-%d %H:%M:%S");
        metadata["last_modified"] = ss.str();
        
        // 文件路径
        metadata["file_path"] = fs::absolute(path).string();
        
        // 父目录
        metadata["parent_directory"] = path.parent_path().string();
        
        // 文档类型
        DocumentType type = detectDocumentType(filePath);
        metadata["document_type"] = std::to_string(static_cast<int>(type));
        
        switch (type) {
            case DocumentType::TEXT:
                metadata["document_type_name"] = "TEXT";
                break;
            case DocumentType::PDF:
                metadata["document_type_name"] = "PDF";
                break;
            case DocumentType::WORD:
                metadata["document_type_name"] = "WORD";
                break;
            case DocumentType::HTML:
                metadata["document_type_name"] = "HTML";
                break;
            case DocumentType::MARKDOWN:
                metadata["document_type_name"] = "MARKDOWN";
                break;
            case DocumentType::JSON:
                metadata["document_type_name"] = "JSON";
                break;
            case DocumentType::XML:
                metadata["document_type_name"] = "XML";
                break;
            case DocumentType::CSV:
                metadata["document_type_name"] = "CSV";
                break;
            default:
                metadata["document_type_name"] = "UNKNOWN";
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("提取文件系统元数据异常: {}", e.what());
    }
    
    return metadata;
}

std::unordered_map<std::string, std::string> BasicMetadataExtractor::extractContentMetadata(
    const std::string& content) const {
    
    std::unordered_map<std::string, std::string> metadata;
    
    try {
        if (content.empty()) {
            return metadata;
        }
        
        // 计算字符数
        metadata["character_count"] = std::to_string(content.size());
        
        // 计算行数
        size_t lineCount = std::count(content.begin(), content.end(), '\n') + 1;
        metadata["line_count"] = std::to_string(lineCount);
        
        // 计算单词数（简单估算）
        std::regex wordRegex(R"(\b\w+\b)");
        auto words_begin = std::sregex_iterator(content.begin(), content.end(), wordRegex);
        auto words_end = std::sregex_iterator();
        metadata["word_count"] = std::to_string(std::distance(words_begin, words_end));
        
        // 提取标题（启发式，仅适用于一些常见格式）
        std::regex titleRegex(R"((?:^|\n)#+\s+(.+)(?:\n|$)|<title>([^<]+)</title>|<h1[^>]*>([^<]+)</h1>)");
        std::smatch titleMatch;
        if (std::regex_search(content, titleMatch, titleRegex)) {
            for (size_t i = 1; i < titleMatch.size(); ++i) {
                if (titleMatch[i].matched && !titleMatch[i].str().empty()) {
                    metadata["title"] = titleMatch[i].str();
                    break;
                }
            }
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("提取内容元数据异常: {}", e.what());
    }
    
    return metadata;
}

// ===== PDFMetadataExtractor实现 =====

std::unordered_map<std::string, std::string> PDFMetadataExtractor::extract(
    const std::string& filePath) const {
    
    std::unordered_map<std::string, std::string> metadata;
    
    try {
        // 先获取基本元数据
        BasicMetadataExtractor basicExtractor;
        metadata = basicExtractor.extract(filePath);
        
        // 确认文件是PDF
        if (metadata["document_type_name"] != "PDF") {
            LOG_WARN("文件不是PDF格式: {}", filePath);
            return metadata;
        }
        
        // 实际的PDF元数据提取需要PDF解析库支持
        // 这里是一个简化的实现
        std::ifstream file(filePath, std::ios::binary);
        if (!file) {
            LOG_WARN("无法打开PDF文件: {}", filePath);
            return metadata;
        }
        
        // 读取文件前4KB
        char buffer[4096];
        file.read(buffer, sizeof(buffer));
        std::string content(buffer, file.gcount());
        
        // 查找PDF信息字典
        std::regex infoRegex(R"(/Info\s+(\d+)\s+\d+\s+R)");
        std::smatch infoMatch;
        if (std::regex_search(content, infoMatch, infoRegex)) {
            metadata["pdf_info_ref"] = infoMatch[1].str();
        }
        
        // 提取PDF版本
        std::regex versionRegex(R"(%PDF-(\d+\.\d+))");
        std::smatch versionMatch;
        if (std::regex_search(content, versionMatch, versionRegex)) {
            metadata["pdf_version"] = versionMatch[1].str();
        }
        
        // 提取标题
        std::regex titleRegex(R"(/Title\s*\(([^)]+)\))");
        std::smatch titleMatch;
        if (std::regex_search(content, titleMatch, titleRegex)) {
            metadata["title"] = titleMatch[1].str();
        }
        
        // 提取作者
        std::regex authorRegex(R"(/Author\s*\(([^)]+)\))");
        std::smatch authorMatch;
        if (std::regex_search(content, authorMatch, authorRegex)) {
            metadata["author"] = authorMatch[1].str();
        }
        
        // 提取创建日期
        std::regex creationDateRegex(R"(/CreationDate\s*\(([^)]+)\))");
        std::smatch creationDateMatch;
        if (std::regex_search(content, creationDateMatch, creationDateRegex)) {
            metadata["creation_date"] = creationDateMatch[1].str();
        }
        
        // 提取修改日期
        std::regex modDateRegex(R"(/ModDate\s*\(([^)]+)\))");
        std::smatch modDateMatch;
        if (std::regex_search(content, modDateMatch, modDateRegex)) {
            metadata["modification_date"] = modDateMatch[1].str();
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("提取PDF元数据异常: {}", e.what());
    }
    
    return metadata;
}

std::unordered_map<std::string, std::string> PDFMetadataExtractor::extractFromContent(
    const std::string& content,
    const std::string& fileName) const {
    
    std::unordered_map<std::string, std::string> metadata;
    
    try {
        // 先获取基本元数据
        BasicMetadataExtractor basicExtractor;
        metadata = basicExtractor.extractFromContent(content, fileName);
        
        // 确认内容是PDF
        if (content.substr(0, 4) != "%PDF") {
            LOG_WARN("内容不是PDF格式");
            return metadata;
        }
        
        // 提取PDF版本
        std::regex versionRegex(R"(%PDF-(\d+\.\d+))");
        std::smatch versionMatch;
        if (std::regex_search(content, versionMatch, versionRegex)) {
            metadata["pdf_version"] = versionMatch[1].str();
        }
        
        // 提取标题
        std::regex titleRegex(R"(/Title\s*\(([^)]+)\))");
        std::smatch titleMatch;
        if (std::regex_search(content, titleMatch, titleRegex)) {
            metadata["title"] = titleMatch[1].str();
        }
        
        // 提取作者
        std::regex authorRegex(R"(/Author\s*\(([^)]+)\))");
        std::smatch authorMatch;
        if (std::regex_search(content, authorMatch, authorRegex)) {
            metadata["author"] = authorMatch[1].str();
        }
        
        // 提取创建日期
        std::regex creationDateRegex(R"(/CreationDate\s*\(([^)]+)\))");
        std::smatch creationDateMatch;
        if (std::regex_search(content, creationDateMatch, creationDateRegex)) {
            metadata["creation_date"] = creationDateMatch[1].str();
        }
        
        // 提取修改日期
        std::regex modDateRegex(R"(/ModDate\s*\(([^)]+)\))");
        std::smatch modDateMatch;
        if (std::regex_search(content, modDateMatch, modDateRegex)) {
            metadata["modification_date"] = modDateMatch[1].str();
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("从内容提取PDF元数据异常: {}", e.what());
    }
    
    return metadata;
}

// ===== WordMetadataExtractor实现 =====

std::unordered_map<std::string, std::string> WordMetadataExtractor::extract(
    const std::string& filePath) const {
    // 简化实现，实际应使用专门的库
    BasicMetadataExtractor basicExtractor;
    auto metadata = basicExtractor.extract(filePath);
    metadata["document_type_name"] = "WORD";
    
    LOG_WARN("Word元数据提取需要专门的库支持，返回基本元数据");
    return metadata;
}

std::unordered_map<std::string, std::string> WordMetadataExtractor::extractFromContent(
    const std::string& content,
    const std::string& fileName) const {
    // 简化实现，实际应使用专门的库
    BasicMetadataExtractor basicExtractor;
    auto metadata = basicExtractor.extractFromContent(content, fileName);
    metadata["document_type_name"] = "WORD";
    
    LOG_WARN("Word元数据提取需要专门的库支持，返回基本元数据");
    return metadata;
}

// ===== HTMLMetadataExtractor实现 =====

std::unordered_map<std::string, std::string> HTMLMetadataExtractor::extract(
    const std::string& filePath) const {
    
    std::unordered_map<std::string, std::string> metadata;
    
    try {
        // 先获取基本元数据
        BasicMetadataExtractor basicExtractor;
        metadata = basicExtractor.extract(filePath);
        
        // 确认文件是HTML
        if (metadata["document_type_name"] != "HTML") {
            LOG_WARN("文件不是HTML格式: {}", filePath);
            return metadata;
        }
        
        // 读取文件内容
        std::ifstream file(filePath);
        if (!file) {
            LOG_WARN("无法打开HTML文件: {}", filePath);
            return metadata;
        }
        
        std::string content((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
        
        // 传递文件名而不是完整路径
        std::string fileName = std::filesystem::path(filePath).filename().string();
        auto htmlMetadata = extractFromContent(content, fileName);
        
        // 合并HTML特定的元数据
        metadata.insert(htmlMetadata.begin(), htmlMetadata.end());
        
    } catch (const std::exception& e) {
        LOG_ERROR("提取HTML元数据异常: {}", e.what());
    }
    
    return metadata;
}

std::unordered_map<std::string, std::string> HTMLMetadataExtractor::extractFromContent(
    const std::string& content,
    const std::string& fileName) const {
    
    std::unordered_map<std::string, std::string> metadata;
    
    try {
        // 先获取基本元数据
        BasicMetadataExtractor basicExtractor;
        metadata = basicExtractor.extractFromContent(content, fileName);
        
        // 确认内容是HTML
        if (content.find("<!DOCTYPE html>") == std::string::npos &&
            content.find("<html") == std::string::npos) {
            LOG_WARN("内容不是HTML格式");
            return metadata;
        }
        
        // 提取标题
        std::regex titleRegex(R"(<title>([^<]+)</title>)");
        std::smatch titleMatch;
        if (std::regex_search(content, titleMatch, titleRegex)) {
            metadata["title"] = titleMatch[1].str();
        }
        
        // 提取meta标签信息
        std::regex metaRegex(R"delimiter(<meta\s+(?:name|property)="([^"]+)"\s+content="([^"]+)")delimiter");
        std::sregex_iterator metaIt(content.begin(), content.end(), metaRegex);
        std::sregex_iterator end;
        
        while (metaIt != end) {
            std::smatch match = *metaIt;
            std::string name = match[1].str();
            std::string value = match[2].str();
            
            metadata["meta_" + name] = value;
            
            ++metaIt;
        }
        
        // 提取h1标签内容
        std::regex h1Regex(R"(<h1[^>]*>([^<]+)</h1>)");
        std::smatch h1Match;
        if (std::regex_search(content, h1Match, h1Regex)) {
            metadata["h1"] = h1Match[1].str();
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("从内容提取HTML元数据异常: {}", e.what());
    }
    
    return metadata;
}

// ===== MetadataExtractorFactory实现 =====

std::unique_ptr<MetadataExtractor> MetadataExtractorFactory::createExtractor(DocumentType type) {
    switch (type) {
        case DocumentType::PDF:
            return std::make_unique<PDFMetadataExtractor>();
        case DocumentType::WORD:
            return std::make_unique<WordMetadataExtractor>();
        case DocumentType::HTML:
            return std::make_unique<HTMLMetadataExtractor>();
        case DocumentType::TEXT:
        case DocumentType::MARKDOWN:
        case DocumentType::JSON:
        case DocumentType::XML:
        case DocumentType::CSV:
        case DocumentType::UNKNOWN:
        default:
            return std::make_unique<BasicMetadataExtractor>();
    }
}

std::unique_ptr<MetadataExtractor> MetadataExtractorFactory::createExtractor(
    const std::string& filePath) {
    
    DocumentType type = MetadataExtractor::detectDocumentType(filePath);
    return createExtractor(type);
}

std::unordered_map<std::string, std::string> MetadataExtractorFactory::extractMetadata(
    const std::string& filePath) {
    
    auto extractor = createExtractor(filePath);
    return extractor->extract(filePath);
}

std::unordered_map<std::string, std::string> MetadataExtractorFactory::extractMetadataFromContent(
    const std::string& content,
    DocumentType type,
    const std::string& fileName) {
    
    auto extractor = createExtractor(type);
    return extractor->extractFromContent(content, fileName);
}

} // namespace preprocessor
} // namespace kb 