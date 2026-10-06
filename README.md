# 2026-2 Computer Network — Chat & File Transfer + Basic ARP

두 PC를 LAN선으로 연결하고 Npcap을 이용해 raw Ethernet frame으로 채팅과 파일을 주고받는 MFC 프로젝트입니다.

## 핵심 구조

```text
Cipc2019Dlg
 ├─ CChatAppLayer ─┐
 ├─ CFileLayer    ─┤
 └─ CARPLayer     ─┤
                   └─ CEthernetLayer ── CNILayer(Npcap) ── LAN Card
```

- 채팅 EtherType: `0x2080`
- 파일 EtherType: `0x2090`
- ARP EtherType: `0x0806`
- Basic ARP 요청/응답, 캐시·만료·삭제, 알아낸 MAC의 파일 전송 적용 지원
- 채팅 payload: 최대 1496 bytes (`1500 - 4-byte header`)
- 파일 payload: 최대 1488 bytes (`1500 - 12-byte header`)
- 수신 파일: 프로그램 실행 폴더의 `ReceivedFiles`

구현 발표를 준비할 때는 [IMPLEMENTATION_GUIDE.md](IMPLEMENTATION_GUIDE.md)를 먼저 읽으세요.

## 실행 순서

1. Npcap과 WpdPack 개발 파일을 준비합니다.
2. Visual Studio에서 `ipc2019/ipc2019.sln`을 엽니다.
3. `x64 / Debug`로 빌드합니다.
4. 두 PC에서 실제 유선 Ethernet 어댑터를 선택하고 연결합니다.
5. 각 PC의 Source에는 자신의 MAC, Destination에는 상대 PC의 MAC을 입력합니다.
6. 짧은 채팅을 양방향으로 시험한 뒤 파일을 전송합니다.

Wireshark 표시 필터:

```text
arp || eth.type == 0x2080 || eth.type == 0x2090
```

## ARP 실습

[ARP_GUIDE.md](ARP_GUIDE.md)에 두 PC 설정, 요청·응답 데모, 캐시 만료, Wireshark 필드와 코드 설명을 정리했습니다.

어댑터 연결 → Source MAC / My IPv4 입력 → **ARP 설정** → Target IPv4 입력 → **ARP 요청** → Complete 확인 → **MAC 적용** → 기존 채팅·파일 전송 순서입니다. ARP 실험 자체에는 Destination MAC 입력이 필요하지 않습니다.

자동 테스트: `python3 tests/run_arp_tests.py` (Linux/g++, ASan/UBSan). Windows MFC 빌드와 두 PC 실통신은 별도로 확인해야 합니다.
