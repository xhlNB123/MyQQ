#pragma once
#include <afxwin.h>
#include <afxcmn.h>
#include <map>
#include <vector>
#include <string>
#include "resource.h"
#include "AppContext.h"
#include "FriendRequestsDlg.h"
#include "ChatDlg.h"

class CProfileDlg;

class CMainDlg : public CDialogEx {
public:
    CMainDlg(CWnd* pParent = nullptr);
    virtual ~CMainDlg();
    enum { IDD = IDD_MAIN_DIALOG };
    void OnChatClosed(int friendId);
    void OnGroupChatClosed(int groupId);
    void OpenGroupChat(int groupId, const CString& name);
    void OpenGroupMembers(int groupId, const CString& name);
    void ViewProfile(int userId);          // 请求查看某用户资料
    void InviteToGroup(int groupId, int friendId);
    unsigned long long NextRequestId() { return ++nextRequestId_; }
protected:
    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    afx_msg void OnSearch();
    afx_msg void OnRefresh();
    afx_msg void OnOpenChat();
    afx_msg void OnProfile();
    afx_msg void OnSettings();
    afx_msg void OnFriendRequests();
    afx_msg void OnGroups();          // 打开群聊 hub
    afx_msg void OnSearchGroup();     // 查找群
    afx_msg void OnViewProfileBtn();  // 查看选中好友资料
    afx_msg void OnDblClkFriend(NMHDR*, LRESULT*);
    afx_msg LRESULT OnNetMessage(WPARAM, LPARAM);
    afx_msg LRESULT OnNetClosed(WPARAM, LPARAM);
    afx_msg void OnDestroy();
    DECLARE_MESSAGE_MAP()
private:
    void HandleLine(const std::string& line);
    void RequestFriendList();
    void OpenChatWith(int friendId, const CString& nick);
    void DeliverChat(const ClientChatMessage& m);
    int SelectedFriendId();
    CString SelectedFriendNick();
    void UpdateRequestButton();
    void CloseChatWindows();
    void FinishLogout(INT_PTR result);

    CListCtrl friendList_;
    std::map<int, CChatDlg*> chatWnds_;      // 一对一，键 peerId
    std::map<int, CChatDlg*> groupWnds_;     // 群聊，键 groupId
    std::vector<ClientFriendRequest> friendRequests_;
    CFriendRequestsDlg* requestsDlg_ = nullptr;
    CProfileDlg* profileDlg_ = nullptr;
    CWnd* groupDlg_ = nullptr;               // 群 hub（打开时接收 GROUP_LIST/SEARCH 结果）
    CWnd* membersDlg_ = nullptr;             // 群成员窗
    CWnd* viewProfileDlg_ = nullptr;         // 查看资料窗（等 VIEW_PROFILE_RESP）
    unsigned long long nextRequestId_ = 0;
    INT_PTR logoutResult_ = ID_MAIN_EXIT_APP;
    bool logoutPending_ = false;
    bool destroyingChats_ = false;
};
