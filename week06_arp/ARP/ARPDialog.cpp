#include "pch.h"
#include "ARPApp.h"
#include "ARPDialog.h"
#include "afxdialogex.h"

CARPDialog::CARPDialog(CWnd* parent)
    : CDialogEx(IDD_ARP_DIALOG, parent),
      m_NI(const_cast<char*>("NI")), m_Ethernet(const_cast<char*>("Ethernet")),
      m_ARP(const_cast<char*>("ARP")) {
    m_Manager.AddLayer(&m_NI);
    m_Manager.AddLayer(&m_Ethernet);
    m_Manager.AddLayer(&m_ARP);
    m_Manager.ConnectLayers(const_cast<char*>("NI ( *Ethernet ( *ARP ) )"));
}
CARPDialog::~CARPDialog() {
    // Stop callbacks before ARP/Ethernet members are destroyed, even if DoModal fails.
    m_NI.StopReceive();
    // Objects are members: never call LayerManager::DeAllocLayer on them.
}
void CARPDialog::DoDataExchange(CDataExchange* exchange) {
    CDialogEx::DoDataExchange(exchange);
    DDX_Control(exchange, IDC_ADAPTER, m_Adapter);
    DDX_Control(exchange, IDC_CACHE, m_Cache);
}
BEGIN_MESSAGE_MAP(CARPDialog, CDialogEx)
    ON_BN_CLICKED(IDC_CONNECT, &CARPDialog::OnConnect)
    ON_BN_CLICKED(IDC_CONFIGURE, &CARPDialog::OnConfigure)
    ON_BN_CLICKED(IDC_REQUEST, &CARPDialog::OnRequest)
    ON_BN_CLICKED(IDC_DELETE, &CARPDialog::OnDelete)
    ON_BN_CLICKED(IDC_CLEAR, &CARPDialog::OnClear)
    ON_WM_TIMER()
    ON_WM_DESTROY()
END_MESSAGE_MAP()
BOOL CARPDialog::OnInitDialog() {
    CDialogEx::OnInitDialog();
    const auto icon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
    SetIcon(icon, TRUE); SetIcon(icon, FALSE);
    m_Cache.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    m_Cache.InsertColumn(0, _T("IP Address"), LVCFMT_LEFT, 145);
    m_Cache.InsertColumn(1, _T("Ethernet Address"), LVCFMT_LEFT, 175);
    m_Cache.InsertColumn(2, _T("Status"), LVCFMT_LEFT, 95);
    m_Cache.InsertColumn(3, _T("남은 시간(초)"), LVCFMT_LEFT, 100);
    for (int i = 0; i < m_NI.GetAdapterCount(); ++i) {
        CString text; text.Format(_T("%S"), m_NI.GetAdapterDescription(i));
        m_Adapter.AddString(text);
    }
    if (m_Adapter.GetCount()) m_Adapter.SetCurSel(0);
    GetDlgItem(IDC_CONFIGURE)->EnableWindow(FALSE);
    GetDlgItem(IDC_REQUEST)->EnableWindow(FALSE);
    if (!m_Adapter.GetCount()) SetStatus(_T("어댑터가 없습니다. Npcap 설치 상태를 확인하세요."));
    SetTimer(1, 1000, nullptr);
    return TRUE;
}
void CARPDialog::SetStatus(const CString& text) { SetDlgItemText(IDC_STATUS, text); }
BOOL CARPDialog::ReadIp(int control, arp::Ip& ip) {
    CString text; GetDlgItemText(control, text); text.Trim();
    CStringA ascii(text);
    if (arp::ParseIp(ascii.GetString(), ip)) return TRUE;
    AfxMessageBox(_T("유효한 IPv4 주소를 입력하세요. 예: 192.168.10.1"));
    return FALSE;
}
BOOL CARPDialog::ReadMac(arp::Mac& mac) {
    CString text; GetDlgItemText(IDC_SELF_MAC, text); text.Trim(); text.Replace(_T('-'), _T(':'));
    if (text.GetLength() != 17) return FALSE;
    for (int i = 0; i < 6; ++i) {
        unsigned value = 0;
        for (int j = 0; j < 2; ++j) {
            const TCHAR ch = text[i * 3 + j];
            unsigned digit;
            if (ch >= _T('0') && ch <= _T('9')) digit = ch - _T('0');
            else if (ch >= _T('a') && ch <= _T('f')) digit = ch - _T('a') + 10;
            else if (ch >= _T('A') && ch <= _T('F')) digit = ch - _T('A') + 10;
            else return FALSE;
            value = value * 16 + digit;
        }
        if (i < 5 && text[i * 3 + 2] != _T(':')) return FALSE;
        mac[i] = static_cast<unsigned char>(value);
    }
    return arp::ValidMac(mac);
}
void CARPDialog::OnConnect() {
    const int index = m_Adapter.GetCurSel();
    if (index == CB_ERR) { AfxMessageBox(_T("어댑터를 선택하세요.")); return; }
    m_Configured = FALSE; m_Connected = FALSE; m_HasRequest = FALSE;
    m_ARP.Disable();
    GetDlgItem(IDC_REQUEST)->EnableWindow(FALSE);
    GetDlgItem(IDC_CONFIGURE)->EnableWindow(FALSE);
    RefreshCache();
    if (!m_NI.SetAdapter(index)) {
        CString error; error.Format(_T("어댑터 연결 실패: %S"), m_NI.GetLastError());
        SetStatus(error); AfxMessageBox(error); return;
    }
    m_Connected = TRUE; m_ActiveAdapter = index;
    GetDlgItem(IDC_CONFIGURE)->EnableWindow(TRUE);
    SetStatus(_T("연결 완료. 선택한 어댑터의 실제 MAC과 자기 IP를 입력하고 주소 적용하세요."));
}
void CARPDialog::OnConfigure() {
    if (!m_Connected) return;
    arp::Ip ip; arp::Mac mac;
    if (!ReadIp(IDC_SELF_IP, ip)) return;
    if (!ReadMac(mac)) {
        AfxMessageBox(_T("자기 MAC에 어댑터의 물리적 주소를 입력하세요. 예: AA:BB:CC:DD:EE:FF")); return;
    }
    // Pause capture while changing identity; no reply can use mismatched old IP/new MAC.
    m_NI.StopReceive();
    m_ARP.Disable();
    m_Ethernet.SetSourceAddress(mac.data());
    m_Configured = m_ARP.Configure(ip, mac);
    // Reopen pcap so a breakloop flag cannot carry across StopReceive/StartReceive.
    m_Connected = m_NI.SetAdapter(m_ActiveAdapter);
    if (!m_Connected) { m_Configured = FALSE; m_ARP.Disable(); }
    m_HasRequest = FALSE;
    GetDlgItem(IDC_REQUEST)->EnableWindow(m_Configured);
    RefreshCache();
    SetStatus(m_Configured ? _T("주소 적용 완료. 내 IP 대상 요청에 자동 응답합니다.") : _T("수신 시작 실패. 어댑터를 다시 연결하세요."));
}
void CARPDialog::OnRequest() {
    if (!m_Configured) return;
    arp::Ip target;
    if (!ReadIp(IDC_TARGET_IP, target)) return;
    if (!m_ARP.Request(target)) {
        SetStatus(_T("ARP 요청 실패. 대상 IP(자기 IP 제외)와 어댑터 상태를 확인하세요.")); return;
    }
    m_LastTarget = target; m_HasRequest = TRUE;
    SetStatus(_T("ARP Request를 전송했습니다. 응답을 기다립니다."));
    RefreshCache();
}
void CARPDialog::RefreshCache() {
    CString selectedIp;
    const int selected = m_Cache.GetNextItem(-1, LVNI_SELECTED);
    if (selected >= 0) selectedIp = m_Cache.GetItemText(selected, 0);
    const auto entries = m_ARP.Snapshot();
    const ULONGLONG now = GetTickCount64();
    m_Cache.SetRedraw(FALSE);
    m_Cache.DeleteAllItems();
    for (const auto& e : entries) {
        CString ip, mac, ttl;
        ip.Format(_T("%u.%u.%u.%u"), e.ip[0], e.ip[1], e.ip[2], e.ip[3]);
        if (e.complete) mac.Format(_T("%02X:%02X:%02X:%02X:%02X:%02X"),
            e.mac[0], e.mac[1], e.mac[2], e.mac[3], e.mac[4], e.mac[5]);
        else mac = _T("??:??:??:??:??:??");
        const ULONGLONG lifetime = e.complete ? arp::CompleteLifetime : arp::IncompleteLifetime;
        const ULONGLONG elapsed = now - e.updatedAt;
        ttl.Format(_T("%llu"), elapsed >= lifetime ? 0ULL : (lifetime - elapsed + 999) / 1000);
        const int row = m_Cache.InsertItem(m_Cache.GetItemCount(), ip);
        m_Cache.SetItemText(row, 1, mac);
        m_Cache.SetItemText(row, 2, e.complete ? _T("Complete") : _T("Incomplete"));
        m_Cache.SetItemText(row, 3, ttl);
        if (ip == selectedIp) m_Cache.SetItemState(row, LVIS_SELECTED, LVIS_SELECTED);
        if (m_HasRequest && e.ip == m_LastTarget && e.complete) {
            SetStatus(_T("ARP Reply 수신 완료. 대상 IP의 MAC을 캐시에 저장했습니다."));
            m_HasRequest = FALSE;
        }
    }
    m_Cache.SetRedraw(TRUE); m_Cache.Invalidate(FALSE);
    if (m_HasRequest) {
        bool pending = false;
        for (const auto& e : entries) if (e.ip == m_LastTarget) pending = true;
        if (!pending) { SetStatus(_T("대기 항목이 만료되거나 삭제됐습니다. 필요하면 다시 요청하세요.")); m_HasRequest = FALSE; }
    }
}
void CARPDialog::OnDelete() {
    const int row = m_Cache.GetNextItem(-1, LVNI_SELECTED);
    if (row < 0) { AfxMessageBox(_T("삭제할 캐시 항목을 선택하세요.")); return; }
    CStringA text(m_Cache.GetItemText(row, 0)); arp::Ip ip;
    if (arp::ParseIp(text.GetString(), ip)) m_ARP.Remove(ip);
    RefreshCache();
}
void CARPDialog::OnClear() { m_ARP.Clear(); m_HasRequest = FALSE; RefreshCache(); SetStatus(_T("ARP 캐시를 모두 삭제했습니다.")); }
void CARPDialog::OnTimer(UINT_PTR event) {
    if (event == 1) RefreshCache();
    else CDialogEx::OnTimer(event);
}
void CARPDialog::OnDestroy() {
    KillTimer(1); m_NI.StopReceive();
    CDialogEx::OnDestroy();
}
