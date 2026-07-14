// =====================================================================
// Socket.h 的实现（阻塞式 TCP + 按行收发）
// =====================================================================
#include "Socket.h"
#include <ws2tcpip.h>

namespace myqq {

bool InitWinsock() {
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
}

void CleanupWinsock() {
    WSACleanup();
}

bool GetLocalHostInfo(std::string& hostName, std::string& ipv4) {
    char name[256] = {0};
    if (gethostname(name, sizeof(name)) != 0)
        return false;
    hostName = name;

    addrinfo hints = {0};
    hints.ai_family = AF_INET;
    addrinfo* res = nullptr;
    if (getaddrinfo(name, nullptr, &hints, &res) != 0 || !res)
        return false;

    char ipStr[INET_ADDRSTRLEN] = {0};
    auto* addr = reinterpret_cast<sockaddr_in*>(res->ai_addr);
    inet_ntop(AF_INET, &addr->sin_addr, ipStr, sizeof(ipStr));
    ipv4 = ipStr;
    freeaddrinfo(res);
    return true;
}

TcpSocket::TcpSocket()
    : sock_(INVALID_SOCKET), sendMutex_(std::make_shared<std::mutex>()) {}
TcpSocket::TcpSocket(SOCKET s)
    : sock_(s), sendMutex_(std::make_shared<std::mutex>()) {}
TcpSocket::~TcpSocket() { Close(); }

TcpSocket::TcpSocket(TcpSocket&& other) noexcept
    : sock_(other.sock_), recvBuf_(std::move(other.recvBuf_)),
      sendMutex_(std::move(other.sendMutex_)) {
    other.sock_ = INVALID_SOCKET;
    if (!sendMutex_) sendMutex_ = std::make_shared<std::mutex>();
}

TcpSocket& TcpSocket::operator=(TcpSocket&& other) noexcept {
    if (this != &other) {
        Close();
        sock_ = other.sock_;
        recvBuf_ = std::move(other.recvBuf_);
        sendMutex_ = std::move(other.sendMutex_);
        other.sock_ = INVALID_SOCKET;
        if (!sendMutex_) sendMutex_ = std::make_shared<std::mutex>();
    }
    return *this;
}

void TcpSocket::Close() {
    if (sock_ != INVALID_SOCKET) {
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
    }
}

bool TcpSocket::Connect(const std::string& ip, unsigned short port) {
    sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock_ == INVALID_SOCKET) return false;

    sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    if (connect(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        Close();
        return false;
    }
    return true;
}

bool TcpSocket::Listen(unsigned short port, int backlog) {
    sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock_ == INVALID_SOCKET) return false;

    BOOL opt = TRUE;
    setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<char*>(&opt), sizeof(opt));

    sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port);

    if (bind(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        Close();
        return false;
    }
    if (listen(sock_, backlog) != 0) {
        Close();
        return false;
    }
    return true;
}

TcpSocket TcpSocket::Accept() {
    SOCKET c = accept(sock_, nullptr, nullptr);
    return TcpSocket(c);
}

bool TcpSocket::SendLine(const std::string& line) {
    if (sock_ == INVALID_SOCKET) return false;
    std::lock_guard<std::mutex> lock(*sendMutex_);
    size_t total = 0;
    while (total < line.size()) {
        int n = send(sock_, line.data() + total,
                     static_cast<int>(line.size() - total), 0);
        if (n <= 0) return false;
        total += n;
    }
    return true;
}

bool TcpSocket::RecvLine(std::string& line) {
    // 先看缓冲里是否已经有完整一行
    for (;;) {
        size_t pos = recvBuf_.find('\n');
        if (pos != std::string::npos) {
            line = recvBuf_.substr(0, pos);
            recvBuf_.erase(0, pos + 1);
            return true;
        }
        char buf[1024];
        int n = recv(sock_, buf, sizeof(buf), 0);
        if (n <= 0) return false;   // 连接关闭或出错
        recvBuf_.append(buf, n);
    }
}

} // namespace myqq
