#pragma once
#include <afxwin.h>
#include "resource.h"

class CCreateGroupDlg : public CDialogEx {
public:
    CCreateGroupDlg(CWnd* pParent = nullptr) : CDialogEx(IDD_CREATE_GROUP_DIALOG, pParent) {}
    enum { IDD = IDD_CREATE_GROUP_DIALOG };
    CString name_;
    int requireApproval_ = 0;
protected:
    void DoDataExchange(CDataExchange* pDX) override;
    void OnOK() override;
};
