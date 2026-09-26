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
	//////////////////////// fill the blank ///////////////////////////////
	// Ethernet ������ �ּ� return
	return m_sHeader.enet_dstaddr;
	///////////////////////////////////////////////////////////////////////
}

void CEthernetLayer::SetSourceAddress(unsigned char* pAddress)
{
	//////////////////////// fill the blank ///////////////////////////////
		// �Ѱܹ��� source �ּҸ� Ethernet source�ּҷ� ����
	memcpy(m_sHeader.enet_srcaddr, pAddress, 6);
	///////////////////////////////////////////////////////////////////////
}

void CEthernetLayer::SetDestinAddress(unsigned char* pAddress)
{
	memcpy(m_sHeader.enet_dstaddr, pAddress, 6);
}

BOOL CEthernetLayer::Send(unsigned char* ppayload, int nlength)
{
	return Send(ppayload, nlength, ETHER_TYPE_CHAT_NETWORK);
}

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

	CSingleLock lock(&m_SendLock, TRUE);
	m_sHeader.enet_type = nType;
	memset(m_sHeader.enet_data, 0, ETHER_MAX_DATA_SIZE);
	memcpy(m_sHeader.enet_data, ppayload, nlength);

	return mp_UnderLayer->Send(
		(unsigned char*)&m_sHeader,
		nlength + ETHER_HEADER_SIZE);
}

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
