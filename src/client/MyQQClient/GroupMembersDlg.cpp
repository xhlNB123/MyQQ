#include "pch.h"
#include "GroupMembersDlg.h"
#include "MainDlg.h"
#include "AppContext.h"
#include "../../common/Message.h"

using namespace myqq;

static CString U8(const std::string& s){ if(s.empty())return CString();
    int n=MultiByteToWideChar(CP_UTF8,0,s.c_str(),(int)s.size(),nullptr,0);
    CStringW w; auto b=w.GetBuffer(n); MultiByteToWideChar(CP_UTF8,0,s.c_str(),(int)s.size(),b,n); w.ReleaseBuffer(n); return CString(w); }
static CString Dec(const std::string& e){ std::string s; return DecodeWireText(e,s)?U8(s):CString(); }

BEGIN_MESSAGE_MAP(CGroupMembersDlg, CDialogEx)
    ON_BN_CLICKED(IDC_GM_VIEW_BTN, &CGroupMembersDlg::OnView)
    ON_BN_CLICKED(IDC_GM_INVITE_BTN, &CGroupMembersDlg::OnInvite)
END_MESSAGE_MAP()

void CGroupMembersDlg::DoDataExchange(CDataExchange* pDX){
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_GM_LIST, list_);
}
BOOL CGroupMembersDlg::OnInitDialog(){
    CDialogEx::OnInitDialog();
    CString title; title.Format(_T("群成员 - %s"),name_.GetString()); SetWindowText(title);
    list_.SetExtendedStyle(LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES);
    list_.InsertColumn(0,_T("QQ号"),LVCFMT_LEFT,70);
    list_.InsertColumn(1,_T("昵称"),LVCFMT_LEFT,120);
    list_.InsertColumn(2,_T("身份"),LVCFMT_LEFT,50);
    g_ctx.net.Send(Pack("GROUP_MEMBERS",{std::to_string(groupId_)}));
    return TRUE;
}
// GROUP_MEMBERS_RESP|status|groupId|count|[uid,nickB64,role]*
void CGroupMembersDlg::OnMembers(const std::vector<std::string>& t){
    if(t.size()<3||t[1]!="0") return;
    list_.DeleteAllItems(); ids_.clear();
    for(size_t i=3; i+2<t.size(); i+=3){
        int row=list_.InsertItem(list_.GetItemCount(),U8(t[i]));
        list_.SetItemText(row,1,Dec(t[i+1]));
        list_.SetItemText(row,2,t[i+2]=="1"?_T("群主"):_T("成员"));
        ids_.push_back(atoi(t[i].c_str()));
    }
}
void CGroupMembersDlg::OnView(){
    POSITION p=list_.GetFirstSelectedItemPosition(); if(!p){ AfxMessageBox(_T("请选择成员")); return; }
    int r=list_.GetNextSelectedItem(p);
    if(r>=0&&r<(int)ids_.size()) main_->ViewProfile(ids_[r]);
}
void CGroupMembersDlg::OnInvite(){
    CString qq; GetDlgItemText(IDC_GM_INVITE_QQ,qq); qq.Trim();
    int fid=_ttoi(qq);
    if(fid<=0){ AfxMessageBox(_T("请输入要邀请的好友 QQ 号")); return; }
    main_->InviteToGroup(groupId_, fid);
}
