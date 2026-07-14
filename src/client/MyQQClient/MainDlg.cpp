#include "pch.h"
#include "MainDlg.h"
#include "ChatDlg.h"
#include "ProfileDlg.h"
#include "SettingsDlg.h"
#include "VerifyDlg.h"
#include "GroupDlg.h"
#include "GroupMembersDlg.h"
#include "ViewProfileDlg.h"
#include "AppContext.h"
#include "../../common/Message.h"
#include <algorithm>

using namespace myqq;

static CString U82CS(const std::string& s){ if(s.empty())return{};int n=MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0);CStringW w;auto b=w.GetBuffer(n);MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),b,n);w.ReleaseBuffer(n);return CString(w); }
static std::string CS2U8M(const CString& c){CStringW w(c);if(w.IsEmpty())return{};int n=WideCharToMultiByte(CP_UTF8,0,w,w.GetLength(),nullptr,0,nullptr,nullptr);std::string s(n,'\0');WideCharToMultiByte(CP_UTF8,0,w,w.GetLength(),&s[0],n,nullptr,nullptr);return s;}
static CString DecodeCS(const std::string& e){std::string s;return DecodeWireText(e,s)?U82CS(s):CString();}

// 从 t[base]... 解析：msgId|from|to|timeB64|kind|contentB64|fileId|nameB64|size
static void FillMsgFromTail(ClientChatMessage& m,const std::vector<std::string>& t,size_t base){
    m.msgId=_atoi64(t[base].c_str());
    m.senderId=atoi(t[base+1].c_str());
    m.receiverId=atoi(t[base+2].c_str());
    m.sendTime=DecodeCS(t[base+3]);
    m.kind=atoi(t[base+4].c_str());
    m.content=DecodeCS(t[base+5]);
    m.fileId=_atoi64(t[base+6].c_str());
    m.fileName=DecodeCS(t[base+7]);
    m.fileSize=_atoi64(t[base+8].c_str());
}

CMainDlg::CMainDlg(CWnd* p):CDialogEx(IDD_MAIN_DIALOG,p){}
CMainDlg::~CMainDlg(){}
void CMainDlg::DoDataExchange(CDataExchange* pDX){CDialogEx::DoDataExchange(pDX);DDX_Control(pDX,IDC_FRIEND_LIST,friendList_);}
BEGIN_MESSAGE_MAP(CMainDlg,CDialogEx)
 ON_BN_CLICKED(IDC_SEARCH_BTN,&CMainDlg::OnSearch) ON_BN_CLICKED(IDC_REFRESH_BTN,&CMainDlg::OnRefresh)
 ON_BN_CLICKED(IDC_OPEN_CHAT_BTN,&CMainDlg::OnOpenChat) ON_BN_CLICKED(IDC_PROFILE_BTN,&CMainDlg::OnProfile)
 ON_BN_CLICKED(IDC_SETTINGS_BTN,&CMainDlg::OnSettings) ON_BN_CLICKED(IDC_FRIEND_REQUESTS_BTN,&CMainDlg::OnFriendRequests)
 ON_BN_CLICKED(IDC_GROUP_BTN,&CMainDlg::OnGroups) ON_BN_CLICKED(IDC_SEARCH_GROUP_BTN,&CMainDlg::OnSearchGroup)
 ON_BN_CLICKED(IDC_VIEW_PROFILE_BTN,&CMainDlg::OnViewProfileBtn)
 ON_NOTIFY(NM_DBLCLK,IDC_FRIEND_LIST,&CMainDlg::OnDblClkFriend)
 ON_MESSAGE(WM_NET_MESSAGE,&CMainDlg::OnNetMessage) ON_MESSAGE(WM_NET_CLOSED,&CMainDlg::OnNetClosed) ON_WM_DESTROY()
END_MESSAGE_MAP()

BOOL CMainDlg::OnInitDialog(){
 CDialogEx::OnInitDialog();g_ctx.net.SetNotifyWnd(GetSafeHwnd());
 friendList_.SetExtendedStyle(LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES);friendList_.InsertColumn(0,_T("ID"),LVCFMT_LEFT,65);friendList_.InsertColumn(1,_T("昵称"),LVCFMT_LEFT,120);friendList_.InsertColumn(2,_T("状态"),LVCFMT_LEFT,60);
 CString self;self.Format(_T("%S  |  QQ号 %d  |  在线"),g_ctx.selfNick.c_str(),g_ctx.selfId);SetDlgItemText(IDC_MAIN_SELF_INFO,self);
 UpdateRequestButton();RequestFriendList();g_ctx.net.Send(Pack("FRIEND_SYNC",{}));g_ctx.net.Send(Pack("GROUP_SYNC",{}));return TRUE;
}
void CMainDlg::RequestFriendList(){g_ctx.net.Send(Pack("FRIEND_LIST",{}));}
void CMainDlg::OnRefresh(){RequestFriendList();}
void CMainDlg::OnSearch(){CString kw;GetDlgItemText(IDC_SEARCH_KEYWORD,kw);if(kw.IsEmpty()){AfxMessageBox(_T("请输入账号或昵称"));return;}g_ctx.net.Send(Pack("SEARCH",{EncodeWireText(CS2U8M(kw))}));}
int CMainDlg::SelectedFriendId(){POSITION p=friendList_.GetFirstSelectedItemPosition();if(!p)return 0;return _ttoi(friendList_.GetItemText(friendList_.GetNextSelectedItem(p),0));}
CString CMainDlg::SelectedFriendNick(){POSITION p=friendList_.GetFirstSelectedItemPosition();if(!p)return{};return friendList_.GetItemText(friendList_.GetNextSelectedItem(p),1);}
void CMainDlg::OnOpenChat(){int id=SelectedFriendId();if(!id){AfxMessageBox(_T("请先选择好友"));return;}OpenChatWith(id,SelectedFriendNick());}
void CMainDlg::OnDblClkFriend(NMHDR*,LRESULT* r){OnOpenChat();*r=0;}
void CMainDlg::OpenChatWith(int id,const CString& nick){auto it=chatWnds_.find(id);if(it!=chatWnds_.end()&&IsWindow(it->second->GetSafeHwnd())){it->second->SetForegroundWindow();return;}auto d=new CChatDlg(id,nick,this);d->Create(IDD_CHAT_DIALOG,this);chatWnds_[id]=d;d->ShowWindow(SW_SHOW);}
void CMainDlg::OnChatClosed(int id){if(!destroyingChats_)chatWnds_.erase(id);}
void CMainDlg::OnGroupChatClosed(int gid){if(!destroyingChats_)groupWnds_.erase(gid);}
void CMainDlg::CloseChatWindows(){
    destroyingChats_=true;
    auto c1=chatWnds_; chatWnds_.clear();
    for(auto& p:c1) if(p.second&&IsWindow(p.second->GetSafeHwnd())) p.second->DestroyWindow();
    auto c2=groupWnds_; groupWnds_.clear();
    for(auto& p:c2) if(p.second&&IsWindow(p.second->GetSafeHwnd())) p.second->DestroyWindow();
    destroyingChats_=false;
}
void CMainDlg::OpenGroupChat(int gid,const CString& name){
    auto it=groupWnds_.find(gid);
    if(it!=groupWnds_.end()&&IsWindow(it->second->GetSafeHwnd())){it->second->SetForegroundWindow();return;}
    auto d=new CChatDlg(gid,name,this,true);
    d->Create(IDD_CHAT_DIALOG,this); groupWnds_[gid]=d; d->ShowWindow(SW_SHOW);
}
void CMainDlg::OpenGroupMembers(int gid,const CString& name){
    CGroupMembersDlg dlg(this,gid,name,this); membersDlg_=&dlg; dlg.DoModal(); membersDlg_=nullptr;
}
void CMainDlg::ViewProfile(int userId){
    g_ctx.net.Send(Pack("VIEW_PROFILE",{std::to_string(userId)}));
    CViewProfileDlg dlg(this); viewProfileDlg_=&dlg; dlg.DoModal(); viewProfileDlg_=nullptr;
}
void CMainDlg::InviteToGroup(int gid,int friendId){
    g_ctx.net.Send(Pack("GROUP_INVITE",{std::to_string(gid),std::to_string(friendId)}));
}
void CMainDlg::OnGroups(){ CGroupDlg dlg(this,this); groupDlg_=&dlg; dlg.DoModal(); groupDlg_=nullptr; }
void CMainDlg::OnSearchGroup(){ OnGroups(); }
void CMainDlg::OnViewProfileBtn(){ int id=SelectedFriendId(); if(!id){AfxMessageBox(_T("请先选择好友"));return;} ViewProfile(id); }
void CMainDlg::OnProfile(){CProfileDlg d(this);profileDlg_=&d;g_ctx.net.Send(Pack("GET_PROFILE",{}));d.DoModal();profileDlg_=nullptr;CString self;self.Format(_T("%S  |  QQ号 %d  |  在线"),g_ctx.selfNick.c_str(),g_ctx.selfId);SetDlgItemText(IDC_MAIN_SELF_INFO,self);}
void CMainDlg::OnSettings(){
 CSettingsDlg d(this);
 if(d.DoModal()!=IDOK||d.GetAction()==CSettingsDlg::None)return;
 INT_PTR result=d.GetAction()==CSettingsDlg::SwitchAccount?ID_MAIN_SWITCH_ACCOUNT:ID_MAIN_EXIT_APP;
 // 通知服务端登出（切换账号保留 TCP 连接），不等待响应，直接结束主窗口。
 if(g_ctx.net.IsConnected())g_ctx.net.Send(Pack("LOGOUT",{}));
 if(result==ID_MAIN_EXIT_APP)g_ctx.net.Close();
 FinishLogout(result);
}
void CMainDlg::FinishLogout(INT_PTR result){logoutPending_=false;CloseChatWindows();EndDialog(result);}
void CMainDlg::OnFriendRequests(){CFriendRequestsDlg d(friendRequests_,this);requestsDlg_=&d;d.DoModal();requestsDlg_=nullptr;}
void CMainDlg::UpdateRequestButton(){CString s;s.Format(_T("好友申请 (%d"),(int)friendRequests_.size());s+=_T(")");SetDlgItemText(IDC_FRIEND_REQUESTS_BTN,s);}

LRESULT CMainDlg::OnNetMessage(WPARAM,LPARAM){for(auto& line:g_ctx.net.DrainMessages())HandleLine(line);return 0;}
LRESULT CMainDlg::OnNetClosed(WPARAM,LPARAM){if(logoutPending_)FinishLogout(logoutResult_);else AfxMessageBox(_T("与服务端的连接已断开"));return 0;}

void CMainDlg::HandleLine(const std::string& line){
 auto t=Unpack(line);if(t.empty())return;const auto& c=t[0];
 if(c=="FRIEND_LIST_RESP"&&t.size()>=3){friendList_.DeleteAllItems();for(size_t i=3;i+3<t.size();i+=4){int r=friendList_.InsertItem(friendList_.GetItemCount(),U82CS(t[i]));friendList_.SetItemText(r,1,DecodeCS(t[i+2]));friendList_.SetItemText(r,2,t[i+3]=="1"?_T("在线"):_T("离线"));}}
 else if(c=="SEARCH_RESP"&&t.size()>=7&&t[1]=="0"){int target=atoi(t[3].c_str());CString nick=DecodeCS(t[5]);CString prompt;prompt.Format(_T("找到 %s (QQ:%d)，是否发送好友申请？"),nick.GetString(),target);if(AfxMessageBox(prompt,MB_YESNO)==IDYES){CString def;def.Format(_T("你好，我是 %S，请求添加你为好友。"),g_ctx.selfNick.c_str());CVerifyDlg d(def,this);if(d.DoModal()==IDOK)g_ctx.net.Send(Pack("ADD_FRIEND",{std::to_string(target),EncodeWireText(CS2U8M(d.GetText()))}));}}
 else if(c=="ADD_FRIEND_RESP"){AfxMessageBox(t.size()>1&&t[1]=="0"?_T("好友申请已发送"):_T("申请失败：已是好友或已有待处理申请"));}
 else if(c=="FRIEND_REQUEST_PUSH"&&t.size()>=7){long long rid=_atoi64(t[1].c_str());bool exists=false;for(auto&r:friendRequests_)if(r.requestId==rid)exists=true;if(!exists){ClientFriendRequest r;r.requestId=rid;r.senderId=atoi(t[2].c_str());r.senderAccount=DecodeCS(t[3]);r.senderNick=DecodeCS(t[4]);r.verifyText=DecodeCS(t[5]);r.createdTime=DecodeCS(t[6]);friendRequests_.push_back(r);UpdateRequestButton();}}
 else if(c=="FRIEND_ACK_RESP"&&t.size()>=3){long long rid=_atoi64(t[2].c_str());if(t[1]=="0"){friendRequests_.erase(std::remove_if(friendRequests_.begin(),friendRequests_.end(),[&](const ClientFriendRequest&r){return r.requestId==rid;}),friendRequests_.end());UpdateRequestButton();RequestFriendList();if(requestsDlg_&&IsWindow(requestsDlg_->GetSafeHwnd()))requestsDlg_->EndDialog(IDOK);}else AfxMessageBox(_T("处理好友申请失败"));}
 else if(c=="FRIEND_RESULT_PUSH"&&t.size()>=6){CString nick=DecodeCS(t[3]);CString msg;msg.Format(t[4]=="1"?_T("%s 已接受你的好友申请"):_T("%s 已拒绝你的好友申请"),nick.GetString());AfxMessageBox(msg);g_ctx.net.Send(Pack("FRIEND_RESULT_SEEN",{t[1]}));if(t[4]=="1")RequestFriendList();}
 else if(c=="CHAT_PUSH"&&t.size()>=10){ClientChatMessage m;FillMsgFromTail(m,t,1);DeliverChat(m);}
 else if(c=="CHAT_ACK"&&t.size()>=3){unsigned long long cid=_strtoui64(t[2].c_str(),nullptr,10);for(auto&p:chatWnds_)p.second->OnChatAck(cid,atoi(t[1].c_str()),t.size()>3?_atoi64(t[3].c_str()):0,t.size()>4?DecodeCS(t[4]):CString());}
 else if(c=="CHAT_HISTORY_BEGIN"&&t.size()>=5){auto id=_strtoui64(t[2].c_str(),nullptr,10);int peer=atoi(t[3].c_str());auto it=chatWnds_.find(peer);if(it!=chatWnds_.end())it->second->OnHistoryBegin(id,atoi(t[4].c_str()));}
 else if(c=="CHAT_HISTORY_ITEM"&&t.size()>=10){auto req=_strtoui64(t[1].c_str(),nullptr,10);ClientChatMessage m;FillMsgFromTail(m,t,2);for(auto&p:chatWnds_)if(p.second->HistoryRequestId()==req)p.second->OnHistoryItem(req,m);}
 else if(c=="CHAT_HISTORY_END"&&t.size()>=4){auto req=_strtoui64(t[1].c_str(),nullptr,10);for(auto&p:chatWnds_)if(p.second->HistoryRequestId()==req)p.second->OnHistoryEnd(req,t[2]=="1",_atoi64(t[3].c_str()));}
 // 群消息：GROUP_PUSH|msgId|groupId|senderId|senderNickB64|timeB64|kind|contentB64|fileId|nameB64|size
 else if(c=="GROUP_PUSH"&&t.size()>=11){int gid=atoi(t[2].c_str());ClientChatMessage m;m.msgId=_atoi64(t[1].c_str());m.senderId=atoi(t[3].c_str());m.senderNick=DecodeCS(t[4]);m.receiverId=gid;m.sendTime=DecodeCS(t[5]);m.kind=atoi(t[6].c_str());m.content=DecodeCS(t[7]);m.fileId=_atoi64(t[8].c_str());m.fileName=DecodeCS(t[9]);m.fileSize=_atoi64(t[10].c_str());auto it=groupWnds_.find(gid);if(it!=groupWnds_.end())it->second->OnLiveMessage(m);}
 else if(c=="GROUP_CHAT_ACK"&&t.size()>=3){unsigned long long cid=_strtoui64(t[2].c_str(),nullptr,10);for(auto&p:groupWnds_)p.second->OnChatAck(cid,atoi(t[1].c_str()),t.size()>3?_atoi64(t[3].c_str()):0,t.size()>4?DecodeCS(t[4]):CString());}
 else if(c=="GROUP_HISTORY_BEGIN"&&t.size()>=5){auto id=_strtoui64(t[2].c_str(),nullptr,10);int gid=atoi(t[3].c_str());auto it=groupWnds_.find(gid);if(it!=groupWnds_.end())it->second->OnHistoryBegin(id,atoi(t[4].c_str()));}
 else if(c=="GROUP_HISTORY_ITEM"&&t.size()>=11){auto req=_strtoui64(t[1].c_str(),nullptr,10);ClientChatMessage m;m.msgId=_atoi64(t[2].c_str());m.receiverId=atoi(t[3].c_str());m.senderId=atoi(t[4].c_str());m.senderNick=DecodeCS(t[5]);m.sendTime=DecodeCS(t[6]);m.kind=atoi(t[7].c_str());m.content=DecodeCS(t[8]);m.fileId=_atoi64(t[9].c_str());m.fileName=DecodeCS(t[10]);m.fileSize=(t.size()>11)?_atoi64(t[11].c_str()):0;for(auto&p:groupWnds_)if(p.second->HistoryRequestId()==req)p.second->OnHistoryItem(req,m);}
 else if(c=="GROUP_HISTORY_END"&&t.size()>=4){auto req=_strtoui64(t[1].c_str(),nullptr,10);for(auto&p:groupWnds_)if(p.second->HistoryRequestId()==req)p.second->OnHistoryEnd(req,t[2]=="1",_atoi64(t[3].c_str()));}
 // 文件传输：广播到所有一对一 + 群窗口，各窗口按 token 自过滤
 else if(c=="FILE_BEGIN_ACK"&&t.size()>=3){CString tk(t[1].c_str());int st=atoi(t[2].c_str());for(auto&p:chatWnds_)p.second->OnFileBeginAck(tk,st);for(auto&p:groupWnds_)p.second->OnFileBeginAck(tk,st);}
 else if(c=="FILE_DONE"&&t.size()>=3){CString tk(t[1].c_str());int st=atoi(t[2].c_str());long long mid=t.size()>3?_atoi64(t[3].c_str()):0;CString tm=t.size()>4?DecodeCS(t[4]):CString();for(auto&p:chatWnds_)p.second->OnFileDone(tk,st,mid,tm);for(auto&p:groupWnds_)p.second->OnFileDone(tk,st,mid,tm);}
 else if(c=="FILE_DATA_BEGIN"&&t.size()>=3){CString rq(t[1].c_str());int st=atoi(t[2].c_str());int kd=t.size()>3?atoi(t[3].c_str()):0;CString nm=t.size()>4?DecodeCS(t[4]):CString();long long tot=t.size()>5?_atoi64(t[5].c_str()):0;for(auto&p:chatWnds_)p.second->OnFileDataBegin(rq,st,kd,nm,tot);for(auto&p:groupWnds_)p.second->OnFileDataBegin(rq,st,kd,nm,tot);}
 else if(c=="FILE_DATA_CHUNK"&&t.size()>=4){CString rq(t[1].c_str());std::string d;DecodeWireText(t[3],d);for(auto&p:chatWnds_)p.second->OnFileDataChunk(rq,d);for(auto&p:groupWnds_)p.second->OnFileDataChunk(rq,d);}
 else if(c=="FILE_DATA_END"&&t.size()>=2){CString rq(t[1].c_str());for(auto&p:chatWnds_)p.second->OnFileDataEnd(rq);for(auto&p:groupWnds_)p.second->OnFileDataEnd(rq);}
 else if(c=="GET_PROFILE_RESP"||c=="UPDATE_PROFILE_RESP"){if(profileDlg_&&IsWindow(profileDlg_->GetSafeHwnd()))profileDlg_->SendMessage(WM_NET_MESSAGE,0,(LPARAM)new std::string(line));}
 else if(c=="VIEW_PROFILE_RESP"){if(t.size()>1&&t[1]!="0"){AfxMessageBox(t.size()>2?DecodeCS(t[2]):CString(_T("无法查看")));}else if(viewProfileDlg_&&IsWindow(viewProfileDlg_->GetSafeHwnd()))((CViewProfileDlg*)viewProfileDlg_)->Fill(t);}
 // 群通知
 else if(c=="GROUP_CREATE_RESP"){if(t.size()>1&&t[1]=="0"){AfxMessageBox(_T("建群成功，群号：")+U82CS(t[2]));g_ctx.net.Send(Pack("GROUP_LIST",{}));}else AfxMessageBox(_T("建群失败"));}
 else if(c=="GROUP_LIST_RESP"){if(groupDlg_&&IsWindow(groupDlg_->GetSafeHwnd()))((CGroupDlg*)groupDlg_)->OnGroupList(t);}
 else if(c=="GROUP_SEARCH_RESP"){if(groupDlg_&&IsWindow(groupDlg_->GetSafeHwnd()))((CGroupDlg*)groupDlg_)->OnGroupSearch(t);}
 else if(c=="GROUP_MEMBERS_RESP"){if(membersDlg_&&IsWindow(membersDlg_->GetSafeHwnd()))((CGroupMembersDlg*)membersDlg_)->OnMembers(t);}
 else if(c=="GROUP_APPLY_RESP"){if(t.size()>2)AfxMessageBox(t[1]=="0"?(t[2]=="joined"?_T("已加入群"):_T("已提交申请，等待群主审批")):_T("加群失败"));}
 else if(c=="GROUP_INVITE_RESP"){AfxMessageBox(t.size()>1&&t[1]=="0"?_T("邀请已发送"):_T("邀请失败（对方非好友或已在群）"));}
 else if(c=="GROUP_APPLY_PUSH"&&t.size()>=5){CString gn=DecodeCS(t[3]),who=DecodeCS(t[4]);CString msg;msg.Format(_T("%s 申请加入群「%s」，是否同意？"),who.GetString(),gn.GetString());int r=AfxMessageBox(msg,MB_YESNO);g_ctx.net.Send(Pack("GROUP_APPROVE",{t[1],r==IDYES?"1":"0"}));}
 else if(c=="GROUP_INVITE_PUSH"&&t.size()>=5){CString gn=DecodeCS(t[3]),who=DecodeCS(t[4]);CString msg;msg.Format(_T("%s 邀请你加入群「%s」，是否接受？"),who.GetString(),gn.GetString());int r=AfxMessageBox(msg,MB_YESNO);g_ctx.net.Send(Pack("GROUP_INVITE_ACK",{t[1],r==IDYES?"1":"0"}));}
 else if(c=="GROUP_RESULT_PUSH"&&t.size()>=5){CString gn=DecodeCS(t[3]);bool joined=(t[4]=="1");CString m2;m2.Format(joined?_T("你已加入群「%s」"):_T("入群「%s」未通过"),gn.GetString());AfxMessageBox(m2);g_ctx.net.Send(Pack("GROUP_RESULT_SEEN",{t[1]}));if(joined)g_ctx.net.Send(Pack("GROUP_LIST",{}));}
 else if(c=="LOGOUT_RESP"&&logoutPending_)FinishLogout(logoutResult_);
}
void CMainDlg::DeliverChat(const ClientChatMessage& m){
    int peer = m.senderId==g_ctx.selfId ? m.receiverId : m.senderId;
    auto it=chatWnds_.find(peer);
    if(it==chatWnds_.end()){OpenChatWith(peer,_T(""));it=chatWnds_.find(peer);}
    if(it!=chatWnds_.end()) it->second->OnLiveMessage(m);
}
void CMainDlg::OnDestroy(){CloseChatWindows();CDialogEx::OnDestroy();}
