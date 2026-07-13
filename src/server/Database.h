#pragma once
// =====================================================================
// 服务端数据访问层：封装 SQLite，向业务逻辑提供高层操作。
// 数据库文件默认 myqq.db，首次运行自动建表 + 初始化字典。
// =====================================================================

#include <string>
#include <vector>

struct sqlite3;   // 前向声明，避免头文件暴露 sqlite3.h

namespace myqq {

// 好友/查询结果的简单数据结构
struct UserInfo {
    int         userId = 0;
    std::string account;
    std::string nickName;
    int         status = 0;   // 0 离线 1 在线
};

// 完整个人资料（个人设置窗体读写）
struct ProfileInfo {
    int         userId = 0;
    std::string account;          // 只读
    std::string nickName;
    int         gender = 0;        // 0 未知 1 男 2 女
    int         starId = 0;        // 星座 1..12，0 未设置
    int         bloodTypeId = 0;   // 血型 1..5，0 未设置
    std::string signature;         // 个性签名
    std::string avatar;            // 头像标识/路径
};

class Database {
public:
    Database();
    ~Database();

    // 打开数据库并执行建表脚本（schemaSql 为脚本内容）
    bool Open(const std::string& dbPath, const std::string& schemaSql);
    void Close();

    // ---------- 账号 ----------
    // 注册：成功返回分配的 userId(>0)，账号已存在返回 -1，出错返回 0
    int  Register(const std::string& account, const std::string& password,
                  const std::string& nickName);
    // 登录：成功返回 userId(>0)，账号不存在/密码错返回 0
    int  Login(const std::string& account, const std::string& password);
    void SetStatus(int userId, int status);

    // ---------- 好友 ----------
    // 按账号或昵称模糊查找（不含自己）
    std::vector<UserInfo> SearchUsers(const std::string& keyword, int selfId);
    // 双向加好友；成功 true
    bool AddFriend(int userId, int friendId);
    // 好友列表
    std::vector<UserInfo> GetFriends(int userId);
    bool AreFriends(int a, int b);

    // ---------- 个人信息 ----------
    // 读取个人资料；成功 true（found 置为是否存在该用户）
    bool GetProfile(int userId, ProfileInfo& out);
    // 更新个人资料（昵称/性别/星座/血型/签名/头像）；成功 true
    bool UpdateProfile(const ProfileInfo& p);

    // ---------- 消息 ----------
    // 存一条消息，返回 msgId(>0) 或 0
    long long SaveMessage(int senderId, int receiverId, int typeId,
                          const std::string& content);

    const std::string& LastError() const { return lastErr_; }

private:
    sqlite3*    db_ = nullptr;
    std::string lastErr_;

    bool Exec(const std::string& sql);   // 执行无结果集 SQL
};

} // namespace myqq
