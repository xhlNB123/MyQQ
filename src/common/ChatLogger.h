#pragma once
// =====================================================================
// 聊天记录持久化：满足"自动建立一个文件存储聊天记录"的需求。
// 每个会话对应一个文本文件，追加写入带时间戳的消息。
// =====================================================================

#include <string>

namespace myqq {

class ChatLogger {
public:
    // logDir：日志目录（不存在则创建）；selfId/peerId：会话双方 QQ 号
    ChatLogger(const std::string& logDir, int selfId, int peerId);

    // 追加一条消息，format: [时间] 发送者昵称: 内容
    void Append(const std::string& sender, const std::string& content);

    const std::string& FilePath() const { return filePath_; }

private:
    std::string filePath_;
    static std::string NowString();       // "2026-07-13 11:20:33"
};

} // namespace myqq
