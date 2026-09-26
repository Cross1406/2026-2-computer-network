#pragma once

#include "BaseLayer.h"
#include "pch.h"

class CFileLayer : public CBaseLayer
{
public:
    CFileLayer(char* pName);
    virtual ~CFileLayer();

    BOOL Send(unsigned char* ppayload, int nlength) override;
    BOOL Receive(unsigned char* ppayload) override;

    BOOL StartFileSend(const CString& filePath);
    BOOL IsSending() const;

private:
    enum
    {
        FILE_TYPE_START = 0x01,
        FILE_TYPE_DATA = 0x02,
        FILE_TYPE_END = 0x03,
        FILE_PACKET_HEADER_SIZE = 9,
        FILE_DATA_SIZE = ETHER_MAX_DATA_SIZE - FILE_PACKET_HEADER_SIZE
    };

#pragma pack(push, 1)
    typedef struct _FILE_PACKET
    {
        unsigned char type;
        unsigned int sequence;
        unsigned int dataLength;
        unsigned char data[FILE_DATA_SIZE];
    } FILE_PACKET, *PFILE_PACKET;
#pragma pack(pop)

    static UINT FileTransferThread(LPVOID pParam);
    UINT SendFile();
    BOOL SendPacket(
        unsigned char type,
        unsigned int sequence,
        const unsigned char* data,
        unsigned int dataLength);
    BOOL ReceiveStart(PFILE_PACKET packet);
    BOOL ReceiveData(PFILE_PACKET packet);
    BOOL ReceiveEnd(PFILE_PACKET packet);
    void NotifyDialog(const CString& message);

private:
    CString m_SendFilePath;
    CWinThread* m_pSendThread;
    volatile BOOL m_bSending;

    CFile m_ReceiveFile;
    BOOL m_bReceiving;
    unsigned int m_ExpectedSequence;
    ULONGLONG m_ExpectedFileSize;
    ULONGLONG m_ReceivedFileSize;
    CString m_ReceiveFilePath;
};
