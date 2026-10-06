// EthernetLayer.h: interface for the CEthernetLayer class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ETHERNETLAYER_H__7857C9C2_B459_4DC8_B9B3_4E6C8B587B29__INCLUDED_)
#define AFX_ETHERNETLAYER_H__7857C9C2_B459_4DC8_B9B3_4E6C8B587B29__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "BaseLayer.h"
#include "pch.h"
#include <afxmt.h>

// Wireshark에서 확인하는 사용자 정의 EtherType.
// 구조체 메모리에는 little-endian 값이 들어가므로 *_NETWORK 상수를 전송에 사용한다.
#define ETHER_TYPE_CHAT 0x2080
#define ETHER_TYPE_FILE 0x2090
#define ETHER_TYPE_CHAT_NETWORK 0x8020
#define ETHER_TYPE_FILE_NETWORK 0x9020
#define ETHER_TYPE_ARP 0x0806
#define ETHER_TYPE_ARP_NETWORK 0x0608

// 응용 계층 패킷에 목적지/출발지 MAC과 EtherType을 붙이는 계층.
// 수신 시에는 주소와 타입을 검사한 뒤 ChatApp 또는 File로 역다중화한다.
class CEthernetLayer
	: public CBaseLayer
{
private:
	inline void		ResetHeader();

public:
	BOOL Receive(unsigned char* ppayload) override;
	BOOL Receive(unsigned char* ppayload, int length) override;
	BOOL SendTo(unsigned char* payload, int length, unsigned short type, const unsigned char destination[6]);
	BOOL			Send(unsigned char* ppayload, int nlength);
	BOOL			Send(unsigned char* ppayload, int nlength, unsigned short nType);
	void			SetDestinAddress(unsigned char* pAddress);
	void			SetSourceAddress(unsigned char* pAddress);
	unsigned char* GetDestinAddress();
	unsigned char* GetSourceAddress();

	CEthernetLayer(char* pName);
	virtual ~CEthernetLayer();

	// Ethernet II frame: [Dst MAC 6][Src MAC 6][Type 2][Payload <= 1500]
	typedef struct _ETHERNET_HEADER {

		unsigned char	enet_dstaddr[6];		// destination address of ethernet layer
		unsigned char	enet_srcaddr[6];		// source address of ethernet layer
		unsigned short	enet_type;		// type of ethernet layer
		unsigned char	enet_data[ETHER_MAX_DATA_SIZE]; // frame data

	} ETHERNET_HEADER, * PETHERNET_HEADER;

protected:
	ETHERNET_HEADER	m_sHeader;
	CCriticalSection m_SendLock;
};

#endif // !defined(AFX_ETHERNETLAYER_H__7857C9C2_B459_4DC8_B9B3_4E6C8B587B29__INCLUDED_)
