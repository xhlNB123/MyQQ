// =====================================================================
// Message.h 的实现
// =====================================================================
#include "Message.h"
#include <sstream>

namespace myqq {

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
    std::vector<std::string> tokens;
    std::string field;
    std::istringstream ss(line);
    while (std::getline(ss, field, kFieldSep)) {
        // 去掉行尾可能残留的 '\r' / '\n'
        while (!field.empty() && (field.back() == '\r' || field.back() == '\n'))
            field.pop_back();
        tokens.push_back(field);
    }
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
        case Cmd::kFriendReqAck:  return "FRIEND_ACK";
        case Cmd::kChat:          return "CHAT";
        case Cmd::kSysMessage:    return "SYS_MSG";
        case Cmd::kGetProfile:    return "GET_PROFILE";
        case Cmd::kUpdateProfile: return "UPDATE_PROFILE";
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
    if (s == "FRIEND_ACK")     return Cmd::kFriendReqAck;
    if (s == "CHAT")           return Cmd::kChat;
    if (s == "SYS_MSG")        return Cmd::kSysMessage;
    if (s == "GET_PROFILE")    return Cmd::kGetProfile;
    if (s == "UPDATE_PROFILE") return Cmd::kUpdateProfile;
    if (s == "VERSION")        return Cmd::kVersion;
    if (s == "PING")           return Cmd::kHeartbeat;
    return Cmd::kUnknown;
}

} // namespace myqq
