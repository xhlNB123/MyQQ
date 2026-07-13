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

#include "../common/Socket.h"
#include "../common/Message.h"
#include "../common/Protocol.h"
#include "Database.h"
#include "SessionManager.h"

using namespace myqq;

static Database       g_db;
static SessionManager g_sessions;

// 读取整个文件内容（建表脚本）
static std::string ReadFile(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return "";
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

// 处理单条请求。curUser：本连接已登录的 userId（0 表示未登录，可被回写）。
static std::string HandleRequest(const std::string& line, TcpSocket* conn, int& curUser) {
    auto t = Unpack(line);
    if (t.empty()) return Pack("ERR", {"empty"});
    Cmd cmd = StrToCmd(t[0]);

    switch (cmd) {
        case Cmd::kRegister: {                       // REGISTER|account|password|nickname
            if (t.size() < 4) return Pack("REGISTER_RESP", {"1", "参数不足"});
            int id = g_db.Register(t[1], t[2], t[3]);
            if (id > 0)  return Pack("REGISTER_RESP", {"0", std::to_string(id)});
            if (id == -1) return Pack("REGISTER_RESP", {"4", "账号已存在"});
            return Pack("REGISTER_RESP", {"5", g_db.LastError()});
        }
        case Cmd::kLogin: {                          // LOGIN|account|password
            if (t.size() < 3) return Pack("LOGIN_RESP", {"1", "参数不足"});
            int id = g_db.Login(t[1], t[2]);
            if (id <= 0) return Pack("LOGIN_RESP", {"2", "账号或密码错误"});
            curUser = id;
            g_db.SetStatus(id, 1);
            g_sessions.Bind(id, conn);
            std::cout << "[server] user " << id << " 登录\n";
            return Pack("LOGIN_RESP", {"0", std::to_string(id)});
        }
        case Cmd::kSearchUser: {                     // SEARCH|keyword
            if (t.size() < 2) return Pack("SEARCH_RESP", {"1"});
            auto users = g_db.SearchUsers(t[1], curUser);
            std::vector<std::string> args = {"0", std::to_string(users.size())};
            for (auto& u : users) {                  // 每个好友: id,account,nick,status
                args.push_back(std::to_string(u.userId));
                args.push_back(u.account);
                args.push_back(u.nickName);
                args.push_back(std::to_string(u.status));
            }
            return Pack("SEARCH_RESP", args);
        }
        case Cmd::kAddFriend: {                       // ADD_FRIEND|fromId|toId|verify
            if (t.size() < 3) return Pack("ADD_FRIEND_RESP", {"1"});
            int from = atoi(t[1].c_str()), to = atoi(t[2].c_str());
            bool ok = g_db.AddFriend(from, to);
            if (ok) {
                // 通知对方（若在线）有新好友
                g_sessions.PushTo(to, Pack("SYS_MSG",
                    {"你已被 " + std::to_string(from) + " 添加为好友"}));
            }
            return Pack("ADD_FRIEND_RESP", {ok ? "0" : "5"});
        }
        case Cmd::kFriendList: {                       // FRIEND_LIST|userId
            int uid = (t.size() >= 2) ? atoi(t[1].c_str()) : curUser;
            auto fs = g_db.GetFriends(uid);
            std::vector<std::string> args = {"0", std::to_string(fs.size())};
            for (auto& u : fs) {
                args.push_back(std::to_string(u.userId));
                args.push_back(u.account);
                args.push_back(u.nickName);
                args.push_back(std::to_string(u.status));
            }
            return Pack("FRIEND_LIST_RESP", args);
        }
        case Cmd::kChat: {                             // CHAT|fromId|toId|content
            if (t.size() < 4) return Pack("CHAT_ACK", {"1"});
            int from = atoi(t[1].c_str()), to = atoi(t[2].c_str());
            const std::string& content = t[3];
            g_db.SaveMessage(from, to, 1, content);   // 入库（离线也保存）
            // 实时推送给在线的接收方
            g_sessions.PushTo(to, Pack("CHAT_PUSH",
                {std::to_string(from), content}));
            return Pack("CHAT_ACK", {"0"});
        }
        case Cmd::kVersion:
            return Pack("VERSION_RESP", {kAppVersion});
        case Cmd::kHeartbeat:
            return Pack("PONG", {});
        default:
            return Pack("ERR", {"unknown-cmd"});
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
        g_db.SetStatus(curUser, 0);
        g_sessions.Remove(curUser);
        std::cout << "[server] user " << curUser << " 下线\n";
    }
    conn.Close();
}

int main() {
    if (!InitWinsock()) { std::cerr << "WSAStartup failed\n"; return 1; }

    std::string schema = ReadFile("database/schema_sqlite.sql");
    if (schema.empty()) schema = ReadFile("../database/schema_sqlite.sql");
    if (!g_db.Open("myqq.db", schema)) {
        std::cerr << "打开数据库失败: " << g_db.LastError() << "\n";
        CleanupWinsock();
        return 1;
    }
    std::cout << "[server] 数据库就绪 (myqq.db)\n";

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
