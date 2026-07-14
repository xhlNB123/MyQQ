// =====================================================================
// MFC 应用入口。以「登录对话框」为主窗口启动；登录成功后由登录框
// 负责拉起主窗口。整个进程共享一个 AppContext（含网络连接）。
// =====================================================================
#include "pch.h"
#include <gdiplus.h>
#include "MyQQClientApp.h"
#include "AppContext.h"
#include "LoginDlg.h"
#include "MainDlg.h"
#include "../../common/Socket.h"
#pragma comment(lib, "gdiplus.lib")

// 全局上下文实例
AppContext g_ctx;
static ULONG_PTR g_gdiplusToken = 0;

CMyQQClientApp theApp;

BEGIN_MESSAGE_MAP(CMyQQClientApp, CWinApp)
END_MESSAGE_MAP()

CMyQQClientApp::CMyQQClientApp() {}

BOOL CMyQQClientApp::InitInstance()
{
    CWinApp::InitInstance();

    // 初始化 GDI+（图片缩略图/预览用）
    Gdiplus::GdiplusStartupInput gdiIn;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiIn, nullptr);

    // 初始化 Winsock（整个进程一次）
    if (!myqq::InitWinsock()) {
        AfxMessageBox(_T("Winsock 初始化失败，无法进行网络通信。"));
        return FALSE;
    }

    // 登录 → 主窗口循环；主窗口返回“切换账号”时重新显示登录窗口。
    // 注意：这里不把任何模态对话框设为 m_pMainWnd。否则该窗口 EndDialog 销毁时
    // MFC 会投递 WM_QUIT，导致紧接着的下一个 DoModal 立即退出（表现为“点登录就结束进程”）。
    bool running = true;
    while (running) {
        CLoginDlg login;
        if (login.DoModal() != IDOK || g_ctx.selfId <= 0) break;

        CMainDlg mainDlg;
        INT_PTR result = mainDlg.DoModal();
        if (result == ID_MAIN_SWITCH_ACCOUNT) {
            g_ctx.ResetSessionState();
            continue;
        }
        running = false;
    }

    // 退出清理
    g_ctx.net.Close();
    myqq::CleanupWinsock();
    if (g_gdiplusToken) Gdiplus::GdiplusShutdown(g_gdiplusToken);

    // 所有窗口已关闭，结束程序
    return FALSE;
}
