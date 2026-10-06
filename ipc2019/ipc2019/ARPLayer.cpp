#include "pch.h"
#include "ARPLayer.h"
#include "EthernetLayer.h"

BOOL CARPLayer::Configure(const arp::Ip& ip, const arp::Mac& mac) {
    CSingleLock lock(&m_Lock, TRUE);
    return m_Engine.Configure(ip, mac);
}
void CARPLayer::Disable() { CSingleLock lock(&m_Lock, TRUE); m_Engine.Disable(); }
BOOL CARPLayer::MatchesMac(const arp::Mac& mac) {
    CSingleLock lock(&m_Lock, TRUE); return m_Engine.MatchesMac(mac);
}
BOOL CARPLayer::Request(const arp::Ip& target) {
    CSingleLock lock(&m_Lock, TRUE);
    arp::Packet packet;
    if (!mp_UnderLayer || !m_Engine.Request(target, GetTickCount64(), packet)) return FALSE;
    auto* ethernet = static_cast<CEthernetLayer*>(mp_UnderLayer);
    const auto broadcast = arp::Broadcast();
    if (!ethernet->SendTo(packet.data(), static_cast<int>(packet.size()),
                         ETHER_TYPE_ARP_NETWORK, broadcast.data())) {
        m_Engine.Remove(target); // Failed sends must not leave a fake pending request.
        return FALSE;
    }
    return TRUE;
}
BOOL CARPLayer::Receive(unsigned char* data, int length) {
    if (length < 28) return FALSE;
    CSingleLock lock(&m_Lock, TRUE);
    arp::Packet reply; arp::Mac destination; bool sendReply = false;
    if (!m_Engine.Receive(data, static_cast<std::size_t>(length), GetTickCount64(),
                          reply, destination, sendReply)) return FALSE;
    if (sendReply) {
        if (!mp_UnderLayer) return FALSE;
        return static_cast<CEthernetLayer*>(mp_UnderLayer)->SendTo(
            reply.data(), static_cast<int>(reply.size()), ETHER_TYPE_ARP_NETWORK, destination.data());
    }
    return TRUE; // Reply received: update cache only; never reply to a reply.
}
std::vector<arp::Entry> CARPLayer::Snapshot() {
    CSingleLock lock(&m_Lock, TRUE); return m_Engine.Snapshot(GetTickCount64());
}
BOOL CARPLayer::Lookup(const arp::Ip& ip, arp::Mac& mac) {
    CSingleLock lock(&m_Lock, TRUE); return m_Engine.Lookup(ip, GetTickCount64(), mac);
}
void CARPLayer::Remove(const arp::Ip& ip) { CSingleLock lock(&m_Lock, TRUE); m_Engine.Remove(ip); }
void CARPLayer::Clear() { CSingleLock lock(&m_Lock, TRUE); m_Engine.Clear(); }
