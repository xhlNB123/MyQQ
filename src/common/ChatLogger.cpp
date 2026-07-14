// =====================================================================
// ChatLogger.h 的实现
// =====================================================================
#include "ChatLogger.h"
#include <fstream>
#include <ctime>

namespace myqq {

ChatLogger::ChatLogger(const std::filesystem::path& logDir, int selfId, int peerId) {
    std::error_code ec;
    std::filesystem::create_directories(logDir, ec);
    filePath_ = logDir / ("chat_" + std::to_string(selfId)
                       + "_" + std::to_string(peerId) + ".txt");
    if (ec) return;

    std::ofstream probe(filePath_, std::ios::app);
    ready_ = static_cast<bool>(probe);
}

std::string ChatLogger::NowString() {
    time_t t = time(nullptr);
    tm lt;
    localtime_s(&lt, &t);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &lt);
    return buf;
}

bool ChatLogger::Append(const std::string& sender, const std::string& content) {
    if (!ready_) return false;
    std::ofstream ofs(filePath_, std::ios::app);
    if (!ofs) {
        ready_ = false;
        return false;
    }
    ofs << "[" << NowString() << "] " << sender << ": " << content << "\n";
    return static_cast<bool>(ofs);
}

} // namespace myqq
