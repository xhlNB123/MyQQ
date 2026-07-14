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

bool SessionManager::IsOnline(int userId) {
    std::lock_guard<std::mutex> lk(mtx_);
    return online_.count(userId) > 0;
}

} // namespace myqq
