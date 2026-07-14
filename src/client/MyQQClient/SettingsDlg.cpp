#include "pch.h"
#include "SettingsDlg.h"
#include "AppContext.h"
#include "../../common/Protocol.h"

BEGIN_MESSAGE_MAP(CSettingsDlg, CDialogEx)
    ON_BN_CLICKED(IDC_SETTINGS_SWITCH_BTN, &CSettingsDlg::OnSwitchAccount)
    ON_BN_CLICKED(IDC_SETTINGS_EXIT_BTN, &CSettingsDlg::OnExitApplication)
END_MESSAGE_MAP()

BOOL CSettingsDlg::OnInitDialog() {
    CDialogEx::OnInitDialog();
    CString info;
    info.Format(_T("账号：%S\r\nQQ号：%d\r\n服务器：%S:%u\r\n连接：%s\r\n版本：%S"),
        g_ctx.selfAccount.c_str(), g_ctx.selfId, g_ctx.serverIp.c_str(), g_ctx.serverPort,
        g_ctx.net.IsConnected() ? _T("已连接") : _T("已断开"), myqq::kAppVersion);
    SetDlgItemText(IDC_SETTINGS_INFO, info);
    return TRUE;
}

void CSettingsDlg::OnSwitchAccount() {
    action_ = SwitchAccount;
    EndDialog(IDOK);
}

void CSettingsDlg::OnExitApplication() {
    action_ = ExitApplication;
    EndDialog(IDOK);
}
