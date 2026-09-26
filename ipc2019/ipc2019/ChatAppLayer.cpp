#include "stdafx.h"
#include "pch.h"
#include "ChatAppLayer.h"
#include <atlconv.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#define new DEBUG_NEW
#endif

CChatAppLayer::CChatAppLayer(char* pName)
    : CBaseLayer(pName),
      m_ExpectedLength(0)
{
    ResetHeader();
}

CChatAppLayer::~CChatAppLayer()
{
}

unsigned short CChatAppLayer::Swap16(unsigned short value)
{
    return static_cast<unsigned short>(
        (value << 8) | (value >> 8));
}

void CChatAppLayer::ResetHeader()
{
    memset(&m_sHeader, 0, sizeof(m_sHeader));
}

BOOL CChatAppLayer::Send(unsigned char* ppayload, int nlength)
{
    if (ppayload == nullptr ||
        nlength <= 0 ||
        nlength > 0xffff ||
        mp_UnderLayer == nullptr)
    {
        return FALSE;
    }

    int offset = 0;

    while (offset < nlength)
    {
        int fragmentLength =
            (nlength - offset > APP_DATA_SIZE)
            ? APP_DATA_SIZE
            : nlength - offset;

        memset(m_sHeader.capp_data, 0, APP_DATA_SIZE);
        memcpy(
            m_sHeader.capp_data,
            ppayload + offset,
            fragmentLength);

        m_sHeader.capp_totlen =
            Swap16(static_cast<unsigned short>(nlength));
        m_sHeader.capp_unused = 0;

        if (offset == 0 && fragmentLength < nlength)
            m_sHeader.capp_type = CHAT_TYPE_FIRST;
        else if (offset + fragmentLength < nlength)
            m_sHeader.capp_type = CHAT_TYPE_MIDDLE;
        else
            m_sHeader.capp_type = CHAT_TYPE_LAST;

        if (!mp_UnderLayer->Send(
                reinterpret_cast<unsigned char*>(&m_sHeader),
                APP_HEADER_SIZE + fragmentLength))
        {
            return FALSE;
        }

        offset += fragmentLength;
    }

    return TRUE;
}

BOOL CChatAppLayer::Receive(unsigned char* ppayload)
{
    if (ppayload == nullptr)
        return FALSE;

    PCHAT_APP_HEADER header =
        reinterpret_cast<PCHAT_APP_HEADER>(ppayload);
    unsigned int totalLength = Swap16(header->capp_totlen);

    if (totalLength == 0)
        return FALSE;

    if (header->capp_type == CHAT_TYPE_FIRST)
    {
        m_ReceiveBuffer.clear();
        m_ReceiveBuffer.reserve(totalLength);
        m_ExpectedLength = totalLength;
    }
    else if (header->capp_type == CHAT_TYPE_LAST &&
             m_ReceiveBuffer.empty())
    {
        m_ExpectedLength = totalLength;
    }
    else if (m_ExpectedLength != totalLength)
    {
        m_ReceiveBuffer.clear();
        m_ExpectedLength = 0;
        return FALSE;
    }

    unsigned int remaining =
        m_ExpectedLength - static_cast<unsigned int>(m_ReceiveBuffer.size());
    unsigned int fragmentLength =
        remaining > APP_DATA_SIZE ? APP_DATA_SIZE : remaining;

    m_ReceiveBuffer.insert(
        m_ReceiveBuffer.end(),
        header->capp_data,
        header->capp_data + fragmentLength);

    if (header->capp_type != CHAT_TYPE_LAST)
        return TRUE;

    if (m_ReceiveBuffer.size() != m_ExpectedLength)
    {
        m_ReceiveBuffer.clear();
        m_ExpectedLength = 0;
        return FALSE;
    }

    m_ReceiveBuffer.push_back('\0');
    CString receivedMessage = CA2W(
        reinterpret_cast<const char*>(m_ReceiveBuffer.data()),
        CP_UTF8);
    CString displayMessage = _T("[RECV] ") + receivedMessage;

    m_ReceiveBuffer.clear();
    m_ExpectedLength = 0;

    return mp_aUpperLayer[0]->Receive(
        reinterpret_cast<unsigned char*>(
            const_cast<LPTSTR>(displayMessage.GetString())));
}
