#pragma once
#include <afxwin.h>
#include <afxcmn.h>
#include <vector>
#include <string>
#include "resource.h"

class CMainDlg;

// 群聊 hub：我的群列表 + 创建群 + 查找群/申请加入
class CGroupDlg : public CDialogEx {
public:
    CGroupDlg(CMainDlg* main, CWnd* pParent = nullptr) : CDialogEx(IDD_GROUP_DIALOG, pParent), main_(main) {}
    enum { IDD = IDD_GROUP_DIALOG };
    void OnGroupList(const std::vector<std::string>& t);    // GROUP_LIST_RESP
    void OnGroupSearch(const std::vector<std::string>& t);  // GROUP_SEARCH_RESP
protected:
    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    afx_msg void OnCreate();
    afx_msg void OnOpen();
    afx_msg void OnSearch();
    afx_msg void OnApply();
    afx_msg void OnDblClkMine(NMHDR*, LRESULT*);
    DECLARE_MESSAGE_MAP()
private:
    CMainDlg* main_;
    CListCtrl mine_, found_;
    std::vector<long long> mineIds_, foundIds_;
    std::vector<CString> mineNames_;
};
