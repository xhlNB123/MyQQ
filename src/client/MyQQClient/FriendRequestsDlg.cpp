#include "pch.h"
#include "FriendRequestsDlg.h"
#include "AppContext.h"
#include "../../common/Message.h"

void CFriendRequestsDlg::DoDataExchange(CDataExchange* pDX) {
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_REQUEST_LIST, list_);
}

BEGIN_MESSAGE_MAP(CFriendRequestsDlg, CDialogEx)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_REQUEST_LIST, &CFriendRequestsDlg::OnSelectionChanged)
    ON_BN_CLICKED(IDC_REQUEST_ACCEPT_BTN, &CFriendRequestsDlg::OnAccept)
    ON_BN_CLICKED(IDC_REQUEST_REJECT_BTN, &CFriendRequestsDlg::OnReject)
END_MESSAGE_MAP()

BOOL CFriendRequestsDlg::OnInitDialog() {
    CDialogEx::OnInitDialog();
    list_.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    list_.InsertColumn(0, _T("QQ号"), LVCFMT_LEFT, 60);
    list_.InsertColumn(1, _T("昵称"), LVCFMT_LEFT, 90);
    list_.InsertColumn(2, _T("时间"), LVCFMT_LEFT, 115);
    RefreshList();
    return TRUE;
}

void CFriendRequestsDlg::RefreshList() {
    list_.DeleteAllItems();
    for (size_t i = 0; i < requests_.size(); ++i) {
        CString id; id.Format(_T("%d"), requests_[i].senderId);
        int row = list_.InsertItem(static_cast<int>(i), id);
        list_.SetItemText(row, 1, requests_[i].senderNick);
        list_.SetItemText(row, 2, requests_[i].createdTime);
    }
    SetDlgItemText(IDC_REQUEST_DETAIL, _T(""));
}

void CFriendRequestsDlg::OnSelectionChanged(NMHDR*, LRESULT* result) {
    POSITION pos = list_.GetFirstSelectedItemPosition();
    if (pos) {
        int row = list_.GetNextSelectedItem(pos);
        if (row >= 0 && static_cast<size_t>(row) < requests_.size())
            SetDlgItemText(IDC_REQUEST_DETAIL, requests_[row].verifyText);
    }
    *result = 0;
}

void CFriendRequestsDlg::Resolve(bool accept) {
    POSITION pos = list_.GetFirstSelectedItemPosition();
    if (!pos) { AfxMessageBox(_T("请先选择一条好友申请")); return; }
    int row = list_.GetNextSelectedItem(pos);
    if (row < 0 || static_cast<size_t>(row) >= requests_.size()) return;
    g_ctx.net.Send(myqq::Pack("FRIEND_ACK", {
        std::to_string(requests_[row].requestId), accept ? "1" : "0"}));
    GetDlgItem(IDC_REQUEST_ACCEPT_BTN)->EnableWindow(FALSE);
    GetDlgItem(IDC_REQUEST_REJECT_BTN)->EnableWindow(FALSE);
}

void CFriendRequestsDlg::OnAccept() { Resolve(true); }
void CFriendRequestsDlg::OnReject() { Resolve(false); }
