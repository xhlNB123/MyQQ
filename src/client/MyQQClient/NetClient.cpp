#include "pch.h"
#include "NetClient.h"
#include "../../common/Protocol.h"

NetClient::NetClient() : connected_(false), notifyWnd_(nullptr) {
    myqq::InitWinsock();
}

NetClient::~NetClient() {
    Close();
    myqq::CleanupWinsock();
}

bool NetClient::Connect(const std::string& ip, unsigned short port, HWND notifyWnd) {
    if (connected_.load()) return true;
    if (!sock_.Connect(ip, port)) return false;

    notifyWnd_.store(notifyWnd);
    connected_.store(true);
    recvThread_ = std::thread(&NetClient::RecvLoop, this);
    return true;
}

bool NetClient::Send(const std::string& line) {
    if (!connected_.load()) return false;
    std::string out = line;
    if (out.empty() || out.back() != myqq::kMsgEnd) out.push_back(myqq::kMsgEnd);
    return sock_.SendLine(out);
}

void NetClient::RecvLoop() {
    std::string line;
    while (connected_.load()) {
        if (!sock_.RecvLine(line)) break;   // 断开或出错
        HWND wnd = notifyWnd_.load();
        if (wnd) {
            // 堆分配一份，交给窗口线程处理后释放，避免跨线程访问栈对象
            std::string* payload = new std::string(line);
            ::PostMessage(wnd, WM_NET_MESSAGE, 0, reinterpret_cast<LPARAM>(payload));
        }
    }
    connected_.store(false);
    HWND wnd = notifyWnd_.load();
    if (wnd) ::PostMessage(wnd, WM_NET_CLOSED, 0, 0);
}

void NetClient::Close() {
    if (connected_.exchange(false)) {
        sock_.Close();
    }
    if (recvThread_.joinable()) {
        // 若在接收线程内部调用则不能 join 自己；此处约定仅 UI 线程调用
        if (std::this_thread::get_id() != recvThread_.get_id())
            recvThread_.join();
        else
            recvThread_.detach();
    }
}
