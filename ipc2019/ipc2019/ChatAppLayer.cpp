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

// 프로세서의 바이트 순서와 네트워크 바이트 순서 사이를 변환한다.
unsigned short CChatAppLayer::Swap16(unsigned short value)
{
    return static_cast<unsigned short>(
        (value << 8) | (value >> 8));
}

void CChatAppLayer::ResetHeader()
{
    memset(&m_sHeader, 0, sizeof(m_sHeader));
}

// UTF-8 채팅 문자열을 1496-byte 이하 조각으로 나누어 Ethernet 계층으로 보낸다.
BOOL CChatAppLayer::Send(unsigned char* ppayload, int nlength)
{
    if (ppayload == nullptr ||
        nlength <= 0 ||
        nlength > 0xffff ||
        mp_UnderLayer == nullptr)
    {
        return FALSE;
    }

    // offset은 원본 메시지에서 다음에 보낼 위치이다.
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

        // ChatApp header까지 포함한 길이를 Ethernet payload 길이로 전달한다.
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

// 조각을 순서대로 버퍼에 누적하고 LAST에서 UTF-8 문자열로 복원한다.
BOOL CChatAppLayer::Receive(unsigned char* ppayload)
{
    if (ppayload == nullptr)
        return FALSE;

    PCHAT_APP_HEADER header =
        reinterpret_cast<PCHAT_APP_HEADER>(ppayload);
    unsigned int totalLength = Swap16(header->capp_totlen);

    if (totalLength == 0)
        return FALSE;

    // FIRST에서는 이전 상태를 비우고 전체 길이만큼 재조립 버퍼를 준비한다.
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

    // 마지막 조각 전까지는 버퍼에만 저장하고 수신을 계속한다.
    if (header->capp_type != CHAT_TYPE_LAST)
        return TRUE;

    if (m_ReceiveBuffer.size() != m_ExpectedLength)
    {
        m_ReceiveBuffer.clear();
        m_ExpectedLength = 0;
        return FALSE;
    }

    // 완성된 UTF-8 바이트열 끝에 NULL을 붙인 후 화면 표시용 CString으로 변환한다.
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


// Captured length is mandatory on the NI -> Ethernet receive path.
BOOL CChatAppLayer::Receive(unsigned char* payload, int length)
{
    if (!payload || length < APP_HEADER_SIZE) return FALSE;
    auto* header = reinterpret_cast<PCHAT_APP_HEADER>(payload);
    const unsigned int total = Swap16(header->capp_totlen);
    if (header->capp_type != CHAT_TYPE_FIRST && header->capp_type != CHAT_TYPE_MIDDLE &&
        header->capp_type != CHAT_TYPE_LAST) return FALSE;
    const unsigned int used = header->capp_type == CHAT_TYPE_FIRST ? 0 :
                             static_cast<unsigned int>(m_ReceiveBuffer.size());
    if (total <= used) return FALSE;
    const unsigned int remaining = total - used;
    const unsigned int needed = remaining > APP_DATA_SIZE ? APP_DATA_SIZE : remaining;
    if (static_cast<unsigned int>(length - APP_HEADER_SIZE) < needed) return FALSE;
    return Receive(payload);
}
