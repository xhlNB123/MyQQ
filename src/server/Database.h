#pragma once
// =====================================================================
// 服务端数据访问层：封装 SQLite，向业务逻辑提供高层操作。
// 数据库文件默认 myqq.db，首次运行自动建表 + 初始化字典。
// =====================================================================

#include <string>
#include <vector>
#include <mutex>

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
    int         gender = 0;
    int         starId = 0;
    int         bloodTypeId = 0;
    std::string signature;
    std::string avatar;
};

struct MessageInfo {
    long long msgId = 0;
    int senderId = 0;
    int receiverId = 0;
    int typeId = 1;
    std::string content;
    std::string sendTime;
    int isRead = 0;
};

struct FriendRequestInfo {
    long long requestId = 0;
    int senderId = 0;
    int receiverId = 0;
    std::string senderAccount;
    std::string senderNick;
    std::string receiverNick;
    std::string verifyText;
    int status = 0;
    std::string createdTime;
    std::string handledTime;
    int resultAcknowledged = 0;
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
    bool UserExists(int userId);
    bool GetUserInfo(int userId, UserInfo& out);

    // 发送/处理好友申请。返回 0 成功；3 用户不存在；4 已是好友/已有待处理；5 服务器错误。
    int CreateFriendRequest(int senderId, int receiverId, const std::string& verify,
                            FriendRequestInfo& out);
    std::vector<FriendRequestInfo> GetPendingIncomingRequests(int receiverId);
    std::vector<FriendRequestInfo> GetUnacknowledgedOutgoingResults(int senderId);
    int ResolveFriendRequest(long long requestId, int receiverId, bool accept,
                             FriendRequestInfo& out);
    bool AcknowledgeFriendResult(long long requestId, int senderId);

    // ---------- 个人信息 ----------
    // 读取个人资料；成功 true（found 置为是否存在该用户）
    bool GetProfile(int userId, ProfileInfo& out);
    // 更新个人资料（昵称/性别/星座/血型/签名/头像）；成功 true
    bool UpdateProfile(const ProfileInfo& p);

    // ---------- 消息 ----------
    // 存一条消息，返回 msgId(>0) 或 0
    long long SaveMessage(int senderId, int receiverId, int typeId,
                          const std::string& content);
    bool GetMessageById(long long msgId, MessageInfo& out);
    std::vector<MessageInfo> GetConversation(int userId, int peerId,
                                             long long beforeMsgId, int limit);

    const std::string& LastError() const { return lastErr_; }

private:
    sqlite3*    db_ = nullptr;
    std::string lastErr_;
    std::recursive_mutex mutex_;

    bool Exec(const std::string& sql);   // 执行无结果集 SQL
};

} // namespace myqq
