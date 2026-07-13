// =====================================================================
// ChatLogger.h 的实现
// =====================================================================
#include "ChatLogger.h"
#include <fstream>
#include <ctime>
#include <direct.h>   // _mkdir

namespace myqq {

ChatLogger::ChatLogger(const std::string& logDir, int selfId, int peerId) {
    _mkdir(logDir.c_str());   // 已存在则忽略
    filePath_ = logDir + "/chat_" + std::to_string(selfId)
              + "_" + std::to_string(peerId) + ".txt";
}

std::string ChatLogger::NowString() {
    time_t t = time(nullptr);
    tm lt;
    localtime_s(&lt, &t);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &lt);
    return buf;
}

void ChatLogger::Append(const std::string& sender, const std::string& content) {
    std::ofstream ofs(filePath_, std::ios::app);
    if (!ofs) return;
    ofs << "[" << NowString() << "] " << sender << ": " << content << "\n";
}

} // namespace myqq
