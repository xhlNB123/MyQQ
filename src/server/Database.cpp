// =====================================================================
// Database.h 的实现（SQLite，参数化查询防注入）
// =====================================================================
#include "Database.h"
#include "../../third_party/sqlite/sqlite3.h"
#include "../common/Protocol.h"
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
    // 旧库迁移：列已存在会报错，忽略即可。
    sqlite3_exec(db_, "ALTER TABLE Messages ADD COLUMN FileId INTEGER;",
                 nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "ALTER TABLE Users ADD COLUMN Visibility INTEGER DEFAULT 0;",
                 nullptr, nullptr, nullptr);
    Exec("PRAGMA user_version=4;");
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

void Database::SetVisibility(int userId, int visibility) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_, "UPDATE Users SET Visibility=? WHERE UserId=?;", -1, &st, nullptr);
    sqlite3_bind_int(st, 1, visibility); sqlite3_bind_int(st, 2, userId);
    sqlite3_step(st); sqlite3_finalize(st);
}

bool Database::CanViewProfile(int viewer, int target) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (viewer == target) return true;
    if (AreFriends(viewer, target)) return true;
    // 非好友：仅当 target.Visibility==0（允许同群可见）且两人同群
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_, "SELECT IFNULL(Visibility,0) FROM Users WHERE UserId=?;", -1, &st, nullptr);
    sqlite3_bind_int(st, 1, target);
    int vis = 1;
    if (sqlite3_step(st) == SQLITE_ROW) vis = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    if (vis != 0) return false;
    return ShareAnyGroup(viewer, target);
}

// ---------------- 群聊 ----------------
long long Database::CreateGroup(int ownerId, const std::string& name, int requireApproval) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "INSERT INTO Groups(GroupName,OwnerId,RequireApproval) VALUES(?,?,?);", -1, &st, nullptr);
    sqlite3_bind_text(st, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 2, ownerId);
    sqlite3_bind_int(st, 3, requireApproval ? 1 : 0);
    int rc = sqlite3_step(st); sqlite3_finalize(st);
    if (rc != SQLITE_DONE) { lastErr_ = sqlite3_errmsg(db_); return 0; }
    long long gid = sqlite3_last_insert_rowid(db_);
    AddGroupMember(gid, ownerId, 1);   // 群主入群
    return gid;
}

bool Database::GetGroup(long long groupId, GroupInfo& out) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT g.GroupId,g.GroupName,g.OwnerId,g.RequireApproval,"
        "(SELECT COUNT(*) FROM GroupMembers m WHERE m.GroupId=g.GroupId) "
        "FROM Groups g WHERE g.GroupId=?;", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, groupId);
    bool found = false;
    if (sqlite3_step(st) == SQLITE_ROW) {
        out.groupId = sqlite3_column_int64(st, 0);
        out.name = reinterpret_cast<const char*>(sqlite3_column_text(st, 1));
        out.ownerId = sqlite3_column_int(st, 2);
        out.requireApproval = sqlite3_column_int(st, 3);
        out.memberCount = sqlite3_column_int(st, 4);
        found = true;
    }
    sqlite3_finalize(st);
    return found;
}

std::vector<GroupInfo> Database::SearchGroups(const std::string& keyword) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<GroupInfo> out;
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT g.GroupId,g.GroupName,g.OwnerId,g.RequireApproval,"
        "(SELECT COUNT(*) FROM GroupMembers m WHERE m.GroupId=g.GroupId) "
        "FROM Groups g WHERE CAST(g.GroupId AS TEXT)=? OR g.GroupName LIKE ? LIMIT 50;",
        -1, &st, nullptr);
    std::string like = "%" + keyword + "%";
    sqlite3_bind_text(st, 1, keyword.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, like.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(st) == SQLITE_ROW) {
        GroupInfo g;
        g.groupId = sqlite3_column_int64(st, 0);
        g.name = reinterpret_cast<const char*>(sqlite3_column_text(st, 1));
        g.ownerId = sqlite3_column_int(st, 2);
        g.requireApproval = sqlite3_column_int(st, 3);
        g.memberCount = sqlite3_column_int(st, 4);
        out.push_back(g);
    }
    sqlite3_finalize(st);
    return out;
}

std::vector<GroupInfo> Database::GetMyGroups(int userId) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<GroupInfo> out;
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT g.GroupId,g.GroupName,g.OwnerId,g.RequireApproval,"
        "(SELECT COUNT(*) FROM GroupMembers m WHERE m.GroupId=g.GroupId),mm.Role "
        "FROM Groups g JOIN GroupMembers mm ON mm.GroupId=g.GroupId "
        "WHERE mm.UserId=? ORDER BY g.GroupId;", -1, &st, nullptr);
    sqlite3_bind_int(st, 1, userId);
    while (sqlite3_step(st) == SQLITE_ROW) {
        GroupInfo g;
        g.groupId = sqlite3_column_int64(st, 0);
        g.name = reinterpret_cast<const char*>(sqlite3_column_text(st, 1));
        g.ownerId = sqlite3_column_int(st, 2);
        g.requireApproval = sqlite3_column_int(st, 3);
        g.memberCount = sqlite3_column_int(st, 4);
        g.myRole = sqlite3_column_int(st, 5);
        out.push_back(g);
    }
    sqlite3_finalize(st);
    return out;
}

std::vector<GroupMemberInfo> Database::GetGroupMembers(long long groupId) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<GroupMemberInfo> out;
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT m.UserId,u.NickName,m.Role FROM GroupMembers m "
        "JOIN Users u ON u.UserId=m.UserId WHERE m.GroupId=? ORDER BY m.Role DESC,m.UserId;",
        -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, groupId);
    while (sqlite3_step(st) == SQLITE_ROW) {
        GroupMemberInfo m;
        m.userId = sqlite3_column_int(st, 0);
        m.nickName = reinterpret_cast<const char*>(sqlite3_column_text(st, 1));
        m.role = sqlite3_column_int(st, 2);
        out.push_back(m);
    }
    sqlite3_finalize(st);
    return out;
}

bool Database::IsGroupMember(int userId, long long groupId) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_, "SELECT 1 FROM GroupMembers WHERE UserId=? AND GroupId=?;", -1, &st, nullptr);
    sqlite3_bind_int(st, 1, userId); sqlite3_bind_int64(st, 2, groupId);
    bool yes = sqlite3_step(st) == SQLITE_ROW;
    sqlite3_finalize(st);
    return yes;
}

bool Database::AddGroupMember(long long groupId, int userId, int role) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "INSERT OR IGNORE INTO GroupMembers(GroupId,UserId,Role) VALUES(?,?,?);", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, groupId); sqlite3_bind_int(st, 2, userId); sqlite3_bind_int(st, 3, role);
    int rc = sqlite3_step(st); sqlite3_finalize(st);
    return rc == SQLITE_DONE;
}

bool Database::ShareAnyGroup(int a, int b) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT 1 FROM GroupMembers x JOIN GroupMembers y ON x.GroupId=y.GroupId "
        "WHERE x.UserId=? AND y.UserId=? LIMIT 1;", -1, &st, nullptr);
    sqlite3_bind_int(st, 1, a); sqlite3_bind_int(st, 2, b);
    bool yes = sqlite3_step(st) == SQLITE_ROW;
    sqlite3_finalize(st);
    return yes;
}

// ---------------- 文件/图片 ----------------
long long Database::CreateFileRecord(int ownerId, const std::string& fileName,
                                     long long fileSize, int kind,
                                     const std::string& storePath) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "INSERT INTO Files(OwnerId,FileName,FileSize,Kind,StorePath) VALUES(?,?,?,?,?);",
        -1, &st, nullptr);
    sqlite3_bind_int  (st, 1, ownerId);
    sqlite3_bind_text (st, 2, fileName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 3, fileSize);
    sqlite3_bind_int  (st, 4, kind);
    sqlite3_bind_text (st, 5, storePath.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE) { lastErr_ = sqlite3_errmsg(db_); return 0; }
    return sqlite3_last_insert_rowid(db_);
}

bool Database::GetFileRecord(long long fileId, FileRecord& out) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT FileId,OwnerId,FileName,FileSize,Kind,StorePath FROM Files WHERE FileId=?;",
        -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, fileId);
    bool found = false;
    if (sqlite3_step(st) == SQLITE_ROW) {
        out.fileId = sqlite3_column_int64(st, 0);
        out.ownerId = sqlite3_column_int(st, 1);
        out.fileName = reinterpret_cast<const char*>(sqlite3_column_text(st, 2));
        out.fileSize = sqlite3_column_int64(st, 3);
        out.kind = sqlite3_column_int(st, 4);
        out.storePath = reinterpret_cast<const char*>(sqlite3_column_text(st, 5));
        found = true;
    }
    sqlite3_finalize(st);
    return found;
}

long long Database::SaveFileMessage(int senderId, int receiverId, int kind,
                                    long long fileId, const std::string& caption) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    int typeId = (kind == kKindImage) ? 5 : 6;
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "INSERT INTO Messages(SenderId,ReceiverId,TypeId,Content,FileId) VALUES(?,?,?,?,?);",
        -1, &st, nullptr);
    sqlite3_bind_int  (st, 1, senderId);
    sqlite3_bind_int  (st, 2, receiverId);
    sqlite3_bind_int  (st, 3, typeId);
    sqlite3_bind_text (st, 4, caption.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 5, fileId);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE) { lastErr_ = sqlite3_errmsg(db_); return 0; }
    return sqlite3_last_insert_rowid(db_);
}

// ---------------- 消息 ----------------
long long Database::SaveMessage(int senderId, int receiverId, int typeId,
                                const std::string& content) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
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

// TypeId -> kind：5 图片=1，6 文件=2，其它=0 文本
static int TypeIdToKind(int typeId) {
    if (typeId == 5) return kKindImage;
    if (typeId == 6) return kKindFile;
    return kKindText;
}

// 读取一行 Messages+Files 联表结果到 MessageInfo（列顺序见 SQL）
static void ReadMessageRow(sqlite3_stmt* st, MessageInfo& m) {
    m.msgId = sqlite3_column_int64(st, 0);
    m.senderId = sqlite3_column_int(st, 1);
    m.receiverId = sqlite3_column_int(st, 2);
    m.typeId = sqlite3_column_int(st, 3);
    const unsigned char* c = sqlite3_column_text(st, 4);
    m.content = c ? reinterpret_cast<const char*>(c) : "";
    c = sqlite3_column_text(st, 5);
    m.sendTime = c ? reinterpret_cast<const char*>(c) : "";
    m.isRead = sqlite3_column_int(st, 6);
    m.fileId = sqlite3_column_int64(st, 7);
    c = sqlite3_column_text(st, 8);
    m.fileName = c ? reinterpret_cast<const char*>(c) : "";
    m.fileSize = sqlite3_column_int64(st, 9);
    m.kind = TypeIdToKind(m.typeId);
}

static const char* kMsgSelect =
    "SELECT m.MsgId,m.SenderId,m.ReceiverId,m.TypeId,m.Content,m.SendTime,m.IsRead,"
    "IFNULL(m.FileId,0),IFNULL(f.FileName,''),IFNULL(f.FileSize,0) "
    "FROM Messages m LEFT JOIN Files f ON f.FileId=m.FileId ";

bool Database::GetMessageById(long long msgId, MessageInfo& out) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    sqlite3_stmt* st = nullptr;
    std::string sql = std::string(kMsgSelect) + "WHERE m.MsgId=?;";
    sqlite3_prepare_v2(db_, sql.c_str(), -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, msgId);
    bool found = false;
    if (sqlite3_step(st) == SQLITE_ROW) { ReadMessageRow(st, out); found = true; }
    sqlite3_finalize(st); return found;
}

std::vector<MessageInfo> Database::GetConversation(int userId, int peerId,
                                                    long long beforeMsgId, int limit) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<MessageInfo> out;
    if (limit < 1) limit = 1; if (limit > 50) limit = 50;
    sqlite3_stmt* st = nullptr;
    std::string sql = std::string(kMsgSelect) +
        "WHERE m.TypeId IN (1,5,6) AND ((m.SenderId=? AND m.ReceiverId=?) OR (m.SenderId=? AND m.ReceiverId=?)) "
        "AND (?=0 OR m.MsgId<?) ORDER BY m.MsgId DESC LIMIT ?;";
    sqlite3_prepare_v2(db_, sql.c_str(), -1, &st, nullptr);
    sqlite3_bind_int(st, 1, userId); sqlite3_bind_int(st, 2, peerId);
    sqlite3_bind_int(st, 3, peerId); sqlite3_bind_int(st, 4, userId);
    sqlite3_bind_int64(st, 5, beforeMsgId); sqlite3_bind_int64(st, 6, beforeMsgId);
    sqlite3_bind_int(st, 7, limit);
    while (sqlite3_step(st) == SQLITE_ROW) {
        MessageInfo m;
        ReadMessageRow(st, m);
        out.push_back(m);
    }
    sqlite3_finalize(st);
    std::reverse(out.begin(), out.end());
    return out;
}

// ---------------- 群消息 ----------------
long long Database::SaveGroupMessage(long long groupId, int senderId, int typeId,
                                     const std::string& content, long long fileId) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "INSERT INTO GroupMessages(GroupId,SenderId,TypeId,Content,FileId) VALUES(?,?,?,?,?);",
        -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, groupId);
    sqlite3_bind_int  (st, 2, senderId);
    sqlite3_bind_int  (st, 3, typeId);
    sqlite3_bind_text (st, 4, content.c_str(), -1, SQLITE_TRANSIENT);
    if (fileId > 0) sqlite3_bind_int64(st, 5, fileId); else sqlite3_bind_null(st, 5);
    int rc = sqlite3_step(st); sqlite3_finalize(st);
    if (rc != SQLITE_DONE) { lastErr_ = sqlite3_errmsg(db_); return 0; }
    return sqlite3_last_insert_rowid(db_);
}

static const char* kGMsgSelect =
    "SELECT gm.MsgId,gm.GroupId,gm.SenderId,u.NickName,gm.TypeId,gm.Content,gm.SendTime,"
    "IFNULL(gm.FileId,0),IFNULL(f.FileName,''),IFNULL(f.FileSize,0) "
    "FROM GroupMessages gm JOIN Users u ON u.UserId=gm.SenderId "
    "LEFT JOIN Files f ON f.FileId=gm.FileId ";

static void ReadGroupMsgRow(sqlite3_stmt* st, GroupMessageInfo& m) {
    m.msgId = sqlite3_column_int64(st, 0);
    m.groupId = sqlite3_column_int64(st, 1);
    m.senderId = sqlite3_column_int(st, 2);
    m.senderNick = reinterpret_cast<const char*>(sqlite3_column_text(st, 3));
    m.typeId = sqlite3_column_int(st, 4);
    m.content = reinterpret_cast<const char*>(sqlite3_column_text(st, 5));
    m.sendTime = reinterpret_cast<const char*>(sqlite3_column_text(st, 6));
    m.fileId = sqlite3_column_int64(st, 7);
    const unsigned char* c = sqlite3_column_text(st, 8);
    m.fileName = c ? reinterpret_cast<const char*>(c) : "";
    m.fileSize = sqlite3_column_int64(st, 9);
    m.kind = (m.typeId == 5) ? 1 : (m.typeId == 6) ? 2 : 0;
}

bool Database::GetGroupMessageById(long long msgId, GroupMessageInfo& out) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    sqlite3_stmt* st = nullptr;
    std::string sql = std::string(kGMsgSelect) + "WHERE gm.MsgId=?;";
    sqlite3_prepare_v2(db_, sql.c_str(), -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, msgId);
    bool found = false;
    if (sqlite3_step(st) == SQLITE_ROW) { ReadGroupMsgRow(st, out); found = true; }
    sqlite3_finalize(st);
    return found;
}

std::vector<GroupMessageInfo> Database::GetGroupConversation(long long groupId,
                              long long beforeMsgId, int limit) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<GroupMessageInfo> out;
    if (limit < 1) limit = 1; if (limit > 50) limit = 50;
    sqlite3_stmt* st = nullptr;
    std::string sql = std::string(kGMsgSelect) +
        "WHERE gm.GroupId=? AND (?=0 OR gm.MsgId<?) ORDER BY gm.MsgId DESC LIMIT ?;";
    sqlite3_prepare_v2(db_, sql.c_str(), -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, groupId);
    sqlite3_bind_int64(st, 2, beforeMsgId); sqlite3_bind_int64(st, 3, beforeMsgId);
    sqlite3_bind_int(st, 4, limit);
    while (sqlite3_step(st) == SQLITE_ROW) { GroupMessageInfo m; ReadGroupMsgRow(st, m); out.push_back(m); }
    sqlite3_finalize(st);
    std::reverse(out.begin(), out.end());
    return out;
}

// ---------------- 入群状态机 ----------------
// 读取 GroupRequests 联表结果（列顺序见各 SQL）
static void ReadGroupReq(sqlite3_stmt* st, GroupReqInfo& r) {
    r.reqId = sqlite3_column_int64(st, 0);
    r.groupId = sqlite3_column_int64(st, 1);
    r.targetId = sqlite3_column_int(st, 2);
    r.inviterId = sqlite3_column_int(st, 3);
    r.needInviteeOk = sqlite3_column_int(st, 4);
    r.needOwnerOk = sqlite3_column_int(st, 5);
    r.inviteeOk = sqlite3_column_int(st, 6);
    r.ownerOk = sqlite3_column_int(st, 7);
    r.status = sqlite3_column_int(st, 8);
    const unsigned char* c = sqlite3_column_text(st, 9);
    r.groupName = c ? reinterpret_cast<const char*>(c) : "";
    c = sqlite3_column_text(st, 10); r.targetNick = c ? reinterpret_cast<const char*>(c) : "";
    c = sqlite3_column_text(st, 11); r.inviterNick = c ? reinterpret_cast<const char*>(c) : "";
}
static const char* kReqSelect =
    "SELECT r.ReqId,r.GroupId,r.TargetId,r.InviterId,r.NeedInviteeOk,r.NeedOwnerOk,"
    "r.InviteeOk,r.OwnerOk,r.Status,g.GroupName,tu.NickName,IFNULL(iu.NickName,'') "
    "FROM GroupRequests r JOIN Groups g ON g.GroupId=r.GroupId "
    "JOIN Users tu ON tu.UserId=r.TargetId LEFT JOIN Users iu ON iu.UserId=r.InviterId ";

// 自己按群号申请
int Database::CreateGroupApply(int userId, long long groupId, GroupReqInfo& out) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    GroupInfo g;
    if (!GetGroup(groupId, g)) return 3;
    if (IsGroupMember(userId, groupId)) return 4;
    int needOwner = g.requireApproval ? 1 : 0;
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "INSERT INTO GroupRequests(GroupId,TargetId,InviterId,NeedInviteeOk,NeedOwnerOk,InviteeOk,OwnerOk) "
        "VALUES(?,?,0,0,?,1,?);", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, groupId); sqlite3_bind_int(st, 2, userId);
    sqlite3_bind_int(st, 3, needOwner); sqlite3_bind_int(st, 4, needOwner ? 0 : 1);
    int rc = sqlite3_step(st); sqlite3_finalize(st);
    if (rc != SQLITE_DONE) { lastErr_ = sqlite3_errmsg(db_); return 5; }
    long long reqId = sqlite3_last_insert_rowid(db_);
    if (!needOwner) { AddGroupMember(groupId, userId, 0);
        sqlite3_prepare_v2(db_, "UPDATE GroupRequests SET Status=1,HandledTime=datetime('now','localtime') WHERE ReqId=?;", -1, &st, nullptr);
        sqlite3_bind_int64(st, 1, reqId); sqlite3_step(st); sqlite3_finalize(st);
    }
    out.reqId = reqId; out.groupId = groupId; out.targetId = userId;
    out.needOwnerOk = needOwner; out.status = needOwner ? 0 : 1; out.groupName = g.name;
    return 0;
}

// 成员邀请好友（需被邀请人同意；若群需审批还需群主同意）
int Database::CreateGroupInvite(int inviterId, long long groupId, int friendId, GroupReqInfo& out) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    GroupInfo g;
    if (!GetGroup(groupId, g)) return 3;
    if (!IsGroupMember(inviterId, groupId)) return 2;
    if (IsGroupMember(friendId, groupId)) return 4;
    int needOwner = g.requireApproval ? 1 : 0;
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_,
        "INSERT INTO GroupRequests(GroupId,TargetId,InviterId,NeedInviteeOk,NeedOwnerOk,InviteeOk,OwnerOk) "
        "VALUES(?,?,?,1,?,0,?);", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, groupId); sqlite3_bind_int(st, 2, friendId);
    sqlite3_bind_int(st, 3, inviterId); sqlite3_bind_int(st, 4, needOwner);
    sqlite3_bind_int(st, 5, needOwner ? 0 : 1);
    int rc = sqlite3_step(st); sqlite3_finalize(st);
    if (rc != SQLITE_DONE) { lastErr_ = sqlite3_errmsg(db_); return 5; }
    out.reqId = sqlite3_last_insert_rowid(db_); out.groupId = groupId; out.targetId = friendId;
    out.inviterId = inviterId; out.needInviteeOk = 1; out.needOwnerOk = needOwner; out.status = 0;
    out.groupName = g.name;
    return 0;
}

// 内部：读一条 request（加锁前提由调用方持有）
static bool LoadReq(sqlite3* db, long long reqId, GroupReqInfo& r) {
    sqlite3_stmt* st = nullptr;
    std::string sql = std::string(kReqSelect) + "WHERE r.ReqId=?;";
    sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, reqId);
    bool found = false;
    if (sqlite3_step(st) == SQLITE_ROW) { ReadGroupReq(st, r); found = true; }
    sqlite3_finalize(st);
    return found;
}

// 完成入群（内部）：加成员 + Status=1
void Database::finalizeJoin(long long reqId, long long groupId, int targetId) {
    AddGroupMember(groupId, targetId, 0);
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_, "UPDATE GroupRequests SET Status=1,HandledTime=datetime('now','localtime') WHERE ReqId=?;", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, reqId); sqlite3_step(st); sqlite3_finalize(st);
}

int Database::ResolveInvite(long long reqId, int targetId, bool accept, GroupReqInfo& out) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!LoadReq(db_, reqId, out)) return 3;
    if (out.targetId != targetId || out.status != 0 || out.inviteeOk) return 4;
    if (!accept) {
        sqlite3_stmt* st = nullptr;
        sqlite3_prepare_v2(db_, "UPDATE GroupRequests SET Status=2,HandledTime=datetime('now','localtime') WHERE ReqId=?;", -1, &st, nullptr);
        sqlite3_bind_int64(st, 1, reqId); sqlite3_step(st); sqlite3_finalize(st);
        out.status = 2; return 0;
    }
    // 同意：标记 InviteeOk，若还需群主审批则留待审批，否则入群
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_, "UPDATE GroupRequests SET InviteeOk=1 WHERE ReqId=?;", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, reqId); sqlite3_step(st); sqlite3_finalize(st);
    out.inviteeOk = 1;
    if (out.needOwnerOk && !out.ownerOk) { out.status = 0; return 0; }  // 转待群主审批
    finalizeJoin(reqId, out.groupId, out.targetId); out.status = 1;
    return 0;
}

int Database::ResolveApprove(long long reqId, int ownerId, bool accept, GroupReqInfo& out) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!LoadReq(db_, reqId, out)) return 3;
    GroupInfo g;
    if (!GetGroup(out.groupId, g) || g.ownerId != ownerId) return 2;   // 非群主
    if (out.status != 0 || out.ownerOk) return 4;
    if (!accept) {
        sqlite3_stmt* st = nullptr;
        sqlite3_prepare_v2(db_, "UPDATE GroupRequests SET Status=2,HandledTime=datetime('now','localtime') WHERE ReqId=?;", -1, &st, nullptr);
        sqlite3_bind_int64(st, 1, reqId); sqlite3_step(st); sqlite3_finalize(st);
        out.status = 2; return 0;
    }
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_, "UPDATE GroupRequests SET OwnerOk=1 WHERE ReqId=?;", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, reqId); sqlite3_step(st); sqlite3_finalize(st);
    out.ownerOk = 1;
    if (out.needInviteeOk && !out.inviteeOk) { out.status = 0; return 0; }  // 还等被邀请人
    finalizeJoin(reqId, out.groupId, out.targetId); out.status = 1;
    return 0;
}

// 待我确认的邀请：TargetId=我，InviteeOk=0，Status=0
std::vector<GroupReqInfo> Database::GetPendingInvites(int targetId) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<GroupReqInfo> out;
    sqlite3_stmt* st = nullptr;
    std::string sql = std::string(kReqSelect) +
        "WHERE r.TargetId=? AND r.InviterId>0 AND r.InviteeOk=0 AND r.Status=0 ORDER BY r.ReqId;";
    sqlite3_prepare_v2(db_, sql.c_str(), -1, &st, nullptr);
    sqlite3_bind_int(st, 1, targetId);
    while (sqlite3_step(st) == SQLITE_ROW) { GroupReqInfo r; ReadGroupReq(st, r); out.push_back(r); }
    sqlite3_finalize(st);
    return out;
}

// 待我审批（我是群主）：该群 owner=我，OwnerOk=0，Status=0，且（自己申请 或 邀请已被同意）
std::vector<GroupReqInfo> Database::GetPendingApprovals(int ownerId) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<GroupReqInfo> out;
    sqlite3_stmt* st = nullptr;
    std::string sql = std::string(kReqSelect) +
        "WHERE g.OwnerId=? AND r.NeedOwnerOk=1 AND r.OwnerOk=0 AND r.Status=0 "
        "AND (r.InviterId=0 OR r.InviteeOk=1) ORDER BY r.ReqId;";
    sqlite3_prepare_v2(db_, sql.c_str(), -1, &st, nullptr);
    sqlite3_bind_int(st, 1, ownerId);
    while (sqlite3_step(st) == SQLITE_ROW) { GroupReqInfo r; ReadGroupReq(st, r); out.push_back(r); }
    sqlite3_finalize(st);
    return out;
}

// 我未读的结果：TargetId=我，Status<>0，ResultAck=0
std::vector<GroupReqInfo> Database::GetUnackedGroupResults(int userId) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<GroupReqInfo> out;
    sqlite3_stmt* st = nullptr;
    std::string sql = std::string(kReqSelect) +
        "WHERE r.TargetId=? AND r.Status<>0 AND r.ResultAck=0 ORDER BY r.ReqId;";
    sqlite3_prepare_v2(db_, sql.c_str(), -1, &st, nullptr);
    sqlite3_bind_int(st, 1, userId);
    while (sqlite3_step(st) == SQLITE_ROW) { GroupReqInfo r; ReadGroupReq(st, r); out.push_back(r); }
    sqlite3_finalize(st);
    return out;
}

bool Database::AckGroupResult(long long reqId, int userId) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_, "UPDATE GroupRequests SET ResultAck=1 WHERE ReqId=? AND TargetId=?;", -1, &st, nullptr);
    sqlite3_bind_int64(st, 1, reqId); sqlite3_bind_int(st, 2, userId);
    int rc = sqlite3_step(st); sqlite3_finalize(st);
    return rc == SQLITE_DONE && sqlite3_changes(db_) == 1;
}

} // namespace myqq
