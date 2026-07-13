#pragma once
// =====================================================================
// 个人设置对话框 CProfileDlg（模态）
// 对应 PPT：个人设置界面 —— 昵称/性别/星座/血型/头像/签名 编辑
//   打开时发 GET_PROFILE 拉取当前资料，保存时发 UPDATE_PROFILE。
// =====================================================================
#include <afxwin.h>
#include <vector>
#include <string>
#include "resource.h"

class CProfileDlg : public CDialogEx {
public:
    CProfileDlg(CWnd* pParent = nullptr);
    enum { IDD = IDD_PROFILE_DIALOG };

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;

    afx_msg void OnSave();                            // UPDATE_PROFILE
    afx_msg LRESULT OnNetMessage(WPARAM w, LPARAM l); // 收 GET/UPDATE_PROFILE_RESP
    DECLARE_MESSAGE_MAP()

private:
    void FillProfile(const std::vector<std::string>& t);  // 用响应填充控件

    CComboBox m_gender;   // 未知/男/女
    CComboBox m_star;     // 12 星座
    CComboBox m_blood;    // A/B/O/AB/未知
    CComboBox m_avatar;   // 头像 1..8
    CString   m_account;
    CString   m_nickname;
    CString   m_signature;
    bool      m_loaded = false;   // GET_PROFILE 是否已返回并填充（防止未加载就保存清空资料）
};
