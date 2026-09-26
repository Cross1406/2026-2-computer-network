#pragma once

#include "BaseLayer.h"
#include <pcap.h>

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
    static UINT ReceiveThread(LPVOID pParam);
    UINT CaptureLoop();
    BOOL RefreshAdapterList();

private:
    pcap_if_t* m_pAllDevs;
    pcap_t* m_pAdapter;
    CWinThread* m_pReceiveThread;
    volatile BOOL m_bRunning;
    char m_ErrorBuffer[PCAP_ERRBUF_SIZE];
};
