// =====================================================================
// MFC 应用入口。以「登录对话框」为主窗口启动；登录成功后由登录框
// 负责拉起主窗口。整个进程共享一个 AppContext（含网络连接）。
// =====================================================================
#include "pch.h"
#include "MyQQClientApp.h"
#include "AppContext.h"
#include "LoginDlg.h"
#include "MainDlg.h"
#include "../../common/Socket.h"

// 全局上下文实例
AppContext g_ctx;

CMyQQClientApp theApp;

BEGIN_MESSAGE_MAP(CMyQQClientApp, CWinApp)
END_MESSAGE_MAP()

CMyQQClientApp::CMyQQClientApp() {}

BOOL CMyQQClientApp::InitInstance()
{
    CWinApp::InitInstance();

    // 初始化 Winsock（整个进程一次）
    if (!myqq::InitWinsock()) {
        AfxMessageBox(_T("Winsock 初始化失败，无法进行网络通信。"));
        return FALSE;
    }

    // 先跑登录框（模态）；登录成功（EndDialog(IDOK)）后再跑主窗口（模态）。
    // 主窗口内的聊天窗口为非模态子窗口，模态消息循环仍会为其派发消息。
    {
        CLoginDlg login;
        if (login.DoModal() == IDOK && g_ctx.selfId > 0) {
            CMainDlg mainDlg;
            m_pMainWnd = &mainDlg;
            mainDlg.DoModal();
        }
    }

    // 退出清理
    g_ctx.net.Close();
    myqq::CleanupWinsock();

    // 所有窗口已关闭，结束程序
    return FALSE;
}
