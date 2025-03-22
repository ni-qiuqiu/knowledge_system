/**
 * @file dialogue_manager.h
 * @brief 对话管理器，负责管理多轮对话上下文
 */

#ifndef KB_DIALOGUE_MANAGER_H
#define KB_DIALOGUE_MANAGER_H

#include <string>
#include <vector>
#include <memory>
#include <deque>
#include "query/query_parser.h"
#include "query/answer_generator.h"

namespace kb {
namespace query {

/**
 * @brief 对话消息结构
 */
struct DialogueMessage {
    enum class Role {
        USER,       ///< 用户消息
        SYSTEM,     ///< 系统消息
        ASSISTANT   ///< 助手消息
    };
    
    Role role;                ///< 消息角色
    std::string content;      ///< 消息内容
    std::string timestamp;    ///< 时间戳
    std::unordered_map<std::string, std::string> metadata; ///< 元数据
    
    // 构造函数
    DialogueMessage(Role r = Role::USER, const std::string& c = "")
        : role(r), content(c) {}
};

/**
 * @brief 对话上下文结构
 */
struct DialogueContext {
    std::string sessionId;                      ///< 会话ID
    std::string userId;                         ///< 用户ID
    std::deque<DialogueMessage> messages;       ///< 消息历史
    size_t maxHistorySize;                      ///< 最大历史记录大小
    std::unordered_map<std::string, std::string> attributes; ///< 上下文属性
    
    // 构造函数
    DialogueContext() : maxHistorySize(20) {}
    
    // 添加消息
    void addMessage(const DialogueMessage& message) {
        // 如果超过最大历史记录大小，移除最早的消息
        if (messages.size() >= maxHistorySize) {
            messages.pop_front();
        }
        messages.push_back(message);
    }
    
    // 清空历史
    void clearHistory() {
        messages.clear();
    }
};

/**
 * @brief 对话管理器类
 */
class DialogueManager {
public:
    /**
     * @brief 构造函数
     */
    DialogueManager();
    
    /**
     * @brief 析构函数
     */
    virtual ~DialogueManager();
    
    /**
     * @brief 创建新会话
     * @param userId 用户ID
     * @param sessionId 会话ID（为空则自动生成）
     * @return 会话ID
     */
    std::string createSession(const std::string& userId, const std::string& sessionId = "");
    
    /**
     * @brief 获取会话上下文
     * @param sessionId 会话ID
     * @return 会话上下文指针
     */
    std::shared_ptr<DialogueContext> getSession(const std::string& sessionId);

    /**
     * @brief 添加助手回复
     * @param sessionId 会话ID
     * @param message 助手回复
     * @return 会话上下文指针
     */
    std::shared_ptr<DialogueContext> addAssistantResponse(const std::string& sessionId, const std::string& message);
    
    /**
     * @brief
     * @param sessionId 会话ID
     * @param message 用户消息
     * @return 会话上下文指针
     */
    std::shared_ptr<DialogueContext> addUserMessage(const std::string& sessionId, const std::string& message);
    
    /**
     * @brief 添加系统回复
     * @param sessionId 会话ID
     * @param message 系统回复
     * @return 会话上下文指针
     */
    std::shared_ptr<DialogueContext> addSystemResponse(const std::string& sessionId, const std::string& message);
    
    /**
     * @brief 获取会话历史
     * @param sessionId 会话ID
     * @param maxMessages 最大消息数量（0表示所有）
     * @return 会话历史消息
     */
    std::vector<DialogueMessage> getHistory(const std::string& sessionId, size_t maxMessages = 0);
    
    /**
     * @brief 清除会话历史
     * @param sessionId 会话ID
     */
    void clearHistory(const std::string& sessionId);
    
    /**
     * @brief 删除会话
     * @param sessionId 会话ID
     * @return 是否成功
     */
    bool removeSession(const std::string& sessionId);

    /**
     * @brief 获取活跃会话数量
     * @return 活跃会话数量
     */
    size_t getActiveSessionCount() const;
    
    /**
     * @brief 设置会话属性
     * @param sessionId 会话ID
     * @param key 属性键
     * @param value 属性值
     */
    void setContextAttribute(const std::string& sessionId, const std::string& key, const std::string& value);
    
    /**
     * @brief 获取会话属性
     * @param sessionId 会话ID
     * @param key 属性键
     * @return 属性值
     */
    std::string getContextAttribute(const std::string& sessionId, const std::string& key);

private:
    /**
     * @brief 生成会话ID
     * @return 会话ID
     */
    std::string generateSessionId();
    
    std::unordered_map<std::string, std::shared_ptr<DialogueContext>> sessions_;  ///< 会话映射
};

} // namespace query
} // namespace kb

#endif // KB_DIALOGUE_MANAGER_H 