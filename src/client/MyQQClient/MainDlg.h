#pragma once
#include <afxwin.h>
#include <afxcmn.h>
#include <map>
#include <vector>
#include <string>
#include "resource.h"
#include "AppContext.h"
#include "FriendRequestsDlg.h"

class CChatDlg;
class CProfileDlg;

class CMainDlg : public CDialogEx {
public:
    CMainDlg(CWnd* pParent = nullptr);
    virtual ~CMainDlg();
    enum { IDD = IDD_MAIN_DIALOG };
    void OnChatClosed(int friendId);
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
    afx_msg void OnDblClkFriend(NMHDR*, LRESULT*);
    afx_msg LRESULT OnNetMessage(WPARAM, LPARAM);
    afx_msg LRESULT OnNetClosed(WPARAM, LPARAM);
    afx_msg void OnDestroy();
    DECLARE_MESSAGE_MAP()
private:
    void HandleLine(const std::string& line);
    void RequestFriendList();
    void OpenChatWith(int friendId, const CString& nick);
    void DeliverChat(long long msgId, int fromId, int toId,
                     const CString& sendTime, const CString& text);
    int SelectedFriendId();
    CString SelectedFriendNick();
    void UpdateRequestButton();
    void CloseChatWindows();
    void FinishLogout(INT_PTR result);

    CListCtrl friendList_;
    std::map<int, CChatDlg*> chatWnds_;
    std::vector<ClientFriendRequest> friendRequests_;
    CFriendRequestsDlg* requestsDlg_ = nullptr;
    CProfileDlg* profileDlg_ = nullptr;
    unsigned long long nextRequestId_ = 0;
    INT_PTR logoutResult_ = ID_MAIN_EXIT_APP;
    bool logoutPending_ = false;
    bool destroyingChats_ = false;
};
