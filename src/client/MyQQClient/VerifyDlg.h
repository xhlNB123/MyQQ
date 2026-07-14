#pragma once
#include <afxwin.h>
#include "resource.h"

class CVerifyDlg : public CDialogEx {
public:
    CVerifyDlg(const CString& defaultText, CWnd* pParent = nullptr)
        : CDialogEx(IDD_VERIFY_DIALOG, pParent), text_(defaultText) {}
    enum { IDD = IDD_VERIFY_DIALOG };
    CString GetText() const { return text_; }
protected:
    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    void OnOK() override;
private:
    CString text_;
};
