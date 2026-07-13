// =====================================================================
// 注册对话框实现
// 说明：注册复用 g_ctx.net 的连接（登录窗口已连上服务端后再打开注册）。
//       提交后同步等待一次 REGISTER_RESP，避免与登录窗口的接收回调冲突，
//       这里在打开注册框前由调用方把通知窗口切到本对话框。
// =====================================================================
#include "pch.h"
#include "RegisterDlg.h"
#include "AppContext.h"
#include "../../common/Message.h"
#include "../../common/Protocol.h"
#include <vector>
#include <string>

CRegisterDlg::CRegisterDlg(CWnd* pParent) : CDialogEx(IDD, pParent) {}

void CRegisterDlg::DoDataExchange(CDataExchange* pDX) {
    CDialogEx::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_REG_ACCOUNT,  m_account);
    DDX_Text(pDX, IDC_REG_PASSWORD, m_password);
    DDX_Text(pDX, IDC_REG_PASSWORD2, m_password2);
    DDX_Text(pDX, IDC_REG_NICKNAME, m_nickname);
}

BEGIN_MESSAGE_MAP(CRegisterDlg, CDialogEx)
    ON_BN_CLICKED(IDC_REG_SUBMIT_BTN, &CRegisterDlg::OnSubmit)
    ON_MESSAGE(WM_NET_MESSAGE, &CRegisterDlg::OnNetMessage)
END_MESSAGE_MAP()

void CRegisterDlg::OnSubmit() {
    UpdateData(TRUE);
    if (m_account.IsEmpty() || m_password.IsEmpty()) {
        AfxMessageBox(_T("账号和密码不能为空"));
        return;
    }
    if (m_password != m_password2) {
        AfxMessageBox(_T("两次输入的密码不一致"));
        return;
    }
    if (!g_ctx.net.IsConnected()) {
        AfxMessageBox(_T("尚未连接服务端，请先在登录窗口连接"));
        return;
    }
    // 接收通知切到本窗口，收 REGISTER_RESP
    g_ctx.net.SetNotifyWnd(GetSafeHwnd());

    CT2A account(m_account), password(m_password), nick(m_nickname);
    std::string line = myqq::Pack("REGISTER",
        { std::string(account), std::string(password), std::string(nick) });
    g_ctx.net.Send(line);
}

LRESULT CRegisterDlg::OnNetMessage(WPARAM, LPARAM lParam) {
    std::string* pLine = reinterpret_cast<std::string*>(lParam);
    std::vector<std::string> t = myqq::Unpack(*pLine);
    delete pLine;

    if (!t.empty() && t[0] == "REGISTER_RESP") {
        int status = t.size() > 1 ? atoi(t[1].c_str()) : 1;
        if (status == 0) {
            CString msg;
            msg.Format(_T("注册成功，你的账号 UserId = %d，请返回登录。"),
                       t.size() > 2 ? atoi(t[2].c_str()) : 0);
            AfxMessageBox(msg);
            EndDialog(IDOK);
        } else {
            CString err = t.size() > 2 ? CString(CA2T(t[2].c_str())) : _T("注册失败");
            AfxMessageBox(err);
        }
    }
    return 0;
}
