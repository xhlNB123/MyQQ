// =====================================================================
// Database.h 的实现（SQLite，参数化查询防注入）
// =====================================================================
#include "Database.h"
#include "../../third_party/sqlite/sqlite3.h"

namespace myqq {

Database::Database() {}
Database::~Database() { Close(); }

bool Database::Open(const std::string& dbPath, const std::string& schemaSql) {
    if (sqlite3_open(dbPath.c_str(), &db_) != SQLITE_OK) {
        lastErr_ = sqlite3_errmsg(db_);
        return false;
    }
    sqlite3_exec(db_, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);
    // 执行建表 + 字典初始化脚本
    return Exec(schemaSql);
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
        "INSERT INTO Users(Account,Password,NickName) VALUES(?,?,?);",
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

} // namespace myqq
