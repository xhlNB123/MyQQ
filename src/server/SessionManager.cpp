// =====================================================================
// SessionManager.h 的实现
// =====================================================================
#include "SessionManager.h"
#include "../common/Socket.h"

namespace myqq {

void SessionManager::Bind(int userId, TcpSocket* conn) {
    std::lock_guard<std::mutex> lk(mtx_);
    online_[userId] = conn;
}

bool SessionManager::Remove(int userId, TcpSocket* conn) {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = online_.find(userId);
    if (it == online_.end() || it->second != conn) return false;
    online_.erase(it);
    return true;
}

bool SessionManager::PushTo(int userId, const std::string& line) {
    TcpSocket* conn = nullptr;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = online_.find(userId);
        if (it == online_.end() || !it->second) return false;
        conn = it->second;
    }
    return conn->SendLine(line);
}

void SessionManager::PushToMany(const std::vector<int>& userIds, const std::string& line, int exceptUser) {
    // 先在锁内收集目标连接，再逐个发送（发送时不持锁，避免阻塞 I/O 占锁）
    std::vector<TcpSocket*> targets;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        for (int uid : userIds) {
            if (uid == exceptUser) continue;
            auto it = online_.find(uid);
            if (it != online_.end() && it->second) targets.push_back(it->second);
        }
    }
    for (auto* c : targets) c->SendLine(line);
}

bool SessionManager::IsOnline(int userId) {
    std::lock_guard<std::mutex> lk(mtx_);
    return online_.count(userId) > 0;
}

} // namespace myqq
