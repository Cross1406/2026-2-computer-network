#include "stdafx.h"
#include "pch.h"
#include "FileLayer.h"
#include "EthernetLayer.h"
#include <atlconv.h>

CFileLayer::CFileLayer(char* pName)
    : CBaseLayer(pName),
      m_pSendThread(nullptr),
      m_bSending(FALSE),
      m_bReceiving(FALSE),
      m_ExpectedSequence(0),
      m_ExpectedFileSize(0),
      m_ReceivedFileSize(0)
{
}

CFileLayer::~CFileLayer()
{
    if (m_pSendThread != nullptr)
    {
        WaitForSingleObject(m_pSendThread->m_hThread, 3000);
        delete m_pSendThread;
        m_pSendThread = nullptr;
    }

    if (m_bReceiving)
    {
        m_ReceiveFile.Close();
        m_bReceiving = FALSE;
    }
}

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

BOOL CFileLayer::SendPacket(
    unsigned char type,
    unsigned int sequence,
    const unsigned char* data,
    unsigned int dataLength)
{
    if (dataLength > FILE_DATA_SIZE || mp_UnderLayer == nullptr)
        return FALSE;

    FILE_PACKET packet = {};
    packet.type = type;
    packet.sequence = sequence;
    packet.dataLength = dataLength;

    if (data != nullptr && dataLength > 0)
        memcpy(packet.data, data, dataLength);

    CEthernetLayer* ethernet =
        static_cast<CEthernetLayer*>(mp_UnderLayer);

    return ethernet->Send(
        reinterpret_cast<unsigned char*>(&packet),
        FILE_PACKET_HEADER_SIZE + dataLength,
        ETHER_TYPE_FILE_NETWORK);
}

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
            CString fileName = m_SendFilePath;
            int slash = max(fileName.ReverseFind(_T('\\')),
                            fileName.ReverseFind(_T('/')));
            if (slash >= 0)
                fileName = fileName.Mid(slash + 1);

            CStringA utf8FileName = CW2A(fileName, CP_UTF8);
            unsigned int nameLength =
                static_cast<unsigned int>(utf8FileName.GetLength() + 1);

            unsigned char startData[FILE_DATA_SIZE] = {};
            unsigned int startLength =
                static_cast<unsigned int>(sizeof(fileSize)) + nameLength;

            if (startLength > FILE_DATA_SIZE)
            {
                success = FALSE;
            }
            else
            {
                memcpy(startData, &fileSize, sizeof(fileSize));
                memcpy(
                    startData + sizeof(fileSize),
                    utf8FileName.GetString(),
                    nameLength);

                success = SendPacket(
                    FILE_TYPE_START,
                    0,
                    startData,
                    startLength);
            }

            unsigned int sequence = 1;
            unsigned char fileData[FILE_DATA_SIZE] = {};

            while (success)
            {
                UINT bytesRead = file.Read(fileData, FILE_DATA_SIZE);
                if (bytesRead == 0)
                    break;

                success = SendPacket(
                    FILE_TYPE_DATA,
                    sequence++,
                    fileData,
                    bytesRead);
            }

            if (success)
                success = SendPacket(
                    FILE_TYPE_END,
                    sequence,
                    nullptr,
                    0);

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

BOOL CFileLayer::Receive(unsigned char* ppayload)
{
    if (ppayload == nullptr)
        return FALSE;

    PFILE_PACKET packet =
        reinterpret_cast<PFILE_PACKET>(ppayload);

    if (packet->dataLength > FILE_DATA_SIZE)
        return FALSE;

    switch (packet->type)
    {
    case FILE_TYPE_START:
        return ReceiveStart(packet);
    case FILE_TYPE_DATA:
        return ReceiveData(packet);
    case FILE_TYPE_END:
        return ReceiveEnd(packet);
    default:
        return FALSE;
    }
}

BOOL CFileLayer::ReceiveStart(PFILE_PACKET packet)
{
    if (packet->sequence != 0 ||
        packet->dataLength <= sizeof(ULONGLONG))
    {
        return FALSE;
    }

    if (m_bReceiving)
    {
        m_ReceiveFile.Close();
        m_bReceiving = FALSE;
    }

    memcpy(
        &m_ExpectedFileSize,
        packet->data,
        sizeof(m_ExpectedFileSize));

    const char* rawName = reinterpret_cast<const char*>(
        packet->data + sizeof(m_ExpectedFileSize));
    size_t maximumNameLength =
        packet->dataLength - sizeof(m_ExpectedFileSize);

    if (memchr(rawName, '\0', maximumNameLength) == nullptr)
        return FALSE;

    CString fileName = CA2W(rawName, CP_UTF8);
    int slash = max(fileName.ReverseFind(_T('\\')),
                    fileName.ReverseFind(_T('/')));
    if (slash >= 0)
        fileName = fileName.Mid(slash + 1);

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
    NotifyDialog(_T(">> Receiving file: ") + fileName);
    return TRUE;
}

BOOL CFileLayer::ReceiveData(PFILE_PACKET packet)
{
    if (!m_bReceiving ||
        packet->sequence != m_ExpectedSequence)
    {
        return FALSE;
    }

    TRY
    {
        m_ReceiveFile.Write(
            packet->data,
            packet->dataLength);
    }
    CATCH(CFileException, e)
    {
        UNREFERENCED_PARAMETER(e);
        m_ReceiveFile.Close();
        m_bReceiving = FALSE;
        return FALSE;
    }
    END_CATCH

    m_ReceivedFileSize += packet->dataLength;
    ++m_ExpectedSequence;
    return TRUE;
}

BOOL CFileLayer::ReceiveEnd(PFILE_PACKET packet)
{
    if (!m_bReceiving ||
        packet->sequence != m_ExpectedSequence)
    {
        return FALSE;
    }

    m_ReceiveFile.Close();
    m_bReceiving = FALSE;

    CString status;
    if (m_ReceivedFileSize == m_ExpectedFileSize)
        status = _T(">> File received: ") + m_ReceiveFilePath;
    else
        status = _T(">> File size mismatch: ") + m_ReceiveFilePath;

    NotifyDialog(status);
    return m_ReceivedFileSize == m_ExpectedFileSize;
}

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
