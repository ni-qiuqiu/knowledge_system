#ifndef KB_UTILS_H
#define KB_UTILS_H

#include <string>
#include <vector>
#include <functional>
#include <openssl/sha.h>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <regex>
#include <boost/filesystem.hpp>
#include <random>

namespace kb {
namespace utils {

/**
 * @brief   生成随机字符串
 * @param length 字符串长度
 * @return 随机字符串
 */
inline std::string generateRandomString(size_t length) {
    std::string result;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 35);
    
    for (size_t i = 0; i < length; ++i) {
        result += "0123456789abcdefghijklmnopqrstuvwxyz"[dis(gen)];
    }
    return result;
}

/**
 * @brief 计算字符串的SHA-256哈希值
 * @param str 输入字符串
 * @return SHA-256哈希值的十六进制字符串
 */
inline std::string sha256(const std::string& str) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, str.c_str(), str.length());
    SHA256_Final(hash, &sha256);
    
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    
    return ss.str();
}

/**
 * @brief 递归查找指定目录下的所有文件
 * @param directory 目录路径
 * @param extensions 文件扩展名列表，如{"txt", "pdf"}，空列表表示所有文件
 * @return 文件路径列表
 */
inline std::vector<std::string> findFiles(const std::string& directory, 
                                         const std::vector<std::string>& extensions = {}) {
    std::vector<std::string> files;
    namespace fs = boost::filesystem;
    
    if (!fs::exists(directory) || !fs::is_directory(directory)) {
        return files;
    }
    
    for (fs::recursive_directory_iterator it(directory), end; it != end; ++it) {
        if (fs::is_regular_file(it->path())) {
            // 如果扩展名列表为空，或者文件扩展名匹配
            if (extensions.empty()) {
                files.push_back(it->path().string());
            } else {
                std::string ext = it->path().extension().string();
                // 移除扩展名前的点
                if (!ext.empty() && ext[0] == '.') {
                    ext = ext.substr(1);
                }
                
                for (const auto& validExt : extensions) {
                    if (ext == validExt) {
                        files.push_back(it->path().string());
                        break;
                    }
                }
            }
        }
    }
    
    return files;
}

/**
 * @brief 读取整个文件内容
 * @param filePath 文件路径
 * @return 文件内容字符串
 */
inline std::string readFile(const std::string& filePath) {
    std::ifstream in(filePath, std::ios::in | std::ios::binary);
    if (!in) {
        throw std::runtime_error("无法打开文件: " + filePath);
    }
    
    std::string contents;
    in.seekg(0, std::ios::end);
    contents.resize(in.tellg());
    in.seekg(0, std::ios::beg);
    in.read(&contents[0], contents.size());
    in.close();
    
    return contents;
}

/**
 * @brief 正则表达式替换
 * @param source 源字符串
 * @param regex 正则表达式
 * @param replacement 替换字符串
 * @return 替换后的字符串
 */
inline std::string regexReplace(const std::string& source, 
                               const std::string& regex, 
                               const std::string& replacement) {
    return std::regex_replace(source, std::regex(regex), replacement);
}

/**
 * @brief 分割字符串
 * @param s 源字符串
 * @param delimiter 分隔符
 * @return 分割后的字符串列表
 */
inline std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

/**
 * @brief 去除字符串首尾的空白字符
 * @param s 源字符串
 * @return 处理后的字符串
 */
inline std::string trim(const std::string& s) {
    auto wsfront = std::find_if_not(s.begin(), s.end(), [](int c) { return std::isspace(c); });
    auto wsback = std::find_if_not(s.rbegin(), s.rend(), [](int c) { return std::isspace(c); }).base();
    return (wsback <= wsfront ? std::string() : std::string(wsfront, wsback));
}

inline std::string toLower(const std::string& s){
    std::string lowerStr;
    std::transform(s.begin(), s.end(), std::back_inserter(lowerStr), ::tolower);
    return lowerStr;
}

} // namespace utils
} // namespace kb

#endif // KB_UTILS_H 