# 6주차 · Basic ARP 전용 프로젝트

Visual Studio에서 **`week06_arp/Week06ARP.sln`**을 엽니다. 프로젝트 이름은 `Week06ARP`, 실행 파일은 `Week06ARP.exe`입니다.

이 폴더는 이전 `ipc2019` 프로젝트와 소스·리소스·프로젝트 GUID·빌드 출력이 독립적입니다. 기존 폴더는 수정하지 않았으며, 이 프로젝트에는 채팅/파일 계층과 파일 전송 화면이 없습니다. 공통 NI/Ethernet/Base/LayerManager 구조만 재사용하여 ARPLayer를 연결했습니다. `ARP` 폴더에 필요한 파일이 모두 있어 이 주차 폴더를 다른 위치에 복사해도 빌드할 수 있습니다.

## 처음 열기

1. 저장소 main을 Pull합니다. 기존 로컬 변경이 섞여 있다면 다른 새 폴더에 저장소를 복제해 기존 작업을 보존하세요.
2. Visual Studio에서 **파일 → 열기 → 프로젝트/솔루션**을 선택합니다.
3. 저장소의 `week06_arp/Week06ARP.sln`을 선택합니다. 이전 `ipc2019.sln`을 열면 이전 화면이 나옵니다.
4. 상단에서 **Debug / x64**를 선택하고 **빌드 → 솔루션 다시 빌드**합니다.
5. 성공하면 **Ctrl+F5**로 실행합니다. 창 제목은 **6주차 · Basic ARP**입니다.

Npcap과 WpdPack 설치는 기존 실습과 같습니다. 프로젝트에는 `C:\WpdPack\Include`, x64는 `C:\WpdPack\Lib\x64`, Win32는 `C:\WpdPack\Lib`가 설정되어 있습니다. 기본 toolset `v145`를 사용하므로 Visual Studio 버전이 다르면 설치된 toolset으로 재대상 지정하세요. 빌드 결과와 `.vs` 설정은 주차 폴더마다 따로 생깁니다.

## 화면 구성

- ARP 캐시: **IP Address / Ethernet Address / Status / 남은 시간(초)**
- **선택 삭제 / 전체 삭제**
- **대상 IP / ARP 요청**
- **어댑터 선택 / 연결**
- **자기 MAC / 자기 IP / 주소 적용**

사진의 Proxy ARP/GARP 영역은 이후 확장 범위이며 현재 화면에는 Basic ARP만 있습니다. OS ARP 캐시를 보여주는 것이 아니라 직접 구현한 ARPLayer의 캐시를 표시합니다.

## 두 PC 데모

학교에서 지정한 주소가 있으면 그 주소를 사용하세요. 아래는 같은 LAN의 다른 장치가 사용하지 않는 프로그램 시험 주소를 선택한 예시입니다.

| 항목 | PC A | PC B |
|---|---|---|
| 자기 MAC | A 유선 어댑터의 실제 MAC | B 유선 어댑터의 실제 MAC |
| 자기 IP | 192.168.10.1 | 192.168.10.2 |
| 대상 IP | 192.168.10.2 | 192.168.10.1 |

1. 두 PC를 같은 LAN에 연결합니다.
2. 각 PC에서 올바른 유선 어댑터를 선택하고 **연결**을 누릅니다.
3. Windows `ipconfig /all`에서 해당 어댑터의 물리적 주소를 확인해 **자기 MAC**에 입력합니다. 콜론/하이픈 구분자 모두 허용됩니다.
4. 서로 다른 **자기 IP**를 입력하고 두 PC 모두 **주소 적용**을 누릅니다.
5. A에서 B IP를 **대상 IP**에 입력하고 **ARP 요청**을 누릅니다.
6. B가 Request를 받으면 A IP→A MAC을 저장하고 Reply합니다. A에는 B IP→B MAC이 Complete로 표시됩니다.
7. 반대 방향도 동일하게 확인합니다. UI는 약 1초 간격으로 갱신하므로 빠른 응답이면 Incomplete를 보지 못할 수도 있습니다.
8. 없는 시험 주소를 요청하면 Incomplete로 남고 3분 후 삭제됩니다. Complete는 마지막 주소 학습부터 20분 후 삭제됩니다.
9. 선택 삭제/전체 삭제를 확인합니다. 자동 재시도는 없으므로 필요하면 다시 요청합니다.

자기 IP 입력은 프로그램 내부 ARP 주소 설정이며 Windows IP를 변경하지 않습니다. Windows 자체가 같은 IP에 응답할 수 있으므로 운영체제 응답을 프로그램 성공으로 오인하지 않도록 주소와 양쪽 프로그램 캐시를 함께 확인하세요. 어댑터나 자기 주소를 다시 적용하면 캐시가 초기화됩니다.

Wireshark 표시 필터는 **`arp`**입니다. 요청은 Ethernet 목적지 `FF:FF:FF:FF:FF:FF`, Opcode 1이고 응답은 요청자의 MAC으로 보내는 Opcode 2입니다. EtherType은 `0x0806`, Hardware type=1, Protocol type=`0x0800`, 주소 길이는 6/4입니다.

## 코드 설명

```text
Dialog → ARPLayer → EthernetLayer → NILayer → 랜카드
```

- `ARPDialog.cpp`: 버튼 처리, 어댑터/주소 설정, UI 스레드의 캐시 갱신
- `ARPProtocol.h`: 28바이트 ARP 포맷, Request/Reply 처리, IP별 캐시와 만료
- `ARPLayer.cpp`: 프로토콜 엔진과 Ethernet 송수신 연결, 캐시 동기화
- `EthernetLayer.cpp`: ARP 프레임만 송수신; 60바이트 최소 프레임 패딩
- `NILayer.cpp`: Npcap 송수신, ARP만 캡처, caplen 전달
- `BaseLayer` / `LayerManager`: 기존 계층 연결 방식 재사용

## 검증 범위

저장소 루트에서 `python3 tests/run_arp_tests.py`를 실행하면 기존 프로젝트와 이 6주차 프로젝트의 패킷/캐시 및 실제 Ethernet/ARP 소스를 각각 검증합니다. AddressSanitizer/UndefinedBehaviorSanitizer를 사용하며 주차별 VS 파일·리소스·의존성도 확인합니다. Windows MFC GUI 빌드와 실제 두 PC 통신은 별도로 실행해야 합니다.

다음 주차는 이 폴더를 기반으로 **새 주차 폴더·별도 `.sln`·별도 프로젝트 GUID**를 만들고 확장합니다. Git 브랜치는 공통 main으로 사용해도 주차 폴더가 서로 덮어쓰이지 않습니다.
