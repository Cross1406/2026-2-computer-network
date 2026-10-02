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
	memcpy(m_sHeader.enet_srcaddr, pAddress, 6);
}

void CEthernetLayer::SetDestinAddress(unsigned char* pAddress)
{
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

	// 공유 프레임 버퍼(m_sHeader)를 한 번에 한 송신만 수정하도록 잠근다.
	CSingleLock lock(&m_SendLock, TRUE);
	m_sHeader.enet_type = nType;
	memset(m_sHeader.enet_data, 0, ETHER_MAX_DATA_SIZE);
	memcpy(m_sHeader.enet_data, ppayload, nlength);

	return mp_UnderLayer->Send(
		(unsigned char*)&m_sHeader,
		nlength + ETHER_HEADER_SIZE);
}

// Npcap에서 받은 Ethernet frame을 필터링하고 알맞은 상위 계층으로 전달한다.
BOOL CEthernetLayer::Receive(unsigned char* ppayload)
{
	PETHERNET_HEADER pFrame = (PETHERNET_HEADER)ppayload;

	BOOL bSuccess = FALSE;

	// Ignore normal IP/ARP traffic captured by Npcap.
	if (pFrame->enet_type != ETHER_TYPE_CHAT_NETWORK &&
		pFrame->enet_type != ETHER_TYPE_FILE_NETWORK)
		return FALSE;

	const unsigned char broadcastAddress[6] =
		{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	// Accept only frames addressed to this host or broadcast frames.
	if (memcmp(pFrame->enet_dstaddr, m_sHeader.enet_srcaddr, 6) != 0 &&
		memcmp(pFrame->enet_dstaddr, broadcastAddress, 6) != 0)
		return FALSE;

	// Ignore a copy of a frame sent by this application.
	if (memcmp(pFrame->enet_srcaddr, m_sHeader.enet_srcaddr, 6) == 0)
		return FALSE;

	// EtherType을 이용한 demultiplexing:
	// upper[0] = ChatApp, upper[1] = File.
	if (pFrame->enet_type == ETHER_TYPE_CHAT_NETWORK &&
		m_nUpperLayerCount > 0 &&
		mp_aUpperLayer[0] != nullptr)
	{
		bSuccess = mp_aUpperLayer[0]->Receive(
			(unsigned char*)pFrame->enet_data);
	}
	else if (pFrame->enet_type == ETHER_TYPE_FILE_NETWORK &&
		m_nUpperLayerCount > 1 &&
		mp_aUpperLayer[1] != nullptr)
	{
		bSuccess = mp_aUpperLayer[1]->Receive(
			(unsigned char*)pFrame->enet_data);
	}

	return bSuccess;
}
