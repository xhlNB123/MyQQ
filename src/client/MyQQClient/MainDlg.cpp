#include "pch.h"
#include "MainDlg.h"
#include "ChatDlg.h"
#include "ProfileDlg.h"
#include "SettingsDlg.h"
#include "VerifyDlg.h"
#include "AppContext.h"
#include "../../common/Message.h"
#include <algorithm>

using namespace myqq;

static CString U82CS(const std::string& s){ if(s.empty())return{};int n=MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0);CStringW w;auto b=w.GetBuffer(n);MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),b,n);w.ReleaseBuffer(n);return CString(w); }
static std::string CS2U8M(const CString& c){CStringW w(c);if(w.IsEmpty())return{};int n=WideCharToMultiByte(CP_UTF8,0,w,w.GetLength(),nullptr,0,nullptr,nullptr);std::string s(n,'\0');WideCharToMultiByte(CP_UTF8,0,w,w.GetLength(),&s[0],n,nullptr,nullptr);return s;}
static CString DecodeCS(const std::string& e){std::string s;return DecodeWireText(e,s)?U82CS(s):CString();}

CMainDlg::CMainDlg(CWnd* p):CDialogEx(IDD_MAIN_DIALOG,p){}
CMainDlg::~CMainDlg(){}
void CMainDlg::DoDataExchange(CDataExchange* pDX){CDialogEx::DoDataExchange(pDX);DDX_Control(pDX,IDC_FRIEND_LIST,friendList_);}
BEGIN_MESSAGE_MAP(CMainDlg,CDialogEx)
 ON_BN_CLICKED(IDC_SEARCH_BTN,&CMainDlg::OnSearch) ON_BN_CLICKED(IDC_REFRESH_BTN,&CMainDlg::OnRefresh)
 ON_BN_CLICKED(IDC_OPEN_CHAT_BTN,&CMainDlg::OnOpenChat) ON_BN_CLICKED(IDC_PROFILE_BTN,&CMainDlg::OnProfile)
 ON_BN_CLICKED(IDC_SETTINGS_BTN,&CMainDlg::OnSettings) ON_BN_CLICKED(IDC_FRIEND_REQUESTS_BTN,&CMainDlg::OnFriendRequests)
 ON_NOTIFY(NM_DBLCLK,IDC_FRIEND_LIST,&CMainDlg::OnDblClkFriend)
 ON_MESSAGE(WM_NET_MESSAGE,&CMainDlg::OnNetMessage) ON_MESSAGE(WM_NET_CLOSED,&CMainDlg::OnNetClosed) ON_WM_DESTROY()
END_MESSAGE_MAP()

BOOL CMainDlg::OnInitDialog(){
 CDialogEx::OnInitDialog();g_ctx.net.SetNotifyWnd(GetSafeHwnd());
 friendList_.SetExtendedStyle(LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES);friendList_.InsertColumn(0,_T("ID"),LVCFMT_LEFT,65);friendList_.InsertColumn(1,_T("昵称"),LVCFMT_LEFT,120);friendList_.InsertColumn(2,_T("状态"),LVCFMT_LEFT,60);
 CString self;self.Format(_T("%S  |  QQ号 %d  |  在线"),g_ctx.selfNick.c_str(),g_ctx.selfId);SetDlgItemText(IDC_MAIN_SELF_INFO,self);
 UpdateRequestButton();RequestFriendList();g_ctx.net.Send(Pack("FRIEND_SYNC",{}));return TRUE;
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
void CMainDlg::CloseChatWindows(){destroyingChats_=true;auto copy=chatWnds_;chatWnds_.clear();for(auto& p:copy)if(p.second&&IsWindow(p.second->GetSafeHwnd()))p.second->DestroyWindow();destroyingChats_=false;}
void CMainDlg::OnProfile(){CProfileDlg d(this);profileDlg_=&d;g_ctx.net.Send(Pack("GET_PROFILE",{}));d.DoModal();profileDlg_=nullptr;CString self;self.Format(_T("%S  |  QQ号 %d  |  在线"),g_ctx.selfNick.c_str(),g_ctx.selfId);SetDlgItemText(IDC_MAIN_SELF_INFO,self);}
void CMainDlg::OnSettings(){CSettingsDlg d(this);if(d.DoModal()!=IDOK||d.GetAction()==CSettingsDlg::None)return;logoutResult_=d.GetAction()==CSettingsDlg::SwitchAccount?ID_MAIN_SWITCH_ACCOUNT:ID_MAIN_EXIT_APP;logoutPending_=true;g_ctx.net.Send(Pack("LOGOUT",{}));}
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
 else if(c=="CHAT_PUSH"&&t.size()>=6){ClientChatMessage m;m.msgId=_atoi64(t[1].c_str());m.senderId=atoi(t[2].c_str());m.receiverId=atoi(t[3].c_str());m.sendTime=DecodeCS(t[4]);m.content=DecodeCS(t[5]);DeliverChat(m.msgId,m.senderId,m.receiverId,m.sendTime,m.content);}
 else if(c=="CHAT_ACK"&&t.size()>=3){unsigned long long cid=_strtoui64(t[2].c_str(),nullptr,10);for(auto&p:chatWnds_)p.second->OnChatAck(cid,atoi(t[1].c_str()),t.size()>3?_atoi64(t[3].c_str()):0,t.size()>4?DecodeCS(t[4]):CString());}
 else if(c=="CHAT_HISTORY_BEGIN"&&t.size()>=5){auto id=_strtoui64(t[2].c_str(),nullptr,10);int peer=atoi(t[3].c_str());auto it=chatWnds_.find(peer);if(it!=chatWnds_.end())it->second->OnHistoryBegin(id,atoi(t[4].c_str()));}
 else if(c=="CHAT_HISTORY_ITEM"&&t.size()>=7){auto req=_strtoui64(t[1].c_str(),nullptr,10);ClientChatMessage m;m.msgId=_atoi64(t[2].c_str());m.senderId=atoi(t[3].c_str());m.receiverId=atoi(t[4].c_str());m.sendTime=DecodeCS(t[5]);m.content=DecodeCS(t[6]);for(auto&p:chatWnds_)if(p.second->HistoryRequestId()==req)p.second->OnHistoryItem(req,m);}
 else if(c=="CHAT_HISTORY_END"&&t.size()>=4){auto req=_strtoui64(t[1].c_str(),nullptr,10);for(auto&p:chatWnds_)if(p.second->HistoryRequestId()==req)p.second->OnHistoryEnd(req,t[2]=="1",_atoi64(t[3].c_str()));}
 else if(c=="GET_PROFILE_RESP"||c=="UPDATE_PROFILE_RESP"){if(profileDlg_&&IsWindow(profileDlg_->GetSafeHwnd()))profileDlg_->SendMessage(WM_NET_MESSAGE,0,(LPARAM)new std::string(line));}
 else if(c=="LOGOUT_RESP"&&logoutPending_)FinishLogout(logoutResult_);
}
void CMainDlg::DeliverChat(long long id,int from,int to,const CString& time,const CString& text){int peer=from==g_ctx.selfId?to:from;auto it=chatWnds_.find(peer);if(it==chatWnds_.end()){OpenChatWith(peer,_T(""));it=chatWnds_.find(peer);}ClientChatMessage m;m.msgId=id;m.senderId=from;m.receiverId=to;m.sendTime=time;m.content=text;it->second->OnLiveMessage(m);}
void CMainDlg::OnDestroy(){CloseChatWindows();CDialogEx::OnDestroy();}
