# 구현 설명 가이드

## 1. 한 문장 설명

이 프로그램은 응용 데이터를 Chat/File 패킷으로 만들고 Ethernet frame으로 캡슐화한 뒤, Npcap을 이용해 두 LAN 카드 사이에서 직접 송수신합니다.

## 2. 계층 구조

```text
                         송신 ↓            수신 ↑
Cipc2019Dlg (UI)
 ├─ CChatAppLayer  : 채팅 단편화/재조립
 └─ CFileLayer     : 파일 분할/저장/진행률
          ↓
CEthernetLayer      : MAC header, EtherType, 필터링, 역다중화
          ↓
CNILayer            : 어댑터 선택, Npcap 송수신, 수신 thread
          ↓
LAN Card / LAN Cable
```

`CBaseLayer`는 계층 연결용 공통 인터페이스이고, `CLayerManager`는 위 객체들을 상·하위 포인터로 연결합니다.

## 3. 송신 흐름

### 채팅

1. `Cipc2019Dlg::SendData()`가 문자열을 UTF-8로 변환합니다.
2. `CChatAppLayer::Send()`가 최대 1496 bytes씩 단편화합니다.
3. 4-byte Chat header에 전체 길이와 FIRST/MIDDLE/LAST 종류를 기록합니다.
4. `CEthernetLayer::Send()`가 목적지 MAC, 출발지 MAC, `0x2080` EtherType을 붙입니다.
5. `CNILayer::Send()`가 `pcap_sendpacket()`으로 frame을 전송합니다.

### 파일

1. Dialog에서 파일을 선택하고 `CFileLayer::StartFileSend()`를 호출합니다.
2. 별도 worker thread가 `SendFile()`을 실행합니다.
3. START 패킷으로 64-bit 전체 파일 크기와 UTF-8 파일명을 보냅니다.
4. 파일을 1488 bytes씩 읽어 DATA 패킷으로 보냅니다.
5. 각 패킷에 sequence 번호를 기록하고 마지막에 END 패킷을 보냅니다.
6. 전송 byte 비율을 계산해 UI progress bar를 갱신합니다.

## 4. 수신 흐름

1. `CNILayer::CaptureLoop()`가 수신 thread에서 `pcap_next_ex()`로 frame을 받습니다.
2. BPF filter가 `0x2080`, `0x2090` frame만 통과시킵니다.
3. `CEthernetLayer::Receive()`가 다음을 검사합니다.
   - 과제에서 사용하는 EtherType인가?
   - 목적지 MAC이 내 MAC 또는 broadcast인가?
   - 내가 송신한 frame을 다시 잡은 것은 아닌가?
4. EtherType이 `0x2080`이면 ChatApp, `0x2090`이면 File로 전달합니다.
5. ChatApp은 LAST까지 조각을 합쳐 채팅창에 표시합니다.
6. File은 sequence 순서를 검사하며 `ReceivedFiles`에 기록하고, END에서 전체 크기를 검증합니다.

## 5. 파일 패킷 구조

| 필드 | 크기 | 역할 |
|---|---:|---|
| `fapp_totlen` | 4 bytes | 현재 패킷 data 길이 |
| `fapp_type` | 2 bytes | 파일 데이터 식별값 |
| `fapp_msg_type` | 1 byte | START / DATA / END |
| `fapp_unused` | 1 byte | 예약 필드 |
| `fapp_seq_num` | 4 bytes | 조각 순서 |
| `fapp_data` | 최대 1488 bytes | 파일 정보 또는 실제 데이터 |

총 application header는 12 bytes이므로 Ethernet payload 1500 bytes에서 실제 파일 데이터는 최대 1488 bytes입니다.

## 6. thread를 사용하는 이유

- `CNILayer`의 수신은 패킷이 올 때까지 반복 대기합니다.
- 큰 파일 송신도 많은 패킷을 순차 전송하므로 시간이 걸립니다.
- 이를 UI thread에서 처리하면 창이 멈춥니다.
- 따라서 수신과 파일 송신을 worker thread에서 수행하고, `PostMessage()`로 UI thread에 결과만 전달합니다.

## 7. 발표 때 보여줄 함수

1. `Cipc2019Dlg` 생성자: 전체 계층 구조
2. `CChatAppLayer::Send/Receive`: 채팅 단편화와 재조립
3. `CFileLayer::SendFile`: START → DATA → END
4. `CFileLayer::ReceiveStart/Data/End`: 파일 수신 상태 변화
5. `CEthernetLayer::Send/Receive`: 캡슐화, MAC 검사, demultiplexing
6. `CNILayer::SetAdapter/CaptureLoop/Send`: Npcap 실제 송수신

## 8. 1분 구현 설명 대본

> 프로그램은 Dialog, ChatApp/File, Ethernet, NI 계층으로 구성했습니다. 채팅은 4-byte header를 사용해 최대 1496 bytes씩, 파일은 12-byte header를 사용해 최대 1488 bytes씩 단편화합니다. Ethernet 계층은 MAC 주소와 채팅 0x2080 또는 파일 0x2090 EtherType을 붙이고, NI 계층이 Npcap으로 실제 랜카드에 frame을 전송합니다. 수신 시에는 목적지 MAC과 EtherType을 검사한 뒤 해당 응용 계층으로 전달합니다. 파일은 sequence 번호로 순서를 확인하고 END에서 전체 크기를 검증하여 ReceivedFiles 폴더에 저장합니다. 수신과 파일 송신은 별도 thread에서 처리해 UI가 멈추지 않도록 했습니다.

## 9. 예상 질문

### 왜 파일 데이터가 1488 bytes인가요?

Ethernet payload 최대 1500 bytes에서 File header 12 bytes를 제외했기 때문입니다.

### IP 주소 없이 통신하는 이유는 무엇인가요?

이 과제는 IP 계층을 사용하지 않고 Data Link 계층의 Ethernet frame과 MAC 주소로 직접 통신하기 때문입니다.

### sequence 번호가 필요한 이유는 무엇인가요?

단편화된 파일 조각이 기대한 순서대로 도착했는지 검사하고 정확하게 재조립하기 위해 사용합니다.

### 송신 완료가 실제 수신 완료를 뜻하나요?

아닙니다. 송신 완료는 모든 `pcap_sendpacket()` 호출이 성공했다는 뜻이고, 실제 성공은 수신 PC의 완료 메시지와 생성된 파일 크기/내용으로 확인해야 합니다.

### 진행률은 어떻게 계산하나요?

`전송 또는 수신한 누적 byte / 전체 파일 byte * 100`으로 계산합니다.

## 10. 데모 체크

- 두 PC 모두 실제 유선 Ethernet 어댑터 선택
- 서로 반대가 되도록 Source/Destination MAC 입력
- 양방향 채팅 확인
- 큰 파일 전송 중 두 progress bar 확인
- 수신 PC의 `ReceivedFiles`에서 파일 확인
- Wireshark에서 `0x2080`, `0x2090` 패킷 확인
