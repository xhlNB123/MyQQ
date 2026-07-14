#pragma once
// =====================================================================
// Winsock 轻量封装（阻塞式 TCP）
// 客户端与服务端共用：负责初始化、连接、收发、按行读取。
// =====================================================================

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601   // Windows 7+：启用 inet_ntop / inet_pton
#endif

#include <winsock2.h>
#include <string>
#include <memory>
#include <mutex>

#pragma comment(lib, "ws2_32.lib")

namespace myqq {

// 进程启动/退出时各调用一次
bool InitWinsock();
void CleanupWinsock();

// 获取本机主机名与 IPv4（用于"查询按钮获取本地主机名和 IP"）
bool GetLocalHostInfo(std::string& hostName, std::string& ipv4);

// 简单 TCP 封装
class TcpSocket {
public:
    TcpSocket();
    explicit TcpSocket(SOCKET s);
    ~TcpSocket();
    TcpSocket(const TcpSocket&) = delete;
    TcpSocket& operator=(const TcpSocket&) = delete;
    TcpSocket(TcpSocket&& other) noexcept;
    TcpSocket& operator=(TcpSocket&& other) noexcept;

    // 客户端：连接远端
    bool Connect(const std::string& ip, unsigned short port);

    // 服务端：监听 / 接受
    bool Listen(unsigned short port, int backlog = 16);
    TcpSocket Accept();

    // 收发
    bool SendLine(const std::string& line);   // 发送一整行（含 '\n'）
    bool RecvLine(std::string& line);          // 读到一行为止

    bool   IsValid() const { return sock_ != INVALID_SOCKET; }
    SOCKET Raw() const { return sock_; }
    void   Close();

private:
    SOCKET      sock_;
    std::string recvBuf_;   // 粘包/半包处理缓冲
    std::shared_ptr<std::mutex> sendMutex_;  // 同一连接的响应/推送串行发送
};

} // namespace myqq
