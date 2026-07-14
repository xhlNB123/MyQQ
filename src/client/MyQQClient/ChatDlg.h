#pragma once
#include <afxwin.h>
#include <map>
#include <vector>
#include <memory>
#include "resource.h"
#include "../../common/ChatLogger.h"

class CMainDlg;

// kind: 0 文本 1 图片 2 文件
struct ClientChatMessage {
    long long msgId = 0;
    int senderId = 0;
    int receiverId = 0;
    CString sendTime;
    CString content;
    int kind = 0;
    long long fileId = 0;
    CString fileName;
    long long fileSize = 0;
    CString localPath;      // 已下载到本地的路径（图片缓存/文件另存）
};

class CChatDlg : public CDialogEx {
public:
    CChatDlg(int peerId, const CString& peerNick, CMainDlg* pMain, CWnd* pParent = nullptr);
    ~CChatDlg() override;
    enum { IDD = IDD_CHAT_DIALOG };
    int PeerId() const { return peerId_; }
    unsigned long long HistoryRequestId() const { return historyRequestId_; }

    void OnHistoryBegin(unsigned long long requestId, int count);
    void OnHistoryItem(unsigned long long requestId, const ClientChatMessage& msg);
    void OnHistoryEnd(unsigned long long requestId, bool hasMore, long long nextCursor);
    void OnLiveMessage(const ClientChatMessage& msg);
    void OnChatAck(unsigned long long clientId, int status, long long msgId, const CString& sendTime);
    // 文件传输回调（由 MainDlg 路由）
    void OnFileBeginAck(const CString& token, int status);
    void OnFileDone(const CString& token, int status, long long msgId, const CString& sendTime);
    void OnFileDataBegin(const CString& reqId, int status, int kind, const CString& name, long long total);
    void OnFileDataChunk(const CString& reqId, const std::string& data);
    void OnFileDataEnd(const CString& reqId);

protected:
    void DoDataExchange(CDataExchange*) override;
    BOOL OnInitDialog() override;
    void PostNcDestroy() override;
    afx_msg void OnSend();
    afx_msg void OnSendImage();
    afx_msg void OnSendFile();
    afx_msg void OnLoadOlder();
    afx_msg void OnShowVersion();
    afx_msg void OnContextMenu(CWnd*, CPoint);
    afx_msg void OnClose();
    afx_msg void OnMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT lpMIS);
    afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDIS);
    afx_msg void OnDblClkList();
    DECLARE_MESSAGE_MAP()

private:
    void RequestHistory(long long beforeMsgId);
    void InsertMessage(const ClientChatMessage& msg, bool logNew);
    void RebuildOrder();
    void RefreshList();
    void StartUpload(const CString& path, int kind);
    void EnsureImageDownloaded(const ClientChatMessage& m);
    void EnsureThumb(const ClientChatMessage& m);   // 已下载则生成缩略图
    int  TextWidth();                                // 正文可用宽度（像素）
    CString CacheDir() const;

    int peerId_;
    CString peerNick_;
    CMainDlg* main_;
    std::unique_ptr<myqq::ChatLogger> logger_;
    bool logWarningShown_ = false;

    CListBox msgList_;
    std::map<long long, ClientChatMessage> messages_;   // msgId -> 消息
    std::vector<long long> order_;                       // 列表行 -> msgId（升序）
    std::map<long long, HBITMAP> thumbs_;                // msgId -> 缩略图位图

    std::map<unsigned long long, CString> pendingSends_; // 文本待确认
    unsigned long long historyRequestId_ = 0;
    unsigned long long nextClientMsgId_ = 0;
    long long nextBeforeMsgId_ = 0;
    bool hasMore_ = false;

    // 上传：token -> 元信息
    struct Upload { CString name; int kind; long long size; };
    std::map<CString, Upload> uploads_;
    unsigned long long nextToken_ = 0;

    // 下载：reqId -> 目标信息
    struct Download { CString savePath; long long forMsgId; int kind; std::string buf; long long total; };
    std::map<CString, std::shared_ptr<Download>> downloads_;
};
