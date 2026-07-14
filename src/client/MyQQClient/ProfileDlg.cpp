// =====================================================================
// CProfileDlg 实现：个人资料读取/编辑/保存
//   打开 → GET_PROFILE|selfId；保存 → UPDATE_PROFILE|selfId|...
//   星座/血型用下拉框，索引即字典 Id（星座 1..12、血型 1..5）。
// =====================================================================
#include "pch.h"
#include "ProfileDlg.h"
#include "AppContext.h"
#include "../../common/Message.h"
#include "../../common/Protocol.h"

using namespace myqq;

// UTF-8(std::string) <-> CString 互转（传输统一用 UTF-8）
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

static const wchar_t* kStars[] = {
    L"白羊座", L"金牛座", L"双子座", L"巨蟹座", L"狮子座", L"处女座",
    L"天秤座", L"天蝎座", L"射手座", L"摩羯座", L"水瓶座", L"双鱼座"
};
static const wchar_t* kBloods[] = { L"A", L"B", L"O", L"AB", L"未知" };

CProfileDlg::CProfileDlg(CWnd* pParent) : CDialogEx(IDD_PROFILE_DIALOG, pParent) {}

void CProfileDlg::DoDataExchange(CDataExchange* pDX) {
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_PROFILE_GENDER, m_gender);
    DDX_Control(pDX, IDC_PROFILE_STAR,   m_star);
    DDX_Control(pDX, IDC_PROFILE_BLOOD,  m_blood);
    DDX_Control(pDX, IDC_PROFILE_AVATAR, m_avatar);
    DDX_Text(pDX, IDC_PROFILE_ACCOUNT,   m_account);
    DDX_Text(pDX, IDC_PROFILE_NICKNAME,  m_nickname);
    DDX_Text(pDX, IDC_PROFILE_SIGNATURE, m_signature);
}

BEGIN_MESSAGE_MAP(CProfileDlg, CDialogEx)
    ON_BN_CLICKED(IDC_PROFILE_SAVE_BTN, &CProfileDlg::OnSave)
    ON_MESSAGE(WM_NET_MESSAGE, &CProfileDlg::OnNetMessage)
END_MESSAGE_MAP()

BOOL CProfileDlg::OnInitDialog() {
    CDialogEx::OnInitDialog();
    SetWindowText(_T("个性化设置"));

    // 下拉框填充：索引 0 = 未设置，其余按字典 Id 排列
    m_gender.AddString(_T("未知"));
    m_gender.AddString(_T("男"));
    m_gender.AddString(_T("女"));

    m_star.AddString(_T("(未设置)"));
    for (auto s : kStars) m_star.AddString(s);

    m_blood.AddString(_T("(未设置)"));
    for (auto b : kBloods) m_blood.AddString(b);

    m_avatar.AddString(_T("(无)"));
    for (int i = 1; i <= 8; ++i) { CString a; a.Format(_T("头像%d"), i); m_avatar.AddString(a); }

    // 主窗口是登录阶段唯一网络 owner，会将资料响应转发到本对话框。
    return TRUE;
}

// GET_PROFILE_RESP|status|userId|account|nick|gender|starId|bloodTypeId|signature|avatar
void CProfileDlg::FillProfile(const std::vector<std::string>& t) {
    if (t.size() < 10 || t[1] != "0") return;
    std::string account, nick, signature, avatarText;
    if (!DecodeWireText(t[3], account) || !DecodeWireText(t[4], nick)
        || !DecodeWireText(t[8], signature) || !DecodeWireText(t[9], avatarText)) return;
    m_account   = U8ToCS(account);
    m_nickname  = U8ToCS(nick);
    int gender  = atoi(t[5].c_str());
    int starId  = atoi(t[6].c_str());
    int bloodId = atoi(t[7].c_str());
    m_signature = U8ToCS(signature);
    CString avatar = U8ToCS(avatarText);

    m_gender.SetCurSel((gender >= 0 && gender <= 2) ? gender : 0);
    m_star.SetCurSel((starId >= 1 && starId <= 12) ? starId : 0);   // 索引=Id，0=未设置
    m_blood.SetCurSel((bloodId >= 1 && bloodId <= 5) ? bloodId : 0);
    // 头像格式约定 "avatar_N"（N=1..8）；解析失败则选 (无)
    int an = 0;
    if (avatar.GetLength() > 7 && avatar.Left(7) == _T("avatar_"))
        an = _ttoi(avatar.Mid(7));
    m_avatar.SetCurSel((an >= 1 && an <= 8) ? an : 0);

    UpdateData(FALSE);   // 把 m_account/m_nickname/m_signature 刷到控件
    m_loaded = true;     // 资料已就绪，允许保存
}

void CProfileDlg::OnSave() {
    // 资料还没加载完就保存，会把数据库里的资料清空 —— 直接拦下
    if (!m_loaded) { AfxMessageBox(_T("资料尚未加载完成，请稍候再保存")); return; }

    UpdateData(TRUE);
    // 字段里若含分隔符 '|' 会破坏协议，先挡掉
    if (m_nickname.Find(_T('|')) >= 0 || m_signature.Find(_T('|')) >= 0) {
        AfxMessageBox(_T("昵称和签名不能包含 '|' 字符"));
        return;
    }
    // 昵称留空则默认回退成账号名（不再硬性拦人）
    if (m_nickname.IsEmpty()) m_nickname = m_account;

    int gender  = m_gender.GetCurSel();  if (gender  < 0) gender  = 0;
    int starId  = m_star.GetCurSel();    if (starId  < 0) starId  = 0;  // 索引即 Id
    int bloodId = m_blood.GetCurSel();   if (bloodId < 0) bloodId = 0;
    int avatarN = m_avatar.GetCurSel();  if (avatarN < 0) avatarN = 0;
    std::string avatar = (avatarN >= 1) ? ("avatar_" + std::to_string(avatarN)) : "";

    g_ctx.net.Send(Pack("UPDATE_PROFILE", {
        EncodeWireText(CSToU8(m_nickname)),
        std::to_string(gender),
        std::to_string(starId),
        std::to_string(bloodId),
        EncodeWireText(CSToU8(m_signature)),
        EncodeWireText(avatar)
    }));
}

LRESULT CProfileDlg::OnNetMessage(WPARAM, LPARAM lParam) {
    std::string* pLine = reinterpret_cast<std::string*>(lParam);
    if (!pLine) return 0;
    std::vector<std::string> t = Unpack(*pLine);
    delete pLine;
    if (t.empty()) return 0;

    if (t[0] == "GET_PROFILE_RESP") {
        if (t.size() >= 2 && t[1] == "0") FillProfile(t);
        else AfxMessageBox(_T("获取个人资料失败"));
    } else if (t[0] == "UPDATE_PROFILE_RESP") {
        if (t.size() >= 2 && t[1] == "0") {
            // 同步更新全局昵称，主窗口标题/信息栏可据此刷新
            g_ctx.selfNick = CSToU8(m_nickname);
            AfxMessageBox(_T("保存成功"));
            EndDialog(IDOK);
        } else {
            AfxMessageBox(_T("保存失败"));
        }
    }
    return 0;
}
