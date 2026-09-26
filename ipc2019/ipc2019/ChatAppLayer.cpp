// ChatAppLayer.cpp: implementation of the CChatAppLayer class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "pch.h"
#include "ChatAppLayer.h"
#include <atlconv.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CChatAppLayer::CChatAppLayer(char* pName)
	: CBaseLayer(pName),
	mp_Dlg(NULL)
{
	ResetHeader();
}

CChatAppLayer::~CChatAppLayer()
{

}

void CChatAppLayer::SetSourceAddress(unsigned int src_addr)
{
	m_sHeader.app_srcaddr = src_addr;
}

void CChatAppLayer::SetDestinAddress(unsigned int dst_addr)
{
	m_sHeader.app_dstaddr = dst_addr;
}

void CChatAppLayer::ResetHeader()
{
	m_sHeader.app_srcaddr = 0x00000000;
	m_sHeader.app_dstaddr = 0x00000000;
	m_sHeader.app_length = 0x0000;
	m_sHeader.app_type = 0x00;
	memset(m_sHeader.app_data, 0, APP_DATA_SIZE);
}

unsigned int CChatAppLayer::GetSourceAddress()
{
	return m_sHeader.app_srcaddr;
}

unsigned int CChatAppLayer::GetDestinAddress()
{
	return m_sHeader.app_dstaddr;
}

BOOL CChatAppLayer::Send(unsigned char* ppayload, int nlength)
{
	m_sHeader.app_length = (unsigned short)nlength;

	BOOL bSuccess = FALSE;
	//////////////////////// fill the blank ///////////////////////////////
		// 메모리 복사로 데이터를 header에 저장
		// ChatApp 레이어의 헤더에 데이터와 그 길이를 저장한다.
	memcpy(m_sHeader.app_data, ppayload, nlength > APP_DATA_SIZE ? APP_DATA_SIZE : nlength);

	// ChatApp 레이어의 밑에 레이어인 Ethertnet 레이어에 데이터를 넘겨준다.
	// 메로리 참조로 ChatApp의(헤더 + 데이터)와 (데이터 길이+헤더길이)를
	// 다음 계층의 data로 넘겨준다.
	bSuccess = mp_UnderLayer->Send((unsigned char*)&m_sHeader, nlength + APP_HEADER_SIZE);
	///////////////////////////////////////////////////////////////////////
	return bSuccess;
}

BOOL CChatAppLayer::Receive(unsigned char* ppayload)
{
	// ppayload를 ChatApp 헤더 구조체로 넣는다.
	PCHAT_APP_HEADER app_hdr = (PCHAT_APP_HEADER)ppayload;

	unsigned char receivedData[APP_DATA_SIZE + 1] = {};
	int receivedLength =
		app_hdr->app_length > APP_DATA_SIZE ? APP_DATA_SIZE : app_hdr->app_length;

	memcpy(receivedData, app_hdr->app_data, receivedLength);
	CString receivedMessage = CA2W(
		reinterpret_cast<const char*>(receivedData),
		CP_UTF8);
	CString displayMessage = _T("[RECV] ") + receivedMessage;

	return mp_aUpperLayer[0]->Receive(
		reinterpret_cast<unsigned char*>(
			const_cast<LPTSTR>(displayMessage.GetString())));
}


