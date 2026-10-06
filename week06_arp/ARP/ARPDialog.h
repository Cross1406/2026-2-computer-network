#pragma once
#include "afxdialogex.h"
#include "LayerManager.h"
#include "NILayer.h"
#include "EthernetLayer.h"
#include "ARPLayer.h"

// Member ownership keeps this weekly project independent of Chat/File layers.
class CARPDialog : public CDialogEx {
public:
    explicit CARPDialog(CWnd* parent = nullptr);
    ~CARPDialog() override;
protected:
    BOOL OnInitDialog() override;
    void DoDataExchange(CDataExchange* exchange) override;
    void OnOK() override {} // Enter must not close a network experiment.
    afx_msg void OnConnect();
    afx_msg void OnConfigure();
    afx_msg void OnRequest();
    afx_msg void OnDelete();
    afx_msg void OnClear();
    afx_msg void OnTimer(UINT_PTR event);
    afx_msg void OnDestroy();
    DECLARE_MESSAGE_MAP()
private:
    void RefreshCache();
    BOOL ReadIp(int control, arp::Ip& ip);
    BOOL ReadMac(arp::Mac& mac);
    void SetStatus(const CString& text);
    CLayerManager m_Manager;
    CNILayer m_NI;
    CEthernetLayer m_Ethernet;
    CARPLayer m_ARP;
    CComboBox m_Adapter;
    CListCtrl m_Cache;
    BOOL m_Connected = FALSE;
    int m_ActiveAdapter = -1;
    BOOL m_Configured = FALSE;
    arp::Ip m_LastTarget{};
    BOOL m_HasRequest = FALSE;
};
