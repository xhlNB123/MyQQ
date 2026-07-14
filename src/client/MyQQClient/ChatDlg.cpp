#include "pch.h"
#include "ChatDlg.h"
#include "MainDlg.h"
#include "AppContext.h"
#include "../../common/Message.h"
#include "../../common/Protocol.h"
#include "../../common/PathUtils.h"

using namespace myqq;

static std::string CS2U8(const CString& cs) {
    CStringW w(cs); if (w.IsEmpty()) return {};
    int n = WideCharToMultiByte(CP_UTF8,0,w,w.GetLength(),nullptr,0,nullptr,nullptr);
    std::string s(n,'\0'); WideCharToMultiByte(CP_UTF8,0,w,w.GetLength(),&s[0],n,nullptr,nullptr); return s;
}

CChatDlg::CChatDlg(int peerId, const CString& peerNick, CMainDlg* main, CWnd* parent)
    : CDialogEx(IDD_CHAT_DIALOG,parent), peerId_(peerId), peerNick_(peerNick), main_(main) {}
CChatDlg::~CChatDlg() {}
void CChatDlg::DoDataExchange(CDataExchange* pDX) { CDialogEx::DoDataExchange(pDX); }

BEGIN_MESSAGE_MAP(CChatDlg,CDialogEx)
    ON_BN_CLICKED(IDC_CHAT_SEND_BTN,&CChatDlg::OnSend)
    ON_BN_CLICKED(IDC_CHAT_LOAD_OLDER_BTN,&CChatDlg::OnLoadOlder)
    ON_COMMAND(ID_SHOW_VERSION,&CChatDlg::OnShowVersion)
    ON_WM_CONTEXTMENU()
    ON_WM_CLOSE()
END_MESSAGE_MAP()

BOOL CChatDlg::OnInitDialog() {
    CDialogEx::OnInitDialog();
    CString title; title.Format(_T("与 %s (ID:%d) 聊天中"),peerNick_.GetString(),peerId_); SetWindowText(title);
    try { logger_=std::make_unique<ChatLogger>(ExecutableDirectory()/L"logs",g_ctx.selfId,peerId_); } catch (...) {}
    if (!logger_ || !logger_->IsReady()) { logWarningShown_=true; AfxMessageBox(_T("无法创建聊天记录，聊天仍可继续。")); }
    RequestHistory(0);
    return TRUE;
}

void CChatDlg::RequestHistory(long long before) {
    historyRequestId_=main_->NextRequestId();
    g_ctx.net.Send(Pack("CHAT_HISTORY",{std::to_string(peerId_),std::to_string(before),"50",std::to_string(historyRequestId_)}));
    SetDlgItemText(IDC_CHAT_HISTORY,_T("正在加载聊天记录...\r\n"));
}

void CChatDlg::OnHistoryBegin(unsigned long long requestId,int) {
    if(requestId!=historyRequestId_) return;
    if(messages_.empty()) SetDlgItemText(IDC_CHAT_HISTORY,_T(""));
}
void CChatDlg::OnHistoryItem(unsigned long long requestId,const ClientChatMessage& msg) {
    if(requestId==historyRequestId_) InsertMessage(msg,false);
}
void CChatDlg::OnHistoryEnd(unsigned long long requestId,bool more,long long cursor) {
    if(requestId!=historyRequestId_) return;
    hasMore_=more; nextBeforeMsgId_=cursor;
    GetDlgItem(IDC_CHAT_LOAD_OLDER_BTN)->EnableWindow(hasMore_);
    RenderMessages();
}
void CChatDlg::OnLiveMessage(const ClientChatMessage& msg) { InsertMessage(msg,true); RenderMessages(); }

void CChatDlg::InsertMessage(const ClientChatMessage& msg,bool logNew) {
    if(msg.msgId<=0 || messages_.count(msg.msgId)) return;
    messages_[msg.msgId]=msg;
    if(logNew && logger_ && !logger_->Append(std::to_string(msg.senderId),CS2U8(msg.content)) && !logWarningShown_) {
        logWarningShown_=true; AfxMessageBox(_T("聊天记录写入失败，聊天仍可继续。"));
    }
}

void CChatDlg::RenderMessages() {
    CString all;
    for(auto& pair:messages_) {
        const auto& m=pair.second;
        CString who=(m.senderId==g_ctx.selfId)?_T("我"):(peerNick_.IsEmpty()?_T("对方"):peerNick_);
        CString line; line.Format(_T("[%s] %s: %s\r\n"),m.sendTime.GetString(),who.GetString(),m.content.GetString()); all+=line;
    }
    SetDlgItemText(IDC_CHAT_HISTORY,all);
    CEdit* edit=(CEdit*)GetDlgItem(IDC_CHAT_HISTORY); if(edit) edit->SetSel(-1,-1);
}

void CChatDlg::OnSend() {
    CString text; GetDlgItemText(IDC_CHAT_INPUT,text); text.Trim(); if(text.IsEmpty()) return;
    unsigned long long cid=++nextClientMsgId_; pendingSends_[cid]=text;
    g_ctx.net.Send(Pack("CHAT",{std::to_string(peerId_),std::to_string(cid),EncodeWireText(CS2U8(text))}));
    SetDlgItemText(IDC_CHAT_INPUT,_T(""));
}

void CChatDlg::OnChatAck(unsigned long long clientId,int status,long long msgId,const CString& sendTime) {
    auto it=pendingSends_.find(clientId); if(it==pendingSends_.end()) return;
    CString content=it->second; pendingSends_.erase(it);
    if(status!=0) { AfxMessageBox(_T("消息发送失败")); return; }
    ClientChatMessage m; m.msgId=msgId; m.senderId=g_ctx.selfId; m.receiverId=peerId_; m.sendTime=sendTime; m.content=content;
    InsertMessage(m,true); RenderMessages();
}
void CChatDlg::OnLoadOlder(){ if(hasMore_) RequestHistory(nextBeforeMsgId_); }
void CChatDlg::OnContextMenu(CWnd*,CPoint point){ CMenu m;m.CreatePopupMenu();m.AppendMenu(MF_STRING,ID_SHOW_VERSION,_T("查询软件版本"));m.TrackPopupMenu(TPM_LEFTALIGN|TPM_RIGHTBUTTON,point.x,point.y,this); }
void CChatDlg::OnShowVersion(){ AfxMessageBox(CString(_T("软件版本："))+CString(myqq::kAppVersion)); }
void CChatDlg::OnClose(){ DestroyWindow(); }
void CChatDlg::PostNcDestroy(){ if(main_) main_->OnChatClosed(peerId_); delete this; }
