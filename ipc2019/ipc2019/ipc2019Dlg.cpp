
// ipc2019Dlg.cpp: 구현 파일
//

#include "pch.h"
#include "framework.h"
#include "ipc2019.h"
#include "ipc2019Dlg.h"
#include "afxdialogex.h"
#include <atlconv.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


// 응용 프로그램 정보에 사용되는 CAboutDlg 대화 상자입니다.

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 지원입니다.

// 구현입니다.
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// Cipc2019Dlg 대화 상자



Cipc2019Dlg::Cipc2019Dlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_IPC2019_DIALOG, pParent)
	, CBaseLayer("ChatDlg")
	, m_bSendReady(FALSE)
	, m_nAckReady( -1 )

	, m_stSrcAddr(_T("00:00:00:00:00:00"))
	, m_stDstAddr(_T("00:00:00:00:00:00"))
	, m_stMessage(_T(""))
{
	//대화상자 멤버 변수 초기화
	//  m_unDstAddr = 0;
	//  unSrcAddr = 0;
	//  m_stMessage = _T("");
	//대화 상자 멤버 초기화 완료

	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	//Protocol Layer Setting
	m_LayerMgr.AddLayer(new CChatAppLayer("ChatApp"));
	m_LayerMgr.AddLayer(new CEthernetLayer("Ethernet"));
	m_LayerMgr.AddLayer(new CFileLayer("File"));
	m_LayerMgr.AddLayer(new CNILayer("NI"));
	m_LayerMgr.AddLayer(this);

	// 레이어를 연결한다. (레이어 생성)
	m_LayerMgr.ConnectLayers("NI ( *Ethernet ( *ChatApp ( *ChatDlg ) ) ) )");

	m_ChatApp = (CChatAppLayer*)m_LayerMgr.GetLayer("ChatApp");
	m_Ethernet = (CEthernetLayer*)m_LayerMgr.GetLayer("Ethernet");
	m_NILayer = (CNILayer*)m_LayerMgr.GetLayer("NI");
	//Protocol Layer Setting
}

void Cipc2019Dlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_SRC, m_stSrcAddr);
	DDX_Text(pDX, IDC_EDIT_DST, m_stDstAddr);
	DDX_Text(pDX, IDC_EDIT_MSG, m_stMessage);
	DDX_Control(pDX, IDC_LIST_CHAT, m_ListChat);
	DDX_Control(pDX, IDC_COMBO_ADAPTER, m_AdapterCombo);
}

// 레지스트리에 등록하기 위한 변수
UINT nRegSendMsg;
UINT nRegAckMsg;
// 레지스트리에 등록하기 위한 변수


BEGIN_MESSAGE_MAP(Cipc2019Dlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON_ADDR, &Cipc2019Dlg::OnBnClickedButtonAddr)
	ON_BN_CLICKED(IDC_BUTTON_SEND, &Cipc2019Dlg::OnBnClickedButtonSend)
	ON_WM_TIMER()

	ON_REGISTERED_MESSAGE(nRegSendMsg, OnRegSendMsg)
	//////////////////////// fill the blank ///////////////////////////////
		// Ack 레지스터 등록
	ON_REGISTERED_MESSAGE(nRegAckMsg, OnRegAckMsg)
	///////////////////////////////////////////////////////////////////////
	
	
	ON_BN_CLICKED(IDC_CHECK_TOALL, &Cipc2019Dlg::OnBnClickedCheckToall)
	ON_BN_CLICKED(IDC_BUTTON_ADAPTER_CONNECT, &Cipc2019Dlg::OnBnClickedButtonAdapterConnect)
END_MESSAGE_MAP()


// Cipc2019Dlg 메시지 처리기

BOOL Cipc2019Dlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 시스템 메뉴에 "정보..." 메뉴 항목을 추가합니다.

	// IDM_ABOUTBOX는 시스템 명령 범위에 있어야 합니다.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 이 대화 상자의 아이콘을 설정합니다.  응용 프로그램의 주 창이 대화 상자가 아닐 경우에는
	//  프레임워크가 이 작업을 자동으로 수행합니다.
	SetIcon(m_hIcon, TRUE);			// 큰 아이콘을 설정합니다.
	SetIcon(m_hIcon, FALSE);		// 작은 아이콘을 설정합니다.

	// Npcap에서 찾은 네트워크 어댑터를 선택 목록에 표시한다.
	if (m_NILayer != nullptr)
	{
		for (int i = 0; i < m_NILayer->GetAdapterCount(); ++i)
		{
			const char* description = m_NILayer->GetAdapterDescription(i);
			CString adapterText;
			adapterText.Format(_T("%S"), description != nullptr ? description : "Unknown adapter");
			m_AdapterCombo.AddString(adapterText);
		}

		if (m_AdapterCombo.GetCount() > 0)
			m_AdapterCombo.SetCurSel(0);
	}

	SetRegstryMessage();
	SetDlgState(IPC_INITIALIZING);

	return TRUE;  // 포커스를 컨트롤에 설정하지 않으면 TRUE를 반환합니다.
}

void Cipc2019Dlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 대화 상자에 최소화 단추를 추가할 경우 아이콘을 그리려면
//  아래 코드가 필요합니다.  문서/뷰 모델을 사용하는 MFC 애플리케이션의 경우에는
//  프레임워크에서 이 작업을 자동으로 수행합니다.

void Cipc2019Dlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 그리기를 위한 디바이스 컨텍스트입니다.

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 클라이언트 사각형에서 아이콘을 가운데에 맞춥니다.
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 아이콘을 그립니다.
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// 사용자가 최소화된 창을 끄는 동안에 커서가 표시되도록 시스템에서
//  이 함수를 호출합니다.
HCURSOR Cipc2019Dlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}




void Cipc2019Dlg::OnBnClickedButtonSend()
{
	UpdateData(TRUE);

	if (!m_stMessage.IsEmpty())
	{
		SendData();
		m_stMessage.Empty();
		(CEdit*)GetDlgItem(IDC_EDIT3)->SetFocus();
	}

	UpdateData(FALSE);
}

void Cipc2019Dlg::SetRegstryMessage()
{
	nRegSendMsg = RegisterWindowMessage(_T("Send IPC Message"));
	//////////////////////// fill the blank ///////////////////////////////
		// Ack 레지스트리의 메시지를 설정
	nRegAckMsg = RegisterWindowMessage(_T("Ack IPC Message"));
	///////////////////////////////////////////////////////////////////////
}

void Cipc2019Dlg::SendData()
{
	m_ListChat.AddString(_T("[SEND] ") + m_stMessage);

	// Ethernet payload uses UTF-8 so Korean text is transmitted correctly.
	CStringA utf8Message = CW2A(m_stMessage, CP_UTF8);
	m_ChatApp->Send(
		reinterpret_cast<unsigned char*>(utf8Message.GetBuffer()),
		utf8Message.GetLength());
}

BOOL Cipc2019Dlg::Receive(unsigned char* ppayload)
{
	if (m_nAckReady == -1)
	{
		//////////////////////// fill the blank ///////////////////////////////
				// 현재 과제에서는 쓰이지 않음.
				// 여기서 FALSE처리를 해도 다음 함수들에서 TRUE처리가 되므로
				// return은 의미 없다.
				// 다음 과제에서 Receive시 Ack 메시지를 받은 경우
				// 어떠한 처리 과정에 쓰일 것으로 추정.
		///////////////////////////////////////////////////////////////////////
	}

	m_ListChat.AddString((LPCTSTR)ppayload);
	return TRUE;
}

BOOL Cipc2019Dlg::PreTranslateMessage(MSG* pMsg)
{
	// TODO: Add your specialized code here and/or call the base class
	switch (pMsg->message)
	{
	case WM_KEYDOWN:
		switch (pMsg->wParam)
		{
		case VK_RETURN:
			if (::GetDlgCtrlID(::GetFocus()) == IDC_EDIT3)
				OnBnClickedButtonSend();					
			return FALSE;
		case VK_ESCAPE: return FALSE;
		}
		break;
	}

	return CDialog::PreTranslateMessage(pMsg);
}


void Cipc2019Dlg::SetDlgState(int state)
{
	UpdateData(TRUE);

	CButton* pChkButton = (CButton*)GetDlgItem(IDC_CHECK1);

	CButton* pSendButton = (CButton*)GetDlgItem(bt_send);
	CButton* pSetAddrButton = (CButton*)GetDlgItem(bt_setting);
	CEdit* pMsgEdit = (CEdit*)GetDlgItem(IDC_EDIT3);
	CEdit* pSrcEdit = (CEdit*)GetDlgItem(IDC_EDIT1);
	CEdit* pDstEdit = (CEdit*)GetDlgItem(IDC_EDIT2);

	switch (state)
	{
	case IPC_INITIALIZING:
		pSendButton->EnableWindow(FALSE);
		pMsgEdit->EnableWindow(FALSE);
		m_ListChat.EnableWindow(FALSE);
		break;
	case IPC_READYTOSEND:
		pSendButton->EnableWindow(TRUE);
		pMsgEdit->EnableWindow(TRUE);
		m_ListChat.EnableWindow(TRUE);
		break;
	case IPC_WAITFORACK:	break;
	case IPC_ERROR:		break;
	case IPC_UNICASTMODE:
		m_stDstAddr = _T("00:00:00:00:00:00");
		pDstEdit->EnableWindow(TRUE);
		break;
	case IPC_BROADCASTMODE:
		m_stDstAddr = _T("FF:FF:FF:FF:FF:FF");
		pDstEdit->EnableWindow(FALSE);
		break;
	case IPC_ADDR_SET:
		pSetAddrButton->SetWindowText(_T("재설정(&R)"));
		pSrcEdit->EnableWindow(FALSE);
		pDstEdit->EnableWindow(FALSE);
		//pChkButton->EnableWindow(FALSE);
		break;
	case IPC_ADDR_RESET:
		pSetAddrButton->SetWindowText(_T("설정(&O)"));
		pSrcEdit->EnableWindow(TRUE);
		if (!pChkButton->GetCheck())
			pDstEdit->EnableWindow(TRUE);
		pChkButton->EnableWindow(TRUE);
		break;
	}

	UpdateData(FALSE);
}


void Cipc2019Dlg::EndofProcess()
{
	m_LayerMgr.DeAllocLayer();
}

// Send메시지 레지스트리가 켜졌을 때
LRESULT Cipc2019Dlg::OnRegSendMsg(WPARAM wParam, LPARAM lParam)
{
	//////////////////////// fill the blank ///////////////////////////////
	if (m_nAckReady) {
		// File 레이어에서 상대방이 전송한 메시지가 담긴 파일을 가져옴
		if (m_LayerMgr.GetLayer("File")->Receive())
		{
			// 메시지를 받았다면 Ack 신호를 브로드캐스트로 날린다.
			::SendMessage(HWND_BROADCAST, nRegAckMsg, 0, 0);
		}
	}
	///////////////////////////////////////////////////////////////////////
	return 0;
}

LRESULT Cipc2019Dlg::OnRegAckMsg(WPARAM wParam, LPARAM lParam)
{
	if (!m_nAckReady) { // Ack 신호를 받으면 타이머를 멈춘다.
		m_nAckReady = -1;
		KillTimer(1);
	}

	return 0;
}

void Cipc2019Dlg::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: Add your message handler code here and/or call default
	m_ListChat.AddString(_T(">> The last message was time-out.."));
	m_nAckReady = -1;
	KillTimer(1);

	CDialog::OnTimer(nIDEvent);
}


BOOL Cipc2019Dlg::ParseMacAddress(const CString& text, unsigned char address[6])
{
	CString normalized(text);
	normalized.Trim();
	normalized.Replace(_T('-'), _T(':'));

	unsigned int value[6] = {};
	if (normalized.GetLength() != 17 ||
		_stscanf_s(
			normalized,
			_T("%2x:%2x:%2x:%2x:%2x:%2x"),
			&value[0], &value[1], &value[2],
			&value[3], &value[4], &value[5]) != 6)
	{
		return FALSE;
	}

	for (int i = 0; i < 6; ++i)
		address[i] = static_cast<unsigned char>(value[i]);

	return TRUE;
}

void Cipc2019Dlg::OnBnClickedButtonAddr()
{
	UpdateData(TRUE);

	if (m_bSendReady)
	{
		SetDlgState(IPC_ADDR_RESET);
		SetDlgState(IPC_INITIALIZING);
		m_bSendReady = FALSE;
		return;
	}

	unsigned char sourceAddress[6] = {};
	unsigned char destinationAddress[6] = {};

	if (!ParseMacAddress(m_stSrcAddr, sourceAddress) ||
		!ParseMacAddress(m_stDstAddr, destinationAddress))
	{
		AfxMessageBox(
			_T("MAC 주소를 AA:BB:CC:DD:EE:FF 형식으로 입력하세요."),
			MB_OK | MB_ICONERROR);
		return;
	}

	const unsigned char zeroAddress[6] = {};
	if (memcmp(sourceAddress, zeroAddress, 6) == 0)
	{
		AfxMessageBox(_T("Source MAC 주소를 입력하세요."), MB_OK | MB_ICONERROR);
		return;
	}

	m_Ethernet->SetSourceAddress(sourceAddress);
	m_Ethernet->SetDestinAddress(destinationAddress);

	SetDlgState(IPC_ADDR_SET);
	SetDlgState(IPC_READYTOSEND);
	m_bSendReady = TRUE;
}

void Cipc2019Dlg::OnBnClickedCheckToall()
{
	CButton* pChkButton = (CButton*)GetDlgItem(IDC_CHECK_TOALL);

	if (pChkButton->GetCheck()) {
		SetDlgState(IPC_BROADCASTMODE);
	}
	else {
		SetDlgState(IPC_UNICASTMODE);
	}
}


void Cipc2019Dlg::OnBnClickedButtonAdapterConnect()
{
	int adapterIndex = m_AdapterCombo.GetCurSel();

	if (adapterIndex == CB_ERR || m_NILayer == nullptr)
	{
		AfxMessageBox(_T("연결할 네트워크 어댑터를 선택하세요."));
		return;
	}

	if (!m_NILayer->SetAdapter(adapterIndex))
	{
		CString errorMessage;
		errorMessage.Format(_T("어댑터 연결 실패\n%S"), m_NILayer->GetLastError());
		AfxMessageBox(errorMessage, MB_OK | MB_ICONERROR);
		return;
	}

	SetDlgItemText(IDC_BUTTON_ADAPTER_CONNECT, _T("연결됨"));
	m_ListChat.AddString(_T(">> Network adapter connected."));
}
