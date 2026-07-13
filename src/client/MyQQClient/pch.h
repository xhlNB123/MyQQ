#pragma once
// =====================================================================
// 预编译头：MFC 客户端公共包含
// =====================================================================

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif

// 目标平台：Windows 7+
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <afxwin.h>         // MFC 核心与标准组件
#include <afxext.h>         // MFC 扩展
#include <afxdialogex.h>    // CDialogEx
#include <afxcmn.h>         // MFC 公共控件（CListCtrl 等）

#include "resource.h"
