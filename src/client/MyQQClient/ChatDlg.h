#pragma once
#include <afxwin.h>
#include <map>
#include <memory>
#include "resource.h"
#include "../../common/ChatLogger.h"

class CMainDlg;

struct ClientChatMessage {
    long long msgId = 0;
    int senderId = 0;
    int receiverId = 0;
    CString sendTime;
    CString content;
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
protected:
    void DoDataExchange(CDataExchange*) override;
    BOOL OnInitDialog() override;
    void PostNcDestroy() override;
    afx_msg void OnSend();
    afx_msg void OnLoadOlder();
    afx_msg void OnShowVersion();
    afx_msg void OnContextMenu(CWnd*, CPoint);
    afx_msg void OnClose();
    DECLARE_MESSAGE_MAP()
private:
    void RequestHistory(long long beforeMsgId);
    void InsertMessage(const ClientChatMessage& msg, bool logNew);
    void RenderMessages();

    int peerId_;
    CString peerNick_;
    CMainDlg* main_;
    std::unique_ptr<myqq::ChatLogger> logger_;
    bool logWarningShown_ = false;
    std::map<long long, ClientChatMessage> messages_;
    std::map<unsigned long long, CString> pendingSends_;
    unsigned long long historyRequestId_ = 0;
    unsigned long long nextClientMsgId_ = 0;
    long long nextBeforeMsgId_ = 0;
    bool hasMore_ = false;
};
