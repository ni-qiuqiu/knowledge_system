/**
 * @file utils.cpp
 * @brief 通用工具函数实现
 */

#include "common/utils.h"
#include <random>
#include <chrono>
#include <sstream>
#include <iomanip>

// 大多数工具函数都是内联的，这个文件仅用于实现更复杂的非内联函数

namespace kb {
namespace utils {

// 现在还没有非内联函数需要实现

std::string generateUUID() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hexChars = "0123456789abcdef";
    
    std::string uuid = "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";
    
    for (auto& c : uuid) {
        if (c == 'x') {
            c = hexChars[dis(gen)];
        } else if (c == 'y') {
            c = hexChars[(dis(gen) & 0x3) | 0x8];
        }
    }
    
    return uuid;
}

std::string getCurrentTimeString() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    
    return ss.str();
}

} // namespace utils
} // namespace kb 