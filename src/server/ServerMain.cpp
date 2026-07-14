// =====================================================================
// MyQQ 服务端（控制台版）—— 局域网聊天服务器
// 职责：监听端口、接受客户端、解析协议、读写 SQLite、在线消息转发。
// 每个客户端一个线程；登录后在 SessionManager 登记，聊天消息实时推送。
// 编译：一并加入 ../common/*.cpp、Database.cpp、SessionManager.cpp、
//       ../../third_party/sqlite/sqlite3.c
// =====================================================================
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <filesystem>

#include "../common/Socket.h"
#include "../common/PathUtils.h"
#include "../common/Message.h"
#include "../common/Protocol.h"
#include "Database.h"
#include "SessionManager.h"

using namespace myqq;

static Database       g_db;
static SessionManager g_sessions;

// 读取整个文件内容（建表脚本）
static std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return "";
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

static bool DecodeField(const std::vector<std::string>& t, size_t index,
                        std::string& value, size_t maxBytes) {
    return index < t.size() && DecodeWireText(t[index], value) && value.size() <= maxBytes;
}

static std::string AuthError(const char* response) {
    return Pack(response, {"2", EncodeWireText("请先登录")});
}

// 处理单条请求。curUser：本连接已登录的 userId（0 表示未登录，可被回写）。
static std::string HandleRequest(const std::string& line, TcpSocket* conn, int& curUser) {
    if (line.size() > kMaxPacketSize) return Pack("ERR", {"1", EncodeWireText("报文过长")});
    auto t = Unpack(line);
    if (t.empty()) return Pack("ERR", {"1", EncodeWireText("空报文")});
    Cmd cmd = StrToCmd(t[0]);

    switch (cmd) {
        case Cmd::kRegister: {
            std::string account, password, nick;
            if (!DecodeField(t, 1, account, 50) || !DecodeField(t, 2, password, 128)
                || !DecodeField(t, 3, nick, 50) || account.empty() || password.empty())
                return Pack("REGISTER_RESP", {"1", EncodeWireText("参数错误")});
            int id = g_db.Register(account, password, nick);
            if (id > 0) return Pack("REGISTER_RESP", {"0", std::to_string(id)});
            if (id == -1) return Pack("REGISTER_RESP", {"4", EncodeWireText("账号已存在")});
            return Pack("REGISTER_RESP", {"5", EncodeWireText(g_db.LastError())});
        }
        case Cmd::kLogin: {
            if (curUser != 0) return Pack("LOGIN_RESP", {"1", EncodeWireText("请先退出当前账号")});
            std::string account, password;
            if (!DecodeField(t, 1, account, 50) || !DecodeField(t, 2, password, 128))
                return Pack("LOGIN_RESP", {"1", EncodeWireText("参数错误")});
            int id = g_db.Login(account, password);
            if (id <= 0) return Pack("LOGIN_RESP", {"2", EncodeWireText("账号或密码错误")});
            curUser = id; g_db.SetStatus(id, 1); g_sessions.Bind(id, conn);
            std::cout << "[server] user " << id << " 登录\n";
            return Pack("LOGIN_RESP", {"0", std::to_string(id)});
        }
        case Cmd::kLogout: {
            if (curUser == 0) return Pack("LOGOUT_RESP", {"0"});
            int old = curUser;
            if (g_sessions.Remove(old, conn)) g_db.SetStatus(old, 0);
            curUser = 0;
            return Pack("LOGOUT_RESP", {"0"});
        }
        case Cmd::kSearchUser: {
            if (!curUser) return AuthError("SEARCH_RESP");
            std::string keyword;
            if (!DecodeField(t, 1, keyword, 50)) return Pack("SEARCH_RESP", {"1"});
            auto users = g_db.SearchUsers(keyword, curUser);
            std::vector<std::string> args = {"0", std::to_string(users.size())};
            for (auto& u : users) {
                args.push_back(std::to_string(u.userId)); args.push_back(EncodeWireText(u.account));
                args.push_back(EncodeWireText(u.nickName)); args.push_back(std::to_string(u.status));
            }
            return Pack("SEARCH_RESP", args);
        }
        case Cmd::kFriendList: {
            if (!curUser) return AuthError("FRIEND_LIST_RESP");
            auto fs = g_db.GetFriends(curUser);
            std::vector<std::string> args = {"0", std::to_string(fs.size())};
            for (auto& u : fs) {
                args.push_back(std::to_string(u.userId)); args.push_back(EncodeWireText(u.account));
                args.push_back(EncodeWireText(u.nickName)); args.push_back(std::to_string(u.status));
            }
            return Pack("FRIEND_LIST_RESP", args);
        }
        case Cmd::kAddFriend: {
            if (!curUser) return AuthError("ADD_FRIEND_RESP");
            std::string verify;
            if (t.size() < 3 || !DecodeField(t, 2, verify, 200))
                return Pack("ADD_FRIEND_RESP", {"1", "0", "invalid"});
            FriendRequestInfo r;
            int status = g_db.CreateFriendRequest(curUser, atoi(t[1].c_str()), verify, r);
            if (status == 0) {
                g_sessions.PushTo(r.receiverId, Pack("FRIEND_REQUEST_PUSH", {
                    std::to_string(r.requestId), std::to_string(r.senderId),
                    EncodeWireText(r.senderAccount), EncodeWireText(r.senderNick),
                    EncodeWireText(r.verifyText), EncodeWireText(r.createdTime)}));
            }
            return Pack("ADD_FRIEND_RESP", {std::to_string(status), std::to_string(r.requestId), status == 0 ? "pending" : "failed"});
        }
        case Cmd::kFriendReqAck: {
            if (!curUser) return AuthError("FRIEND_ACK_RESP");
            if (t.size() < 3) return Pack("FRIEND_ACK_RESP", {"1"});
            FriendRequestInfo r;
            bool accept = atoi(t[2].c_str()) != 0;
            int status = g_db.ResolveFriendRequest(_atoi64(t[1].c_str()), curUser, accept, r);
            if (status == 0) {
                g_sessions.PushTo(r.senderId, Pack("FRIEND_RESULT_PUSH", {
                    std::to_string(r.requestId), std::to_string(r.receiverId),
                    EncodeWireText(r.receiverNick), accept ? "1" : "2", EncodeWireText(r.handledTime)}));
            }
            return Pack("FRIEND_ACK_RESP", {std::to_string(status), t[1], accept ? "1" : "2"});
        }
        case Cmd::kFriendSync: {
            if (!curUser) return AuthError("FRIEND_SYNC_RESP");
            auto incoming = g_db.GetPendingIncomingRequests(curUser);
            auto results = g_db.GetUnacknowledgedOutgoingResults(curUser);
            for (auto& r : incoming) conn->SendLine(Pack("FRIEND_REQUEST_PUSH", {
                std::to_string(r.requestId), std::to_string(r.senderId), EncodeWireText(r.senderAccount),
                EncodeWireText(r.senderNick), EncodeWireText(r.verifyText), EncodeWireText(r.createdTime)}));
            for (auto& r : results) conn->SendLine(Pack("FRIEND_RESULT_PUSH", {
                std::to_string(r.requestId), std::to_string(r.receiverId), EncodeWireText(r.receiverNick),
                std::to_string(r.status), EncodeWireText(r.handledTime)}));
            return Pack("FRIEND_SYNC_RESP", {"0", std::to_string(incoming.size()), std::to_string(results.size())});
        }
        case Cmd::kFriendResultSeen:
            if (!curUser) return AuthError("FRIEND_RESULT_SEEN_RESP");
            return Pack("FRIEND_RESULT_SEEN_RESP", {
                g_db.AcknowledgeFriendResult(t.size() > 1 ? _atoi64(t[1].c_str()) : 0, curUser) ? "0" : "3",
                t.size() > 1 ? t[1] : "0"});
        case Cmd::kChat: {
            if (!curUser) return AuthError("CHAT_ACK");
            std::string content;
            std::string clientId = t.size() > 2 ? t[2] : "0";
            int to = t.size() > 1 ? atoi(t[1].c_str()) : 0;
            if (!DecodeField(t, 3, content, 2000) || content.empty() || !g_db.AreFriends(curUser, to))
                return Pack("CHAT_ACK", {"1", clientId, EncodeWireText("只能向好友发送非空消息")});
            long long id = g_db.SaveMessage(curUser, to, 1, content);
            MessageInfo m;
            if (!id || !g_db.GetMessageById(id, m)) return Pack("CHAT_ACK", {"5", clientId, EncodeWireText(g_db.LastError())});
            g_sessions.PushTo(to, Pack("CHAT_PUSH", {std::to_string(m.msgId), std::to_string(curUser),
                std::to_string(to), EncodeWireText(m.sendTime), EncodeWireText(content)}));
            return Pack("CHAT_ACK", {"0", clientId, std::to_string(m.msgId), EncodeWireText(m.sendTime)});
        }
        case Cmd::kChatHistory: {
            if (!curUser) return AuthError("CHAT_HISTORY_BEGIN");
            if (t.size() < 5) return Pack("CHAT_HISTORY_BEGIN", {"1", "0", "0", "0"});
            int peer = atoi(t[1].c_str()); long long before = _atoi64(t[2].c_str());
            int limit = atoi(t[3].c_str()); std::string requestId = t[4];
            if (!g_db.AreFriends(curUser, peer)) return Pack("CHAT_HISTORY_BEGIN", {"3", requestId, std::to_string(peer), "0"});
            auto messages = g_db.GetConversation(curUser, peer, before, limit);
            conn->SendLine(Pack("CHAT_HISTORY_BEGIN", {"0", requestId, std::to_string(peer), std::to_string(messages.size())}));
            for (auto& m : messages) conn->SendLine(Pack("CHAT_HISTORY_ITEM", {requestId,
                std::to_string(m.msgId), std::to_string(m.senderId), std::to_string(m.receiverId),
                EncodeWireText(m.sendTime), EncodeWireText(m.content)}));
            bool more = !messages.empty() && static_cast<int>(messages.size()) >= (limit > 50 ? 50 : limit);
            return Pack("CHAT_HISTORY_END", {requestId, more ? "1" : "0",
                messages.empty() ? "0" : std::to_string(messages.front().msgId)});
        }
        case Cmd::kGetProfile: {
            if (!curUser) return AuthError("GET_PROFILE_RESP");
            ProfileInfo p;
            if (!g_db.GetProfile(curUser, p)) return Pack("GET_PROFILE_RESP", {"3", EncodeWireText("用户不存在")});
            return Pack("GET_PROFILE_RESP", {"0", std::to_string(p.userId), EncodeWireText(p.account),
                EncodeWireText(p.nickName), std::to_string(p.gender), std::to_string(p.starId),
                std::to_string(p.bloodTypeId), EncodeWireText(p.signature), EncodeWireText(p.avatar)});
        }
        case Cmd::kUpdateProfile: {
            if (!curUser) return AuthError("UPDATE_PROFILE_RESP");
            ProfileInfo p; p.userId = curUser;
            if (t.size() < 7 || !DecodeField(t, 1, p.nickName, 50) || !DecodeField(t, 5, p.signature, 200)
                || !DecodeField(t, 6, p.avatar, 100)) return Pack("UPDATE_PROFILE_RESP", {"1"});
            p.gender = atoi(t[2].c_str()); p.starId = atoi(t[3].c_str()); p.bloodTypeId = atoi(t[4].c_str());
            bool ok = g_db.UpdateProfile(p);
            return Pack("UPDATE_PROFILE_RESP", {ok ? "0" : "5", ok ? "" : EncodeWireText(g_db.LastError())});
        }
        case Cmd::kVersion: return Pack("VERSION_RESP", {kAppVersion});
        case Cmd::kHeartbeat: return Pack("PONG", {});
        default: return Pack("ERR", {"1", EncodeWireText("unknown-cmd")});
    }
}

// 每个客户端一个线程
static void ClientThread(TcpSocket conn) {
    int curUser = 0;
    std::string line;
    while (conn.RecvLine(line)) {
        std::string resp = HandleRequest(line, &conn, curUser);
        if (!conn.SendLine(resp)) break;
    }
    if (curUser > 0) {
        if (g_sessions.Remove(curUser, &conn)) g_db.SetStatus(curUser, 0);
        std::cout << "[server] user " << curUser << " 下线\n";
    }
    conn.Close();
}

int main() {
    // 控制台按 UTF-8 输出，否则中文提示在 GBK(936) 控制台会显示为乱码
    SetConsoleOutputCP(65001);
    if (!InitWinsock()) { std::cerr << "WSAStartup failed\n"; return 1; }

    std::filesystem::path serverDir;
    std::filesystem::path schemaPath;
    std::filesystem::path dataDir;
    std::filesystem::path dbPath;
    try {
        serverDir = ExecutableDirectory();
        schemaPath = serverDir / L"database" / L"schema_sqlite.sql";
        dataDir = serverDir / L"data";
        dbPath = dataDir / L"myqq.db";
        std::filesystem::create_directories(dataDir);
    } catch (const std::exception& e) {
        std::cerr << "[server] 初始化运行目录失败: " << e.what() << "\n";
        CleanupWinsock();
        return 1;
    }

    std::string schema = ReadFile(schemaPath);
    if (schema.empty()) {
        std::cerr << "[server] 找不到或无法读取建表脚本: "
                  << PathToUtf8(schemaPath) << "\n";
        CleanupWinsock();
        return 1;
    }
    if (!g_db.Open(PathToUtf8(dbPath), schema)) {
        std::cerr << "打开数据库失败: " << g_db.LastError() << "\n";
        CleanupWinsock();
        return 1;
    }
    std::cout << "[server] 数据库就绪: " << PathToUtf8(dbPath) << "\n";

    std::string host, ip;
    if (GetLocalHostInfo(host, ip))
        std::cout << "[server] 主机=" << host << " 局域网IP=" << ip << "\n";

    TcpSocket listener;
    if (!listener.Listen(kDefaultPort)) {
        std::cerr << "监听端口 " << kDefaultPort << " 失败\n";
        CleanupWinsock();
        return 1;
    }
    std::cout << "[server] 正在监听端口 " << kDefaultPort
              << "，客户端请连接 " << ip << ":" << kDefaultPort << "\n";

    for (;;) {
        TcpSocket conn = listener.Accept();
        if (!conn.IsValid()) continue;
        std::cout << "[server] 新客户端接入\n";
        std::thread(ClientThread, std::move(conn)).detach();
    }

    g_db.Close();
    CleanupWinsock();
    return 0;
}
