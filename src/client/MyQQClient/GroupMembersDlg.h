#pragma once
#include <afxwin.h>
#include <afxcmn.h>
#include <vector>
#include <string>
#include "resource.h"

class CMainDlg;

class CGroupMembersDlg : public CDialogEx {
public:
    CGroupMembersDlg(CMainDlg* main, int groupId, const CString& name, CWnd* pParent = nullptr)
        : CDialogEx(IDD_GROUP_MEMBERS_DIALOG, pParent), main_(main), groupId_(groupId), name_(name) {}
    enum { IDD = IDD_GROUP_MEMBERS_DIALOG };
    void OnMembers(const std::vector<std::string>& t);   // GROUP_MEMBERS_RESP
protected:
    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    afx_msg void OnView();
    afx_msg void OnInvite();
    DECLARE_MESSAGE_MAP()
private:
    CMainDlg* main_;
    int groupId_;
    CString name_;
    CListCtrl list_;
    std::vector<int> ids_;
};
