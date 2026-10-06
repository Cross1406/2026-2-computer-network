// EthernetLayer.cpp: implementation of the CEthernetLayer class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "pch.h"
#include "EthernetLayer.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CEthernetLayer::CEthernetLayer(char* pName)
	: CBaseLayer(pName)
{
	ResetHeader();
}

CEthernetLayer::~CEthernetLayer()
{
}

// 프레임 버퍼를 초기화하고 기본 EtherType을 채팅으로 설정한다.
void CEthernetLayer::ResetHeader()
{
	memset(m_sHeader.enet_dstaddr, 0, 6);
	memset(m_sHeader.enet_srcaddr, 0, 6);
	memset(m_sHeader.enet_data, 0, ETHER_MAX_DATA_SIZE);
	// Ethernet fields are transmitted in network byte order.
	m_sHeader.enet_type = ETHER_TYPE_CHAT_NETWORK;
}

unsigned char* CEthernetLayer::GetSourceAddress()
{
	return m_sHeader.enet_srcaddr;
}

unsigned char* CEthernetLayer::GetDestinAddress()
{
	// 현재 프레임 헤더에 설정된 목적지 MAC 주소를 반환한다.
	return m_sHeader.enet_dstaddr;
}

void CEthernetLayer::SetSourceAddress(unsigned char* pAddress)
{
	// 사용자가 입력한 6-byte MAC 주소를 송신 헤더에 저장한다.
	CSingleLock lock(&m_SendLock, TRUE);
	memcpy(m_sHeader.enet_srcaddr, pAddress, 6);
}

void CEthernetLayer::SetDestinAddress(unsigned char* pAddress)
{
	CSingleLock lock(&m_SendLock, TRUE);
	memcpy(m_sHeader.enet_dstaddr, pAddress, 6);
}

BOOL CEthernetLayer::Send(unsigned char* ppayload, int nlength)
{
	return Send(ppayload, nlength, ETHER_TYPE_CHAT_NETWORK);
}

// 응용 패킷을 Ethernet II frame으로 캡슐화한다.
// Chat/File 송신 스레드가 동시에 접근할 수 있어 critical section으로 보호한다.
BOOL CEthernetLayer::Send(
	unsigned char* ppayload,
	int nlength,
	unsigned short nType)
{
	if (ppayload == nullptr ||
		nlength <= 0 ||
		nlength > ETHER_MAX_DATA_SIZE ||
		mp_UnderLayer == nullptr)
	{
		return FALSE;
	}

    return SendTo(ppayload, nlength, nType, nullptr);
}

// ARP uses a per-frame destination, preserving the Chat/File destination MAC.
BOOL CEthernetLayer::SendTo(unsigned char* payload, int length, unsigned short type,
                           const unsigned char destination[6])
{
    if (!payload || length <= 0 || length > ETHER_MAX_DATA_SIZE || !mp_UnderLayer)
        return FALSE;
    CSingleLock lock(&m_SendLock, TRUE);
    unsigned char frame[ETHER_HEADER_SIZE + ETHER_MAX_DATA_SIZE] = {};
    memcpy(frame, destination ? destination : m_sHeader.enet_dstaddr, 6);
    memcpy(frame + 6, m_sHeader.enet_srcaddr, 6);
    memcpy(frame + 12, &type, 2);
    memcpy(frame + ETHER_HEADER_SIZE, payload, length);
    // Ethernet minimum without FCS is 60 bytes; NIC appends the FCS.
    const int wireLength = length + ETHER_HEADER_SIZE < 60 ? 60 : length + ETHER_HEADER_SIZE;
    return mp_UnderLayer->Send(frame, wireLength);
}

BOOL CEthernetLayer::Receive(unsigned char* payload)
{
    // A captured frame cannot safely be parsed without its captured length.
    UNREFERENCED_PARAMETER(payload);
    return FALSE;
}

BOOL CEthernetLayer::Receive(unsigned char* frame, int length)
{
    if (!frame || length < ETHER_HEADER_SIZE) return FALSE;
    unsigned short type = 0;
    memcpy(&type, frame + 12, 2);
    if (type != ETHER_TYPE_CHAT_NETWORK && type != ETHER_TYPE_FILE_NETWORK &&
        type != ETHER_TYPE_ARP_NETWORK) return FALSE;
    unsigned char source[6];
    {
        CSingleLock lock(&m_SendLock, TRUE);
        memcpy(source, m_sHeader.enet_srcaddr, 6);
    }
    const unsigned char broadcast[6] = {255,255,255,255,255,255};
    if ((memcmp(frame, source, 6) != 0 && memcmp(frame, broadcast, 6) != 0) ||
        memcmp(frame + 6, source, 6) == 0) return FALSE;
    const int payloadLength = length - ETHER_HEADER_SIZE;
    const int index = type == ETHER_TYPE_CHAT_NETWORK ? 0 : type == ETHER_TYPE_FILE_NETWORK ? 1 : 2;
    const int minimum = index == 0 ? 4 : index == 1 ? 12 : 28;
    if (payloadLength < minimum || m_nUpperLayerCount <= index || !mp_aUpperLayer[index])
        return FALSE;
    if (index == 2 && memcmp(frame + 6, frame + ETHER_HEADER_SIZE + 8, 6) != 0)
        return FALSE; // Ethernet source must match ARP SHA.
    return mp_aUpperLayer[index]->Receive(frame + ETHER_HEADER_SIZE, payloadLength);
}
