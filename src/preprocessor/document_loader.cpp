#include "preprocessor/document_loader.h"
#include "common/logging.h"
#include "common/utils.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <vector>

namespace kb {
namespace preprocessor {

namespace fs = boost::filesystem;

std::string TextFileLoader::loadDocument(const std::string& filePath) const {
    try {
        LOG_DEBUG("加载文本文件: {}", filePath);
        return kb::utils::readFile(filePath);
    } catch (const std::exception& e) {
        LOG_ERROR("加载文本文件失败: {}", std::string(e.what()));
        throw;
    }
}

std::string PDFLoader::loadDocument(const std::string& filePath) const {
    LOG_DEBUG("加载PDF文件: {}", filePath);
    
    // 注意：这里需要集成Poppler库进行PDF解析
    // 当前实现是一个简单的调用外部工具的示例
    
    // 方法1: 调用外部工具 pdftotext (需要安装)
    std::string tempOutputFile = fs::temp_directory_path().string() + "/" + 
        kb::utils::sha256(filePath).substr(0, 16) + ".txt";
    
    std::string command = "pdftotext -enc UTF-8 \"" + filePath + "\" \"" + tempOutputFile + "\"";
    
    int result = std::system(command.c_str());
    if (result != 0) {
        LOG_ERROR("执行pdftotext命令失败，尝试其他方法");
        // 如果外部工具失败，可以在这里添加备选方法
    } else {
        if (fs::exists(tempOutputFile)) {
            std::string content = kb::utils::readFile(tempOutputFile);
            fs::remove(tempOutputFile);
            return content;
        }
    }
    
    // 如果所有方法都失败，抛出异常
    throw std::runtime_error("无法解析PDF文件: " + filePath);
}

std::string WordLoader::loadDocument(const std::string& filePath) const {
    LOG_DEBUG("加载Word文件: {}", filePath);
    
    // 注意：这里需要集成libdocx库或类似库进行Word文档解析
    // 当前实现是一个简单的调用外部工具的示例
    
    // 方法1: 调用docx2txt工具 (需要安装)
    std::string tempOutputFile = fs::temp_directory_path().string() + "/" + 
        kb::utils::sha256(filePath).substr(0, 16) + ".txt";
    
    std::string ext = fs::path(filePath).extension().string();
    boost::algorithm::to_lower(ext);
    
    std::string command;
    if (ext == ".docx") {
        command = "docx2txt \"" + filePath + "\" \"" + tempOutputFile + "\"";
    } else {
        command = "catdoc \"" + filePath + "\" > \"" + tempOutputFile + "\"";
    }
    
    int result = std::system(command.c_str());
    if (result != 0) {
        LOG_ERROR("执行Word转换命令失败，尝试其他方法");
        // 如果外部工具失败，可以在这里添加备选方法
    } else {
        if (fs::exists(tempOutputFile)) {
            std::string content = kb::utils::readFile(tempOutputFile);
            fs::remove(tempOutputFile);
            return content;
        }
    }
    
    // 如果所有方法都失败，抛出异常
    throw std::runtime_error("无法解析Word文件: " + filePath);
}

std::string HTMLLoader::loadDocument(const std::string& filePath) const {
    LOG_DEBUG("加载HTML文件: {}", filePath);
    
    // 读取HTML文件内容
    std::string html = kb::utils::readFile(filePath);
    
    // 使用正则表达式移除HTML标签
    std::string content = kb::utils::regexReplace(html, "<[^>]*>", " ");
    
    // 移除多余空白字符
    content = kb::utils::regexReplace(content, "\\s+", " ");
    
    // 处理HTML实体
    std::vector<std::pair<std::string, std::string>> entities = {
        {"&nbsp;", " "}, {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"},
        {"&quot;", "\""}, {"&apos;", "'"}, {"&#39;", "'"}
    };
    
    for (const auto& entity : entities) {
        boost::replace_all(content, entity.first, entity.second);
    }
    
    return kb::utils::trim(content);
}

std::unique_ptr<DocumentLoader> DocumentLoaderFactory::createLoader(const std::string& filePath) {
    std::string ext = fs::path(filePath).extension().string();
    boost::algorithm::to_lower(ext);
    
    if (ext == ".txt" || ext == ".md" || ext == ".csv") {
        return std::make_unique<TextFileLoader>();
    } else if (ext == ".pdf") {
        return std::make_unique<PDFLoader>();
    } else if (ext == ".doc" || ext == ".docx") {
        return std::make_unique<WordLoader>();
    } else if (ext == ".html" || ext == ".htm") {
        return std::make_unique<HTMLLoader>();
    } else {
        // 对于未知格式，默认使用文本加载器
        LOG_WARN("未知文件格式: {}，尝试以文本方式加载", ext);
        return std::make_unique<TextFileLoader>();
    }
}

std::string DocumentLoaderFactory::loadDocument(const std::string& filePath) {
    auto loader = createLoader(filePath);
    return loader->loadDocument(filePath);
}

std::vector<std::string> DocumentLoaderFactory::loadDocuments(
    const std::vector<std::string>& filePaths,
    std::function<void(size_t, size_t)> progressCallback) {
    
    std::vector<std::string> contents;
    contents.reserve(filePaths.size());
    
    for (size_t i = 0; i < filePaths.size(); ++i) {
        try {
            std::string content = loadDocument(filePaths[i]);
            contents.push_back(content);
        } catch (const std::exception& e) {
            LOG_ERROR("加载文档失败: {} - {}", filePaths[i], e.what());
            // 添加一个空字符串作为占位符
            contents.push_back("");
        }
        
        if (progressCallback) {
            progressCallback(i + 1, filePaths.size());
        }
    }
    
    return contents;
}

} // namespace preprocessor
} // namespace kb 