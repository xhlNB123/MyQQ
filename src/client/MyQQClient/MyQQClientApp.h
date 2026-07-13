#pragma once
// =====================================================================
// 应用程序类：启动时显示登录对话框
// =====================================================================
#include "pch.h"

class CMyQQClientApp : public CWinApp {
public:
    CMyQQClientApp();
    virtual BOOL InitInstance();
    DECLARE_MESSAGE_MAP()
};

extern CMyQQClientApp theApp;
