#pragma once
// =====================================================================
// 聊天记录持久化：满足"自动建立一个文件存储聊天记录"的需求。
// 每个会话对应一个文本文件，追加写入带时间戳的消息。
// =====================================================================

#include <string>
#include <filesystem>

namespace myqq {

class ChatLogger {
public:
    // logDir：日志目录（不存在则创建）；selfId/peerId：会话双方 QQ 号
    ChatLogger(const std::filesystem::path& logDir, int selfId, int peerId);

    // 追加一条消息，format: [时间] 发送者昵称: 内容；失败返回 false
    bool Append(const std::string& sender, const std::string& content);

    const std::filesystem::path& FilePath() const { return filePath_; }
    bool IsReady() const { return ready_; }

private:
    std::filesystem::path filePath_;
    bool ready_ = false;
    static std::string NowString();       // "2026-07-13 11:20:33"
};

} // namespace myqq
