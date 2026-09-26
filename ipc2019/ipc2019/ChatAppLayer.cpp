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
	if (ppayload == nullptr || nlength <= 0 || mp_UnderLayer == nullptr)
		return FALSE;

	int offset = 0;

	while (offset < nlength)
	{
		int fragmentLength =
			(nlength - offset > APP_DATA_SIZE)
			? APP_DATA_SIZE
			: nlength - offset;

		memset(m_sHeader.app_data, 0, APP_DATA_SIZE);
		memcpy(
			m_sHeader.app_data,
			ppayload + offset,
			fragmentLength);

		m_sHeader.app_length =
			static_cast<unsigned short>(fragmentLength);
		m_sHeader.app_type =
			(offset + fragmentLength < nlength)
			? DATA_TYPE_CONT
			: DATA_TYPE_END;

		if (!mp_UnderLayer->Send(
				(unsigned char*)&m_sHeader,
				fragmentLength + APP_HEADER_SIZE))
		{
			return FALSE;
		}

		offset += fragmentLength;
	}

	return TRUE;
}

BOOL CChatAppLayer::Receive(unsigned char* ppayload)
{
	// ppayload를 ChatApp 헤더 구조체로 넣는다.
	PCHAT_APP_HEADER app_hdr = (PCHAT_APP_HEADER)ppayload;

	if (app_hdr->app_length > APP_DATA_SIZE)
	{
		m_ReceiveBuffer.clear();
		return FALSE;
	}

	m_ReceiveBuffer.insert(
		m_ReceiveBuffer.end(),
		app_hdr->app_data,
		app_hdr->app_data + app_hdr->app_length);

	if (app_hdr->app_type == DATA_TYPE_CONT)
		return TRUE;

	if (app_hdr->app_type != DATA_TYPE_END)
	{
		m_ReceiveBuffer.clear();
		return FALSE;
	}

	m_ReceiveBuffer.push_back('\0');
	CString receivedMessage = CA2W(
		reinterpret_cast<const char*>(m_ReceiveBuffer.data()),
		CP_UTF8);
	CString displayMessage = _T("[RECV] ") + receivedMessage;
	m_ReceiveBuffer.clear();

	return mp_aUpperLayer[0]->Receive(
		reinterpret_cast<unsigned char*>(
			const_cast<LPTSTR>(displayMessage.GetString())));
}


