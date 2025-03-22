#include "vectorizer/vectorizer.h"
#include "vectorizer/tfidf_vectorizer.h"
#include "common/logging.h"

#include <algorithm>
#include <stdexcept>

namespace kb {
namespace vectorizer {

// 实现fitTransform方法
Eigen::MatrixXf Vectorizer::fitTransform(const std::vector<std::string>& documents) {
    this->fit(documents);
    return this->transform(documents);
}

// 向量化器工厂函数
std::unique_ptr<Vectorizer> createVectorizer(
    const std::string& type, 
    const std::unordered_map<std::string, std::string>& params) {
    
    LOG_INFO("创建向量化器类型: {}", type);
    
    try {
        if (type == "tfidf") {
            // 解析TF-IDF参数
            float minDf = 0.0;
            float maxDf = 1.0;
            size_t maxFeatures = 0;
            std::string normalization = "l2";
            bool useBinary = false;
            float smooth = 1.0;
            
            // 从参数表中获取值
            auto getParam = [&params](const std::string& key, const std::string& defaultValue) -> std::string {
                auto it = params.find(key);
                return (it != params.end()) ? it->second : defaultValue;
            };
            
            // 转换参数类型
            try {
                if (params.find("min_df") != params.end()) {
                    minDf = std::stof(params.at("min_df"));
                }
                if (params.find("max_df") != params.end()) {
                    maxDf = std::stof(params.at("max_df"));
                }
                if (params.find("max_features") != params.end()) {
                    maxFeatures = std::stoull(params.at("max_features"));
                }
                normalization = getParam("normalization", "l2");
                if (params.find("binary") != params.end()) {
                    useBinary = (params.at("binary") == "true" || params.at("binary") == "1");
                }
                if (params.find("smooth") != params.end()) {
                    smooth = std::stof(params.at("smooth"));
                }
            } catch (const std::exception& e) {
                LOG_ERROR("解析参数失败: {}", std::string(e.what()));
                throw std::invalid_argument("参数格式错误");
            }
            
            // 创建TF-IDF向量化器
            return std::make_unique<TfidfVectorizer>(
                minDf, maxDf, maxFeatures, normalization, useBinary, smooth);
        } 
        // 将来可以添加更多向量化器类型
        // else if (type == "bow") { ... }
        // else if (type == "word2vec") { ... }
        // else if (type == "fasttext") { ... }
        else {
            LOG_ERROR("不支持的向量化器类型: {}" , type);
            throw std::invalid_argument("不支持的向量化器类型: " + type);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("创建向量化器失败: {}" , std::string(e.what()));
        throw;
    }
}

} // namespace vectorizer
} // namespace kb 