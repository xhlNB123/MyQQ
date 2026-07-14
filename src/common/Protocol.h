#pragma once
// =====================================================================
// MyQQ 客户端/服务端 通信协议定义
// 说明：客户端与服务端必须共用本文件，保证消息格式一致。
// 传输方式：TCP（Winsock），一条消息一行，以 '\n' 结尾。
// 消息体：文本协议，字段以 '|' 分隔，格式为 CMD|arg1|arg2|...
// =====================================================================

namespace myqq {

// 默认监听端口（服务端）
const unsigned short kDefaultPort = 6000;

// 单条消息最大长度（字节）
const int kMaxPacketSize = 4096;

// ---------------- 命令字（Command）----------------
// 请求：客户端 -> 服务端
enum class Cmd {
    kUnknown = 0,

    // 账号
    kRegister,        // REGISTER|accountB64|passwordB64|nicknameB64
    kLogin,           // LOGIN|accountB64|passwordB64
    kLogout,          // LOGOUT

    // 好友
    kSearchUser,        // SEARCH|keywordB64
    kAddFriend,         // ADD_FRIEND|targetId|verifyB64
    kFriendList,        // FRIEND_LIST
    kFriendReqAck,      // FRIEND_ACK|requestId|accept(0/1)
    kFriendSync,        // FRIEND_SYNC
    kFriendResultSeen,  // FRIEND_RESULT_SEEN|requestId

    // 消息
    kChat,              // CHAT|targetId|clientMsgId|contentB64
    kChatHistory,       // CHAT_HISTORY|peerId|beforeMsgId|limit|requestId
    kSysMessage,        // 兼容旧系统消息

    // 个人信息
    kGetProfile,      // GET_PROFILE
    kUpdateProfile,   // UPDATE_PROFILE|nickB64|gender|starId|bloodId|signatureB64|avatarB64

    // 系统
    kVersion,         // VERSION
    kHeartbeat,       // PING
};

// 响应状态
enum class Status {
    kOk = 0,
    kFail = 1,
    kAuthError = 2,
    kNotFound = 3,
    kAlreadyExists = 4,
    kServerError = 5,
};

// 字段/命令分隔符
const char kFieldSep = '|';
const char kMsgEnd   = '\n';

// 软件版本（用于右键窗口查询）
const char* const kAppVersion = "MyQQ v1.0.0";

} // namespace myqq
