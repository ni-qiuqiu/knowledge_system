#include "common/logging.h"
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>

namespace kb {
namespace common {

// 全局日志级别，默认为INFO
LogLevel gLogLevel = LogLevel::INFO;

void setLogLevel(LogLevel level) {
    gLogLevel = level;
}

LogLevel getLogLevel() {
    return gLogLevel;
}

std::string logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default:              return "UNKNOWN";
    }
}

std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
       << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

void logInternal(LogLevel level, const char* file, int line, const std::string& message) {
    if (level < gLogLevel) {
        return;
    }
    
    std::string timestamp = getCurrentTimestamp();
    std::string levelStr = logLevelToString(level);
    
    std::cerr << timestamp << " [" << levelStr << "] "
              << file << ":" << line << " - " 
              << message << std::endl;
}

} // namespace common
} // namespace kb