#pragma once
#include "BaseLayer.h"
#include "ARPProtocol.h"
#include <afxmt.h>

// UI thread and NI receive thread share the cache under one critical section.
class CARPLayer : public CBaseLayer {
public:
    explicit CARPLayer(char* name) : CBaseLayer(name) {}
    BOOL Configure(const arp::Ip& ip, const arp::Mac& mac);
    void Disable();
    BOOL MatchesMac(const arp::Mac& mac);
    BOOL Request(const arp::Ip& target);
    BOOL Receive(unsigned char* data, int length) override;
    std::vector<arp::Entry> Snapshot();
    BOOL Lookup(const arp::Ip& ip, arp::Mac& mac);
    void Remove(const arp::Ip& ip);
    void Clear();
private:
    arp::Engine m_Engine;
    CCriticalSection m_Lock;
};
