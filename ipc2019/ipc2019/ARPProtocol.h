#pragma once
// Windows/MFC에 의존하지 않는 ARP wire format과 캐시 상태 기계.
#include <array>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace arp {
using Mac = std::array<unsigned char, 6>;
using Ip = std::array<unsigned char, 4>;
using Packet = std::array<unsigned char, 28>;
constexpr std::uint64_t IncompleteLifetime = 180000; // milliseconds
constexpr std::uint64_t CompleteLifetime = 1200000;
inline Mac Broadcast() { return {{255, 255, 255, 255, 255, 255}}; }
inline bool ValidMac(const Mac& mac) { return mac != Mac{} && (mac[0] & 1) == 0; }
inline bool ValidIp(const Ip& ip) { return ip[0] != 0 && ip[0] < 224 && ip[0] != 127; }

// Strict dotted-decimal parser: no partial input, signs, or trailing text.
inline bool ParseIp(const char* text, Ip& ip) {
    if (!text) return false;
    Ip parsed{};
    for (int part = 0; part < 4; ++part) {
        unsigned value = 0, digits = 0;
        while (*text >= '0' && *text <= '9') {
            value = value * 10 + (*text++ - '0');
            if (++digits > 3 || value > 255) return false;
        }
        if (!digits) return false;
        parsed[part] = static_cast<unsigned char>(value);
        if (part < 3) { if (*text++ != '.') return false; }
        else if (*text != '\0') return false;
    }
    if (!ValidIp(parsed)) return false;
    ip = parsed;
    return true;
}
struct Message {
    unsigned short opcode = 0;
    Mac senderMac{}, targetMac{};
    Ip senderIp{}, targetIp{};
};
inline Packet Encode(const Message& m) {
    // Byte-wise encoding avoids struct padding and host-endian dependencies.
    Packet p{};
    p[1] = 1; p[2] = 8; p[4] = 6; p[5] = 4;
    p[6] = static_cast<unsigned char>(m.opcode >> 8);
    p[7] = static_cast<unsigned char>(m.opcode);
    std::copy(m.senderMac.begin(), m.senderMac.end(), p.begin() + 8);
    std::copy(m.senderIp.begin(), m.senderIp.end(), p.begin() + 14);
    std::copy(m.targetMac.begin(), m.targetMac.end(), p.begin() + 18);
    std::copy(m.targetIp.begin(), m.targetIp.end(), p.begin() + 24);
    return p;
}
inline bool Decode(const unsigned char* p, std::size_t length, Message& m) {
    if (!p || length < 28 || p[0] != 0 || p[1] != 1 ||
        p[2] != 8 || p[3] != 0 || p[4] != 6 || p[5] != 4 ||
        p[6] != 0 || (p[7] != 1 && p[7] != 2)) return false;
    m.opcode = p[7];
    std::copy(p + 8, p + 14, m.senderMac.begin());
    std::copy(p + 14, p + 18, m.senderIp.begin());
    std::copy(p + 18, p + 24, m.targetMac.begin());
    std::copy(p + 24, p + 28, m.targetIp.begin());
    return ValidMac(m.senderMac) && ValidIp(m.senderIp) && ValidIp(m.targetIp);
}
struct Entry {
    Ip ip{};
    Mac mac{};
    bool complete = false;
    std::uint64_t updatedAt = 0;
};
// Caller serializes access. Explicit time makes expiry tests deterministic.
class Engine {
    Ip selfIp{};
    Mac selfMac{};
    bool configured = false;
    std::vector<Entry> entries;
    void Store(const Ip& ip, const Mac& mac, bool complete, std::uint64_t now) {
        for (auto& e : entries) if (e.ip == ip) {
            e.mac = mac; e.complete = complete; e.updatedAt = now; return;
        }
        if (entries.size() >= 256) entries.erase(entries.begin());
        Entry e; e.ip = ip; e.mac = mac; e.complete = complete; e.updatedAt = now;
        entries.push_back(e);
    }
public:
    bool Configure(const Ip& ip, const Mac& mac) {
        if (!ValidIp(ip) || !ValidMac(mac)) return false;
        selfIp = ip; selfMac = mac; configured = true; entries.clear(); return true;
    }
    void Disable() { configured = false; entries.clear(); }
    bool IsConfigured() const { return configured; }
    bool MatchesMac(const Mac& mac) const { return configured && selfMac == mac; }
    void Expire(std::uint64_t now) {
        entries.erase(std::remove_if(entries.begin(), entries.end(), [now](const Entry& e) {
            return now - e.updatedAt >= (e.complete ? CompleteLifetime : IncompleteLifetime);
        }), entries.end());
    }
    bool Request(const Ip& target, std::uint64_t now, Packet& packet) {
        if (!configured || !ValidIp(target) || target == selfIp) return false;
        Expire(now);
        Message m; m.opcode = 1; m.senderMac = selfMac; m.senderIp = selfIp; m.targetIp = target;
        packet = Encode(m);
        // A manual request also refreshes a previously completed entry.
        Store(target, Mac{}, false, now);
        return true;
    }
    bool Receive(const unsigned char* data, std::size_t length, std::uint64_t now,
                 Packet& reply, Mac& destination, bool& sendReply) {
        sendReply = false;
        Message m;
        if (!configured || !Decode(data, length, m) || m.senderMac == selfMac || m.senderIp == selfIp)
            return false;
        Expire(now);
        const bool forUs = m.targetIp == selfIp;
        if (m.opcode == 2 && (!forUs || m.targetMac != selfMac)) return false;
        // RFC-style learning: refresh existing sender entry; create if addressed to us.
        bool existing = false;
        for (const auto& e : entries) if (e.ip == m.senderIp) existing = true;
        if (forUs || existing) Store(m.senderIp, m.senderMac, true, now);
        if (m.opcode == 1 && forUs) {
            Message response; response.opcode = 2;
            response.senderMac = selfMac; response.senderIp = selfIp;
            response.targetMac = m.senderMac; response.targetIp = m.senderIp;
            reply = Encode(response); destination = m.senderMac; sendReply = true;
        }
        return true;
    }
    bool Lookup(const Ip& ip, std::uint64_t now, Mac& mac) {
        Expire(now);
        for (const auto& e : entries) if (e.ip == ip && e.complete) { mac = e.mac; return true; }
        return false;
    }
    std::vector<Entry> Snapshot(std::uint64_t now) { Expire(now); return entries; }
    void Remove(const Ip& ip) {
        entries.erase(std::remove_if(entries.begin(), entries.end(), [&ip](const Entry& e) {
            return e.ip == ip;
        }), entries.end());
    }
    void Clear() { entries.clear(); }
};
}
