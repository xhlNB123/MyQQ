#include "pch.h"
#include "VerifyDlg.h"

void CVerifyDlg::DoDataExchange(CDataExchange* pDX) {
    CDialogEx::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_VERIFY_TEXT, text_);
}

BOOL CVerifyDlg::OnInitDialog() {
    CDialogEx::OnInitDialog();
    UpdateData(FALSE);
    return TRUE;
}

void CVerifyDlg::OnOK() {
    UpdateData(TRUE);
    CStringA utf8(text_);
    if (utf8.GetLength() > 200) {
        AfxMessageBox(_T("验证信息不能超过 200 字节"));
        return;
    }
    CDialogEx::OnOK();
}
