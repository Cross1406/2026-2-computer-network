#pragma once

#include "BaseLayer.h"
#include "pch.h"
#include <atomic>

// 파일 전송 응용 계층.
//
// 파일을 START/DATA/END 패킷으로 나누고 DATA 패킷에는 최대 1488 bytes를 담는다.
// 송신은 worker thread에서 수행하며 수신 파일은 실행 폴더의 ReceivedFiles에 저장한다.
class CFileLayer : public CBaseLayer
{
public:
    CFileLayer(char* pName);
    virtual ~CFileLayer();

    BOOL Send(unsigned char* ppayload, int nlength) override;
    BOOL Receive(unsigned char* ppayload) override;
    BOOL Receive(unsigned char* ppayload, int length) override;

    BOOL StartFileSend(const CString& filePath);
    BOOL IsSending() const;

private:
    enum
    {
        // START: 전체 파일 크기(8 bytes) + UTF-8 파일명
        // DATA : 실제 파일 데이터
        // END  : 모든 DATA 패킷 전송 종료
        FILE_MSG_START = 0x00,
        FILE_MSG_DATA = 0x01,
        FILE_MSG_END = 0x02,
        FILE_DATA_KIND = 0x0001,
        FILE_PACKET_HEADER_SIZE = 12,
        FILE_DATA_SIZE = ETHER_MAX_DATA_SIZE - FILE_PACKET_HEADER_SIZE
    };

#pragma pack(push, 1)
    // 12-byte application header:
    // [payload length:4][kind:2][message type:1][unused:1][sequence:4]
    typedef struct _FILE_PACKET
    {
        unsigned int fapp_totlen;
        unsigned short fapp_type;
        unsigned char fapp_msg_type;
        unsigned char fapp_unused;
        unsigned int fapp_seq_num;
        unsigned char fapp_data[FILE_DATA_SIZE];
    } FILE_PACKET, *PFILE_PACKET;
#pragma pack(pop)

    static UINT FileTransferThread(LPVOID pParam);
    UINT SendFile();
    BOOL SendPacket(
        unsigned char messageType,
        unsigned int sequence,
        const unsigned char* data,
        unsigned int dataLength);
    BOOL ReceiveStart(PFILE_PACKET packet);
    BOOL ReceiveData(PFILE_PACKET packet);
    BOOL ReceiveEnd(PFILE_PACKET packet);
    void NotifyDialog(const CString& message);
    void NotifyProgress(BOOL sending, ULONGLONG completed, ULONGLONG total);

    static unsigned short Swap16(unsigned short value);
    static unsigned int Swap32(unsigned int value);
    static ULONGLONG Swap64(ULONGLONG value);

private:
    // 송신 상태: UI와 worker thread가 공유한다.
    CString m_SendFilePath;
    CWinThread* m_pSendThread;
    std::atomic<bool> m_bSending;
    int m_LastSendProgress;

    // 수신 상태: START에서 초기화되고 END에서 닫힌다.
    CFile m_ReceiveFile;
    BOOL m_bReceiving;
    unsigned int m_ExpectedSequence;
    ULONGLONG m_ExpectedFileSize;
    ULONGLONG m_ReceivedFileSize;
    CString m_ReceiveFilePath;
    int m_LastReceiveProgress;
};
