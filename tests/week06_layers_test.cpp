// Compiles the unchanged production Base/Ethernet/ARP sources with a small MFC shim.
// This tests real layer routing and framing, not the Windows UI/Npcap runtime.
#include "EthernetLayer.h"
#include "ARPLayer.h"
#include <cassert>
#include <iostream>
std::uint64_t testClock = 100;
struct Capture : CBaseLayer {
    std::vector<unsigned char> frame;
    bool fail = false;
    Capture() : CBaseLayer(const_cast<char*>("NI")) {}
    BOOL Send(unsigned char* data, int length) override {
        if (fail) return FALSE;
        frame.assign(data, data + length); return TRUE;
    }
};
struct App : CBaseLayer {
    int received = 0, lastLength = 0;
    App() : CBaseLayer(const_cast<char*>("App")) {}
    BOOL Receive(unsigned char*, int length) override { ++received; lastLength = length; return TRUE; }
};
struct Host {
    Capture ni; CEthernetLayer eth; CARPLayer arpLayer;
    Host(const arp::Ip& ip, const arp::Mac& mac) : eth(const_cast<char*>("Ethernet")), arpLayer(const_cast<char*>("ARP")) {
        ni.SetUpperUnderLayer(&eth);
        eth.SetUpperUnderLayer(&arpLayer);
        eth.SetSourceAddress(const_cast<unsigned char*>(mac.data()));
        assert(arpLayer.Configure(ip,mac));
    }
    BOOL Deliver(const std::vector<unsigned char>& packet) {
        auto copy = packet; return eth.Receive(copy.data(), static_cast<int>(copy.size()));
    }
};
int main() {
    const arp::Ip aIp{{192,168,10,1}}, bIp{{192,168,10,2}}, cIp{{192,168,10,3}};
    const arp::Mac aMac{{2,0,0,0,0,1}}, bMac{{2,0,0,0,0,2}};
    Host a(aIp,aMac), b(bIp,bMac);
    assert(a.arpLayer.Request(bIp));
    auto request=a.ni.frame;
    assert(request.size()==60 && request[12]==8 && request[13]==6);
    const auto broadcast=arp::Broadcast();
    assert(std::memcmp(request.data(),broadcast.data(),6)==0);
    for (std::size_t i=42;i<60;++i) assert(request[i]==0);
    assert(b.Deliver(request));
    assert(b.ni.frame.size()==60 && b.ni.frame[21]==2);
    assert(std::memcmp(b.ni.frame.data(),aMac.data(),6)==0);
    assert(a.Deliver(b.ni.frame));
    arp::Mac found;
    assert(a.arpLayer.Lookup(bIp,found) && found==bMac);
    assert(b.arpLayer.Lookup(aIp,found) && found==aMac);
    assert(a.ni.frame==request); // Replies are not acknowledged with another ARP.
    auto truncated=request; truncated.resize(41); assert(!b.Deliver(truncated));
    auto mismatch=request; mismatch[22]=9; assert(!b.Deliver(mismatch)); // SHA != Ethernet source
    auto unknown=request; unknown[13]=0; assert(!b.Deliver(unknown));
    auto malformed=request; malformed[18]=5; assert(!b.Deliver(malformed)); // HLEN
    assert(!a.Deliver(request)); // local transmit echo
    // This project's Ethernet interface contains no chat/file Send or destination setter.
    unsigned char unsupported[60] = {};
    assert(!a.eth.SendTo(unsupported,28,0x8020,bMac.data()));
    a.ni.fail=true;
    assert(!a.arpLayer.Request(cIp));
    for (const auto& e : a.arpLayer.Snapshot()) assert(e.ip!=cIp);
    testClock += arp::CompleteLifetime;
    assert(!a.arpLayer.Lookup(bIp,found));
    std::cout << "Week06 ARP-only layers: request/reply, padding, malformed-frame rejection passed\n";
}
