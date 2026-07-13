#pragma once
// =====================================================================
// 登录对话框 CLoginDlg
// 对应 PPT：登录界面 + 查询本机主机名/IP + 输入远程服务端 IP/端口并连接
// =====================================================================
#include <afxwin.h>
#include "resource.h"

class CLoginDlg : public CDialogEx {
public:
    CLoginDlg(CWnd* pParent = nullptr);
    enum { IDD = IDD_LOGIN_DIALOG };

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;

    afx_msg void OnQueryLocal();     // 查询本机主机名与 IP
    afx_msg void OnLogin();          // 连接 + 登录
    afx_msg void OnOpenRegister();   // 打开注册对话框
    afx_msg void OnShowVersion();    // 右键菜单：版本查询
    afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
    afx_msg void OnTogglePwd();       // 眼睛按钮：切换密码明文/密文
    // 网络消息回调
    afx_msg LRESULT OnNetMessage(WPARAM w, LPARAM l);
    afx_msg LRESULT OnNetClosed(WPARAM w, LPARAM l);
    DECLARE_MESSAGE_MAP()

private:
    // 确保已连接服务端（读取界面上的 IP/端口）
    bool EnsureConnected();

    bool    m_pwdVisible = false;   // 密码是否明文显示
    CString m_account;
    CString m_password;
    CString m_serverIp;
    CString m_serverPort;
    CString m_localInfo;
};
