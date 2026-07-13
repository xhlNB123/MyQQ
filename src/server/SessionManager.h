#pragma once
// =====================================================================
// 在线会话管理：维护 userId -> 连接 的映射，支持向指定用户推送消息。
// 线程安全（每个客户端一个线程）。
// =====================================================================

#include <map>
#include <mutex>
#include <string>

namespace myqq {

class TcpSocket;   // 前向声明

class SessionManager {
public:
    // 登记/移除在线用户
    void Bind(int userId, TcpSocket* conn);
    void Remove(int userId);

    // 向指定在线用户推送一行消息；用户不在线返回 false
    bool PushTo(int userId, const std::string& line);

    bool IsOnline(int userId);

private:
    std::map<int, TcpSocket*> online_;
    std::mutex                mtx_;
};

} // namespace myqq
