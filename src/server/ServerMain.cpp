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
#include <map>
#include <memory>

#include "../common/Socket.h"
#include "../common/PathUtils.h"
#include "../common/Message.h"
#include "../common/Protocol.h"
#include "Database.h"
#include "SessionManager.h"

using namespace myqq;

static Database       g_db;
static SessionManager g_sessions;
static std::filesystem::path g_filesDir;   // 文件落盘目录 server/data/files

// 进行中的上传会话
struct UploadState {
    long long fileId = 0;
    int       kind = 0;
    int       peerId = 0;         // scope=0 时为好友 userId；scope=1 时为 groupId
    int       scope = 0;          // 0 好友 1 群
    std::string clientMsgId;
    std::string fileName;
    long long expected = 0;
    long long received = 0;
    int       nextSeq = 0;
    std::ofstream ofs;
    std::filesystem::path path;
};

// 每个连接的状态：登录用户 + 上传会话表
struct ConnCtx {
    int curUser = 0;
    std::map<std::string, std::shared_ptr<UploadState>> uploads;   // token -> 上传状态
};

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

// 统一构造 CHAT_PUSH / CHAT_HISTORY_ITEM 的文件相关字段：
// ...|timeB64|kind|contentB64|fileId|nameB64|size
static std::vector<std::string> MsgTail(const MessageInfo& m) {
    return { EncodeWireText(m.sendTime), std::to_string(m.kind),
             EncodeWireText(m.content), std::to_string(m.fileId),
             EncodeWireText(m.fileName), std::to_string(m.fileSize) };
}

// GROUP_PUSH / GROUP_HISTORY_ITEM 主体：
// msgId|groupId|senderId|senderNickB64|timeB64|kind|contentB64|fileId|nameB64|size
static std::vector<std::string> GroupMsgBody(const GroupMessageInfo& m) {
    return { std::to_string(m.msgId), std::to_string(m.groupId), std::to_string(m.senderId),
             EncodeWireText(m.senderNick), EncodeWireText(m.sendTime), std::to_string(m.kind),
             EncodeWireText(m.content), std::to_string(m.fileId),
             EncodeWireText(m.fileName), std::to_string(m.fileSize) };
}
// 群发一条 GROUP_PUSH 给群里除 exceptUser 外的在线成员
static void PushGroupMessage(const GroupMessageInfo& m, int exceptUser) {
    std::vector<int> ids;
    for (auto& mem : g_db.GetGroupMembers(m.groupId)) ids.push_back(mem.userId);
    std::vector<std::string> push = GroupMsgBody(m);
    g_sessions.PushToMany(ids, Pack("GROUP_PUSH", push), exceptUser);
}

// 处理单条请求。ctx.curUser：本连接已登录的 userId（0 表示未登录，可被回写）。
static std::string HandleRequest(const std::string& line, TcpSocket* conn, ConnCtx& ctx) {
    int& curUser = ctx.curUser;
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
            std::vector<std::string> push = {std::to_string(m.msgId), std::to_string(curUser), std::to_string(to)};
            for (auto& s : MsgTail(m)) push.push_back(s);
            g_sessions.PushTo(to, Pack("CHAT_PUSH", push));
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
            for (auto& m : messages) {
                std::vector<std::string> item = {requestId, std::to_string(m.msgId),
                    std::to_string(m.senderId), std::to_string(m.receiverId)};
                for (auto& s : MsgTail(m)) item.push_back(s);
                conn->SendLine(Pack("CHAT_HISTORY_ITEM", item));
            }
            bool more = !messages.empty() && static_cast<int>(messages.size()) >= (limit > 50 ? 50 : limit);
            return Pack("CHAT_HISTORY_END", {requestId, more ? "1" : "0",
                messages.empty() ? "0" : std::to_string(messages.front().msgId)});
        }
        case Cmd::kFileBegin: {          // FILE_BEGIN|targetId|token|kind|nameB64|totalBytes|scope
            if (!curUser) return AuthError("FILE_BEGIN_ACK");
            std::string token = t.size() > 2 ? t[2] : "0";
            if (t.size() < 6) return Pack("FILE_BEGIN_ACK", {token, "1"});
            int target = atoi(t[1].c_str());
            int kind = atoi(t[3].c_str());
            int scope = (t.size() > 6) ? atoi(t[6].c_str()) : 0;
            std::string name;
            long long total = _atoi64(t[5].c_str());
            bool okTarget = (scope == 1) ? g_db.IsGroupMember(curUser, target)
                                         : g_db.AreFriends(curUser, target);
            if (!DecodeField(t, 4, name, 260) || name.empty() ||
                (kind != kKindImage && kind != kKindFile) ||
                total <= 0 || total > kMaxFileBytes || !okTarget)
                return Pack("FILE_BEGIN_ACK", {token, "1"});
            auto st = std::make_shared<UploadState>();
            st->kind = kind; st->peerId = target; st->scope = scope;
            st->clientMsgId = token; st->fileName = name; st->expected = total;
            // 先落临时文件，FILE_END 成功后再建 Files 记录
            st->path = g_filesDir / ("tmp_" + std::to_string(curUser) + "_" + token);
            st->ofs.open(st->path, std::ios::binary | std::ios::trunc);
            if (!st->ofs) return Pack("FILE_BEGIN_ACK", {token, "5"});
            ctx.uploads[token] = st;
            return Pack("FILE_BEGIN_ACK", {token, "0"});
        }
        case Cmd::kFileChunk: {                        // FILE_CHUNK|token|seq|dataB64（无回执）
            if (!curUser || t.size() < 4) return std::string();
            auto it = ctx.uploads.find(t[1]);
            if (it == ctx.uploads.end()) return std::string();
            auto st = it->second;
            std::string data;
            if (atoi(t[2].c_str()) != st->nextSeq || !DecodeWireText(t[3], data) ||
                st->received + (long long)data.size() > st->expected) {
                st->ofs.close(); std::error_code ec; std::filesystem::remove(st->path, ec);
                ctx.uploads.erase(it);
                return std::string();
            }
            st->ofs.write(data.data(), data.size());
            st->received += data.size();
            st->nextSeq++;
            return std::string();   // 分块不回执，减少往返
        }
        case Cmd::kFileEnd: {                          // FILE_END|token
            if (!curUser || t.size() < 2) return Pack("FILE_DONE", {"0", "1"});
            std::string token = t[1];
            auto it = ctx.uploads.find(token);
            if (it == ctx.uploads.end()) return Pack("FILE_DONE", {token, "3"});
            auto st = it->second;
            st->ofs.close();
            if (st->received != st->expected) {
                std::error_code ec; std::filesystem::remove(st->path, ec); ctx.uploads.erase(it);
                return Pack("FILE_DONE", {token, "1"});
            }
            // StorePath 存空串；实际文件固定在 g_filesDir/<fileId>，FILE_GET 按 fileId 定位
            long long fileId = g_db.CreateFileRecord(curUser, st->fileName, st->expected, st->kind, "");
            if (!fileId) { std::error_code ec; std::filesystem::remove(st->path, ec); ctx.uploads.erase(it); return Pack("FILE_DONE", {token, "5"}); }
            std::filesystem::path finalPath = g_filesDir / std::to_string(fileId);
            std::error_code ec; std::filesystem::rename(st->path, finalPath, ec);
            if (ec) { std::filesystem::remove(st->path, ec); ctx.uploads.erase(it); return Pack("FILE_DONE", {token, "5"}); }
            int peer = st->peerId, kind = st->kind, scope = st->scope; std::string fname = st->fileName;
            ctx.uploads.erase(it);
            int typeId = (kind == kKindImage) ? 5 : 6;
            if (scope == 1) {   // 群文件
                long long gmsgId = g_db.SaveGroupMessage(peer, curUser, typeId, fname, fileId);
                GroupMessageInfo gm;
                if (!gmsgId || !g_db.GetGroupMessageById(gmsgId, gm)) return Pack("FILE_DONE", {token, "5"});
                PushGroupMessage(gm, curUser);
                return Pack("FILE_DONE", {token, "0", std::to_string(gm.msgId), EncodeWireText(gm.sendTime)});
            }
            long long msgId = g_db.SaveFileMessage(curUser, peer, kind, fileId, fname);
            MessageInfo m;
            if (!msgId || !g_db.GetMessageById(msgId, m)) return Pack("FILE_DONE", {token, "5"});
            std::vector<std::string> push = {std::to_string(m.msgId), std::to_string(curUser), std::to_string(m.receiverId)};
            for (auto& s : MsgTail(m)) push.push_back(s);
            g_sessions.PushTo(m.receiverId, Pack("CHAT_PUSH", push));
            return Pack("FILE_DONE", {token, "0", std::to_string(m.msgId), EncodeWireText(m.sendTime)});
        }
        case Cmd::kFileGet: {                          // FILE_GET|fileId|requestId
            if (!curUser) return AuthError("FILE_DATA_BEGIN");
            if (t.size() < 3) return Pack("FILE_DATA_BEGIN", {"0", "1"});
            long long fileId = _atoi64(t[1].c_str());
            std::string reqId = t[2];
            FileRecord fr;
            if (!g_db.GetFileRecord(fileId, fr))
                return Pack("FILE_DATA_BEGIN", {reqId, "3"});
            // 鉴权：上传者本人 / 好友 / 同群，才可下载
            if (curUser != fr.ownerId && !g_db.AreFriends(curUser, fr.ownerId) &&
                !g_db.ShareAnyGroup(curUser, fr.ownerId))
                return Pack("FILE_DATA_BEGIN", {reqId, "2"});
            std::ifstream ifs(fr.storePath.empty() ? (g_filesDir / std::to_string(fileId)) : std::filesystem::path(fr.storePath), std::ios::binary);
            if (!ifs) return Pack("FILE_DATA_BEGIN", {reqId, "3"});
            conn->SendLine(Pack("FILE_DATA_BEGIN", {reqId, "0", std::to_string(fr.kind),
                EncodeWireText(fr.fileName), std::to_string(fr.fileSize)}));
            std::vector<char> buf(kFileChunkBytes);
            int seq = 0;
            while (ifs) {
                ifs.read(buf.data(), buf.size());
                std::streamsize n = ifs.gcount();
                if (n <= 0) break;
                conn->SendLine(Pack("FILE_DATA_CHUNK", {reqId, std::to_string(seq++),
                    EncodeWireText(std::string(buf.data(), (size_t)n))}));
            }
            return Pack("FILE_DATA_END", {reqId, "0"});
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
            if (ok && t.size() > 7) g_db.SetVisibility(curUser, atoi(t[7].c_str()));  // 可选可见性字段
            return Pack("UPDATE_PROFILE_RESP", {ok ? "0" : "5", ok ? "" : EncodeWireText(g_db.LastError())});
        }
        case Cmd::kViewProfile: {                      // VIEW_PROFILE|userId
            if (!curUser) return AuthError("VIEW_PROFILE_RESP");
            int target = t.size() > 1 ? atoi(t[1].c_str()) : 0;
            if (!g_db.CanViewProfile(curUser, target))
                return Pack("VIEW_PROFILE_RESP", {"2", EncodeWireText("对方未公开资料")});
            ProfileInfo p;
            if (!g_db.GetProfile(target, p)) return Pack("VIEW_PROFILE_RESP", {"3", EncodeWireText("用户不存在")});
            return Pack("VIEW_PROFILE_RESP", {"0", std::to_string(p.userId), EncodeWireText(p.account),
                EncodeWireText(p.nickName), std::to_string(p.gender), std::to_string(p.starId),
                std::to_string(p.bloodTypeId), EncodeWireText(p.signature), EncodeWireText(p.avatar)});
        }
        case Cmd::kGroupCreate: {                       // GROUP_CREATE|nameB64|requireApproval
            if (!curUser) return AuthError("GROUP_CREATE_RESP");
            std::string name;
            if (!DecodeField(t, 1, name, 60) || name.empty()) return Pack("GROUP_CREATE_RESP", {"1", "0"});
            long long gid = g_db.CreateGroup(curUser, name, t.size() > 2 ? atoi(t[2].c_str()) : 0);
            return Pack("GROUP_CREATE_RESP", {gid ? "0" : "5", std::to_string(gid)});
        }
        case Cmd::kGroupSearch: {                       // GROUP_SEARCH|keywordB64
            if (!curUser) return AuthError("GROUP_SEARCH_RESP");
            std::string kw;
            if (!DecodeField(t, 1, kw, 60)) return Pack("GROUP_SEARCH_RESP", {"1"});
            auto gs = g_db.SearchGroups(kw);
            std::vector<std::string> args = {"0", std::to_string(gs.size())};
            for (auto& g : gs) { args.push_back(std::to_string(g.groupId)); args.push_back(EncodeWireText(g.name));
                args.push_back(std::to_string(g.memberCount)); args.push_back(std::to_string(g.requireApproval)); }
            return Pack("GROUP_SEARCH_RESP", args);
        }
        case Cmd::kGroupList: {                         // GROUP_LIST
            if (!curUser) return AuthError("GROUP_LIST_RESP");
            auto gs = g_db.GetMyGroups(curUser);
            std::vector<std::string> args = {"0", std::to_string(gs.size())};
            for (auto& g : gs) { args.push_back(std::to_string(g.groupId)); args.push_back(EncodeWireText(g.name));
                args.push_back(std::to_string(g.myRole)); }
            return Pack("GROUP_LIST_RESP", args);
        }
        case Cmd::kGroupMembers: {                      // GROUP_MEMBERS|groupId
            if (!curUser) return AuthError("GROUP_MEMBERS_RESP");
            long long gid = t.size() > 1 ? _atoi64(t[1].c_str()) : 0;
            if (!g_db.IsGroupMember(curUser, gid)) return Pack("GROUP_MEMBERS_RESP", {"2", std::to_string(gid), "0"});
            auto ms = g_db.GetGroupMembers(gid);
            std::vector<std::string> args = {"0", std::to_string(gid), std::to_string(ms.size())};
            for (auto& m : ms) { args.push_back(std::to_string(m.userId)); args.push_back(EncodeWireText(m.nickName));
                args.push_back(std::to_string(m.role)); }
            return Pack("GROUP_MEMBERS_RESP", args);
        }
        case Cmd::kGroupApply: {                        // GROUP_APPLY|groupId
            if (!curUser) return AuthError("GROUP_APPLY_RESP");
            long long gid = t.size() > 1 ? _atoi64(t[1].c_str()) : 0;
            GroupReqInfo r;
            int status = g_db.CreateGroupApply(curUser, gid, r);
            if (status != 0) return Pack("GROUP_APPLY_RESP", {std::to_string(status), "fail"});
            if (r.status == 1) return Pack("GROUP_APPLY_RESP", {"0", "joined"});
            // 需审批：推给群主
            GroupInfo g; g_db.GetGroup(gid, g);
            UserInfo me; g_db.GetUserInfo(curUser, me);
            g_sessions.PushTo(g.ownerId, Pack("GROUP_APPLY_PUSH", {std::to_string(r.reqId),
                std::to_string(gid), EncodeWireText(g.name), EncodeWireText(me.nickName)}));
            return Pack("GROUP_APPLY_RESP", {"0", "pending"});
        }
        case Cmd::kGroupInvite: {                       // GROUP_INVITE|groupId|friendId
            if (!curUser) return AuthError("GROUP_INVITE_RESP");
            long long gid = t.size() > 1 ? _atoi64(t[1].c_str()) : 0;
            int friendId = t.size() > 2 ? atoi(t[2].c_str()) : 0;
            if (!g_db.AreFriends(curUser, friendId)) return Pack("GROUP_INVITE_RESP", {"1"});
            GroupReqInfo r;
            int status = g_db.CreateGroupInvite(curUser, gid, friendId, r);
            if (status != 0) return Pack("GROUP_INVITE_RESP", {std::to_string(status)});
            GroupInfo g; g_db.GetGroup(gid, g);
            UserInfo me; g_db.GetUserInfo(curUser, me);
            g_sessions.PushTo(friendId, Pack("GROUP_INVITE_PUSH", {std::to_string(r.reqId),
                std::to_string(gid), EncodeWireText(g.name), EncodeWireText(me.nickName)}));
            return Pack("GROUP_INVITE_RESP", {"0"});
        }
        case Cmd::kGroupInviteAck: {                    // GROUP_INVITE_ACK|reqId|accept
            if (!curUser) return AuthError("GROUP_INVITE_ACK_RESP");
            long long reqId = t.size() > 1 ? _atoi64(t[1].c_str()) : 0;
            bool accept = t.size() > 2 && atoi(t[2].c_str()) != 0;
            GroupReqInfo r;
            int status = g_db.ResolveInvite(reqId, curUser, accept, r);
            if (status != 0) return Pack("GROUP_INVITE_ACK_RESP", {std::to_string(status)});
            GroupInfo g; g_db.GetGroup(r.groupId, g);
            if (r.status == 0 && r.needOwnerOk) {   // 转待群主审批
                UserInfo me; g_db.GetUserInfo(curUser, me);
                g_sessions.PushTo(g.ownerId, Pack("GROUP_APPLY_PUSH", {std::to_string(r.reqId),
                    std::to_string(r.groupId), EncodeWireText(g.name), EncodeWireText(me.nickName)}));
            } else if (r.status == 1) {             // 已入群，通知群在线成员刷新（可选）；结果给自己
                g_sessions.PushTo(curUser, Pack("GROUP_RESULT_PUSH", {std::to_string(r.reqId),
                    std::to_string(r.groupId), EncodeWireText(g.name), "1"}));
            }
            return Pack("GROUP_INVITE_ACK_RESP", {"0"});
        }
        case Cmd::kGroupApprove: {                      // GROUP_APPROVE|reqId|accept
            if (!curUser) return AuthError("GROUP_APPROVE_RESP");
            long long reqId = t.size() > 1 ? _atoi64(t[1].c_str()) : 0;
            bool accept = t.size() > 2 && atoi(t[2].c_str()) != 0;
            GroupReqInfo r;
            int status = g_db.ResolveApprove(reqId, curUser, accept, r);
            if (status != 0) return Pack("GROUP_APPROVE_RESP", {std::to_string(status)});
            GroupInfo g; g_db.GetGroup(r.groupId, g);
            g_sessions.PushTo(r.targetId, Pack("GROUP_RESULT_PUSH", {std::to_string(r.reqId),
                std::to_string(r.groupId), EncodeWireText(g.name), std::to_string(r.status)}));
            return Pack("GROUP_APPROVE_RESP", {"0"});
        }
        case Cmd::kGroupSync: {                         // GROUP_SYNC
            if (!curUser) return AuthError("GROUP_SYNC_RESP");
            for (auto& r : g_db.GetPendingInvites(curUser))
                conn->SendLine(Pack("GROUP_INVITE_PUSH", {std::to_string(r.reqId), std::to_string(r.groupId),
                    EncodeWireText(r.groupName), EncodeWireText(r.inviterNick)}));
            for (auto& r : g_db.GetPendingApprovals(curUser))
                conn->SendLine(Pack("GROUP_APPLY_PUSH", {std::to_string(r.reqId), std::to_string(r.groupId),
                    EncodeWireText(r.groupName), EncodeWireText(r.targetNick)}));
            for (auto& r : g_db.GetUnackedGroupResults(curUser))
                conn->SendLine(Pack("GROUP_RESULT_PUSH", {std::to_string(r.reqId), std::to_string(r.groupId),
                    EncodeWireText(r.groupName), std::to_string(r.status)}));
            return Pack("GROUP_SYNC_RESP", {"0"});
        }
        case Cmd::kGroupResultSeen:
            if (!curUser) return AuthError("GROUP_RESULT_SEEN_RESP");
            return Pack("GROUP_RESULT_SEEN_RESP", {
                g_db.AckGroupResult(t.size() > 1 ? _atoi64(t[1].c_str()) : 0, curUser) ? "0" : "3"});
        case Cmd::kGroupChat: {                         // GROUP_CHAT|groupId|clientMsgId|contentB64
            if (!curUser) return AuthError("GROUP_CHAT_ACK");
            std::string clientId = t.size() > 2 ? t[2] : "0";
            long long gid = t.size() > 1 ? _atoi64(t[1].c_str()) : 0;
            std::string content;
            if (!DecodeField(t, 3, content, 2000) || content.empty() || !g_db.IsGroupMember(curUser, gid))
                return Pack("GROUP_CHAT_ACK", {"1", clientId, EncodeWireText("非群成员或空消息")});
            long long id = g_db.SaveGroupMessage(gid, curUser, 1, content, 0);
            GroupMessageInfo gm;
            if (!id || !g_db.GetGroupMessageById(id, gm)) return Pack("GROUP_CHAT_ACK", {"5", clientId});
            PushGroupMessage(gm, curUser);
            return Pack("GROUP_CHAT_ACK", {"0", clientId, std::to_string(gm.msgId), EncodeWireText(gm.sendTime)});
        }
        case Cmd::kGroupHistory: {                      // GROUP_HISTORY|groupId|before|limit|reqId
            if (!curUser) return AuthError("GROUP_HISTORY_BEGIN");
            if (t.size() < 5) return Pack("GROUP_HISTORY_BEGIN", {"1", "0", "0", "0"});
            long long gid = _atoi64(t[1].c_str()); long long before = _atoi64(t[2].c_str());
            int limit = atoi(t[3].c_str()); std::string reqId = t[4];
            if (!g_db.IsGroupMember(curUser, gid)) return Pack("GROUP_HISTORY_BEGIN", {"2", reqId, std::to_string(gid), "0"});
            auto msgs = g_db.GetGroupConversation(gid, before, limit);
            conn->SendLine(Pack("GROUP_HISTORY_BEGIN", {"0", reqId, std::to_string(gid), std::to_string(msgs.size())}));
            for (auto& m : msgs) {
                std::vector<std::string> item = {reqId};
                for (auto& s : GroupMsgBody(m)) item.push_back(s);
                conn->SendLine(Pack("GROUP_HISTORY_ITEM", item));
            }
            bool more = !msgs.empty() && (int)msgs.size() >= (limit > 50 ? 50 : limit);
            return Pack("GROUP_HISTORY_END", {reqId, more ? "1" : "0",
                msgs.empty() ? "0" : std::to_string(msgs.front().msgId)});
        }
        case Cmd::kVersion: return Pack("VERSION_RESP", {kAppVersion});
        case Cmd::kHeartbeat: return Pack("PONG", {});
        default: return Pack("ERR", {"1", EncodeWireText("unknown-cmd")});
    }
}

// 每个客户端一个线程
static void ClientThread(TcpSocket conn) {
    ConnCtx ctx;
    std::string line;
    while (conn.RecvLine(line)) {
        std::string resp = HandleRequest(line, &conn, ctx);
        if (!conn.SendLine(resp)) break;
    }
    // 清理未完成的上传临时文件
    for (auto& kv : ctx.uploads) {
        if (kv.second) {
            kv.second->ofs.close();
            std::error_code ec; std::filesystem::remove(kv.second->path, ec);
        }
    }
    if (ctx.curUser > 0) {
        if (g_sessions.Remove(ctx.curUser, &conn)) g_db.SetStatus(ctx.curUser, 0);
        std::cout << "[server] user " << ctx.curUser << " 下线\n";
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
        g_filesDir = dataDir / L"files";
        std::filesystem::create_directories(dataDir);
        std::filesystem::create_directories(g_filesDir);
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
