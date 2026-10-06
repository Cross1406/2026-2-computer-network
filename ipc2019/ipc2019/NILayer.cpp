#include "pch.h"
#include "NILayer.h"

CNILayer::CNILayer(char* pName)
    : CBaseLayer(pName),
      m_pAllDevs(nullptr),
      m_pAdapter(nullptr),
      m_pReceiveThread(nullptr),
      m_bRunning(FALSE)
{
    memset(m_ErrorBuffer, 0, sizeof(m_ErrorBuffer));
    RefreshAdapterList();
}

CNILayer::~CNILayer()
{
    StopReceive();

    if (m_pAdapter != nullptr)
    {
        pcap_close(m_pAdapter);
        m_pAdapter = nullptr;
    }

    if (m_pAllDevs != nullptr)
    {
        pcap_freealldevs(m_pAllDevs);
        m_pAllDevs = nullptr;
    }
}

// pcap_findalldevs()로 Wi-Fi, Ethernet, 가상 어댑터 목록을 새로 읽는다.
BOOL CNILayer::RefreshAdapterList()
{
    if (m_pAllDevs != nullptr)
    {
        pcap_freealldevs(m_pAllDevs);
        m_pAllDevs = nullptr;
    }

    memset(m_ErrorBuffer, 0, sizeof(m_ErrorBuffer));
    return pcap_findalldevs(&m_pAllDevs, m_ErrorBuffer) != -1;
}

int CNILayer::GetAdapterCount() const
{
    int count = 0;

    for (pcap_if_t* device = m_pAllDevs; device != nullptr; device = device->next)
        ++count;

    return count;
}

const char* CNILayer::GetAdapterName(int nIndex) const
{
    int index = 0;

    for (pcap_if_t* device = m_pAllDevs; device != nullptr; device = device->next, ++index)
    {
        if (index == nIndex)
            return device->name;
    }

    return nullptr;
}

const char* CNILayer::GetAdapterDescription(int nIndex) const
{
    int index = 0;

    for (pcap_if_t* device = m_pAllDevs; device != nullptr; device = device->next, ++index)
    {
        if (index == nIndex)
            return device->description != nullptr ? device->description : device->name;
    }

    return nullptr;
}

// 선택한 어댑터를 promiscuous mode로 열고 과제 EtherType만 캡처하도록 설정한다.
BOOL CNILayer::SetAdapter(int nIndex)
{
    pcap_if_t* device = m_pAllDevs;

    for (int index = 0; device != nullptr && index < nIndex; ++index)
        device = device->next;

    if (device == nullptr)
        return FALSE;

    StopReceive();

    if (m_pAdapter != nullptr)
    {
        pcap_close(m_pAdapter);
        m_pAdapter = nullptr;
    }

    memset(m_ErrorBuffer, 0, sizeof(m_ErrorBuffer));
    // snaplen=65536, promiscuous=1, read timeout=200 ms.
    m_pAdapter = pcap_open_live(
        device->name,
        65536,
        1,
        200,
        m_ErrorBuffer);

    if (m_pAdapter == nullptr)
        return FALSE;

    // Only deliver frames used by this assignment.
    bpf_program filterProgram;
    const char* filterExpression = "ether proto 0x2080 or ether proto 0x2090 or ether proto 0x0806";

    if (pcap_compile(
            m_pAdapter,
            &filterProgram,
            filterExpression,
            1,
            PCAP_NETMASK_UNKNOWN) < 0)
    {
        strncpy_s(m_ErrorBuffer, pcap_geterr(m_pAdapter), _TRUNCATE);
        pcap_close(m_pAdapter);
        m_pAdapter = nullptr;
        return FALSE;
    }

    if (pcap_setfilter(m_pAdapter, &filterProgram) < 0)
    {
        strncpy_s(m_ErrorBuffer, pcap_geterr(m_pAdapter), _TRUNCATE);
        pcap_freecode(&filterProgram);
        pcap_close(m_pAdapter);
        m_pAdapter = nullptr;
        return FALSE;
    }

    pcap_freecode(&filterProgram);
    return StartReceive();
}

// 패킷 대기 때문에 UI가 멈추지 않도록 worker thread를 시작한다.
BOOL CNILayer::StartReceive()
{
    if (m_pAdapter == nullptr)
        return FALSE;

    if (m_bRunning)
        return TRUE;

    m_bRunning = TRUE;
    m_pReceiveThread = AfxBeginThread(ReceiveThread, this, THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);

    if (m_pReceiveThread == nullptr)
    {
        m_bRunning = FALSE;
        return FALSE;
    }

    m_pReceiveThread->m_bAutoDelete = FALSE;
    m_pReceiveThread->ResumeThread();
    return TRUE;
}

void CNILayer::StopReceive()
{
    m_bRunning = FALSE;

    if (m_pReceiveThread != nullptr)
    {
        if (m_pAdapter) pcap_breakloop(m_pAdapter);
        // Join before closing pcap or deleting the thread object.
        WaitForSingleObject(m_pReceiveThread->m_hThread, INFINITE);
        delete m_pReceiveThread;
        m_pReceiveThread = nullptr;
    }
}

UINT CNILayer::ReceiveThread(LPVOID pParam)
{
    CNILayer* layer = static_cast<CNILayer*>(pParam);
    return layer != nullptr ? layer->CaptureLoop() : 0;
}

// pcap_next_ex()로 프레임을 반복 수신하여 Ethernet 계층으로 올린다.
UINT CNILayer::CaptureLoop()
{
    struct pcap_pkthdr* packetHeader = nullptr;
    const u_char* packetData = nullptr;

    while (m_bRunning)
    {
        int result = pcap_next_ex(m_pAdapter, &packetHeader, &packetData);

        if (result == 1)
        {
            if (packetHeader->caplen >= 14 &&
                m_nUpperLayerCount > 0 &&
                mp_aUpperLayer[0] != nullptr)
            {
                mp_aUpperLayer[0]->Receive(
                    const_cast<unsigned char*>(packetData), static_cast<int>(packetHeader->caplen));
            }
        }
        else if (result == -1)
        {
            strncpy_s(m_ErrorBuffer, pcap_geterr(m_pAdapter), _TRUNCATE);
            break;
        }
        else if (result == -2)
        {
            break;
        }
    }

    m_bRunning = FALSE;
    return 0;
}

// Ethernet 계층에서 완성된 raw frame을 실제 랜카드로 전송한다.
BOOL CNILayer::Send(unsigned char* ppayload, int nlength)
{
    if (m_pAdapter == nullptr || ppayload == nullptr || nlength <= 0)
        return FALSE;

    return pcap_sendpacket(
        m_pAdapter,
        reinterpret_cast<const u_char*>(ppayload),
        nlength) == 0;
}

const char* CNILayer::GetLastError() const
{
    return m_ErrorBuffer;
}
