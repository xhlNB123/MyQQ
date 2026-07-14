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
    if (connected_.load()) {
        SetNotifyWnd(notifyWnd);
        return true;
    }
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

void NetClient::SetNotifyWnd(HWND wnd) {
    notifyWnd_.store(wnd);
    bool hasMessages = false;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        hasMessages = !recvQueue_.empty();
    }
    if (wnd && hasMessages) ::PostMessage(wnd, WM_NET_MESSAGE, 0, 0);
}

std::vector<std::string> NetClient::DrainMessages() {
    std::vector<std::string> out;
    std::lock_guard<std::mutex> lock(queueMutex_);
    out.reserve(recvQueue_.size());
    while (!recvQueue_.empty()) {
        out.push_back(std::move(recvQueue_.front()));
        recvQueue_.pop_front();
    }
    return out;
}

void NetClient::RecvLoop() {
    std::string line;
    while (connected_.load()) {
        if (!sock_.RecvLine(line)) break;
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            recvQueue_.push_back(line);
        }
        HWND wnd = notifyWnd_.load();
        if (wnd) ::PostMessage(wnd, WM_NET_MESSAGE, 0, 0);
    }
    connected_.store(false);
    HWND wnd = notifyWnd_.load();
    if (wnd) ::PostMessage(wnd, WM_NET_CLOSED, 0, 0);
}

void NetClient::Close() {
    connected_.store(false);
    sock_.Close();
    if (recvThread_.joinable()) {
        if (std::this_thread::get_id() != recvThread_.get_id())
            recvThread_.join();
        else
            recvThread_.detach();
    }
}
