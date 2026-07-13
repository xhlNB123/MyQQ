#pragma once
// =====================================================================
// 注册对话框 CRegisterDlg
// 对应 PPT：注册界面 —— REGISTER|account|password|nickname
// =====================================================================
#include <afxwin.h>
#include "resource.h"

class CRegisterDlg : public CDialogEx {
public:
    CRegisterDlg(CWnd* pParent = nullptr);
    enum { IDD = IDD_REGISTER_DIALOG };

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;

    afx_msg void OnSubmit();                       // 提交注册
    afx_msg LRESULT OnNetMessage(WPARAM w, LPARAM l);
    DECLARE_MESSAGE_MAP()

private:
    CString m_account;
    CString m_password;
    CString m_password2;
    CString m_nickname;
};
