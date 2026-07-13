#pragma once
// =====================================================================
// 全局应用上下文：跨窗体共享的网络连接与当前登录用户信息
// （课程 demo 规模，用全局单例足够；后续可重构为依赖注入）
// =====================================================================
#include <string>
#include "NetClient.h"

struct AppContext {
    NetClient   net;            // 共享网络连接（登录后主/聊天窗口复用）
    int         selfId = 0;     // 当前登录用户 UserId
    std::string selfAccount;    // 账号
    std::string selfNick;       // 昵称
    std::string serverIp;       // 已连接的服务端 IP
    unsigned short serverPort = 0;
};

// 定义在 MyQQClientApp.cpp
extern AppContext g_ctx;
