#pragma once
#include <afxwin.h>
#include <vector>
#include <string>
#include "resource.h"

// 只读查看他人资料。由 MainDlg 收到 VIEW_PROFILE_RESP 后调用 Fill。
class CViewProfileDlg : public CDialogEx {
public:
    CViewProfileDlg(CWnd* pParent = nullptr) : CDialogEx(IDD_VIEW_PROFILE_DIALOG, pParent) {}
    enum { IDD = IDD_VIEW_PROFILE_DIALOG };
    void Fill(const std::vector<std::string>& t);   // VIEW_PROFILE_RESP tokens
protected:
    BOOL OnInitDialog() override;
    DECLARE_MESSAGE_MAP()
private:
    CString account_, nick_, gender_, star_, blood_, sign_;
};
