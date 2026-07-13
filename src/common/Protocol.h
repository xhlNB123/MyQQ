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
    kRegister,        // REGISTER|account|password|nickname
    kLogin,           // LOGIN|account|password
    kLogout,          // LOGOUT|userId

    // 好友
    kSearchUser,      // SEARCH|keyword                （按账号/昵称查找）
    kAddFriend,       // ADD_FRIEND|fromId|toId|verify
    kFriendList,      // FRIEND_LIST|userId
    kFriendReqAck,    // FRIEND_ACK|reqId|accept(0/1)

    // 消息
    kChat,            // CHAT|fromId|toId|content
    kSysMessage,      // SYS_MSG|toId|content

    // 个人信息
    kGetProfile,      // GET_PROFILE|userId
    kUpdateProfile,   // UPDATE_PROFILE|userId|field=value;...

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
