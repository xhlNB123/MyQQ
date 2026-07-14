#pragma once
#include <afxwin.h>
#include "resource.h"

class CSettingsDlg : public CDialogEx {
public:
    CSettingsDlg(CWnd* pParent = nullptr) : CDialogEx(IDD_SETTINGS_DIALOG, pParent) {}
    enum { IDD = IDD_SETTINGS_DIALOG };
    enum Action { None, SwitchAccount, ExitApplication };
    Action GetAction() const { return action_; }
protected:
    BOOL OnInitDialog() override;
    afx_msg void OnSwitchAccount();
    afx_msg void OnExitApplication();
    DECLARE_MESSAGE_MAP()
private:
    Action action_ = None;
};
