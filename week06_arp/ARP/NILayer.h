#pragma once

#include "BaseLayer.h"
#include <pcap.h>
#include <atomic>

// Network Interface 계층.
// Npcap API로 실제 랜카드를 열고 raw Ethernet frame을 송수신한다.
// 별도 수신 스레드를 사용하므로 UI 스레드는 멈추지 않는다.
class CNILayer : public CBaseLayer
{
public:
    CNILayer(char* pName);
    virtual ~CNILayer();

    BOOL Send(unsigned char* ppayload, int nlength) override;

    int GetAdapterCount() const;
    const char* GetAdapterName(int nIndex) const;
    const char* GetAdapterDescription(int nIndex) const;
    BOOL SetAdapter(int nIndex);

    BOOL StartReceive();
    void StopReceive();

    const char* GetLastError() const;

private:
    // MFC worker thread 진입점과 실제 packet capture loop.
    static UINT ReceiveThread(LPVOID pParam);
    UINT CaptureLoop();
    BOOL RefreshAdapterList();

private:
    pcap_if_t* m_pAllDevs;
    pcap_t* m_pAdapter;
    CWinThread* m_pReceiveThread;
    std::atomic<bool> m_bRunning;
    char m_ErrorBuffer[PCAP_ERRBUF_SIZE];
};
