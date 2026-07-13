#pragma once
// =====================================================================
// 消息打包 / 解包工具
// 将 Cmd + 参数序列化为 "CMD|a1|a2\n"，以及反向解析。
// 客户端与服务端共用。
// =====================================================================

#include <string>
#include <vector>
#include "Protocol.h"

namespace myqq {

// 打包：把命令名与参数拼成一行文本消息
std::string Pack(const std::string& cmd, const std::vector<std::string>& args);

// 解包：把一行文本拆成 tokens（首元素为命令名）
std::vector<std::string> Unpack(const std::string& line);

// 命令字 <-> 字符串 互转
std::string   CmdToStr(Cmd cmd);
Cmd           StrToCmd(const std::string& s);

} // namespace myqq
