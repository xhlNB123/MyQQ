// =====================================================================
// MyQQ 控制台测试客户端 —— 用于联调服务端协议（先于 MFC GUI）
// 用法：ConsoleClient <serverIp> [port]
//       连上后输入原始协议行，如：LOGIN|alice|123456
//       输入 quit 退出。
// 编译：需一并加入 ../common/*.cpp
// =====================================================================
#include <iostream>
#include <string>
#include <thread>
#include <atomic>

#include "../common/Socket.h"
#include "../common/Message.h"
#include "../common/Protocol.h"

using namespace myqq;

// 接收线程：持续读取服务端消息（含服务端主动推送的 CHAT_PUSH / SYS_MSG）。
// 与主线程的键盘输入并行，模拟真实客户端的异步收发。
static void RecvLoop(TcpSocket* sock, std::atomic<bool>* running) {
    std::string resp;
    while (*running && sock->RecvLine(resp)) {
        std::cout << "\n<= " << resp << "\n> " << std::flush;
    }
    *running = false;
}

int main(int argc, char** argv) {
    std::string ip = (argc >= 2) ? argv[1] : "127.0.0.1";
    unsigned short port = (argc >= 3)
        ? static_cast<unsigned short>(atoi(argv[2])) : kDefaultPort;

    if (!InitWinsock()) { std::cerr << "WSAStartup failed\n"; return 1; }

    // 显示本机信息（对应"查询按钮获取本地主机名和 IP"）
    std::string host, myip;
    if (GetLocalHostInfo(host, myip))
        std::cout << "本机: " << host << " (" << myip << ")\n";

    TcpSocket sock;
    if (!sock.Connect(ip, port)) {
        std::cerr << "连接 " << ip << ":" << port << " 失败\n";
        CleanupWinsock();
        return 1;
    }
    std::cout << "已连接 " << ip << ":" << port
              << "，输入协议行（quit 退出）。示例: LOGIN|alice|123456\n> " << std::flush;

    // 独立接收线程：异步显示服务端响应与推送
    std::atomic<bool> running{true};
    std::thread recvThread(RecvLoop, &sock, &running);

    std::string input;
    while (running && std::getline(std::cin, input)) {
        if (input == "quit") break;
        if (input.empty()) { std::cout << "> " << std::flush; continue; }
        if (!sock.SendLine(input + "\n")) {
            std::cerr << "发送失败\n";
            break;
        }
    }

    running = false;
    sock.Close();          // 关闭套接字让接收线程的 RecvLine 返回
    if (recvThread.joinable()) recvThread.join();
    CleanupWinsock();
    return 0;
}
