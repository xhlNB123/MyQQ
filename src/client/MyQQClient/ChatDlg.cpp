// =====================================================================
// CChatDlg 实现：与某个好友的一对一聊天窗口（非模态）
//   - 发送：CHAT|selfId|peerId|content
//   - 接收：由 CMainDlg 收到 CHAT_PUSH 后调用 OnIncoming 转发进来
//   - 每条消息写入聊天记录文件（ChatLogger）
//   - 右键菜单查询软件版本（kAppVersion）
// =====================================================================
#include "pch.h"
#include "ChatDlg.h"
#include "MainDlg.h"
#include "AppContext.h"
#include "../../common/Message.h"
#include "../../common/Protocol.h"

using namespace myqq;

// ---- UTF-8 <-> CString 互转（传输统一用 UTF-8）----
static CString U8ToCS(const std::string& s) {
    if (s.empty()) return CString();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    CStringW w; wchar_t* buf = w.GetBuffer(n);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), buf, n);
    w.ReleaseBuffer(n);
    return CString(w);
}
static std::string CSToU8(const CString& cs) {
    CStringW w(cs);
    if (w.IsEmpty()) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, w, w.GetLength(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, w.GetLength(), &s[0], n, nullptr, nullptr);
    return s;
}

CChatDlg::CChatDlg(int peerId, const CString& peerNick, CMainDlg* pMain, CWnd* pParent)
    : CDialogEx(IDD_CHAT_DIALOG, pParent),
      m_peerId(peerId), m_peerNick(peerNick), m_pMain(pMain) {}

CChatDlg::~CChatDlg() {}

void CChatDlg::DoDataExchange(CDataExchange* pDX) {
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CChatDlg, CDialogEx)
    ON_BN_CLICKED(IDC_CHAT_SEND_BTN, &CChatDlg::OnSend)
    ON_COMMAND(ID_SHOW_VERSION, &CChatDlg::OnShowVersion)
    ON_WM_CONTEXTMENU()
    ON_WM_CLOSE()
END_MESSAGE_MAP()

BOOL CChatDlg::OnInitDialog() {
    CDialogEx::OnInitDialog();

    CString title;
    title.Format(_T("与 %s (ID:%d) 聊天中"), (LPCTSTR)m_peerNick, m_peerId);
    SetWindowText(title);

    // 建立本会话的聊天记录文件：logs/chat_selfId_peerId.txt
    m_logger = std::make_unique<ChatLogger>("logs", g_ctx.selfId, m_peerId);

    return TRUE;
}

// 追加一行到历史文本框，并滚动到底部
void CChatDlg::AppendLine(const CString& who, const CString& text) {
    CEdit* pHist = (CEdit*)GetDlgItem(IDC_CHAT_HISTORY);
    if (!pHist) return;

    SYSTEMTIME st; GetLocalTime(&st);
    CString line;
    line.Format(_T("[%02d:%02d:%02d] %s: %s\r\n"),
                st.wHour, st.wMinute, st.wSecond, (LPCTSTR)who, (LPCTSTR)text);

    int len = pHist->GetWindowTextLength();
    pHist->SetSel(len, len);
    pHist->ReplaceSel(line);
}

void CChatDlg::OnSend() {
    CString text; GetDlgItemText(IDC_CHAT_INPUT, text);
    text.Trim();
    if (text.IsEmpty()) return;
    if (!g_ctx.net.IsConnected()) { AfxMessageBox(_T("连接已断开")); return; }

    // CHAT|fromId|toId|content
    g_ctx.net.Send(Pack("CHAT", {
        std::to_string(g_ctx.selfId),
        std::to_string(m_peerId),
        CSToU8(text)
    }));

    AppendLine(_T("我"), text);
    m_logger->Append(std::to_string(g_ctx.selfId), CSToU8(text));

    SetDlgItemText(IDC_CHAT_INPUT, _T(""));   // 清空输入框
    GetDlgItem(IDC_CHAT_INPUT)->SetFocus();
}

// 主窗口收到 CHAT_PUSH 后转发进来的对方消息
void CChatDlg::OnIncoming(const CString& content) {
    AppendLine(m_peerNick, content);
    m_logger->Append(std::to_string(m_peerId), CSToU8(content));
}

// 右键菜单：查询软件版本
void CChatDlg::OnContextMenu(CWnd* /*pWnd*/, CPoint point) {
    CMenu menu;
    menu.CreatePopupMenu();
    menu.AppendMenu(MF_STRING, ID_SHOW_VERSION, _T("查询软件版本"));
    if (point.x == -1 && point.y == -1) {  // 键盘触发
        CRect rc; GetWindowRect(&rc);
        point = rc.TopLeft();
    }
    menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
}

void CChatDlg::OnShowVersion() {
    AfxMessageBox(CString(_T("软件版本：")) + CString(myqq::kAppVersion));
}

void CChatDlg::OnClose() {
    DestroyWindow();   // 非模态：销毁触发 PostNcDestroy
}

void CChatDlg::PostNcDestroy() {
    // 从主窗口的会话表中移除自己，然后释放
    if (m_pMain) m_pMain->OnChatClosed(m_peerId);
    delete this;
}
