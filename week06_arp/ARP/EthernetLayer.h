#pragma once
#include "BaseLayer.h"
#include <afxmt.h>
#define ETHER_TYPE_ARP 0x0806
#define ETHER_TYPE_ARP_NETWORK 0x0608

// 6주차 전용: ARP 프레임만 처리. 파일/채팅 주소와 분기는 없다.
class CEthernetLayer : public CBaseLayer {
public:
    explicit CEthernetLayer(char* name) : CBaseLayer(name) {}
    void SetSourceAddress(unsigned char* address);
    BOOL SendTo(unsigned char* payload, int length, unsigned short type,
                const unsigned char destination[6]);
    BOOL Receive(unsigned char* frame, int length) override;
private:
    unsigned char m_Source[6] = {};
    CCriticalSection m_Lock;
};
