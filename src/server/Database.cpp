// =====================================================================
// Database.h 的实现（SQLite，参数化查询防注入）
// =====================================================================
#include "Database.h"
#include "../../third_party/sqlite/sqlite3.h"
#include <algorithm>

namespace myqq {

Database::Database() {}
Database::~Database() { Close(); }

bool Database::Open(const std::string& dbPath, const std::string& schemaSql) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (sqlite3_open(dbPath.c_str(), &db_) != SQLITE_OK) {
        lastErr_ = sqlite3_errmsg(db_);
        return false;
    }
    sqlite3_exec(db_, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);
    sqlite3_busy_timeout(db_, 5000);
    // schema 内使用 IF NOT EXISTS，可同时承担旧数据库的表/索引迁移。
    if (!Exec(schemaSql)) return false;
    Exec("PRAGMA user_version=2;");
    return true;
}

void Database::Close() {
    if (db_) { sqlite3_close(db_); db_ = nullptr; }
}

bool Database::Exec(const std::string& sql) {
    char* err = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        lastErr_ = err ? err : "exec failed";
        sqlite3_free(err);
        return false;
    }
    return true;
}

// ---------------- 账号 ----------------
int Database::Register(const std::string& account, const std::string& password,
                       const std::string& nickName) {
    // 先查重
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_, "SELECT UserId FROM Users WHERE Account=?;", -1, &st, nullptr);
    sqlite3_bind_text(st, 1, account.c_str(), -1, SQLITE_TRANSIENT);
    bool exists = (sqlite3_step(st) == SQLITE_ROW);
    sqlite3_finalize(st);
    if (exists) return -1;

    sqlite3_prepare_v2(db_,
        "INSERT INTO Users(Account,Password,NickName,PolicyId) VALUES(?,?,?,2);" ,
        -1, &st, nullptr);
    sqlite3_bind_text(st, 1, account.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, password.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, nickName.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE) { lastErr_ = sqlite3_errmsg(db_); return 0; }
    return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

int Database::Login(const std::string& account, const std::string& password) {
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT UserId FROM Users WHERE Account=? AND Password=?;",
        -1, &st, nullptr);
    sqlite3_bind_text(st, 1, account.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, password.c_str(), -1, SQLITE_TRANSIENT);
    int userId = 0;
    if (sqlite3_step(st) == SQLITE_ROW)
        userId = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    return userId;
}

void Database::SetStatus(int userId, int status) {
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_, "UPDATE Users SET Status=? WHERE UserId=?;", -1, &st, nullptr);
    sqlite3_bind_int(st, 1, status);
    sqlite3_bind_int(st, 2, userId);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

// ---------------- 好友 ----------------
std::vector<UserInfo> Database::SearchUsers(const std::string& keyword, int selfId) {
    std::vector<UserInfo> out;
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT UserId,Account,NickName,Status FROM Users "
        "WHERE (Account LIKE ? OR NickName LIKE ?) AND UserId<>?;",
        -1, &st, nullptr);
    std::string like = "%" + keyword + "%";
    sqlite3_bind_text(st, 1, like.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, like.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (st, 3, selfId);
    while (sqlite3_step(st) == SQLITE_ROW) {
        UserInfo u;
        u.userId   = sqlite3_column_int(st, 0);
        u.account  = reinterpret_cast<const char*>(sqlite3_column_text(st, 1));
        u.nickName = reinterpret_cast<const char*>(sqlite3_column_text(st, 2));
        u.status   = sqlite3_column_int(st, 3);
        out.push_back(u);
    }
    sqlite3_finalize(st);
    return out;
}

bool Database::AddFriend(int userId, int friendId) {
    if (userId == friendId) return false;
    // 双向插入（忽略重复）
    const char* sql = "INSERT OR IGNORE INTO Friends(UserId,FriendId) VALUES(?,?);";
    for (int i = 0; i < 2; ++i) {
        sqlite3_stmt* st = nullptr;
        sqlite3_prepare_v2(db_, sql, -1, &st, nullptr);
        sqlite3_bind_int(st, 1, i == 0 ? userId : friendId);
        sqlite3_bind_int(st, 2, i == 0 ? friendId : userId);
        int rc = sqlite3_step(st);
        sqlite3_finalize(st);
        if (rc != SQLITE_DONE) { lastErr_ = sqlite3_errmsg(db_); return false; }
    }
    return true;
}

std::vector<UserInfo> Database::GetFriends(int userId) {
    std::vector<UserInfo> out;
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT u.UserId,u.Account,u.NickName,u.Status FROM Friends f "
        "JOIN Users u ON u.UserId=f.FriendId WHERE f.UserId=?;",
        -1, &st, nullptr);
    sqlite3_bind_int(st, 1, userId);
    while (sqlite3_step(st) == SQLITE_ROW) {
        UserInfo u;
        u.userId   = sqlite3_column_int(st, 0);
        u.account  = reinterpret_cast<const char*>(sqlite3_column_text(st, 1));
        u.nickName = reinterpret_cast<const char*>(sqlite3_column_text(st, 2));
        u.status   = sqlite3_column_int(st, 3);
        out.push_back(u);
    }
    sqlite3_finalize(st);
    return out;
}

bool Database::AreFriends(int a, int b) {
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT 1 FROM Friends WHERE UserId=? AND FriendId=?;", -1, &st, nullptr);
    sqlite3_bind_int(st, 1, a);
    sqlite3_bind_int(st, 2, b);
    bool yes = (sqlite3_step(st) == SQLITE_ROW);
    sqlite3_finalize(st);
    return yes;
}

bool Database::UserExists(int userId) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_, "SELECT 1 FROM Users WHERE UserId=?;", -1, &st, nullptr);
    sqlite3_bind_int(st, 1, userId);
    bool found = sqlite3_step(st) == SQLITE_ROW;
    sqlite3_finalize(st);
    return found;
}

bool Database::GetUserInfo(int userId, UserInfo& out) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_, "SELECT UserId,Account,NickName,Status FROM Users WHERE UserId=?;", -1, &st, nullptr);
    sqlite3_bind_int(st, 1, userId);
    bool found = false;
    if (sqlite3_step(st) == SQLITE_ROW) {
        out.userId = sqlite3_column_int(st, 0);
        out.account = reinterpret_cast<const char*>(sqlite3_column_text(st, 1));
        out.nickName = reinterpret_cast<const char*>(sqlite3_column_text(st, 2));
        out.status = sqlite3_column_int(st, 3);
        found = true;
    }
    sqlite3_finalize(st);
    return found;
}

int Database::CreateFriendRequest(int senderId, int receiverId, const std::string& verify,
                                  FriendRequestInfo& out) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (senderId == receiverId || !UserExists(receiverId)) return 3;
    if (AreFriends(senderId, receiverId)) return 4;

    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT RequestId,SenderId,ReceiverId FROM FriendRequests "
        "WHERE Status=0 AND ((SenderId=? AND ReceiverId=?) OR (SenderId=? AND ReceiverId=?)) "
        "ORDER BY RequestId DESC LIMIT 1;", -1, &st, nullptr);
    sqlite3_bind_int(st, 1, senderId); sqlite3_bind_int(st, 2, receiverId);
    sqlite3_bind_int(st, 3, receiverId); sqlite3_bind_int(st, 4, senderId);
    if (sqlite3_step(st) == SQLITE_ROW) {
        out.requestId = sqlite3_column_int64(st, 0);
        out.senderId = sqlite3_column_int(st, 1);
        out.receiverId = sqlite3_column_int(st, 2);
        sqlite3_finalize(st);
        return 4;
    }
    sqlite3_finalize(st);

    sqlite3_prepare_v2(db_,
        "INSERT INTO FriendRequests(SenderId,ReceiverId,VerifyText) VALUES(?,?,?);",
        -1, &st, nullptr);
    sqlite3_bind_int(st, 1, senderId); sqlite3_bind_int(st, 2, receiverId);
    sqlite3_bind_text(st, 3, verify.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE) { lastErr_ = sqlite3_errmsg(db_); return 5; }
    out.requestId = sqlite3_last_insert_rowid(db_);
    out.senderId = senderId; out.receiverId = receiverId; out.verifyText = verify;
    // 单独读取发送者展示信息
    UserInfo sender;
    GetUserInfo(senderId, sender);
    out.senderAccount = sender.account; out.senderNick = sender.nickName;
    sqlite3_prepare_v2(db_, "SELECT CreatedTime FROM FriendRequests WHERE RequestId=?;", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, out.requestId);
    if (sqlite3_step(st) == SQLITE_ROW) out.createdTime = reinterpret_cast<const char*>(sqlite3_column_text(st, 0));
    sqlite3_finalize(st);
    return 0;
}

static FriendRequestInfo ReadFriendRequest(sqlite3_stmt* st) {
    FriendRequestInfo r;
    r.requestId = sqlite3_column_int64(st, 0);
    r.senderId = sqlite3_column_int(st, 1); r.receiverId = sqlite3_column_int(st, 2);
    r.verifyText = reinterpret_cast<const char*>(sqlite3_column_text(st, 3));
    r.status = sqlite3_column_int(st, 4);
    const unsigned char* c = sqlite3_column_text(st, 5); if (c) r.createdTime = reinterpret_cast<const char*>(c);
    c = sqlite3_column_text(st, 6); if (c) r.handledTime = reinterpret_cast<const char*>(c);
    r.resultAcknowledged = sqlite3_column_int(st, 7);
    c = sqlite3_column_text(st, 8); if (c) r.senderAccount = reinterpret_cast<const char*>(c);
    c = sqlite3_column_text(st, 9); if (c) r.senderNick = reinterpret_cast<const char*>(c);
    c = sqlite3_column_text(st, 10); if (c) r.receiverNick = reinterpret_cast<const char*>(c);
    return r;
}

std::vector<FriendRequestInfo> Database::GetPendingIncomingRequests(int receiverId) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<FriendRequestInfo> out;
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT r.RequestId,r.SenderId,r.ReceiverId,r.VerifyText,r.Status,r.CreatedTime,r.HandledTime,r.ResultAcknowledged,"
        "s.Account,s.NickName,d.NickName FROM FriendRequests r JOIN Users s ON s.UserId=r.SenderId "
        "JOIN Users d ON d.UserId=r.ReceiverId WHERE r.ReceiverId=? AND r.Status=0 ORDER BY r.RequestId;",
        -1, &st, nullptr);
    sqlite3_bind_int(st, 1, receiverId);
    while (sqlite3_step(st) == SQLITE_ROW) out.push_back(ReadFriendRequest(st));
    sqlite3_finalize(st);
    return out;
}

std::vector<FriendRequestInfo> Database::GetUnacknowledgedOutgoingResults(int senderId) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<FriendRequestInfo> out;
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT r.RequestId,r.SenderId,r.ReceiverId,r.VerifyText,r.Status,r.CreatedTime,r.HandledTime,r.ResultAcknowledged,"
        "s.Account,s.NickName,d.NickName FROM FriendRequests r JOIN Users s ON s.UserId=r.SenderId "
        "JOIN Users d ON d.UserId=r.ReceiverId WHERE r.SenderId=? AND r.Status<>0 AND r.ResultAcknowledged=0 ORDER BY r.RequestId;",
        -1, &st, nullptr);
    sqlite3_bind_int(st, 1, senderId);
    while (sqlite3_step(st) == SQLITE_ROW) out.push_back(ReadFriendRequest(st));
    sqlite3_finalize(st);
    return out;
}

int Database::ResolveFriendRequest(long long requestId, int receiverId, bool accept,
                                   FriendRequestInfo& out) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!Exec("BEGIN IMMEDIATE;")) return 5;
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT r.RequestId,r.SenderId,r.ReceiverId,r.VerifyText,r.Status,r.CreatedTime,r.HandledTime,r.ResultAcknowledged,"
        "s.Account,s.NickName,d.NickName FROM FriendRequests r JOIN Users s ON s.UserId=r.SenderId "
        "JOIN Users d ON d.UserId=r.ReceiverId WHERE r.RequestId=? AND r.ReceiverId=?;",
        -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, requestId); sqlite3_bind_int(st, 2, receiverId);
    if (sqlite3_step(st) != SQLITE_ROW) { sqlite3_finalize(st); Exec("ROLLBACK;"); return 3; }
    out = ReadFriendRequest(st); sqlite3_finalize(st);
    if (out.status != 0) { Exec("ROLLBACK;"); return 4; }
    if (accept && !AddFriend(out.senderId, out.receiverId)) { Exec("ROLLBACK;"); return 5; }
    sqlite3_prepare_v2(db_,
        "UPDATE FriendRequests SET Status=?,HandledTime=datetime('now','localtime') WHERE RequestId=? AND Status=0;",
        -1, &st, nullptr);
    sqlite3_bind_int(st, 1, accept ? 1 : 2); sqlite3_bind_int64(st, 2, requestId);
    int rc = sqlite3_step(st); sqlite3_finalize(st);
    if (rc != SQLITE_DONE || sqlite3_changes(db_) != 1) { Exec("ROLLBACK;"); return 5; }
    if (!Exec("COMMIT;")) { Exec("ROLLBACK;"); return 5; }
    out.status = accept ? 1 : 2;
    sqlite3_prepare_v2(db_, "SELECT HandledTime FROM FriendRequests WHERE RequestId=?;", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, requestId);
    if (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char* handled = sqlite3_column_text(st, 0);
        if (handled) out.handledTime = reinterpret_cast<const char*>(handled);
    }
    sqlite3_finalize(st);
    return 0;
}

bool Database::AcknowledgeFriendResult(long long requestId, int senderId) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_, "UPDATE FriendRequests SET ResultAcknowledged=1 WHERE RequestId=? AND SenderId=? AND Status<>0;", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, requestId); sqlite3_bind_int(st, 2, senderId);
    int rc = sqlite3_step(st); sqlite3_finalize(st);
    return rc == SQLITE_DONE && sqlite3_changes(db_) == 1;
}

// ---------------- 个人信息 ----------------
bool Database::GetProfile(int userId, ProfileInfo& out) {
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT UserId,Account,NickName,Gender,"
        "IFNULL(StarId,0),IFNULL(BloodTypeId,0),"
        "IFNULL(Signature,''),IFNULL(AvatarPath,'') "
        "FROM Users WHERE UserId=?;",
        -1, &st, nullptr);
    sqlite3_bind_int(st, 1, userId);
    bool found = false;
    if (sqlite3_step(st) == SQLITE_ROW) {
        out.userId      = sqlite3_column_int(st, 0);
        out.account     = reinterpret_cast<const char*>(sqlite3_column_text(st, 1));
        out.nickName    = reinterpret_cast<const char*>(sqlite3_column_text(st, 2));
        out.gender      = sqlite3_column_int(st, 3);
        out.starId      = sqlite3_column_int(st, 4);
        out.bloodTypeId = sqlite3_column_int(st, 5);
        out.signature   = reinterpret_cast<const char*>(sqlite3_column_text(st, 6));
        out.avatar      = reinterpret_cast<const char*>(sqlite3_column_text(st, 7));
        found = true;
    }
    sqlite3_finalize(st);
    return found;
}

bool Database::UpdateProfile(const ProfileInfo& p) {
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "UPDATE Users SET NickName=?,Gender=?,StarId=?,"
        "BloodTypeId=?,Signature=?,AvatarPath=? WHERE UserId=?;",
        -1, &st, nullptr);
    sqlite3_bind_text(st, 1, p.nickName.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (st, 2, p.gender);
    // 星座/血型为 0 时存 NULL（未设置）
    if (p.starId > 0) sqlite3_bind_int(st, 3, p.starId); else sqlite3_bind_null(st, 3);
    if (p.bloodTypeId > 0) sqlite3_bind_int(st, 4, p.bloodTypeId); else sqlite3_bind_null(st, 4);
    sqlite3_bind_text(st, 5, p.signature.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 6, p.avatar.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (st, 7, p.userId);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE) { lastErr_ = sqlite3_errmsg(db_); return false; }
    return true;
}

// ---------------- 消息 ----------------
long long Database::SaveMessage(int senderId, int receiverId, int typeId,
                                const std::string& content) {
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "INSERT INTO Messages(SenderId,ReceiverId,TypeId,Content) VALUES(?,?,?,?);",
        -1, &st, nullptr);
    sqlite3_bind_int (st, 1, senderId);
    sqlite3_bind_int (st, 2, receiverId);
    sqlite3_bind_int (st, 3, typeId);
    sqlite3_bind_text(st, 4, content.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE) { lastErr_ = sqlite3_errmsg(db_); return 0; }
    return sqlite3_last_insert_rowid(db_);
}

bool Database::GetMessageById(long long msgId, MessageInfo& out) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT MsgId,SenderId,ReceiverId,TypeId,Content,SendTime,IsRead FROM Messages WHERE MsgId=?;",
        -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, msgId);
    bool found = false;
    if (sqlite3_step(st) == SQLITE_ROW) {
        out.msgId = sqlite3_column_int64(st, 0);
        out.senderId = sqlite3_column_int(st, 1); out.receiverId = sqlite3_column_int(st, 2);
        out.typeId = sqlite3_column_int(st, 3);
        out.content = reinterpret_cast<const char*>(sqlite3_column_text(st, 4));
        out.sendTime = reinterpret_cast<const char*>(sqlite3_column_text(st, 5));
        out.isRead = sqlite3_column_int(st, 6); found = true;
    }
    sqlite3_finalize(st); return found;
}

std::vector<MessageInfo> Database::GetConversation(int userId, int peerId,
                                                    long long beforeMsgId, int limit) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<MessageInfo> out;
    if (limit < 1) limit = 1; if (limit > 50) limit = 50;
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT MsgId,SenderId,ReceiverId,TypeId,Content,SendTime,IsRead FROM Messages "
        "WHERE TypeId=1 AND ((SenderId=? AND ReceiverId=?) OR (SenderId=? AND ReceiverId=?)) "
        "AND (?=0 OR MsgId<?) ORDER BY MsgId DESC LIMIT ?;", -1, &st, nullptr);
    sqlite3_bind_int(st, 1, userId); sqlite3_bind_int(st, 2, peerId);
    sqlite3_bind_int(st, 3, peerId); sqlite3_bind_int(st, 4, userId);
    sqlite3_bind_int64(st, 5, beforeMsgId); sqlite3_bind_int64(st, 6, beforeMsgId);
    sqlite3_bind_int(st, 7, limit);
    while (sqlite3_step(st) == SQLITE_ROW) {
        MessageInfo m;
        m.msgId = sqlite3_column_int64(st, 0); m.senderId = sqlite3_column_int(st, 1);
        m.receiverId = sqlite3_column_int(st, 2); m.typeId = sqlite3_column_int(st, 3);
        m.content = reinterpret_cast<const char*>(sqlite3_column_text(st, 4));
        m.sendTime = reinterpret_cast<const char*>(sqlite3_column_text(st, 5));
        m.isRead = sqlite3_column_int(st, 6); out.push_back(m);
    }
    sqlite3_finalize(st);
    std::reverse(out.begin(), out.end());
    return out;
}

} // namespace myqq
