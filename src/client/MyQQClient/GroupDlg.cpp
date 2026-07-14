#include "pch.h"
#include "GroupDlg.h"
#include "MainDlg.h"
#include "CreateGroupDlg.h"
#include "AppContext.h"
#include "../../common/Message.h"

using namespace myqq;

static CString U8(const std::string& s){ if(s.empty())return CString();
    int n=MultiByteToWideChar(CP_UTF8,0,s.c_str(),(int)s.size(),nullptr,0);
    CStringW w; auto b=w.GetBuffer(n); MultiByteToWideChar(CP_UTF8,0,s.c_str(),(int)s.size(),b,n); w.ReleaseBuffer(n); return CString(w); }
static std::string CS(const CString& c){ CStringW w(c); if(w.IsEmpty())return{};
    int n=WideCharToMultiByte(CP_UTF8,0,w,w.GetLength(),nullptr,0,nullptr,nullptr);
    std::string s(n,'\0'); WideCharToMultiByte(CP_UTF8,0,w,w.GetLength(),&s[0],n,nullptr,nullptr); return s; }
static CString Dec(const std::string& e){ std::string s; return DecodeWireText(e,s)?U8(s):CString(); }

BEGIN_MESSAGE_MAP(CGroupDlg, CDialogEx)
    ON_BN_CLICKED(IDC_GROUP_CREATE_BTN, &CGroupDlg::OnCreate)
    ON_BN_CLICKED(IDC_GROUP_OPEN_BTN, &CGroupDlg::OnOpen)
    ON_BN_CLICKED(IDC_GROUP_SEARCH_BTN, &CGroupDlg::OnSearch)
    ON_BN_CLICKED(IDC_GROUP_APPLY_BTN, &CGroupDlg::OnApply)
    ON_NOTIFY(NM_DBLCLK, IDC_GROUP_MY_LIST, &CGroupDlg::OnDblClkMine)
END_MESSAGE_MAP()

void CGroupDlg::DoDataExchange(CDataExchange* pDX){
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_GROUP_MY_LIST, mine_);
    DDX_Control(pDX, IDC_GROUP_SEARCH_LIST, found_);
}
BOOL CGroupDlg::OnInitDialog(){
    CDialogEx::OnInitDialog();
    SetWindowText(_T("群聊"));
    mine_.SetExtendedStyle(LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES);
    mine_.InsertColumn(0,_T("群号"),LVCFMT_LEFT,70); mine_.InsertColumn(1,_T("群名"),LVCFMT_LEFT,150);
    found_.SetExtendedStyle(LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES);
    found_.InsertColumn(0,_T("群号"),LVCFMT_LEFT,70); found_.InsertColumn(1,_T("群名"),LVCFMT_LEFT,120);
    found_.InsertColumn(2,_T("人数"),LVCFMT_LEFT,45); found_.InsertColumn(3,_T("审批"),LVCFMT_LEFT,45);
    g_ctx.net.Send(Pack("GROUP_LIST",{}));
    return TRUE;
}
// GROUP_LIST_RESP|status|count|[gid,nameB64,role]*
void CGroupDlg::OnGroupList(const std::vector<std::string>& t){
    if(t.size()<2||t[1]!="0") return;
    mine_.DeleteAllItems(); mineIds_.clear(); mineNames_.clear();
    for(size_t i=3; i+2<t.size(); i+=3){
        long long gid=_atoi64(t[i].c_str()); CString name=Dec(t[i+1]);
        int row=mine_.InsertItem(mine_.GetItemCount(),U8(t[i]));
        mine_.SetItemText(row,1,name);
        mineIds_.push_back(gid); mineNames_.push_back(name);
    }
}
// GROUP_SEARCH_RESP|status|count|[gid,nameB64,memberCount,requireApproval]*
void CGroupDlg::OnGroupSearch(const std::vector<std::string>& t){
    if(t.size()<2||t[1]!="0"){ AfxMessageBox(_T("未找到群")); return; }
    found_.DeleteAllItems(); foundIds_.clear();
    for(size_t i=3; i+3<t.size(); i+=4){
        int row=found_.InsertItem(found_.GetItemCount(),U8(t[i]));
        found_.SetItemText(row,1,Dec(t[i+1]));
        found_.SetItemText(row,2,U8(t[i+2]));
        found_.SetItemText(row,3,t[i+3]=="1"?_T("是"):_T("否"));
        foundIds_.push_back(_atoi64(t[i].c_str()));
    }
    if(foundIds_.empty()) AfxMessageBox(_T("未找到群"));
}
void CGroupDlg::OnCreate(){
    CCreateGroupDlg dlg(this);
    if(dlg.DoModal()!=IDOK) return;
    g_ctx.net.Send(Pack("GROUP_CREATE",{EncodeWireText(CS(dlg.name_)),dlg.requireApproval_?"1":"0"}));
    // 结果 GROUP_CREATE_RESP 由 MainDlg 处理后会重发 GROUP_LIST 刷新
}
static int SelRow(CListCtrl& lc){ POSITION p=lc.GetFirstSelectedItemPosition(); return p?lc.GetNextSelectedItem(p):-1; }
void CGroupDlg::OnOpen(){
    int r=SelRow(mine_); if(r<0||r>=(int)mineIds_.size()){ AfxMessageBox(_T("请选择一个群")); return; }
    main_->OpenGroupChat((int)mineIds_[r], mineNames_[r]);
}
void CGroupDlg::OnDblClkMine(NMHDR*,LRESULT* pr){ OnOpen(); *pr=0; }
void CGroupDlg::OnSearch(){
    CString kw; GetDlgItemText(IDC_GROUP_SEARCH_KEYWORD,kw); kw.Trim();
    if(kw.IsEmpty()){ AfxMessageBox(_T("请输入群号或群名")); return; }
    g_ctx.net.Send(Pack("GROUP_SEARCH",{EncodeWireText(CS(kw))}));
}
void CGroupDlg::OnApply(){
    int r=SelRow(found_); if(r<0||r>=(int)foundIds_.size()){ AfxMessageBox(_T("请选择要加入的群")); return; }
    g_ctx.net.Send(Pack("GROUP_APPLY",{std::to_string(foundIds_[r])}));
}
