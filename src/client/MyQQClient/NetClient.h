#pragma once
// =====================================================================
// 客户端网络封装（基于 common/Socket 的 TcpSocket）
// 职责：连接服务端、发送请求、独立接收线程收消息后 PostMessage 回窗口。
// 设计要点：UI 线程绝不阻塞在 recv 上；收到的每一行消息封成堆对象，
//           通过自定义窗口消息 WM_NET_MESSAGE 派发给窗口处理。
// =====================================================================

#include <afxwin.h>
#include <string>
#include <thread>
#include <atomic>
#include "../../common/Socket.h"

// 自定义窗口消息：收到一行服务端消息
//   wParam = 未使用
//   lParam = new std::string*（接收线程 new，窗口处理完 delete）
#define WM_NET_MESSAGE (WM_APP + 100)
// 连接断开通知
#define WM_NET_CLOSED  (WM_APP + 101)

class NetClient {
public:
    NetClient();
    ~NetClient();

    // 连接服务端；成功后启动接收线程，收到的消息 PostMessage 给 notifyWnd
    bool Connect(const std::string& ip, unsigned short port, HWND notifyWnd);

    // 发送一行文本协议（内部自动补 '\n'，若已含则不重复）
    bool Send(const std::string& line);

    // 更换消息通知目标窗口（切换窗体时用，如登录成功切到主窗口）
    void SetNotifyWnd(HWND wnd) { notifyWnd_ = wnd; }

    void Close();
    bool IsConnected() const { return connected_.load(); }

private:
    void RecvLoop();

    myqq::TcpSocket        sock_;
    std::thread            recvThread_;
    std::atomic<bool>      connected_;
    std::atomic<HWND>      notifyWnd_;
};
