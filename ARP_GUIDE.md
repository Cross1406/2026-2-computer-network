# 기존 통합 프로젝트의 Basic ARP 구현·실행 가이드

**6주차 독립 ARP 프로젝트는 [week06_arp/README.md](week06_arp/README.md)를 참고하세요. 아래 안내는 기존 `ipc2019` 통합 버전용입니다.**

기존 Chat/File Transfer 프로젝트에 Basic ARP를 추가했습니다. `NILayer`, `EthernetLayer`, `BaseLayer`, `LayerManager`를 재사용합니다. 채팅 `0x2080`, 파일 `0x2090`은 유지하고 ARP `0x0806`을 추가했습니다.

## 구현 범위

| 기능 | 구현 위치 | 동작 |
|---|---|---|
| ARP wire format | `ARPProtocol.h` | Ethernet/IPv4 ARP 28바이트, 네트워크 바이트 순서로 직접 인코딩 |
| Request | `ARPLayer.cpp` / `Engine::Request` | OP=1, Ethernet 목적지 FF:FF:FF:FF:FF:FF, THA=0 |
| Request 수신 | `Engine::Receive` | 대상 IP가 내 설정 IP면 송신자 캐시 저장 후 OP=2 응답 |
| Reply | `ARPLayer.cpp` | 요청자의 MAC으로 유니캐스트, SHA/SPA에 내 MAC/IP |
| Reply 수신 | `Engine::Receive` | 내 IP/MAC 대상 응답의 SPA→SHA를 Complete로 갱신; Reply에 재응답하지 않음 |
| 캐시 | `Engine` | IP별 하나의 행, Incomplete 180초 / Complete 1200초, 수신 학습 시 시간 갱신 |
| 캐시 UI | `ipc2019Dlg.cpp` | IP, MAC, 상태, 남은 초; 1초마다 갱신; 선택/전체 삭제 |
| 파일 전송 연결 | `OnArpUse` | Target IPv4의 유효 Complete MAC을 기존 Destination에 적용 |
| 수신 분기 | `EthernetLayer.cpp` | upper[0]=Chat, upper[1]=File, upper[2]=ARP |
| Npcap | `NILayer.cpp` | ARP 캡처 필터 추가, 실제 caplen을 상위 계층까지 전달 |

ARP Request/Reply 송신은 프레임별 목적지를 사용하므로 기존 채팅·파일의 Destination을 바꾸지 않습니다. MAC 적용 버튼을 눌렀을 때만 파일 전송 목적지를 바꿉니다. 파일 전송 중에는 목적지·어댑터·자기 주소 변경을 막습니다.

이 버전은 Basic ARP입니다. Proxy ARP, GARP, RARP 및 IPv4 라우팅은 구현 범위에 포함하지 않습니다. 새 IP 패킷 계층을 구현하지 않고 기존 raw Ethernet 파일 전송에 알아낸 MAC을 적용합니다. 파일 버튼을 누를 때 자동 ARP를 기다리는 기능은 없으며, 먼저 요청·응답을 확인한 뒤 MAC 적용을 누릅니다.

## Visual Studio 실행

1. 저장소 `main`을 Pull하고 `ipc2019/ipc2019.sln`을 엽니다.
2. 기존과 동일하게 `x64 / Debug`로 빌드합니다. 저장소의 기존 toolset은 `v145`입니다. 설치된 Visual Studio toolset과 다르면 프로젝트 속성에서 본인 환경의 toolset으로 재대상 지정하세요.
3. Npcap, `C:\WpdPack\Include`, `C:\WpdPack\Lib\x64` 설정은 기존 파일 전송과 같습니다.
4. 두 PC를 같은 LAN에 연결하고 각 프로그램에서 실제 유선 어댑터를 선택해 **연결**을 누릅니다.
5. 각 프로그램의 **Source Address**에 해당 어댑터의 실제 MAC을 입력합니다. Windows `ipconfig /all`의 해당 Ethernet 어댑터 물리적 주소를 사용할 수 있습니다.
6. 각자의 **My IPv4**를 입력하고 **ARP 설정**을 누릅니다. 상대 PC도 반드시 설정해야 자동 응답합니다. Destination MAC 입력이나 기존 MAC 설정 버튼은 ARP 실험에 필요하지 않습니다.

My IPv4는 프로그램 내부 ARP 주소이며 Windows의 IP 설정을 변경하지 않습니다. 수업 지정 주소를 우선 사용하세요. 아래 값은 같은 링크에 연결된 두 프로그램의 예시입니다.

| 입력 | PC A | PC B |
|---|---|---|
| Source Address | A 유선 어댑터의 실제 MAC | B 유선 어댑터의 실제 MAC |
| My IPv4 | 192.168.10.1 | 192.168.10.2 |
| Target IPv4 | 192.168.10.2 | 192.168.10.1 |

기본값이 두 PC에서 같으므로 **B에서는 My/Target을 반드시 반대로 수정**합니다. 프로그램 전용 시험 주소는 같은 LAN의 다른 장치가 사용하지 않는 주소로 선택하세요. Windows 자체도 동일 IP에 ARP 응답할 수 있으므로, 운영체제 응답만 캡처된 것을 프로그램의 Reply 성공으로 판단하지 않습니다.

## 데모 순서와 예상 결과

1. 두 PC에서 ARP 설정을 적용하고 캐시를 전체 삭제합니다.
2. A에서 B의 IP를 Target으로 넣고 **ARP 요청**을 누릅니다.
3. A 캐시에 B가 Incomplete로 들어갑니다. 응답이 빠르면 UI에는 바로 Complete로 보일 수 있습니다.
4. B는 A IP→A MAC을 Complete로 학습하고 자동 Reply합니다.
5. A는 B IP→B MAC을 Complete로 갱신합니다. UI 갱신은 최대 약 1초 걸립니다.
6. B에서도 A에 요청하여 반대 방향을 확인합니다.
7. A에서 **MAC 적용**을 누르면 B MAC이 Destination에 표시됩니다. 기존 채팅과 파일 전송을 실행합니다. B에서도 반대 방향 전송 시 해당 Target의 MAC을 적용합니다.
8. 선택 삭제/전체 삭제 후 행이 지워지는지 확인합니다.
9. 존재하지 않는 시험 IP를 요청해 Incomplete가 3분 뒤 없어지는지 확인합니다. Complete 항목은 마지막 학습에서 20분 뒤 만료됩니다. 자동 재시도는 없으며 필요하면 요청 버튼을 다시 누릅니다.

어댑터를 다시 연결하면 캐시와 ARP 설정이 초기화됩니다. 새 어댑터의 Source MAC을 입력한 뒤 ARP 설정을 다시 적용하세요. Source MAC을 기존 MAC 설정 버튼으로 바꿔도 ARP 설정을 다시 적용해야 합니다.

## Wireshark 확인

표시 필터: `arp`

| 필드 | A가 요청할 때 | B가 응답할 때 |
|---|---|---|
| Ethernet Dst | FF:FF:FF:FF:FF:FF | A MAC |
| Ethernet Src | A MAC | B MAC |
| EtherType | 0x0806 | 0x0806 |
| Hardware type / Protocol type | 1 / 0x0800 | 1 / 0x0800 |
| Hardware size / Protocol size | 6 / 4 | 6 / 4 |
| Opcode | 1 | 2 |
| Sender MAC / IP | A MAC / A IP | B MAC / B IP |
| Target MAC / IP | 00:00:00:00:00:00 / B IP | A MAC / A IP |

프로그램은 14바이트 Ethernet 헤더 + 28바이트 ARP 본문을 만들고 FCS 제외 최소 길이 60바이트까지 0으로 패딩합니다. FCS는 NIC가 추가하며 캡처 환경에 따라 표시되지 않을 수 있습니다.

보고서에는 A/B 설정, Request·Reply 필드 캡처, 양쪽 Complete 캐시, 응답 없는 Incomplete 및 삭제/만료 결과를 기록하세요. 채팅·파일까지 확인할 때는 `arp || eth.type == 0x2080 || eth.type == 0x2090`를 사용합니다.

## 구현 설명용 함수 흐름

- 요청: `OnArpRequest` → `CARPLayer::Request` → `Engine::Request` → `CEthernetLayer::SendTo` → `CNILayer::Send`
- 수신: `CNILayer::CaptureLoop` → `CEthernetLayer::Receive(data, caplen)` → `CARPLayer::Receive` → `Engine::Receive`
- 자동 응답: `Engine::Receive`에서 내 IP 대상 OP=1 판정 → OP=2 생성 → `SendTo`
- 캐시 화면: UI 타이머 → `CARPLayer::Snapshot` → 만료 삭제 → `RefreshArpCache`
- 파일 목적지 적용: `OnArpUse` → `Lookup` → `SetDestinAddress`

ARP 본문 파싱은 길이·HTYPE·PTYPE·HLEN·PLEN·Opcode를 검사합니다. Ethernet 송신자와 ARP SHA가 다르면 폐기하고 자신의 송신 복사본을 무시합니다. 다른 호스트 대상 Request에는 응답하지 않습니다. 기존에 알고 있는 송신자는 다른 대상 Request에서도 정보를 갱신하고, 처음 보는 송신자는 내 IP 대상 패킷에서만 추가합니다. IPv4 중복 주소 탐지용 0.0.0.0 probe 등은 이번 Basic ARP 범위에서 처리하지 않습니다.

캐시는 NI 수신 스레드와 UI 스레드가 함께 사용하므로 critical section으로 보호합니다. UI 컨트롤은 UI 타이머에서만 수정합니다. 캐시는 최대 256행으로 제한하며 새 항목으로 가득 찬 경우 가장 오래 삽입한 행을 제거합니다.

## 자동 검증과 남은 실기기 확인

Linux / g++:

```bash
python3 tests/run_arp_tests.py
```

- 28바이트 wire format을 예상 바이트와 비교
- 두 호스트 요청·응답, 양쪽 캐시 학습, Reply 반복 방지
- 중복 IP 갱신, 3분/20분 경계 만료, 삭제/초기화
- 잘못된 주소, 짧은 패킷, 잘못된 필드, 다른 대상 응답 거부
- **실제 BaseLayer/EthernetLayer/ARPLayer 소스**를 최소 MFC 테스트 shim으로 빌드해 송수신·역다중화·60바이트 패딩·기존 Destination 유지 검증
- AddressSanitizer / UndefinedBehaviorSanitizer로 메모리 범위 확인

GitHub Actions에서도 위 테스트를 실행합니다. 로컬 제한 환경에서 LeakSanitizer가 `/proc` 접근 오류를 내는 경우에만 `ASAN_OPTIONS=detect_leaks=0 python3 tests/run_arp_tests.py`로 실행할 수 있습니다. 주소 범위/정의되지 않은 동작 검사는 유지됩니다.

이 자동 테스트는 Windows MFC UI나 실제 Npcap 랜카드 실행을 대신하지 않습니다. Windows 빌드, 화면 표시, 두 PC 실제 송수신, Wireshark 캡처는 위 데모 순서로 확인해야 합니다.
