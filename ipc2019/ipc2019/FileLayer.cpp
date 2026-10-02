#include "stdafx.h"
#include "pch.h"
#include "FileLayer.h"
#include "EthernetLayer.h"
#include <atlconv.h>

CFileLayer::CFileLayer(char* pName)
    : CBaseLayer(pName),
      m_pSendThread(nullptr),
      m_bSending(FALSE),
      m_LastSendProgress(-1),
      m_bReceiving(FALSE),
      m_ExpectedSequence(0),
      m_ExpectedFileSize(0),
      m_ReceivedFileSize(0),
      m_LastReceiveProgress(-1)
{
}

CFileLayer::~CFileLayer()
{
    if (m_pSendThread != nullptr)
    {
        WaitForSingleObject(m_pSendThread->m_hThread, INFINITE);
        delete m_pSendThread;
        m_pSendThread = nullptr;
    }

    if (m_bReceiving)
    {
        m_ReceiveFile.Close();
        m_bReceiving = FALSE;
    }
}

unsigned short CFileLayer::Swap16(unsigned short value)
{
    return static_cast<unsigned short>(
        (value << 8) | (value >> 8));
}

unsigned int CFileLayer::Swap32(unsigned int value)
{
    return ((value & 0x000000ffU) << 24) |
           ((value & 0x0000ff00U) << 8) |
           ((value & 0x00ff0000U) >> 8) |
           ((value & 0xff000000U) >> 24);
}

ULONGLONG CFileLayer::Swap64(ULONGLONG value)
{
    return (static_cast<ULONGLONG>(Swap32(
                static_cast<unsigned int>(value))) << 32) |
           Swap32(static_cast<unsigned int>(value >> 32));
}

// CBaseLayer 인터페이스 호환용 함수.
// 파일 전송은 경로가 필요한 StartFileSend()를 통해 시작한다.
BOOL CFileLayer::Send(unsigned char* ppayload, int nlength)
{
    UNREFERENCED_PARAMETER(ppayload);
    UNREFERENCED_PARAMETER(nlength);
    return FALSE;
}

BOOL CFileLayer::IsSending() const
{
    return m_bSending;
}

// UI에서 선택한 파일 경로를 저장하고 비동기 송신 스레드를 시작한다.
BOOL CFileLayer::StartFileSend(const CString& filePath)
{
    if (m_bSending || filePath.IsEmpty())
        return FALSE;

    if (m_pSendThread != nullptr)
    {
        WaitForSingleObject(m_pSendThread->m_hThread, INFINITE);
        delete m_pSendThread;
        m_pSendThread = nullptr;
    }

    m_SendFilePath = filePath;
    m_bSending = TRUE;
    m_pSendThread = AfxBeginThread(FileTransferThread, this);

    if (m_pSendThread == nullptr)
    {
        m_bSending = FALSE;
        return FALSE;
    }

    m_pSendThread->m_bAutoDelete = FALSE;
    return TRUE;
}

UINT CFileLayer::FileTransferThread(LPVOID pParam)
{
    CFileLayer* layer = static_cast<CFileLayer*>(pParam);
    return layer != nullptr ? layer->SendFile() : 0;
}

// FILE_PACKET 헤더를 작성한 뒤 EtherType 0x2090으로 Ethernet 계층에 전달한다.
BOOL CFileLayer::SendPacket(
    unsigned char messageType,
    unsigned int sequence,
    const unsigned char* data,
    unsigned int dataLength)
{
    if (dataLength > FILE_DATA_SIZE || mp_UnderLayer == nullptr)
        return FALSE;

    FILE_PACKET packet = {};
    packet.fapp_totlen = Swap32(dataLength);
    packet.fapp_type = Swap16(FILE_DATA_KIND);
    packet.fapp_msg_type = messageType;
    packet.fapp_unused = 0;
    packet.fapp_seq_num = Swap32(sequence);

    if (data != nullptr && dataLength > 0)
        memcpy(packet.fapp_data, data, dataLength);

    CEthernetLayer* ethernet =
        static_cast<CEthernetLayer*>(mp_UnderLayer);

    return ethernet->Send(
        reinterpret_cast<unsigned char*>(&packet),
        FILE_PACKET_HEADER_SIZE + dataLength,
        ETHER_TYPE_FILE_NETWORK);
}

// 송신 전체 흐름:
// 1) 파일 크기/이름을 START로 전송
// 2) 파일을 1488-byte씩 읽어 DATA로 전송
// 3) END를 전송하고 UI에 완료/실패를 알림
UINT CFileLayer::SendFile()
{
    BOOL success = TRUE;
    CFile file;

    TRY
    {
        if (!file.Open(
                m_SendFilePath,
                CFile::modeRead | CFile::shareDenyWrite))
        {
            success = FALSE;
        }

        if (success)
        {
            ULONGLONG fileSize = file.GetLength();
            ULONGLONG networkFileSize = Swap64(fileSize);
            ULONGLONG sentFileSize = 0;
            m_LastSendProgress = -1;
            NotifyProgress(TRUE, 0, fileSize);

            // 경로 전체가 아니라 마지막 파일명만 상대 PC에 전달한다.
            CString fileName = m_SendFilePath;
            int slash = max(fileName.ReverseFind(_T('\\')),
                            fileName.ReverseFind(_T('/')));
            if (slash >= 0)
                fileName = fileName.Mid(slash + 1);

            CStringA utf8FileName = CW2A(fileName, CP_UTF8);
            unsigned int nameLength =
                static_cast<unsigned int>(utf8FileName.GetLength() + 1);

            // START payload = network byte order의 64-bit 파일 크기 + NULL 종료 UTF-8 파일명.
            unsigned char startData[FILE_DATA_SIZE] = {};
            unsigned int startLength =
                static_cast<unsigned int>(sizeof(networkFileSize)) + nameLength;

            if (startLength > FILE_DATA_SIZE)
            {
                success = FALSE;
            }
            else
            {
                memcpy(
                    startData,
                    &networkFileSize,
                    sizeof(networkFileSize));
                memcpy(
                    startData + sizeof(networkFileSize),
                    utf8FileName.GetString(),
                    nameLength);

                success = SendPacket(
                    FILE_MSG_START,
                    0,
                    startData,
                    startLength);
            }

            unsigned int sequence = 1;
            unsigned char fileData[FILE_DATA_SIZE] = {};

            // 파일 내용을 FILE_DATA_SIZE(1488 bytes) 단위로 읽는다.
            while (success)
            {
                UINT bytesRead = file.Read(fileData, FILE_DATA_SIZE);
                if (bytesRead == 0)
                    break;

                success = SendPacket(
                    FILE_MSG_DATA,
                    sequence++,
                    fileData,
                    bytesRead);

                if (success)
                {
                    sentFileSize += bytesRead;
                    NotifyProgress(TRUE, sentFileSize, fileSize);
                }

                Sleep(1);
            }

            if (success)
            {
                success = SendPacket(
                    FILE_MSG_END,
                    sequence,
                    nullptr,
                    0);

                if (success)
                    NotifyProgress(TRUE, fileSize, fileSize);
            }

            file.Close();
        }
    }
    CATCH(CFileException, e)
    {
        UNREFERENCED_PARAMETER(e);
        success = FALSE;
    }
    END_CATCH

    NotifyDialog(
        success
        ? _T(">> File transfer completed.")
        : _T(">> File transfer failed."));

    m_bSending = FALSE;
    return success ? 1 : 0;
}

// fapp_msg_type에 따라 START/DATA/END 전용 처리 함수로 분기한다.
BOOL CFileLayer::Receive(unsigned char* ppayload)
{
    if (ppayload == nullptr)
        return FALSE;

    PFILE_PACKET packet =
        reinterpret_cast<PFILE_PACKET>(ppayload);
    unsigned int dataLength = Swap32(packet->fapp_totlen);

    if (dataLength > FILE_DATA_SIZE ||
        Swap16(packet->fapp_type) != FILE_DATA_KIND)
    {
        return FALSE;
    }

    switch (packet->fapp_msg_type)
    {
    case FILE_MSG_START:
        return ReceiveStart(packet);
    case FILE_MSG_DATA:
        return ReceiveData(packet);
    case FILE_MSG_END:
        return ReceiveEnd(packet);
    default:
        return FALSE;
    }
}

// 새 수신 작업을 초기화하고 ReceivedFiles/<원본 파일명>을 생성한다.
BOOL CFileLayer::ReceiveStart(PFILE_PACKET packet)
{
    unsigned int dataLength = Swap32(packet->fapp_totlen);
    unsigned int sequence = Swap32(packet->fapp_seq_num);

    if (sequence != 0 ||
        dataLength <= sizeof(ULONGLONG))
    {
        return FALSE;
    }

    if (m_bReceiving)
    {
        m_ReceiveFile.Close();
        m_bReceiving = FALSE;
    }

    ULONGLONG networkFileSize = 0;
    memcpy(
        &networkFileSize,
        packet->fapp_data,
        sizeof(networkFileSize));
    m_ExpectedFileSize = Swap64(networkFileSize);

    const char* rawName = reinterpret_cast<const char*>(
        packet->fapp_data + sizeof(networkFileSize));
    size_t maximumNameLength =
        dataLength - sizeof(networkFileSize);

    if (memchr(rawName, '\0', maximumNameLength) == nullptr)
        return FALSE;

    CString fileName = CA2W(rawName, CP_UTF8);
    int slash = max(fileName.ReverseFind(_T('\\')),
                    fileName.ReverseFind(_T('/')));
    if (slash >= 0)
        fileName = fileName.Mid(slash + 1);

    // 전송된 파일명에서 경로와 Windows 금지 문자를 제거하여 경로 조작을 방지한다.
    const TCHAR invalidCharacters[] = _T(":*?\"<>|");
    for (int i = 0; invalidCharacters[i] != _T('\0'); ++i)
        fileName.Replace(invalidCharacters[i], _T('_'));

    if (fileName.IsEmpty())
        fileName = _T("received_file.bin");

    TCHAR currentDirectory[MAX_PATH] = {};
    GetCurrentDirectory(MAX_PATH, currentDirectory);

    CString receiveDirectory(currentDirectory);
    receiveDirectory += _T("\\ReceivedFiles");
    CreateDirectory(receiveDirectory, nullptr);

    m_ReceiveFilePath =
        receiveDirectory + _T("\\") + fileName;

    if (!m_ReceiveFile.Open(
            m_ReceiveFilePath,
            CFile::modeCreate |
            CFile::modeWrite |
            CFile::shareDenyWrite))
    {
        return FALSE;
    }

    m_bReceiving = TRUE;
    m_ExpectedSequence = 1;
    m_ReceivedFileSize = 0;
    m_LastReceiveProgress = -1;
    NotifyProgress(FALSE, 0, m_ExpectedFileSize);
    NotifyDialog(_T(">> Receiving file: ") + fileName);
    return TRUE;
}

// sequence가 기대값과 같은 DATA만 파일 뒤에 기록한다.
// 순서가 다르거나 전체 파일 크기를 넘으면 해당 패킷을 거부한다.
BOOL CFileLayer::ReceiveData(PFILE_PACKET packet)
{
    unsigned int dataLength = Swap32(packet->fapp_totlen);
    unsigned int sequence = Swap32(packet->fapp_seq_num);

    if (!m_bReceiving ||
        sequence != m_ExpectedSequence ||
        m_ReceivedFileSize + dataLength > m_ExpectedFileSize)
    {
        return FALSE;
    }

    TRY
    {
        m_ReceiveFile.Write(
            packet->fapp_data,
            dataLength);
    }
    CATCH(CFileException, e)
    {
        UNREFERENCED_PARAMETER(e);
        m_ReceiveFile.Close();
        m_bReceiving = FALSE;
        return FALSE;
    }
    END_CATCH

    m_ReceivedFileSize += dataLength;
    ++m_ExpectedSequence;
    NotifyProgress(FALSE, m_ReceivedFileSize, m_ExpectedFileSize);
    return TRUE;
}

// 마지막 sequence와 최종 파일 크기를 검사한 뒤 파일을 닫는다.
BOOL CFileLayer::ReceiveEnd(PFILE_PACKET packet)
{
    unsigned int sequence = Swap32(packet->fapp_seq_num);

    if (!m_bReceiving ||
        sequence != m_ExpectedSequence)
    {
        return FALSE;
    }

    m_ReceiveFile.Close();
    m_bReceiving = FALSE;

    CString status;
    if (m_ReceivedFileSize == m_ExpectedFileSize)
    {
        NotifyProgress(FALSE, m_ExpectedFileSize, m_ExpectedFileSize);
        status = _T(">> File received: ") + m_ReceiveFilePath;
    }
    else
        status = _T(">> File size mismatch: ") + m_ReceiveFilePath;

    NotifyDialog(status);
    return m_ReceivedFileSize == m_ExpectedFileSize;
}

// worker thread가 직접 UI 컨트롤을 건드리지 않고 Dialog 계층에 메시지를 전달한다.
void CFileLayer::NotifyDialog(const CString& message)
{
    if (m_nUpperLayerCount > 0 &&
        mp_aUpperLayer[0] != nullptr)
    {
        mp_aUpperLayer[0]->Receive(
            reinterpret_cast<unsigned char*>(
                const_cast<LPTSTR>(message.GetString())));
    }
}

// 전송/수신 byte 비율을 0~100으로 계산한다.
// 같은 퍼센트는 중복 통지하지 않아 UI 메시지 수를 줄인다.
void CFileLayer::NotifyProgress(
    BOOL sending,
    ULONGLONG completed,
    ULONGLONG total)
{
    int progress = total == 0
        ? 100
        : static_cast<int>((completed * 100) / total);

    progress = max(0, min(100, progress));
    int& lastProgress = sending
        ? m_LastSendProgress
        : m_LastReceiveProgress;

    if (progress == lastProgress)
        return;

    lastProgress = progress;

    CString message;
    message.Format(
        sending
            ? _T("__FILE_PROGRESS_SEND__:%d")
            : _T("__FILE_PROGRESS_RECEIVE__:%d"),
        progress);
    NotifyDialog(message);
}
