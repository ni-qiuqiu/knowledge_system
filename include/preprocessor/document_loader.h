/**
 * @file document_loader.h
 * @brief 文档加载器，定义文档加载功能
 */

#ifndef KB_DOCUMENT_LOADER_H
#define KB_DOCUMENT_LOADER_H

#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace kb {
namespace preprocessor {

/**
 * @brief 文档加载器接口
 */
class DocumentLoader {
public:
    /**
     * @brief 析构函数
     */
    virtual ~DocumentLoader() = default;
    
    /**
     * @brief 加载文档内容
     * @param filePath 文件路径
     * @return 文档内容字符串
     */
    virtual std::string loadDocument(const std::string& filePath) const = 0;
};

/**
 * @brief 文本文件加载器
 */
class TextFileLoader : public DocumentLoader {
public:
    /**
     * @brief 加载文本文件内容
     * @param filePath 文件路径
     * @return 文本内容字符串
     */
    std::string loadDocument(const std::string& filePath) const override;
};

/**
 * @brief PDF文件加载器
 */
class PDFLoader : public DocumentLoader {
public:
    /**
     * @brief 加载PDF文件内容
     * @param filePath 文件路径
     * @return 提取的文本内容字符串
     */
    std::string loadDocument(const std::string& filePath) const override;
};

/**
 * @brief Word文件加载器
 */
class WordLoader : public DocumentLoader {
public:
    /**
     * @brief 加载Word文件内容
     * @param filePath 文件路径
     * @return 提取的文本内容字符串
     */
    std::string loadDocument(const std::string& filePath) const override;
};

/**
 * @brief HTML文件加载器
 */
class HTMLLoader : public DocumentLoader {
public:
    /**
     * @brief 加载HTML文件内容
     * @param filePath 文件路径
     * @return 提取的文本内容字符串
     */
    std::string loadDocument(const std::string& filePath) const override;
};

/**
 * @brief 文档加载器工厂
 */
class DocumentLoaderFactory {
public:
    /**
     * @brief 根据文件路径创建合适的文档加载器
     * @param filePath 文件路径
     * @return 文档加载器指针
     */
    static std::unique_ptr<DocumentLoader> createLoader(const std::string& filePath);
    
    /**
     * @brief 加载文档内容
     * @param filePath 文件路径
     * @return 文档内容字符串
     */
    static std::string loadDocument(const std::string& filePath);
    
    /**
     * @brief 批量加载文档
     * @param filePaths 文件路径列表
     * @param progressCallback 进度回调函数，参数为当前处理的索引和总数
     * @return 文档内容列表
     */
    static std::vector<std::string> loadDocuments(
        const std::vector<std::string>& filePaths,
        std::function<void(size_t, size_t)> progressCallback = nullptr);
};

} // namespace preprocessor
} // namespace kb

#endif // KB_DOCUMENT_LOADER_H 