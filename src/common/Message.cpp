// =====================================================================
// Message.h 的实现
// =====================================================================
#include "Message.h"
#include <sstream>

namespace myqq {

std::string EncodeWireText(const std::string& input) {
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string out;
    out.reserve((input.size() * 4 + 2) / 3);
    unsigned int value = 0;
    int bits = -6;
    for (unsigned char c : input) {
        value = (value << 8) | c;
        bits += 8;
        while (bits >= 0) {
            out.push_back(alphabet[(value >> bits) & 0x3F]);
            bits -= 6;
        }
    }
    if (bits > -6) out.push_back(alphabet[((value << 8) >> (bits + 8)) & 0x3F]);
    return out;
}

bool DecodeWireText(const std::string& input, std::string& output) {
    static signed char table[256];
    static bool initialized = false;
    if (!initialized) {
        for (int i = 0; i < 256; ++i) table[i] = -1;
        const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
        for (int i = 0; i < 64; ++i) table[static_cast<unsigned char>(alphabet[i])] = static_cast<signed char>(i);
        initialized = true;
    }
    output.clear();
    output.reserve(input.size() * 3 / 4);
    unsigned int value = 0;
    int bits = -8;
    for (unsigned char c : input) {
        int decoded = table[c];
        if (decoded < 0) return false;
        value = (value << 6) | static_cast<unsigned int>(decoded);
        bits += 6;
        if (bits >= 0) {
            output.push_back(static_cast<char>((value >> bits) & 0xFF));
            bits -= 8;
        }
    }
    // Base64URL 无 padding 时，只允许余 0/2/4 bit；长度 mod 4 == 1 非法。
    return input.size() % 4 != 1;
}

std::string Pack(const std::string& cmd, const std::vector<std::string>& args) {
    std::string out = cmd;
    for (const auto& a : args) {
        out += kFieldSep;
        out += a;
    }
    out += kMsgEnd;
    return out;
}

std::vector<std::string> Unpack(const std::string& line) {
    // 先去掉整行末尾的 '\r'/'\n'
    std::string s = line;
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n'))
        s.pop_back();

    // 手动按分隔符切分：保留末尾空字段（getline 会丢掉行尾空字段，
    // 导致 "A|B|" 只解析出 2 段、"...||" 结尾的空头像/签名字段丢失）。
    std::vector<std::string> tokens;
    std::string field;
    for (char c : s) {
        if (c == kFieldSep) {
            tokens.push_back(field);
            field.clear();
        } else {
            field.push_back(c);
        }
    }
    tokens.push_back(field);   // 最后一段（可能为空）
    return tokens;
}

std::string CmdToStr(Cmd cmd) {
    switch (cmd) {
        case Cmd::kRegister:      return "REGISTER";
        case Cmd::kLogin:         return "LOGIN";
        case Cmd::kLogout:        return "LOGOUT";
        case Cmd::kSearchUser:    return "SEARCH";
        case Cmd::kAddFriend:     return "ADD_FRIEND";
        case Cmd::kFriendList:    return "FRIEND_LIST";
        case Cmd::kFriendReqAck:    return "FRIEND_ACK";
        case Cmd::kFriendSync:      return "FRIEND_SYNC";
        case Cmd::kFriendResultSeen:return "FRIEND_RESULT_SEEN";
        case Cmd::kChat:            return "CHAT";
        case Cmd::kChatHistory:     return "CHAT_HISTORY";
        case Cmd::kSysMessage:      return "SYS_MSG";
        case Cmd::kFileBegin:       return "FILE_BEGIN";
        case Cmd::kFileChunk:       return "FILE_CHUNK";
        case Cmd::kFileEnd:         return "FILE_END";
        case Cmd::kFileGet:         return "FILE_GET";
        case Cmd::kGetProfile:    return "GET_PROFILE";
        case Cmd::kUpdateProfile: return "UPDATE_PROFILE";
        case Cmd::kViewProfile:   return "VIEW_PROFILE";
        case Cmd::kGroupCreate:   return "GROUP_CREATE";
        case Cmd::kGroupSearch:   return "GROUP_SEARCH";
        case Cmd::kGroupList:     return "GROUP_LIST";
        case Cmd::kGroupMembers:  return "GROUP_MEMBERS";
        case Cmd::kGroupApply:    return "GROUP_APPLY";
        case Cmd::kGroupInvite:   return "GROUP_INVITE";
        case Cmd::kGroupInviteAck:return "GROUP_INVITE_ACK";
        case Cmd::kGroupApprove:  return "GROUP_APPROVE";
        case Cmd::kGroupSync:     return "GROUP_SYNC";
        case Cmd::kGroupResultSeen:return "GROUP_RESULT_SEEN";
        case Cmd::kGroupChat:     return "GROUP_CHAT";
        case Cmd::kGroupHistory:  return "GROUP_HISTORY";
        case Cmd::kVersion:       return "VERSION";
        case Cmd::kHeartbeat:     return "PING";
        default:                  return "UNKNOWN";
    }
}

Cmd StrToCmd(const std::string& s) {
    if (s == "REGISTER")       return Cmd::kRegister;
    if (s == "LOGIN")          return Cmd::kLogin;
    if (s == "LOGOUT")         return Cmd::kLogout;
    if (s == "SEARCH")         return Cmd::kSearchUser;
    if (s == "ADD_FRIEND")     return Cmd::kAddFriend;
    if (s == "FRIEND_LIST")    return Cmd::kFriendList;
    if (s == "FRIEND_ACK")        return Cmd::kFriendReqAck;
    if (s == "FRIEND_SYNC")       return Cmd::kFriendSync;
    if (s == "FRIEND_RESULT_SEEN")return Cmd::kFriendResultSeen;
    if (s == "CHAT")              return Cmd::kChat;
    if (s == "CHAT_HISTORY")      return Cmd::kChatHistory;
    if (s == "SYS_MSG")           return Cmd::kSysMessage;
    if (s == "FILE_BEGIN")        return Cmd::kFileBegin;
    if (s == "FILE_CHUNK")        return Cmd::kFileChunk;
    if (s == "FILE_END")          return Cmd::kFileEnd;
    if (s == "FILE_GET")          return Cmd::kFileGet;
    if (s == "GET_PROFILE")    return Cmd::kGetProfile;
    if (s == "UPDATE_PROFILE") return Cmd::kUpdateProfile;
    if (s == "VIEW_PROFILE")   return Cmd::kViewProfile;
    if (s == "GROUP_CREATE")   return Cmd::kGroupCreate;
    if (s == "GROUP_SEARCH")   return Cmd::kGroupSearch;
    if (s == "GROUP_LIST")     return Cmd::kGroupList;
    if (s == "GROUP_MEMBERS")  return Cmd::kGroupMembers;
    if (s == "GROUP_APPLY")    return Cmd::kGroupApply;
    if (s == "GROUP_INVITE")   return Cmd::kGroupInvite;
    if (s == "GROUP_INVITE_ACK") return Cmd::kGroupInviteAck;
    if (s == "GROUP_APPROVE")  return Cmd::kGroupApprove;
    if (s == "GROUP_SYNC")     return Cmd::kGroupSync;
    if (s == "GROUP_RESULT_SEEN") return Cmd::kGroupResultSeen;
    if (s == "GROUP_CHAT")     return Cmd::kGroupChat;
    if (s == "GROUP_HISTORY")  return Cmd::kGroupHistory;
    if (s == "VERSION")        return Cmd::kVersion;
    if (s == "PING")           return Cmd::kHeartbeat;
    return Cmd::kUnknown;
}

} // namespace myqq
