// =====================================================================
// CLoginDlg 实现
// =====================================================================
#include "pch.h"
#include "resource.h"
#include "LoginDlg.h"
#include "RegisterDlg.h"
#include "MainDlg.h"
#include "AppContext.h"
#include "../../common/Message.h"
#include "../../common/Protocol.h"

// UTF-8(std::string) -> CString（服务端中文提示统一 UTF-8，避免乱码）
static CString U8ToCS(const std::string& s) {
    if (s.empty()) return CString();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    CStringW w; wchar_t* buf = w.GetBuffer(n);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), buf, n);
    w.ReleaseBuffer(n);
    return CString(w);
}

CLoginDlg::CLoginDlg(CWnd* pParent)
    : CDialogEx(IDD_LOGIN_DIALOG, pParent) {
    m_serverIp   = _T("127.0.0.1");
    m_serverPort = _T("6000");
}

void CLoginDlg::DoDataExchange(CDataExchange* pDX) {
    CDialogEx::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_LOGIN_ACCOUNT,     m_account);
    DDX_Text(pDX, IDC_LOGIN_PASSWORD,    m_password);
    DDX_Text(pDX, IDC_LOGIN_SERVER_IP,   m_serverIp);
    DDX_Text(pDX, IDC_LOGIN_SERVER_PORT, m_serverPort);
    DDX_Text(pDX, IDC_LOGIN_LOCAL_INFO,  m_localInfo);
}

BEGIN_MESSAGE_MAP(CLoginDlg, CDialogEx)
    ON_BN_CLICKED(IDC_QUERY_LOCAL_BTN, &CLoginDlg::OnQueryLocal)
    ON_BN_CLICKED(IDC_LOGIN_BTN,       &CLoginDlg::OnLogin)
    ON_BN_CLICKED(IDC_REGISTER_BTN,    &CLoginDlg::OnOpenRegister)
    ON_COMMAND(ID_SHOW_VERSION,        &CLoginDlg::OnShowVersion)
    ON_WM_CONTEXTMENU()
    ON_MESSAGE(WM_NET_MESSAGE, &CLoginDlg::OnNetMessage)
    ON_MESSAGE(WM_NET_CLOSED,  &CLoginDlg::OnNetClosed)
END_MESSAGE_MAP()

BOOL CLoginDlg::OnInitDialog() {
    CDialogEx::OnInitDialog();
    SetWindowText(_T("MyQQ 登录"));
    return TRUE;
}

// 查询本机主机名与 IP（对应客户端要点①）
void CLoginDlg::OnQueryLocal() {
    std::string host, ip;
    if (myqq::GetLocalHostInfo(host, ip)) {
        m_localInfo.Format(_T("本机: %S  IP: %S"), host.c_str(), ip.c_str());
    } else {
        m_localInfo = _T("获取本机信息失败");
    }
    UpdateData(FALSE);
}

// 连接服务端（读取界面 IP/端口）
bool CLoginDlg::EnsureConnected() {
    if (g_ctx.net.IsConnected()) return true;
    UpdateData(TRUE);
    CStringA ipA(m_serverIp);
    unsigned short port = (unsigned short)_ttoi(m_serverPort);
    if (port == 0) port = myqq::kDefaultPort;
    if (!g_ctx.net.Connect(std::string(ipA), port, GetSafeHwnd())) {
        AfxMessageBox(_T("连接服务端失败，请检查 IP/端口与防火墙"));
        return false;
    }
    g_ctx.serverIp   = std::string(ipA);
    g_ctx.serverPort = port;
    return true;
}

void CLoginDlg::OnLogin() {
    UpdateData(TRUE);
    if (m_account.IsEmpty() || m_password.IsEmpty()) {
        AfxMessageBox(_T("请输入账号和密码"));
        return;
    }
    if (!EnsureConnected()) return;

    CStringA acc(m_account), pwd(m_password);
    std::string line = myqq::Pack("LOGIN", { std::string(acc), std::string(pwd) });
    g_ctx.net.Send(line);
}

void CLoginDlg::OnOpenRegister() {
    if (!EnsureConnected()) return;
    // 注册期间把消息通知目标切给注册对话框
    CRegisterDlg dlg(this);
    g_ctx.net.SetNotifyWnd(dlg.GetSafeHwnd());  // 占位，真正切换在 dlg 内 OnInitDialog
    dlg.DoModal();
    // 注册对话框关闭后，消息通知目标切回登录窗口
    g_ctx.net.SetNotifyWnd(GetSafeHwnd());
}

// 右键菜单：查询软件版本（对应客户端要点⑥）
void CLoginDlg::OnContextMenu(CWnd* /*pWnd*/, CPoint point) {
    CMenu menu;
    menu.LoadMenu(IDR_CONTEXT_MENU);
    CMenu* pop = menu.GetSubMenu(0);
    if (pop) pop->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
}

void CLoginDlg::OnShowVersion() {
    CString msg;
    msg.Format(_T("软件版本：%S"), myqq::kAppVersion);
    AfxMessageBox(msg);
}

// 处理服务端推送的一行消息
LRESULT CLoginDlg::OnNetMessage(WPARAM, LPARAM lParam) {
    std::string* pLine = reinterpret_cast<std::string*>(lParam);
    if (!pLine) return 0;
    std::vector<std::string> t = myqq::Unpack(*pLine);
    delete pLine;
    if (t.empty()) return 0;

    if (t[0] == "LOGIN_RESP") {
        // LOGIN_RESP|status|userId   或   LOGIN_RESP|status|errmsg
        int status = (t.size() > 1) ? atoi(t[1].c_str()) : 1;
        if (status == 0 && t.size() > 2) {
            g_ctx.selfId      = atoi(t[2].c_str());
            CStringA accA(m_account);
            g_ctx.selfAccount = std::string(accA);
            if (g_ctx.selfNick.empty()) g_ctx.selfNick = std::string(accA);
            // 登录成功：结束登录框，返回 IDOK，由 App 拉起主窗口
            EndDialog(IDOK);
        } else {
            CString err = (t.size() > 2) ? U8ToCS(t[2]) : CString(_T("登录失败"));
            AfxMessageBox(err);
        }
    } else if (t[0] == "VERSION_RESP") {
        // 忽略（可显示）
    }
    return 0;
}

LRESULT CLoginDlg::OnNetClosed(WPARAM, LPARAM) {
    AfxMessageBox(_T("与服务端的连接已断开"));
    return 0;
}
