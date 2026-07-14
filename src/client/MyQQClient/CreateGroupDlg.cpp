#include "pch.h"
#include "CreateGroupDlg.h"

void CCreateGroupDlg::DoDataExchange(CDataExchange* pDX){
    CDialogEx::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_CREATE_GROUP_NAME, name_);
    DDX_Check(pDX, IDC_CREATE_GROUP_APPROVAL, requireApproval_);
}
void CCreateGroupDlg::OnOK(){
    UpdateData(TRUE);
    name_.Trim();
    if(name_.IsEmpty()){ AfxMessageBox(_T("请输入群名")); return; }
    if(name_.Find(_T('|'))>=0){ AfxMessageBox(_T("群名不能包含 '|'")); return; }
    CDialogEx::OnOK();
}
