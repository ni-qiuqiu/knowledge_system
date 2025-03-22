#include "vectorizer/tfidf_vectorizer.h"
#include "common/logging.h"
#include "common/utils.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <numeric>
#include <unordered_set>
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>


namespace kb {
namespace vectorizer {

TfidfVectorizer::TfidfVectorizer(float minDf, 
                                float maxDf, 
                                size_t maxFeatures,
                                const std::string& normalization,
                                bool useBinary,
                                float smooth)
    : minDf_(minDf), 
      maxDf_(maxDf), 
      maxFeatures_(maxFeatures),
      normalization_(normalization),
      useBinary_(useBinary),
      smooth_(smooth),
      numDocuments_(0),
      tokenizer_(&TfidfVectorizer::defaultTokenizer) {

        std::string log_info = " min_df=" + std::to_string(minDf_) + 
              ", max_df=" + std::to_string(maxDf_) +
              ", max_features=" + std::to_string(maxFeatures_) +
              ", normalization=" + normalization_ +
              ", binary=" + (useBinary_ ? "true" : "false") +
              ", smooth=" + std::to_string(smooth_);
    
    LOG_INFO("创建TF-IDF向量化器: {}",log_info);
}

void TfidfVectorizer::fit(const std::vector<std::string>& documents) {
    LOG_INFO("拟合TF-IDF向量化器，文档数: {}" , std::to_string(documents.size()));
    
    if (documents.empty()) {
        LOG_WARN("文档集合为空，无法拟合");
        return;
    }
    
    numDocuments_ = documents.size();
    
    // 对文档进行分词
    auto tokenizedDocs = tokenizeDocuments(documents);
    
    // 构建词汇表
    buildVocabulary(tokenizedDocs);
    
    if (vocabulary_.empty()) {
        LOG_WARN("构建词汇表失败，词汇为空");
        return;
    }
    
    LOG_INFO("词汇表大小: {}" , std::to_string(vocabulary_.size()));
    
    // 计算词频矩阵
    Eigen::SparseMatrix<float> tfMatrix = calculateTf(tokenizedDocs);
    
    // 计算IDF
    idf_ = calculateIdf(tfMatrix);
    
    LOG_INFO("TF-IDF向量化器拟合完成");
}

Eigen::VectorXf TfidfVectorizer::transform(const std::string& document) const {
    std::vector<std::string> tokens = tokenizer_(document);
    
    // 移除停用词
    if (!stopWords_.empty()) {
        tokens.erase(
            std::remove_if(tokens.begin(), tokens.end(), 
                          [this](const std::string& token) {
                              return this->stopWords_.find(token) != this->stopWords_.end();
                          }),
            tokens.end()
        );
    }
    
    // 计算词频
    std::unordered_map<std::string, float> termFreq;
    for (const auto& token : tokens) {
        if (vocabulary_.find(token) != vocabulary_.end()) {
            termFreq[token] += 1.0;
        }
    }
    
    // 创建TF向量
    Eigen::VectorXf tfVector = Eigen::VectorXf::Zero(vocabulary_.size());
    
    for (const auto& pair : termFreq) {
        auto it = vocabulary_.find(pair.first);
        if (it != vocabulary_.end()) {
            float value = useBinary_ ? 1.0f : pair.second;
            tfVector(it->second) = value;
        }
    }
    
    // 应用IDF权重
    Eigen::VectorXf tfidfVector = tfVector.cwiseProduct(idf_);
    
    // 归一化
    if (normalization_ == "l1") {
        float norm = tfidfVector.template lpNorm<1>();
        if (norm > 0) {
            tfidfVector /= norm;
        }
    } else if (normalization_ == "l2") {
        float norm = tfidfVector.norm();
        if (norm > 0) {
            tfidfVector /= norm;
        }
    }
    
    return tfidfVector;
}

Eigen::MatrixXf TfidfVectorizer::transform(const std::vector<std::string>& documents) const {
    if (documents.empty()) {
        return Eigen::MatrixXf();
    }
    
    LOG_INFO("转换文档为TF-IDF矩阵，文档数: {}" , std::to_string(documents.size()));
    
    // 对文档进行分词
    auto tokenizedDocs = tokenizeDocuments(documents);
    
    // 计算词频矩阵
    Eigen::SparseMatrix<float> tfMatrix = calculateTf(tokenizedDocs);
    
    // 转换为密集矩阵
    Eigen::MatrixXf denseTfMatrix = Eigen::MatrixXf(tfMatrix);
    
    // 应用IDF权重
    Eigen::MatrixXf tfidfMatrix(denseTfMatrix.rows(), denseTfMatrix.cols());
    for (int i = 0; i < denseTfMatrix.rows(); ++i) {
        tfidfMatrix.row(i) = denseTfMatrix.row(i).cwiseProduct(idf_.transpose());
    }
    
    // 应用归一化
    if (normalization_ != "none") {
        tfidfMatrix = applyNormalization(tfidfMatrix);
    }
    
    LOG_INFO("TF-IDF转换完成，矩阵大小: {}" , std::to_string(tfidfMatrix.rows()) + 
              "x" + std::to_string(tfidfMatrix.cols()));
    
    return tfidfMatrix;
}

size_t TfidfVectorizer::getVocabularySize() const {
    return vocabulary_.size();
}

std::vector<std::string> TfidfVectorizer::getFeatureNames() const {
    return featureNames_;
}

bool TfidfVectorizer::save(const std::string& filepath) const {
    try {
        std::ofstream ofs(filepath, std::ios::binary);
        if (!ofs) {
            LOG_ERROR("无法打开文件保存模型: {}" , filepath);
            return false;
        }
        
        // 写入参数
        ofs.write(reinterpret_cast<const char*>(&minDf_), sizeof(minDf_));
        ofs.write(reinterpret_cast<const char*>(&maxDf_), sizeof(maxDf_));
        ofs.write(reinterpret_cast<const char*>(&maxFeatures_), sizeof(maxFeatures_));
        
        size_t normLength = normalization_.size();
        ofs.write(reinterpret_cast<const char*>(&normLength), sizeof(normLength));
        ofs.write(normalization_.c_str(), normLength);
        
        ofs.write(reinterpret_cast<const char*>(&useBinary_), sizeof(useBinary_));
        ofs.write(reinterpret_cast<const char*>(&smooth_), sizeof(smooth_));
        ofs.write(reinterpret_cast<const char*>(&numDocuments_), sizeof(numDocuments_));
        
        // 写入词汇表
        size_t vocabSize = vocabulary_.size();
        ofs.write(reinterpret_cast<const char*>(&vocabSize), sizeof(vocabSize));
        
        for (const auto& pair : vocabulary_) {
            size_t wordLength = pair.first.size();
            ofs.write(reinterpret_cast<const char*>(&wordLength), sizeof(wordLength));
            ofs.write(pair.first.c_str(), wordLength);
            ofs.write(reinterpret_cast<const char*>(&pair.second), sizeof(pair.second));
        }
        
        // 写入IDF向量
        size_t idfSize = idf_.size();
        ofs.write(reinterpret_cast<const char*>(&idfSize), sizeof(idfSize));
        ofs.write(reinterpret_cast<const char*>(idf_.data()), idfSize * sizeof(float));
        
        // 写入停用词
        size_t stopWordsSize = stopWords_.size();
        ofs.write(reinterpret_cast<const char*>(&stopWordsSize), sizeof(stopWordsSize));
        
        for (const auto& word : stopWords_) {
            size_t wordLength = word.size();
            ofs.write(reinterpret_cast<const char*>(&wordLength), sizeof(wordLength));
            ofs.write(word.c_str(), wordLength);
        }
        
        LOG_INFO("保存TF-IDF模型到文件: {}" , filepath);
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("保存模型失败: {}" , std::string(e.what()));
        return false;
    }
}

bool TfidfVectorizer::load(const std::string& filepath) {
    try {
        std::ifstream ifs(filepath, std::ios::binary);
        if (!ifs) {
            LOG_ERROR("无法打开文件加载模型: {}" , filepath);
            return false;
        }
        
        // 读取参数
        ifs.read(reinterpret_cast<char*>(&minDf_), sizeof(minDf_));
        ifs.read(reinterpret_cast<char*>(&maxDf_), sizeof(maxDf_));
        ifs.read(reinterpret_cast<char*>(&maxFeatures_), sizeof(maxFeatures_));
        
        size_t normLength;
        ifs.read(reinterpret_cast<char*>(&normLength), sizeof(normLength));
        normalization_.resize(normLength);
        ifs.read(&normalization_[0], normLength);
        
        ifs.read(reinterpret_cast<char*>(&useBinary_), sizeof(useBinary_));
        ifs.read(reinterpret_cast<char*>(&smooth_), sizeof(smooth_));
        ifs.read(reinterpret_cast<char*>(&numDocuments_), sizeof(numDocuments_));
        
        // 读取词汇表
        size_t vocabSize;
        ifs.read(reinterpret_cast<char*>(&vocabSize), sizeof(vocabSize));
        
        vocabulary_.clear();
        featureNames_.resize(vocabSize);
        
        for (size_t i = 0; i < vocabSize; ++i) {
            size_t wordLength;
            ifs.read(reinterpret_cast<char*>(&wordLength), sizeof(wordLength));
            
            std::string word(wordLength, '\0');
            ifs.read(&word[0], wordLength);
            
            size_t index;
            ifs.read(reinterpret_cast<char*>(&index), sizeof(index));
            
            vocabulary_[word] = index;
            featureNames_[index] = word;
        }
        
        // 读取IDF向量
        size_t idfSize;
        ifs.read(reinterpret_cast<char*>(&idfSize), sizeof(idfSize));
        
        idf_.resize(idfSize);
        ifs.read(reinterpret_cast<char*>(idf_.data()), idfSize * sizeof(float));
        
        // 读取停用词
        size_t stopWordsSize;
        ifs.read(reinterpret_cast<char*>(&stopWordsSize), sizeof(stopWordsSize));
        
        stopWords_.clear();
        for (size_t i = 0; i < stopWordsSize; ++i) {
            size_t wordLength;
            ifs.read(reinterpret_cast<char*>(&wordLength), sizeof(wordLength));
            
            std::string word(wordLength, '\0');
            ifs.read(&word[0], wordLength);
            
            stopWords_.insert(word);
        }
        
        LOG_INFO("加载TF-IDF模型从文件: {}" , filepath);
        LOG_INFO("词汇表大小: {}" , std::to_string(vocabulary_.size()));
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("加载模型失败: {}" , std::string(e.what()));
        return false;
    }
}

void TfidfVectorizer::setTokenizer(std::function<std::vector<std::string>(const std::string&)> tokenizer) {
    tokenizer_ = std::move(tokenizer);
}

void TfidfVectorizer::setStopWords(const std::set<std::string>& stopWords) {
    stopWords_ = stopWords;
}

std::vector<std::vector<std::string>> TfidfVectorizer::tokenizeDocuments(const std::vector<std::string>& documents) const {
    std::vector<std::vector<std::string>> tokenizedDocs;
    tokenizedDocs.reserve(documents.size());
    
    for (const auto& doc : documents) {
        std::vector<std::string> tokens = tokenizer_(doc);
        
        // 移除停用词
        if (!stopWords_.empty()) {
            tokens.erase(
                std::remove_if(tokens.begin(), tokens.end(), 
                              [this](const std::string& token) {
                                  return this->stopWords_.find(token) != this->stopWords_.end();
                              }),
                tokens.end()
            );
        }
        
        tokenizedDocs.push_back(std::move(tokens));
    }
    
    return tokenizedDocs;
}

Eigen::SparseMatrix<float> TfidfVectorizer::calculateTf(const std::vector<std::vector<std::string>>& tokenizedDocs) const {
    typedef Eigen::Triplet<float> Triplet;
    std::vector<Triplet> triplets;
    
    // 预估三元组数量
    size_t estNonZeros = 0;
    for (const auto& tokens : tokenizedDocs) {
        estNonZeros += std::min(tokens.size(), vocabulary_.size());
    }
    triplets.reserve(estNonZeros);
    
    for (size_t docId = 0; docId < tokenizedDocs.size(); ++docId) {
        const auto& tokens = tokenizedDocs[docId];
        
        // 计算词频
        std::unordered_map<std::string, float> termFreq;
        for (const auto& token : tokens) {
            if (vocabulary_.find(token) != vocabulary_.end()) {
                termFreq[token] += 1.0;
            }
        }
        
        // 添加到稀疏矩阵
        for (const auto& pair : termFreq) {
            auto it = vocabulary_.find(pair.first);
            if (it != vocabulary_.end()) {
                float value = useBinary_ ? 1.0f : pair.second;
                triplets.emplace_back(docId, it->second, value);
            }
        }
    }
    
    // 创建稀疏矩阵
    Eigen::SparseMatrix<float> tfMatrix(tokenizedDocs.size(), vocabulary_.size());
    tfMatrix.setFromTriplets(triplets.begin(), triplets.end());
    
    return tfMatrix;
}

Eigen::VectorXf TfidfVectorizer::calculateIdf(const Eigen::SparseMatrix<float>& tfMatrix) {
    Eigen::VectorXf df = Eigen::VectorXf::Zero(vocabulary_.size());
    
    // 计算文档频率
    for (int k = 0; k < tfMatrix.outerSize(); ++k) {
        for (Eigen::SparseMatrix<float>::InnerIterator it(tfMatrix, k); it; ++it) {
            df(it.col()) += 1.0;
        }
    }
    
    // 过滤词汇表
    std::unordered_set<size_t> indicesToRemove;
    
    // 应用min_df和max_df过滤
    for (size_t i = 0; i < df.size(); ++i) {
        float docFreq = df(i);
        
        // 检查最小文档频率
        if (minDf_ > 0 && (minDf_ < 1.0 ? docFreq / numDocuments_ < minDf_ : docFreq < minDf_)) {
            indicesToRemove.insert(i);
        }
        
        // 检查最大文档频率
        if (maxDf_ < 1.0 && docFreq / numDocuments_ > maxDf_) {
            indicesToRemove.insert(i);
        }
    }
    
    // 应用max_features过滤
    if (maxFeatures_ > 0 && maxFeatures_ < vocabulary_.size() - indicesToRemove.size()) {
        std::vector<std::pair<float, size_t>> featureScores;
        for (size_t i = 0; i < df.size(); ++i) {
            if (indicesToRemove.find(i) == indicesToRemove.end()) {
                featureScores.emplace_back(df(i), i);
            }
        }
        
        // 排序，留下文档频率最大的前maxFeatures_个特征
        std::sort(featureScores.begin(), featureScores.end(), 
                 [](const auto& a, const auto& b) { return a.first > b.first; });
        
        for (size_t i = maxFeatures_; i < featureScores.size(); ++i) {
            indicesToRemove.insert(featureScores[i].second);
        }
    }
    
    // 如果有需要移除的特征，重建词汇表
    if (!indicesToRemove.empty()) {
        LOG_INFO("过滤特征，{}","从 " + std::to_string(vocabulary_.size()) + 
                  " 减少到 " + std::to_string(vocabulary_.size() - indicesToRemove.size()));
        
        std::unordered_map<std::string, size_t> newVocabulary;
        std::unordered_map<size_t, size_t> indexMapping;
        size_t newIndex = 0;
        
        for (const auto& pair : vocabulary_) {
            if (indicesToRemove.find(pair.second) == indicesToRemove.end()) {
                newVocabulary[pair.first] = newIndex;
                indexMapping[pair.second] = newIndex;
                newIndex++;
            }
        }
        
        // 更新词汇表
        vocabulary_ = std::move(newVocabulary);
        
        // 更新特征名称
        featureNames_.clear();
        featureNames_.resize(vocabulary_.size());
        for (const auto& pair : vocabulary_) {
            featureNames_[pair.second] = pair.first;
        }
        
        // 更新文档频率向量
        Eigen::VectorXf newDf = Eigen::VectorXf::Zero(vocabulary_.size());
        for (size_t i = 0; i < df.size(); ++i) {
            if (indicesToRemove.find(i) == indicesToRemove.end()) {
                newDf(indexMapping[i]) = df(i);
            }
        }
        df = std::move(newDf);
    }
    
    // 计算IDF
    Eigen::VectorXf idf = Eigen::VectorXf::Zero(vocabulary_.size());
    for (size_t i = 0; i < df.size(); ++i) {
        idf(i) = std::log((numDocuments_ + smooth_) / (df(i) + smooth_)) + 1.0;
    }
    
    return idf;
}

Eigen::MatrixXf TfidfVectorizer::applyNormalization(const Eigen::MatrixXf& matrix) const {
    Eigen::MatrixXf normalizedMatrix = matrix;
    
    if (normalization_ == "l1") {
        for (int i = 0; i < normalizedMatrix.rows(); ++i) {
            float norm = normalizedMatrix.row(i).template lpNorm<1>();
            if (norm > 0) {
                normalizedMatrix.row(i) /= norm;
            }
        }
    } else if (normalization_ == "l2") {
        for (int i = 0; i < normalizedMatrix.rows(); ++i) {
            float norm = normalizedMatrix.row(i).norm();
            if (norm > 0) {
                normalizedMatrix.row(i) /= norm;
            }
        }
    }
    
    return normalizedMatrix;
}

void TfidfVectorizer::buildVocabulary(const std::vector<std::vector<std::string>>& tokenizedDocs) {
    vocabulary_.clear();
    
    // 收集所有唯一词汇
    std::unordered_map<std::string, size_t> termDocFreq;
    for (const auto& tokens : tokenizedDocs) {
        std::unordered_set<std::string> uniqueTokens(tokens.begin(), tokens.end());
        for (const auto& token : uniqueTokens) {
            termDocFreq[token]++;
        }
    }
    
    // 应用min_df和max_df过滤
    std::vector<std::string> validTerms;
    for (const auto& pair : termDocFreq) {
        float docFreq = static_cast<float>(pair.second);
        
        bool isValid = true;
        
        // 检查最小文档频率
        if (minDf_ > 0 && (minDf_ < 1.0 ? docFreq / numDocuments_ < minDf_ : docFreq < minDf_)) {
            isValid = false;
        }
        
        // 检查最大文档频率
        if (maxDf_ < 1.0 && docFreq / numDocuments_ > maxDf_) {
            isValid = false;
        }
        
        if (isValid) {
            validTerms.push_back(pair.first);
        }
    }
    
    // 应用max_features过滤
    if (maxFeatures_ > 0 && maxFeatures_ < validTerms.size()) {
        // 按文档频率排序
        std::sort(validTerms.begin(), validTerms.end(), 
                 [&termDocFreq](const std::string& a, const std::string& b) {
                     return termDocFreq[a] > termDocFreq[b];
                 });
        
        validTerms.resize(maxFeatures_);
    }
    
    // 构建词汇表映射
    for (size_t i = 0; i < validTerms.size(); ++i) {
        vocabulary_[validTerms[i]] = i;
    }
    
    // 构建特征名称列表
    featureNames_.resize(vocabulary_.size());
    for (const auto& pair : vocabulary_) {
        featureNames_[pair.second] = pair.first;
    }
}

std::vector<std::string> TfidfVectorizer::defaultTokenizer(const std::string& document) {
    std::vector<std::string> tokens;
    std::istringstream iss(document);
    std::string token;
    
    while (iss >> token) {
        // 简单清洗令牌
        token = kb::utils::trim(token);
        token = kb::utils::toLower(token);
        
        // 移除标点符号
        token.erase(std::remove_if(token.begin(), token.end(), 
                                  [](char c) { return std::ispunct(static_cast<unsigned char>(c)); }),
                   token.end());
        
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    
    return tokens;
}

} // namespace vectorizer
} // namespace kb 