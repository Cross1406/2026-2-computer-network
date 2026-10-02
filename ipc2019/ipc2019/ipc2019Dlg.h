
// ipc2019Dlg.h: 헤더 파일
//

#pragma once

#include "LayerManager.h"	// Added by ClassView
#include "ChatAppLayer.h"	// Added by ClassView
#include "EthernetLayer.h"	// Added by ClassView
#include "FileLayer.h"	// Added by ClassView
#include "NILayer.h"	// Npcap network interface layer
#define WM_APP_LAYER_MESSAGE (WM_APP + 100)

// UI와 각 프로토콜 계층을 연결하는 최상위 계층.
//
// 버튼 이벤트에서 채팅/파일 송신을 시작하고,
// worker thread의 결과는 WM_APP_LAYER_MESSAGE를 통해 UI 스레드에서 표시한다.
class Cipc2019Dlg : public CDialogEx, public CBaseLayer
{
// 생성입니다.
public:
	Cipc2019Dlg(CWnd* pParent = nullptr);	// 표준 생성자입니다.



// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_IPC2019_DIALOG };
#endif

	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);


	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 지원입니다.


// 구현입니다.
protected:
	HICON m_hIcon;

	// 생성된 메시지 맵 함수
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
//	UINT m_unDstAddr;
//	UINT unSrcAddr;
//	CString m_stMessage;
	CString m_stFilePath;
//	CListBox m_ListChat;
	
	afx_msg void OnTimer(UINT_PTR nIDEvent);


public:
	BOOL			Receive(unsigned char* ppayload);
	inline void		SendData();

private:
	// 프로토콜 스택의 소유권과 연결 관계를 관리한다.
	CLayerManager	m_LayerMgr;
	int				m_nAckReady;

	enum {
		IPC_INITIALIZING,
		IPC_READYTOSEND,
		IPC_WAITFORACK,
		IPC_ERROR,
		IPC_BROADCASTMODE,
		IPC_UNICASTMODE,
		IPC_ADDR_SET,
		IPC_ADDR_RESET
	};

	void			SetDlgState(int state);
	BOOL			ParseMacAddress(const CString& text, unsigned char address[6]);
	inline void		EndofProcess();
	inline void		SetRegstryMessage();
	LRESULT			OnRegSendMsg(WPARAM wParam, LPARAM lParam);
	LRESULT			OnRegAckMsg(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnLayerMessage(WPARAM wParam, LPARAM lParam);

	BOOL			m_bSendReady;

	// 자주 사용하는 계층 객체 포인터.
	CChatAppLayer* m_ChatApp;
	CEthernetLayer* m_Ethernet;
	CNILayer* m_NILayer;
	CFileLayer* m_FileLayer;

	// Implementation
	UINT			m_wParam;
	DWORD			m_lParam;
public:
	afx_msg void OnBnClickedButtonAddr();
	afx_msg void OnBnClickedButtonSend();
	CString m_stSrcAddr;
	CString m_stDstAddr;
	CString m_stMessage;
	CListBox m_ListChat;
	CProgressCtrl m_FileSendProgress;
	CProgressCtrl m_FileReceiveProgress;
	afx_msg void OnBnClickedCheckToall();
	afx_msg void OnBnClickedButtonAdapterConnect();
	afx_msg void OnBnClickedButtonFileBrowse();
	afx_msg void OnBnClickedButtonFileSend();
	CComboBox m_AdapterCombo;
};
