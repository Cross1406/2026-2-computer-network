#pragma once

#include "BaseLayer.h"
#include "pch.h"
#include <vector>

// 채팅 메시지의 단편화와 재조립을 담당하는 응용 계층.
//
// Ethernet payload 1500 bytes 중 CHAT_APP_HEADER 4 bytes를 제외하여
// 한 조각에 최대 APP_DATA_SIZE(1496 bytes)를 담는다.
class CChatAppLayer : public CBaseLayer
{
private:
    inline void ResetHeader();

public:
    BOOL Receive(unsigned char* ppayload) override;
    BOOL Receive(unsigned char* ppayload, int length) override;
    BOOL Send(unsigned char* ppayload, int nlength) override;

    CChatAppLayer(char* pName);
    virtual ~CChatAppLayer();

#pragma pack(push, 1)
    // Wire format (총 4-byte header + data):
    // [전체 메시지 길이:2][조각 종류:1][예약:1][채팅 데이터]
    typedef struct _CHAT_APP_HEADER
    {
        unsigned short capp_totlen;
        unsigned char capp_type;
        unsigned char capp_unused;
        unsigned char capp_data[APP_DATA_SIZE];
    } CHAT_APP_HEADER, *PCHAT_APP_HEADER;
#pragma pack(pop)

private:
    enum
    {
        // 메시지가 여러 패킷이면 FIRST -> MIDDLE... -> LAST 순서로 전송한다.
        // 한 패킷에 끝나는 메시지는 LAST 하나만 사용한다.
        CHAT_TYPE_FIRST = 0x00,
        CHAT_TYPE_MIDDLE = 0x01,
        CHAT_TYPE_LAST = 0x02
    };

    static unsigned short Swap16(unsigned short value);

private:
    CHAT_APP_HEADER m_sHeader;
    std::vector<unsigned char> m_ReceiveBuffer;
    unsigned int m_ExpectedLength;
};
