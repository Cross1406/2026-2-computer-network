#pragma once

#include "BaseLayer.h"
#include "pch.h"
#include <vector>

class CChatAppLayer : public CBaseLayer
{
private:
    inline void ResetHeader();

public:
    BOOL Receive(unsigned char* ppayload) override;
    BOOL Send(unsigned char* ppayload, int nlength) override;

    CChatAppLayer(char* pName);
    virtual ~CChatAppLayer();

#pragma pack(push, 1)
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
