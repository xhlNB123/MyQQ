// =====================================================================
// CMainDlg 实现：好友列表 / 查找添加 / 进入聊天 / 接收系统与聊天推送
// =====================================================================
#include "pch.h"
#include "MainDlg.h"
#include "ChatDlg.h"
#include "ProfileDlg.h"
#include "AppContext.h"
#include "../../common/Message.h"
#include "../../common/Protocol.h"

using namespace myqq;

// UTF-8(std::string) <-> CString(宽字符) 转换（与服务端保持 UTF-8 传输）
static CString U8ToCS(const std::string& s) {
    if (s.empty()) return CString();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    CStringW w; wchar_t* buf = w.GetBuffer(n);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), buf, n);
    w.ReleaseBuffer(n);
    return CString(w);
}

CMainDlg::CMainDlg(CWnd* pParent) : CDialogEx(IDD_MAIN_DIALOG, pParent) {}
CMainDlg::~CMainDlg() {}

void CMainDlg::DoDataExchange(CDataExchange* pDX) {
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_FRIEND_LIST, m_friendList);
}

BEGIN_MESSAGE_MAP(CMainDlg, CDialogEx)
    ON_BN_CLICKED(IDC_SEARCH_BTN,    &CMainDlg::OnSearch)
    ON_BN_CLICKED(IDC_REFRESH_BTN,   &CMainDlg::OnRefresh)
    ON_BN_CLICKED(IDC_OPEN_CHAT_BTN, &CMainDlg::OnOpenChat)
    ON_BN_CLICKED(IDC_PROFILE_BTN,   &CMainDlg::OnProfile)
    ON_NOTIFY(NM_DBLCLK, IDC_FRIEND_LIST, &CMainDlg::OnDblClkFriend)
    ON_MESSAGE(WM_NET_MESSAGE, &CMainDlg::OnNetMessage)
    ON_MESSAGE(WM_NET_CLOSED,  &CMainDlg::OnNetClosed)
    ON_WM_DESTROY()
END_MESSAGE_MAP()

BOOL CMainDlg::OnInitDialog() {
    CDialogEx::OnInitDialog();

    // 网络推送改为通知本窗口
    g_ctx.net.SetNotifyWnd(GetSafeHwnd());

    // 好友列表列：ID / 昵称 / 状态
    m_friendList.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    m_friendList.InsertColumn(0, _T("ID"),   LVCFMT_LEFT, 60);
    m_friendList.InsertColumn(1, _T("昵称"), LVCFMT_LEFT, 120);
    m_friendList.InsertColumn(2, _T("状态"), LVCFMT_LEFT, 60);

    CString title;
    title.Format(_T("MyQQ - %s (ID:%d)"), U8ToCS(g_ctx.selfNick), g_ctx.selfId);
    SetWindowText(title);

    RequestFriendList();
    return TRUE;
}

void CMainDlg::RequestFriendList() {
    g_ctx.net.Send(Pack("FRIEND_LIST", { std::to_string(g_ctx.selfId) }));
}

void CMainDlg::OnRefresh() { RequestFriendList(); }

// 打开个人设置（模态）。ProfileDlg 内部会把网络通知切给自己；
// 关闭后这里切回主窗口，并按最新昵称刷新标题。
void CMainDlg::OnProfile() {
    CProfileDlg dlg(this);
    dlg.DoModal();
    g_ctx.net.SetNotifyWnd(GetSafeHwnd());   // 通知目标切回主窗口

    CString title;
    title.Format(_T("MyQQ - %s (ID:%d)"), U8ToCS(g_ctx.selfNick).GetString(), g_ctx.selfId);
    SetWindowText(title);
    RequestFriendList();                     // 昵称可能变了，刷新列表
}

void CMainDlg::OnSearch() {
    CString kw; GetDlgItemText(IDC_SEARCH_KEYWORD, kw);
    if (kw.IsEmpty()) { AfxMessageBox(_T("请输入账号或昵称关键字")); return; }
    CStringA kwUtf8;
    { // CString -> UTF-8
        int n = WideCharToMultiByte(CP_UTF8, 0, kw, -1, nullptr, 0, nullptr, nullptr);
        char* buf = kwUtf8.GetBuffer(n);
        WideCharToMultiByte(CP_UTF8, 0, kw, -1, buf, n, nullptr, nullptr);
        kwUtf8.ReleaseBuffer();
    }
    g_ctx.net.Send(Pack("SEARCH", { std::string(kwUtf8) }));
}

// 取当前选中好友的 UserId（列 0 存的是 ID 文本）
int CMainDlg::SelectedFriendId() {
    POSITION pos = m_friendList.GetFirstSelectedItemPosition();
    if (!pos) return 0;
    int row = m_friendList.GetNextSelectedItem(pos);
    CString idStr = m_friendList.GetItemText(row, 0);
    return _ttoi(idStr);
}

CString CMainDlg::SelectedFriendNick() {
    POSITION pos = m_friendList.GetFirstSelectedItemPosition();
    if (!pos) return CString();
    int row = m_friendList.GetNextSelectedItem(pos);
    return m_friendList.GetItemText(row, 1);
}

void CMainDlg::OnOpenChat() {
    int fid = SelectedFriendId();
    if (fid <= 0) { AfxMessageBox(_T("请先选择一个好友")); return; }
    OpenChatWith(fid, SelectedFriendNick());
}

void CMainDlg::OnDblClkFriend(NMHDR*, LRESULT* pResult) {
    OnOpenChat();
    *pResult = 0;
}

// 打开（或激活已存在的）与某好友的聊天窗口
void CMainDlg::OpenChatWith(int friendId, const CString& nick) {
    auto it = m_chatWnds.find(friendId);
    if (it != m_chatWnds.end() && it->second && ::IsWindow(it->second->GetSafeHwnd())) {
        it->second->SetForegroundWindow();
        return;
    }
    CChatDlg* dlg = new CChatDlg(friendId, nick, this);
    dlg->Create(IDD_CHAT_DIALOG, this);   // 非模态，可多开
    dlg->ShowWindow(SW_SHOW);
    m_chatWnds[friendId] = dlg;
}

// 收到聊天推送时，转交给对应聊天窗口显示；窗口不存在则弹出提示并建立
void CMainDlg::DeliverChat(int fromId, const CString& text) {
    auto it = m_chatWnds.find(fromId);
    if (it == m_chatWnds.end() || !it->second || !::IsWindow(it->second->GetSafeHwnd())) {
        OpenChatWith(fromId, _T(""));  // 昵称未知，先用空
        it = m_chatWnds.find(fromId);
    }
    if (it != m_chatWnds.end() && it->second)
        it->second->OnIncoming(text);
}

// ---------------- 网络消息处理 ----------------
LRESULT CMainDlg::OnNetMessage(WPARAM, LPARAM lParam) {
    std::string* pLine = reinterpret_cast<std::string*>(lParam);
    if (!pLine) return 0;
    std::vector<std::string> t = Unpack(*pLine);
    delete pLine;
    if (t.empty()) return 0;

    const std::string& cmd = t[0];
    if (cmd == "FRIEND_LIST_RESP") {
        // FRIEND_LIST_RESP|status|count|id|acc|nick|status ...（每 4 字段一条）
        m_friendList.DeleteAllItems();
        if (t.size() >= 3) {
            size_t i = 3;
            int row = 0;
            while (i + 3 < t.size() + 1 && i + 2 < t.size()) {
                CString id   = U8ToCS(t[i]);
                CString nick = (i + 2 < t.size()) ? U8ToCS(t[i + 2]) : CString();
                CString st   = (i + 3 < t.size() && t[i + 3] == "1") ? _T("在线") : _T("离线");
                int r = m_friendList.InsertItem(row, id);
                m_friendList.SetItemText(r, 1, nick);
                m_friendList.SetItemText(r, 2, st);
                ++row; i += 4;
            }
        }
    } else if (cmd == "SEARCH_RESP") {
        // SEARCH_RESP|status|count|id|acc|nick|status ...
        if (t.size() >= 3 && t[1] == "0") {
            CString msg;
            for (size_t i = 3; i + 2 < t.size(); i += 4) {
                CString line;
                line.Format(_T("ID:%s  昵称:%s\n"), U8ToCS(t[i]).GetString(), U8ToCS(t[i + 2]).GetString());
                msg += line;
            }
            if (msg.IsEmpty()) msg = _T("未找到匹配用户");
            else {
                // 简单起见：找到即询问是否加为好友（取第一条）
                if (t.size() > 3) {
                    int targetId = atoi(t[3].c_str());
                    if (AfxMessageBox(msg + _T("\n是否添加第一个用户为好友？"), MB_YESNO) == IDYES) {
                        g_ctx.net.Send(Pack("ADD_FRIEND",
                            { std::to_string(g_ctx.selfId), std::to_string(targetId), "" }));
                    }
                    return 0;
                }
            }
            AfxMessageBox(msg);
        } else {
            AfxMessageBox(_T("未找到匹配用户"));
        }
    } else if (cmd == "ADD_FRIEND_RESP") {
        AfxMessageBox(t.size() > 1 && t[1] == "0" ? _T("添加成功") : _T("添加失败"));
        RequestFriendList();
    } else if (cmd == "CHAT_PUSH") {
        // CHAT_PUSH|fromId|content
        if (t.size() >= 3) DeliverChat(atoi(t[1].c_str()), U8ToCS(t[2]));
    } else if (cmd == "SYS_MSG") {
        if (t.size() >= 3) AfxMessageBox(_T("[系统消息] ") + U8ToCS(t[2]));
        RequestFriendList();
    }
    return 0;
}

LRESULT CMainDlg::OnNetClosed(WPARAM, LPARAM) {
    AfxMessageBox(_T("与服务端的连接已断开。"));
    return 0;
}

// 聊天窗口关闭时回调：从会话表移除（窗口对象由其自身 PostNcDestroy 释放）
void CMainDlg::OnChatClosed(int peerId) {
    m_chatWnds.erase(peerId);
}

void CMainDlg::OnDestroy() {
    // 通知服务端登出并关闭子窗口
    if (g_ctx.net.IsConnected())
        g_ctx.net.Send(Pack("LOGOUT", { std::to_string(g_ctx.selfId) }));
    for (auto& kv : m_chatWnds) {
        if (kv.second && ::IsWindow(kv.second->GetSafeHwnd()))
            kv.second->DestroyWindow();
    }
    m_chatWnds.clear();
    CDialogEx::OnDestroy();
}
