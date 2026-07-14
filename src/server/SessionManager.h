#pragma once
// =====================================================================
// 在线会话管理：维护 userId -> 连接 的映射，支持向指定用户推送消息。
// 线程安全（每个客户端一个线程）。
// =====================================================================

#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace myqq {

class TcpSocket;   // 前向声明

class SessionManager {
public:
    // 登记/移除在线用户
    void Bind(int userId, TcpSocket* conn);
    bool Remove(int userId, TcpSocket* conn);

    // 向指定在线用户推送一行消息；用户不在线返回 false
    bool PushTo(int userId, const std::string& line);
    // 向多个在线用户群发（排除 exceptUser，通常是发送者自己）
    void PushToMany(const std::vector<int>& userIds, const std::string& line, int exceptUser);

    bool IsOnline(int userId);

private:
    std::map<int, TcpSocket*> online_;
    std::mutex                mtx_;
};

} // namespace myqq
