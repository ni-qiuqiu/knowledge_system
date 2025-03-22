/**
 * @file dialogue_manager.cpp
 * @brief 对话管理器实现
 */

#include "query/dialogue_manager.h"
#include "common/logging.h"
#include "common/utils.h"
#include <algorithm>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>


namespace kb {
namespace query {

// 构造函数
DialogueManager::DialogueManager() {
    LOG_INFO("对话管理器初始化完成");
}

// 析构函数
DialogueManager::~DialogueManager() {
    LOG_INFO("对话管理器释放资源");
    
    // 释放所有活跃会话
    sessions_.clear();
}

// 创建新会话
std::string DialogueManager::createSession(const std::string& userId, const std::string& sessionId) {
    // 生成会话ID（如果未提供）
    std::string newSessionId = sessionId.empty() ? generateSessionId() : sessionId;
    
    // 检查会话ID是否已存在
    if (sessions_.find(newSessionId) != sessions_.end()) {
        LOG_WARN("会话ID已存在: {}, 将生成新的会话ID", newSessionId);
        newSessionId = generateSessionId();
    }
    
    // 创建新会话上下文
    auto context = std::make_shared<DialogueContext>();
    context->sessionId = newSessionId;
    context->userId = userId;
    
    // 添加系统消息
    DialogueMessage systemMessage(DialogueMessage::Role::SYSTEM, 
                                "欢迎使用知识图谱问答系统。您可以向我询问任何问题，我会尽力回答。");
    
    // 获取当前时间戳
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    
    systemMessage.timestamp = ss.str();
    context->addMessage(systemMessage);
    
    // 存储会话
    sessions_[newSessionId] = context;
    
    LOG_INFO("创建新会话: sessionId={}, userId={}", newSessionId, userId);
    
    return newSessionId;
}

// 获取会话上下文
std::shared_ptr<DialogueContext> DialogueManager::getSession(const std::string& sessionId) {
    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        LOG_DEBUG("获取会话: sessionId={}", sessionId);
        return it->second;
    }
    
    LOG_WARN("会话未找到: sessionId={}", sessionId);
    return nullptr;
}

// 添加用户消息
std::shared_ptr<DialogueContext> DialogueManager::addUserMessage(
    const std::string& sessionId, 
    const std::string& message) {
    
    // 查找会话
    auto context = getSession(sessionId);
    if (!context) {
        LOG_WARN("添加用户消息失败，会话未找到: sessionId={}", sessionId);
        return nullptr;
    }
    
    // 创建用户消息
    DialogueMessage userMessage(DialogueMessage::Role::USER, message);
    userMessage.timestamp = common::getCurrentTimestamp();
    
    // 添加到上下文
    context->addMessage(userMessage);
    
    LOG_INFO("添加用户消息: sessionId={}, message=\"{}\"", sessionId, message);
    
    return context;
}

// 添加系统回复
std::shared_ptr<DialogueContext> DialogueManager::addSystemResponse(
    const std::string& sessionId, 
    const std::string& message) {
    
    // 查找会话
    auto context = getSession(sessionId);
    if (!context) {
        LOG_WARN("添加系统回复失败，会话未找到: sessionId={}", sessionId);
        return nullptr;
    }
    
    // 创建系统消息
    DialogueMessage systemMessage(DialogueMessage::Role::ASSISTANT, message);
    systemMessage.timestamp = common::getCurrentTimestamp();
    
    // 添加到上下文
    context->addMessage(systemMessage);
    
    LOG_INFO("添加系统回复: sessionId={}, message=\"{}\"", sessionId, 
             message.length() > 50 ? message.substr(0, 50) + "..." : message);
    
    return context;
}

// 获取会话历史
std::vector<DialogueMessage> DialogueManager::getHistory(
    const std::string& sessionId, 
    size_t maxMessages) {
    
    // 查找会话
    auto context = getSession(sessionId);
    if (!context) {
        LOG_WARN("获取会话历史失败，会话未找到: sessionId={}", sessionId);
        return {};
    }
    
    // 创建结果容器
    std::vector<DialogueMessage> history;
    
    // 如果maxMessages为0或者大于实际消息数，返回所有消息
    if (maxMessages == 0 || maxMessages >= context->messages.size()) {
        // 使用构造函数将 deque 内容复制到 vector
        history = std::vector<DialogueMessage>(context->messages.begin(), context->messages.end());
    } else {
        // 否则只返回最近的maxMessages条消息
        auto startIt = context->messages.end() - maxMessages;
        // 使用构造函数将部分 deque 内容复制到 vector
        history = std::vector<DialogueMessage>(startIt, context->messages.end());
    }
    
    LOG_DEBUG("获取会话历史: sessionId={}, 返回 {} 条消息", 
              sessionId, history.size());
    
    return history;
}

// 清除会话历史
void DialogueManager::clearHistory(const std::string& sessionId) {
    // 查找会话
    auto context = getSession(sessionId);
    if (!context) {
        LOG_WARN("清除会话历史失败，会话未找到: sessionId={}", sessionId);
        return;
    }
    
    // 清空消息历史
    context->clearHistory();
    
    LOG_INFO("清除会话历史: sessionId={}", sessionId);
}

// 删除会话
bool DialogueManager::removeSession(const std::string& sessionId) {
    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        sessions_.erase(it);
        LOG_INFO("删除会话: sessionId={}", sessionId);
        return true;
    }
    
    LOG_WARN("删除会话失败，会话未找到: sessionId={}", sessionId);
    return false;
}

// 获取活跃会话数量
size_t DialogueManager::getActiveSessionCount() const {
    return sessions_.size();
}

// 设置会话属性
void DialogueManager::setContextAttribute(
    const std::string& sessionId, 
    const std::string& key, 
    const std::string& value) {
    
    // 查找会话
    auto context = getSession(sessionId);
    if (!context) {
        LOG_WARN("设置会话属性失败，会话未找到: sessionId={}", sessionId);
        return;
    }
    
    // 设置属性
    context->attributes[key] = value;
    
    LOG_DEBUG("设置会话属性: sessionId={}, key={}, value={}", 
              sessionId, key, value);
}

// 获取会话属性
std::string DialogueManager::getContextAttribute(
    const std::string& sessionId, 
    const std::string& key) {
    
    // 查找会话
    auto context = getSession(sessionId);
    if (!context) {
        LOG_WARN("获取会话属性失败，会话未找到: sessionId={}", sessionId);
        return "";
    }
    
    // 查找属性
    auto it = context->attributes.find(key);
    if (it != context->attributes.end()) {
        LOG_DEBUG("获取会话属性: sessionId={}, key={}, value={}", 
                  sessionId, key, it->second);
        return it->second;
    }
    
    LOG_DEBUG("会话属性未找到: sessionId={}, key={}", sessionId, key);
    return "";
}

// 生成会话ID
std::string DialogueManager::generateSessionId() {
    // 使用当前时间戳和随机数生成会话ID
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    
    // 生成随机数
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    int randomNum = dis(gen);
    
    // 组合生成会话ID
    std::stringstream ss;
    ss << "session_" << millis << "_" << randomNum;
    
    return ss.str();
}

// 获取当前时间戳的辅助函数
std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
    
    return ss.str();
}

} // namespace query
} // namespace kb 