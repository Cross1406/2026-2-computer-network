#include "../ipc2019/ipc2019/ARPProtocol.h"
#include <cassert>
#include <iostream>
using namespace arp;
const Ip aIp{{192,168,10,1}}, bIp{{192,168,10,2}}, cIp{{192,168,10,3}};
const Mac aMac{{2,0,0,0,0,1}}, bMac{{2,0,0,0,0,2}}, cMac{{2,0,0,0,0,3}};
int main() {
    Ip parsed;
    assert(ParseIp("192.168.10.2", parsed) && parsed == bIp);
    for (const char* s : {"", "1.2.3", "1.2.3.4x", "1..2.3", "-1.2.3.4", "256.1.2.3",
                          "1.2.3.4.5", "0.0.0.0", "224.1.2.3", "127.0.0.1", "1.2.3.0000"})
        assert(!ParseIp(s, parsed));
    Engine a, b, c;
    assert(a.Configure(aIp, aMac) && b.Configure(bIp, bMac) && c.Configure(cIp, cMac));
    Packet request{}, reply{}, ignored{}; Mac destination{}, found{}; bool sendReply;
    assert(a.Request(bIp, 0, request));
    const Packet expected{{0,1,8,0,6,4,0,1, 2,0,0,0,0,1, 192,168,10,1,
                           0,0,0,0,0,0, 192,168,10,2}};
    assert(request == expected);
    assert(a.Snapshot(0).size() == 1 && !a.Snapshot(0)[0].complete);
    assert(!a.Lookup(bIp, 1, found));
    assert(c.Receive(request.data(), 28, 1, ignored, destination, sendReply) && !sendReply);
    assert(c.Snapshot(1).empty());
    assert(b.Receive(request.data(), 28, 1, reply, destination, sendReply) && sendReply);
    assert(destination == aMac && b.Lookup(aIp, 1, found) && found == aMac);
    Message decoded;
    assert(Decode(reply.data(), reply.size(), decoded));
    assert(decoded.opcode == 2 && decoded.senderMac == bMac && decoded.targetMac == aMac);
    assert(a.Receive(reply.data(), 28, 2, ignored, destination, sendReply) && !sendReply);
    assert(a.Lookup(bIp, 2, found) && found == bMac && a.Snapshot(2).size() == 1);
    assert(a.Receive(reply.data(), 28, 3, ignored, destination, sendReply) && !sendReply);
    assert(a.Snapshot(3).size() == 1); // Duplicate IP updates, never adds another row.
    assert(!a.Receive(request.data(), 28, 3, ignored, destination, sendReply)); // self
    decoded.targetIp = cIp;
    auto wrong = Encode(decoded);
    assert(!a.Receive(wrong.data(), 28, 3, ignored, destination, sendReply));
    decoded.targetIp = aIp; decoded.targetMac = cMac; wrong = Encode(decoded);
    assert(!a.Receive(wrong.data(), 28, 3, ignored, destination, sendReply));
    for (std::size_t n=0; n<28; ++n) assert(!Decode(reply.data(), n, decoded));
    assert(!Decode(nullptr, 28, decoded));
    for (int field : {0,1,2,3,4,5,6,7}) {
        auto invalid = reply; invalid[field] = 255;
        assert(!Decode(invalid.data(), invalid.size(), decoded));
    }
    auto padded = std::vector<unsigned char>(60,0);
    std::copy(reply.begin(), reply.end(), padded.begin());
    assert(Decode(padded.data(), padded.size(), decoded));
    assert(a.Lookup(bIp, 3 + CompleteLifetime - 1, found));
    assert(!a.Lookup(bIp, 3 + CompleteLifetime, found));
    assert(a.Request(cIp, 2000000, request));
    assert(a.Snapshot(2000000 + IncompleteLifetime - 1).size() == 1);
    assert(a.Snapshot(2000000 + IncompleteLifetime).empty());
    assert(!a.Request(aIp, 3000000, request));
    assert(a.Request(bIp, 3000000, request)); a.Remove(bIp); assert(a.Snapshot(3000000).empty());
    assert(a.Request(bIp, 3000000, request)); a.Clear(); assert(a.Snapshot(3000000).empty());
    assert(a.Request(bIp, 3000000, request)); a.Configure(cIp, cMac); assert(a.Snapshot(3000000).empty());
    a.Disable(); assert(!a.Request(bIp, 3000000, request));
    assert(!a.Receive(reply.data(),28,3000000,ignored,destination,sendReply));
    assert(!a.Configure(Ip{}, aMac) && !a.Configure(aIp, Broadcast()));
    a.Configure(aIp, aMac);
    for (int i=0; i<300; ++i) {
        Ip target{{192,168, static_cast<unsigned char>(i/250), static_cast<unsigned char>(i%250+1)}};
        assert(a.Request(target, 4000000, request));
    }
    assert(a.Snapshot(4000000).size() == 256);
    std::cout << "ARP protocol: wire bytes, two-host exchange, input validation, cache update/expiry passed\n";
}
