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

void SessionManager::Remove(int userId) {
    std::lock_guard<std::mutex> lk(mtx_);
    online_.erase(userId);
}

bool SessionManager::PushTo(int userId, const std::string& line) {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = online_.find(userId);
    if (it == online_.end() || !it->second) return false;
    return it->second->SendLine(line);
}

bool SessionManager::IsOnline(int userId) {
    std::lock_guard<std::mutex> lk(mtx_);
    return online_.count(userId) > 0;
}

} // namespace myqq
