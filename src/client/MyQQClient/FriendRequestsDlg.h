#pragma once
#include <afxwin.h>
#include <afxcmn.h>
#include <vector>
#include <string>
#include "resource.h"

struct ClientFriendRequest {
    long long requestId = 0;
    int senderId = 0;
    CString senderAccount;
    CString senderNick;
    CString verifyText;
    CString createdTime;
};

class CFriendRequestsDlg : public CDialogEx {
public:
    CFriendRequestsDlg(std::vector<ClientFriendRequest>& requests, CWnd* pParent = nullptr)
        : CDialogEx(IDD_FRIEND_REQUESTS_DIALOG, pParent), requests_(requests) {}
    enum { IDD = IDD_FRIEND_REQUESTS_DIALOG };
protected:
    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    afx_msg void OnSelectionChanged(NMHDR*, LRESULT* result);
    afx_msg void OnAccept();
    afx_msg void OnReject();
    DECLARE_MESSAGE_MAP()
private:
    void RefreshList();
    void Resolve(bool accept);
    CListCtrl list_;
    std::vector<ClientFriendRequest>& requests_;
};
