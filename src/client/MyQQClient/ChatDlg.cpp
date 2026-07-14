#include "pch.h"
#include <gdiplus.h>
#include <thread>
#include <vector>
#include <algorithm>
#include "ChatDlg.h"
#include "MainDlg.h"
#include "AppContext.h"
#include "../../common/Message.h"
#include "../../common/Protocol.h"
#include "../../common/PathUtils.h"
#pragma comment(lib, "gdiplus.lib")

using namespace myqq;

static std::string CS2U8(const CString& cs) {
    CStringW w(cs); if (w.IsEmpty()) return {};
    int n = WideCharToMultiByte(CP_UTF8,0,w,w.GetLength(),nullptr,0,nullptr,nullptr);
    std::string s(n,'\0'); WideCharToMultiByte(CP_UTF8,0,w,w.GetLength(),&s[0],n,nullptr,nullptr); return s;
}
static CString HumanSize(long long n){
    CString s;
    if(n<1024) s.Format(_T("%lld B"),n);
    else if(n<1024*1024) s.Format(_T("%.1f KB"),n/1024.0);
    else s.Format(_T("%.1f MB"),n/1048576.0);
    return s;
}

CChatDlg::CChatDlg(int peerId, const CString& peerNick, CMainDlg* main, CWnd* parent)
    : CDialogEx(IDD_CHAT_DIALOG,parent), peerId_(peerId), peerNick_(peerNick), main_(main) {}
CChatDlg::~CChatDlg(){ for(auto& p:thumbs_) if(p.second) DeleteObject(p.second); }
void CChatDlg::DoDataExchange(CDataExchange* pDX){ CDialogEx::DoDataExchange(pDX); DDX_Control(pDX,IDC_CHAT_HISTORY,msgList_); }

BEGIN_MESSAGE_MAP(CChatDlg,CDialogEx)
    ON_BN_CLICKED(IDC_CHAT_SEND_BTN,&CChatDlg::OnSend)
    ON_BN_CLICKED(IDC_CHAT_SEND_IMAGE_BTN,&CChatDlg::OnSendImage)
    ON_BN_CLICKED(IDC_CHAT_SEND_FILE_BTN,&CChatDlg::OnSendFile)
    ON_BN_CLICKED(IDC_CHAT_LOAD_OLDER_BTN,&CChatDlg::OnLoadOlder)
    ON_LBN_DBLCLK(IDC_CHAT_HISTORY,&CChatDlg::OnDblClkList)
    ON_COMMAND(ID_SHOW_VERSION,&CChatDlg::OnShowVersion)
    ON_WM_CONTEXTMENU()
    ON_WM_MEASUREITEM()
    ON_WM_DRAWITEM()
    ON_WM_CLOSE()
END_MESSAGE_MAP()

BOOL CChatDlg::OnInitDialog(){
    CDialogEx::OnInitDialog();
    CString title; title.Format(_T("与 %s (ID:%d) 聊天中"),peerNick_.GetString(),peerId_); SetWindowText(title);
    try { logger_=std::make_unique<ChatLogger>(ExecutableDirectory()/L"logs",g_ctx.selfId,peerId_); } catch(...){}
    if(!logger_||!logger_->IsReady()){ logWarningShown_=true; }
    RequestHistory(0);
    return TRUE;
}

CString CChatDlg::CacheDir() const {
    CString dir((ExecutableDirectory()/L"cache").wstring().c_str());
    CreateDirectory(dir, nullptr);
    return dir;
}

void CChatDlg::RequestHistory(long long before){
    historyRequestId_=main_->NextRequestId();
    g_ctx.net.Send(Pack("CHAT_HISTORY",{std::to_string(peerId_),std::to_string(before),"50",std::to_string(historyRequestId_)}));
}
void CChatDlg::OnHistoryBegin(unsigned long long,int){}
void CChatDlg::OnHistoryItem(unsigned long long requestId,const ClientChatMessage& msg){
    if(requestId==historyRequestId_) InsertMessage(msg,false);
}
void CChatDlg::OnHistoryEnd(unsigned long long requestId,bool more,long long cursor){
    if(requestId!=historyRequestId_) return;
    hasMore_=more; nextBeforeMsgId_=cursor;
    GetDlgItem(IDC_CHAT_LOAD_OLDER_BTN)->EnableWindow(hasMore_);
    RefreshList();
}
void CChatDlg::OnLiveMessage(const ClientChatMessage& msg){ InsertMessage(msg,true); RefreshList(); }

void CChatDlg::InsertMessage(const ClientChatMessage& msg,bool logNew){
    if(msg.msgId<=0 || messages_.count(msg.msgId)) return;
    messages_[msg.msgId]=msg;
    if(msg.kind==kKindImage) EnsureImageDownloaded(messages_[msg.msgId]);
    if(logNew && logger_){
        CString note = msg.kind==kKindImage?(_T("[图片] ")+msg.fileName)
                     : msg.kind==kKindFile?(_T("[文件] ")+msg.fileName) : msg.content;
        logger_->Append(std::to_string(msg.senderId),CS2U8(note));
    }
}
void CChatDlg::RebuildOrder(){ order_.clear(); for(auto& p:messages_) order_.push_back(p.first); }
void CChatDlg::RefreshList(){
    RebuildOrder();
    msgList_.ResetContent();
    for(size_t i=0;i<order_.size();++i) msgList_.AddString(_T(""));
    if(!order_.empty()) msgList_.SetTopIndex((int)order_.size()-1);
}

// ---------------- 发送文本 ----------------
void CChatDlg::OnSend(){
    CString text; GetDlgItemText(IDC_CHAT_INPUT,text); text.Trim(); if(text.IsEmpty()) return;
    unsigned long long cid=++nextClientMsgId_; pendingSends_[cid]=text;
    g_ctx.net.Send(Pack("CHAT",{std::to_string(peerId_),std::to_string(cid),EncodeWireText(CS2U8(text))}));
    SetDlgItemText(IDC_CHAT_INPUT,_T(""));
}
void CChatDlg::OnChatAck(unsigned long long cid,int status,long long msgId,const CString& sendTime){
    auto it=pendingSends_.find(cid); if(it==pendingSends_.end()) return;
    CString content=it->second; pendingSends_.erase(it);
    if(status!=0){ AfxMessageBox(_T("消息发送失败")); return; }
    ClientChatMessage m; m.msgId=msgId; m.senderId=g_ctx.selfId; m.receiverId=peerId_; m.sendTime=sendTime; m.content=content;
    InsertMessage(m,true); RefreshList();
}

// ---------------- 发送图片/文件 ----------------
void CChatDlg::OnSendImage(){
    CFileDialog dlg(TRUE,nullptr,nullptr,OFN_FILEMUSTEXIST,
        _T("图片文件|*.jpg;*.jpeg;*.png;*.bmp;*.gif|所有文件|*.*||"),this);
    if(dlg.DoModal()==IDOK) StartUpload(dlg.GetPathName(),kKindImage);
}
void CChatDlg::OnSendFile(){
    CFileDialog dlg(TRUE,nullptr,nullptr,OFN_FILEMUSTEXIST,_T("所有文件|*.*||"),this);
    if(dlg.DoModal()==IDOK) StartUpload(dlg.GetPathName(),kKindFile);
}
void CChatDlg::StartUpload(const CString& path,int kind){
    CFile f;
    if(!f.Open(path,CFile::modeRead|CFile::shareDenyWrite)){ AfxMessageBox(_T("无法打开文件")); return; }
    ULONGLONG len=f.GetLength();
    if(len==0||len>(ULONGLONG)kMaxFileBytes){ f.Close(); AfxMessageBox(_T("文件为空或超过 20MB 上限")); return; }
    auto buf=std::make_shared<std::vector<char>>((size_t)len);
    f.Read(buf->data(),(UINT)len); f.Close();
    CString name=path; int slash=name.ReverseFind(_T('\\')); if(slash>=0) name=name.Mid(slash+1);
    CString token; token.Format(_T("u%llu"),++nextToken_);
    uploads_[token]=Upload{name,kind,(long long)len};
    std::string tk=CS2U8(token), nm=EncodeWireText(CS2U8(name)), total=std::to_string((long long)len);
    int pid=peerId_;
    std::thread([tk,nm,kind,pid,total,buf](){
        g_ctx.net.Send(Pack("FILE_BEGIN",{std::to_string(pid),tk,std::to_string(kind),nm,total}));
        int seq=0;
        for(size_t off=0; off<buf->size(); off+=kFileChunkBytes){
            size_t n=(std::min)((size_t)kFileChunkBytes, buf->size()-off);
            g_ctx.net.Send(Pack("FILE_CHUNK",{tk,std::to_string(seq++),EncodeWireText(std::string(buf->data()+off,n))}));
        }
        g_ctx.net.Send(Pack("FILE_END",{tk}));
    }).detach();
}
void CChatDlg::OnFileBeginAck(const CString& token,int status){
    if(!uploads_.count(token)) return;
    if(status!=0){ AfxMessageBox(_T("文件发送被拒绝（超限或非好友）")); uploads_.erase(token); }
}
void CChatDlg::OnFileDone(const CString& token,int status,long long msgId,const CString& sendTime){
    auto it=uploads_.find(token); if(it==uploads_.end()) return;
    Upload up=it->second; uploads_.erase(it);
    if(status!=0){ AfxMessageBox(_T("文件发送失败")); return; }
    ClientChatMessage m; m.msgId=msgId; m.senderId=g_ctx.selfId; m.receiverId=peerId_;
    m.sendTime=sendTime; m.kind=up.kind; m.fileName=up.name; m.fileSize=up.size; m.fileId=0;
    InsertMessage(m,true); RefreshList();
}

// ---------------- 下载 ----------------
void CChatDlg::EnsureImageDownloaded(const ClientChatMessage& m){
    if(m.kind!=kKindImage || m.fileId<=0) return;
    CString path; path.Format(_T("%s\\%lld_%s"),CacheDir().GetString(),m.fileId,m.fileName.GetString());
    if(GetFileAttributes(path)!=INVALID_FILE_ATTRIBUTES){ messages_[m.msgId].localPath=path; EnsureThumb(messages_[m.msgId]); return; }
    CString rq; rq.Format(_T("d%llu"),++nextToken_);
    auto d=std::make_shared<Download>(); d->savePath=path; d->forMsgId=m.msgId; d->kind=kKindImage; d->total=0;
    downloads_[rq]=d;
    g_ctx.net.Send(Pack("FILE_GET",{std::to_string(m.fileId),CS2U8(rq)}));
}
void CChatDlg::OnFileDataBegin(const CString& reqId,int status,int,const CString&,long long total){
    auto it=downloads_.find(reqId); if(it==downloads_.end()) return;
    if(status!=0){ downloads_.erase(it); return; }
    it->second->total=total; it->second->buf.clear();
}
void CChatDlg::OnFileDataChunk(const CString& reqId,const std::string& data){
    auto it=downloads_.find(reqId); if(it!=downloads_.end()) it->second->buf.append(data);
}
void CChatDlg::OnFileDataEnd(const CString& reqId){
    auto it=downloads_.find(reqId); if(it==downloads_.end()) return;
    auto d=it->second; downloads_.erase(it);
    CFile f;
    if(f.Open(d->savePath,CFile::modeCreate|CFile::modeWrite)){
        if(!d->buf.empty()) f.Write(d->buf.data(),(UINT)d->buf.size());
        f.Close();
    }
    if(d->kind==kKindImage && d->forMsgId>0 && messages_.count(d->forMsgId)){
        messages_[d->forMsgId].localPath=d->savePath; EnsureThumb(messages_[d->forMsgId]); RefreshList();
    } else if(d->kind==kKindFile){
        AfxMessageBox(_T("文件已保存：\n")+d->savePath);
    }
}

// ---------------- 缩略图（GDI+）----------------
void CChatDlg::EnsureThumb(const ClientChatMessage& m){
    if(m.localPath.IsEmpty() || thumbs_.count(m.msgId)) return;
    Gdiplus::Bitmap bmp(m.localPath);
    if(bmp.GetLastStatus()!=Gdiplus::Ok) return;
    int iw=bmp.GetWidth(), ih=bmp.GetHeight(); if(iw<=0||ih<=0) return;
    const int MX=140; double s=(std::min)(1.0,(std::min)((double)MX/iw,(double)MX/ih));
    int tw=(std::max)(1,(int)(iw*s)), th=(std::max)(1,(int)(ih*s));
    Gdiplus::Bitmap thumb(tw,th,PixelFormat32bppARGB);
    { Gdiplus::Graphics g(&thumb); g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
      g.DrawImage(&bmp,0,0,tw,th); }
    HBITMAP hb=nullptr; thumb.GetHBITMAP(Gdiplus::Color(255,255,255),&hb);
    if(hb) thumbs_[m.msgId]=hb;
}

// ---------------- 自绘 ----------------
// 正文的绘制格式（换行统一 \n，去掉 \r，避免 DrawText 多算空行）
static CString NormalizeText(const CString& s){ CString r(s); r.Replace(_T("\r\n"),_T("\n")); r.Replace(_T("\r"),_T("\n")); return r; }

// 正文可用宽度（像素）：列表客户区宽减去左右边距
int CChatDlg::TextWidth(){
    if(!msgList_.GetSafeHwnd()) return 240;
    CRect lr; msgList_.GetClientRect(&lr);
    int w=lr.Width()-16; return w<60?60:w;
}

void CChatDlg::OnMeasureItem(int id,LPMEASUREITEMSTRUCT mis){
    if(id==IDC_CHAT_HISTORY && mis->itemID<order_.size()){
        const auto& m=messages_[order_[mis->itemID]];
        if(m.kind==kKindImage) mis->itemHeight=175;
        else if(m.kind==kKindFile) mis->itemHeight=48;
        else {
            CClientDC dc(&msgList_);
            HFONT f=(HFONT)msgList_.SendMessage(WM_GETFONT,0,0);
            HGDIOBJ old = f ? dc.SelectObject(f) : nullptr;
            CString body=NormalizeText(m.content);
            CRect tr(0,0,TextWidth(),0);
            dc.DrawText(body,&tr,DT_LEFT|DT_WORDBREAK|DT_CALCRECT|DT_NOPREFIX);
            if(old) dc.SelectObject(old);
            int h=20 + tr.Height() + 8;         // 头部行 + 正文 + 边距
            if(h<40) h=40; if(h>400) h=400;
            mis->itemHeight=h;
        }
    } else mis->itemHeight=20;
    CDialogEx::OnMeasureItem(id,mis);
}
void CChatDlg::OnDrawItem(int id,LPDRAWITEMSTRUCT dis){
    if(id!=IDC_CHAT_HISTORY || dis->itemID>=order_.size()){ CDialogEx::OnDrawItem(id,dis); return; }
    CDC dc; dc.Attach(dis->hDC); CRect rc(dis->rcItem);
    HFONT f=(HFONT)msgList_.SendMessage(WM_GETFONT,0,0);
    HGDIOBJ oldFont = f ? dc.SelectObject(f) : nullptr;
    dc.FillSolidRect(&rc, GetSysColor(COLOR_WINDOW));
    const auto& m=messages_[order_[dis->itemID]];
    bool mine=(m.senderId==g_ctx.selfId);
    CString who=mine?_T("我"):(peerNick_.IsEmpty()?_T("对方"):peerNick_);
    CString head; head.Format(_T("[%s] %s"),m.sendTime.GetString(),who.GetString());
    dc.SetBkMode(TRANSPARENT); dc.SetTextColor(RGB(120,120,120));
    dc.TextOut(rc.left+8,rc.top+3,head);
    int y=rc.top+20;
    if(m.kind==kKindImage){
        auto it=thumbs_.find(m.msgId);
        if(it!=thumbs_.end()){
            BITMAP bm; GetObject(it->second,sizeof(bm),&bm);
            CDC mem; mem.CreateCompatibleDC(&dc); HGDIOBJ old=mem.SelectObject(it->second);
            dc.BitBlt(rc.left+8,y,bm.bmWidth,bm.bmHeight,&mem,0,0,SRCCOPY);
            mem.SelectObject(old);
        } else { dc.SetTextColor(RGB(0,0,0)); dc.TextOut(rc.left+8,y,_T("[图片] ")+m.fileName+_T("（加载中/双击查看）")); }
    } else if(m.kind==kKindFile){
        dc.SetTextColor(RGB(0,0,0));
        CString line; line.Format(_T("[文件] %s  (%s)  双击另存"),m.fileName.GetString(),HumanSize(m.fileSize).GetString());
        dc.TextOut(rc.left+8,y,line);
    } else {
        dc.SetTextColor(RGB(0,0,0));
        CString body=NormalizeText(m.content);
        CRect tr(rc.left+8,y,rc.left+8+TextWidth(),rc.bottom-2);
        dc.DrawText(body,&tr,DT_LEFT|DT_WORDBREAK|DT_NOPREFIX);
    }
    if(oldFont) dc.SelectObject(oldFont);
    dc.Detach();
}
void CChatDlg::OnDblClkList(){
    int sel=msgList_.GetCurSel(); if(sel<0||sel>=(int)order_.size()) return;
    const auto& m=messages_[order_[sel]];
    if(m.kind==kKindImage){
        if(!m.localPath.IsEmpty()) ShellExecute(nullptr,_T("open"),m.localPath,nullptr,nullptr,SW_SHOW);
        else EnsureImageDownloaded(m);
    } else if(m.kind==kKindFile){
        CFileDialog dlg(FALSE,nullptr,m.fileName,OFN_OVERWRITEPROMPT,_T("所有文件|*.*||"),this);
        if(dlg.DoModal()!=IDOK) return;
        CString rq; rq.Format(_T("d%llu"),++nextToken_);
        auto d=std::make_shared<Download>(); d->savePath=dlg.GetPathName(); d->forMsgId=m.msgId; d->kind=kKindFile; d->total=0;
        downloads_[rq]=d;
        g_ctx.net.Send(Pack("FILE_GET",{std::to_string(m.fileId),CS2U8(rq)}));
    }
}
void CChatDlg::OnLoadOlder(){ if(hasMore_) RequestHistory(nextBeforeMsgId_); }
void CChatDlg::OnContextMenu(CWnd*,CPoint point){ CMenu mn;mn.CreatePopupMenu();mn.AppendMenu(MF_STRING,ID_SHOW_VERSION,_T("查询软件版本"));mn.TrackPopupMenu(TPM_LEFTALIGN|TPM_RIGHTBUTTON,point.x,point.y,this); }
void CChatDlg::OnShowVersion(){ AfxMessageBox(CString(_T("软件版本："))+CString(myqq::kAppVersion)); }
void CChatDlg::OnClose(){ DestroyWindow(); }
void CChatDlg::PostNcDestroy(){ if(main_) main_->OnChatClosed(peerId_); delete this; }
