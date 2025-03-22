#ifndef KB_COMMON_LOGGING_H
#define KB_COMMON_LOGGING_H

#include <string>
#include <sstream>
#include <format>

namespace kb {
namespace common {

/**
 * @brief 日志级别
 */
enum class LogLevel {
    TRACE,  ///< 跟踪级别
    DEBUG,  ///< 调试级别
    INFO,   ///< 信息级别
    WARN,   ///< 警告级别
    ERROR,  ///< 错误级别
    FATAL   ///< 致命级别
};

/**
 * @brief 设置全局日志级别
 * @param level 日志级别
 */
void setLogLevel(LogLevel level);

/**
 * @brief 获取全局日志级别
 * @return 当前日志级别
 */
LogLevel getLogLevel();

/**
 * @brief 日志级别转字符串
 * @param level 日志级别
 * @return 日志级别字符串
 */
std::string logLevelToString(LogLevel level);

/**
 * @brief 获取当前时间戳
 * @return 格式化的当前时间戳
 */
std::string getCurrentTimestamp();

/**
 * @brief 内部日志记录函数
 * @param level 日志级别
 * @param file 文件名
 * @param line 行号
 * @param message 日志消息
 */
void logInternal(LogLevel level, const char* file, int line, const std::string& message);

/**
 * @brief 使用 std::format 格式化日志消息的函数模板
 * @param level 日志级别
 * @param file 文件名
 * @param line 行号
 * @param fmt 格式化字符串
 * @param args 变参列表
 */
template<typename... Args>
void logFormat(LogLevel level, const char* file, int line, std::format_string<Args...> fmt, Args&&... args) {
    std::string formattedMessage = std::format(fmt, std::forward<Args>(args)...);
    logInternal(level, file, line, formattedMessage);
}

/**
 * @brief 简化的日志宏
 */
#define LOG_TRACE(fmt, ...) \
    kb::common::logFormat(kb::common::LogLevel::TRACE, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_DEBUG(fmt, ...) \
    kb::common::logFormat(kb::common::LogLevel::DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_INFO(fmt, ...) \
    kb::common::logFormat(kb::common::LogLevel::INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_WARN(fmt, ...) \
    kb::common::logFormat(kb::common::LogLevel::WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_ERROR(fmt, ...) \
    kb::common::logFormat(kb::common::LogLevel::ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_FATAL(fmt, ...) \
    kb::common::logFormat(kb::common::LogLevel::FATAL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

} // namespace common
} // namespace kb

#endif // KB_COMMON_LOGGING_H