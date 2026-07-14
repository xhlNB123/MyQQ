#include "pch.h"
#include "ViewProfileDlg.h"
#include "../../common/Message.h"

using namespace myqq;

static CString U8(const std::string& s){ if(s.empty())return CString();
    int n=MultiByteToWideChar(CP_UTF8,0,s.c_str(),(int)s.size(),nullptr,0);
    CStringW w; auto b=w.GetBuffer(n); MultiByteToWideChar(CP_UTF8,0,s.c_str(),(int)s.size(),b,n); w.ReleaseBuffer(n); return CString(w); }
static CString Dec(const std::string& e){ std::string s; return DecodeWireText(e,s)?U8(s):CString(); }

static const wchar_t* kStars[]={L"未设置",L"白羊座",L"金牛座",L"双子座",L"巨蟹座",L"狮子座",L"处女座",L"天秤座",L"天蝎座",L"射手座",L"摩羯座",L"水瓶座",L"双鱼座"};
static const wchar_t* kBloods[]={L"未设置",L"A",L"B",L"O",L"AB",L"未知"};
static const wchar_t* kGenders[]={L"未知",L"男",L"女"};

BEGIN_MESSAGE_MAP(CViewProfileDlg, CDialogEx)
END_MESSAGE_MAP()

// VIEW_PROFILE_RESP|status|userId|accountB64|nickB64|gender|starId|bloodId|signatureB64|avatarB64
void CViewProfileDlg::Fill(const std::vector<std::string>& t){
    if(t.size()<9) return;
    account_=Dec(t[2]); nick_=Dec(t[3]);
    int g=atoi(t[4].c_str()); gender_=(g>=0&&g<=2)?kGenders[g]:L"未知";
    int s=atoi(t[5].c_str()); star_=(s>=1&&s<=12)?kStars[s]:L"未设置";
    int b=atoi(t[6].c_str()); blood_=(b>=1&&b<=5)?kBloods[b]:L"未设置";
    sign_=Dec(t[7]);
    if(GetSafeHwnd()){
        SetDlgItemText(IDC_VP_ACCOUNT,account_); SetDlgItemText(IDC_VP_NICK,nick_);
        SetDlgItemText(IDC_VP_GENDER,gender_); SetDlgItemText(IDC_VP_STAR,star_);
        SetDlgItemText(IDC_VP_BLOOD,blood_); SetDlgItemText(IDC_VP_SIGN,sign_);
    }
}

BOOL CViewProfileDlg::OnInitDialog(){
    CDialogEx::OnInitDialog();
    SetWindowText(_T("查看资料"));
    SetDlgItemText(IDC_VP_ACCOUNT,account_); SetDlgItemText(IDC_VP_NICK,nick_);
    SetDlgItemText(IDC_VP_GENDER,gender_); SetDlgItemText(IDC_VP_STAR,star_);
    SetDlgItemText(IDC_VP_BLOOD,blood_); SetDlgItemText(IDC_VP_SIGN,sign_);
    return TRUE;
}
