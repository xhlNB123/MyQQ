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
    afx_msg void OnTogglePwd();                    // 密码框 显/隐 切换
    afx_msg void OnTogglePwd2();                   // 确认密码框 显/隐 切换
    afx_msg LRESULT OnNetMessage(WPARAM w, LPARAM l);
    DECLARE_MESSAGE_MAP()

private:
    CString m_account;
    CString m_password;
    CString m_password2;
    CString m_nickname;
    bool    m_pwdVisible  = false;   // 密码是否明文显示
    bool    m_pwd2Visible = false;   // 确认密码是否明文显示
};
