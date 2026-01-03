#include "pch.h"
#include "ClientSocket.h"

CClientSocket* CClientSocket::m_instance = NULL;
CClientSocket::CHelper CClientSocket::m_helper;
CClientSocket* pclient = CClientSocket::getInstance();

std::string GetErrInfo(int wsaErrCode)
{
    std::string ret;                       // 存储最终返回的错误描述字符串
    LPVOID lpMsgBuf = NULL;
    FormatMessage(    // 标志位：FROM_SYSTEM表示从系统消息表获取；ALLOCATE_BUFFER表示自动分配缓冲区
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
        NULL,
        wsaErrCode,              // 要查询的错误码（WSA错误码）
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf, 0, NULL);
    ret = (char*)lpMsgBuf;
    LocalFree(lpMsgBuf);
    return ret;
}

void Dump(BYTE* pData, size_t nSize)
{
    std::string strOut;
    for (size_t i = 0; i < nSize; i++)
    {
        char buf[8] = "";
        if (i > 0 && (i % 16 == 0)) strOut += "\n";
        snprintf(buf, sizeof(buf), "%02X ", pData[i] & 0xFF);
        strOut += buf;
    }
    strOut += "\n";
    OutputDebugStringA(strOut.c_str());
}

void CClientSocket::threadEntry(void* arg) {
    CClientSocket* thiz = (CClientSocket*)arg;
    thiz->threadFunc();

}

void CClientSocket::threadFunc() {

    if (InitSocket() == false) { return; }
    std::string strBuffer;
    strBuffer.resize(BUFFER_SIZE);

    char* pBuffer = (char*)strBuffer.c_str();
    int index = 0;
    while (m_sock != INVALID_SOCKET) {
        if (m_lstSend.size() > 0) {
            CPacket& head= m_lstSend.front();
            if (Send(head) == false) {
                TRACE("发送失败\r\n");
                continue;
            } 
            auto pr = m_mapAck.insert(std::pair <HANDLE, std::list<CPacket>>(head.hEvent, std::list<CPacket>()));

            int length = recv(m_sock, pBuffer + index, BUFFER_SIZE - index, 0);
            if (length > 0 || index > 0){
                index += length;
                size_t size = (size_t)index;
                CPacket pack((BYTE*)pBuffer, size);
              
                if (size > 0) {
                    pack.hEvent = head.hEvent;
                    pr.first->second.push_back(pack);
                    SetEvent(head.hEvent);


                }
            }
            else if (length <= 0 && index <= 0) {
                CloseSocket();
            }
            m_lstSend.pop_front();
        }

        
    
    }



}