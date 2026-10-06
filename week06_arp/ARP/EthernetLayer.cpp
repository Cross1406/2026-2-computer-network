#include "pch.h"
#include "EthernetLayer.h"

void CEthernetLayer::SetSourceAddress(unsigned char* address) {
    if (!address) return;
    CSingleLock lock(&m_Lock, TRUE);
    memcpy(m_Source, address, 6);
}
BOOL CEthernetLayer::SendTo(unsigned char* payload, int length, unsigned short type,
                          const unsigned char destination[6]) {
    if (!payload || !destination || length != 28 || type != ETHER_TYPE_ARP_NETWORK || !mp_UnderLayer)
        return FALSE;
    CSingleLock lock(&m_Lock, TRUE);
    unsigned char frame[60] = {}; // Header 14 + ARP 28 + zero padding 18. NIC adds FCS.
    memcpy(frame, destination, 6);
    memcpy(frame + 6, m_Source, 6);
    frame[12] = 0x08; frame[13] = 0x06;
    memcpy(frame + 14, payload, 28);
    return mp_UnderLayer->Send(frame, sizeof(frame));
}
BOOL CEthernetLayer::Receive(unsigned char* frame, int length) {
    if (!frame || length < 42 || frame[12] != 0x08 || frame[13] != 0x06)
        return FALSE;
    unsigned char source[6];
    { CSingleLock lock(&m_Lock, TRUE); memcpy(source, m_Source, 6); }
    const unsigned char broadcast[6] = {255,255,255,255,255,255};
    if ((memcmp(frame, source, 6) != 0 && memcmp(frame, broadcast, 6) != 0) ||
        memcmp(frame + 6, source, 6) == 0 || memcmp(frame + 6, frame + 22, 6) != 0)
        return FALSE;
    if (m_nUpperLayerCount < 1 || !mp_aUpperLayer[0]) return FALSE;
    return mp_aUpperLayer[0]->Receive(frame + 14, length - 14);
}
