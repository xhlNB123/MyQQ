#pragma once
// =====================================================================
// 聊天窗口 CChatDlg（非模态）
// 对应 PPT：聊天界面 —— 收发消息 CHAT、显示历史、写聊天记录文件、版本查询
// =====================================================================
#include <afxwin.h>
#include "resource.h"
#include "../../common/ChatLogger.h"
#include <memory>

class CMainDlg;

class CChatDlg : public CDialogEx {
public:
    // peerId/peerNick：对方好友；pMain：所属主窗口（关闭时从其表中移除）
    CChatDlg(int peerId, const CString& peerNick, CMainDlg* pMain, CWnd* pParent = nullptr);
    virtual ~CChatDlg();
    enum { IDD = IDD_CHAT_DIALOG };

    int PeerId() const { return m_peerId; }
    // 由主窗口转发过来的一条对方消息（fromId==m_peerId）
    void OnIncoming(const CString& content);

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;
    virtual void PostNcDestroy() override;

    afx_msg void OnSend();                 // CHAT|selfId|peerId|content
    afx_msg void OnShowVersion();          // 右键菜单：版本查询
    afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
    afx_msg void OnClose();
    DECLARE_MESSAGE_MAP()

private:
    void AppendLine(const CString& who, const CString& text);

    int      m_peerId;
    CString  m_peerNick;
    CMainDlg* m_pMain;
    std::unique_ptr<myqq::ChatLogger> m_logger;   // 聊天记录写文件
};
