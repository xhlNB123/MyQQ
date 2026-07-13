#pragma once
// =====================================================================
// 登录后主窗口 CMainDlg
// 对应 PPT：登录后主界面 —— 好友列表显示、查找/添加好友、进入聊天
// =====================================================================
#include <afxwin.h>
#include <afxcmn.h>
#include <map>
#include "resource.h"

class CChatDlg;

class CMainDlg : public CDialogEx {
public:
    CMainDlg(CWnd* pParent = nullptr);
    virtual ~CMainDlg();
    enum { IDD = IDD_MAIN_DIALOG };

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;

    afx_msg void OnSearch();        // SEARCH|keyword
    afx_msg void OnRefresh();       // FRIEND_LIST|selfId
    afx_msg void OnOpenChat();      // 打开选中好友的聊天窗口
    afx_msg void OnProfile();       // 打开个人设置窗体
    afx_msg void OnDblClkFriend(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg LRESULT OnNetMessage(WPARAM w, LPARAM l);
    afx_msg LRESULT OnNetClosed(WPARAM w, LPARAM l);
    afx_msg void OnDestroy();
    DECLARE_MESSAGE_MAP()

public:
    // 由聊天窗口在关闭(PostNcDestroy)时回调，将自己从会话表移除
    void OnChatClosed(int friendId);

private:
    void RequestFriendList();
    void OpenChatWith(int friendId, const CString& nick);
    void DeliverChat(int fromId, const CString& text);   // 分发收到的聊天消息
    int  SelectedFriendId();                              // 当前选中好友 UserId
    CString SelectedFriendNick();                         // 当前选中好友昵称

    CListCtrl                  m_friendList;   // 好友列表（报表视图）
    // 已打开的聊天窗口：friendId -> 窗口指针（非模态）
    std::map<int, CChatDlg*>   m_chatWnds;
};
