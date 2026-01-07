#include "pch.h"
#include "ClientSocket.h"

CClientSocket* CClientSocket::m_instance = NULL;
CClientSocket::CHelper CClientSocket::m_helper;
CClientSocket* pclient = CClientSocket::getInstance();

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

CClientSocket::CClientSocket():m_nIP(INADDR_ANY), m_nPort(0), m_sock(INVALID_SOCKET), m_bAutoClose(true), m_hThread(INVALID_HANDLE_VALUE) {
    if (InitSockEnv() == FALSE) {
        MessageBox(NULL, _T("初始化Socket环境失败"), _T("初始化错误"), MB_OK | MB_ICONERROR);
        exit(0);
    }
    m_eventInvoke=CreateEvent(NULL, TRUE, FALSE, NULL);
    m_hThread = (HANDLE)_beginthreadex(NULL, 0, &CClientSocket::threadEntry, this, 0, &m_nThreadID);
    if (WaitForSingleObject(m_eventInvoke, 100) == WAIT_TIMEOUT) {
        TRACE("网络消息处理线程启动失败!!\r\n");
    
    }
    CloseHandle(m_eventInvoke);
    m_buffer.resize(BUFFER_SIZE);
    memset(m_buffer.data(), 0, BUFFER_SIZE);

    struct {
        UINT message;
        MSGFUNC func;
    }funcs[] = {
        {WM_SEND_PACK,&CClientSocket::SendPack},
        {0,NULL}

    };

    for (int i = 0; funcs[i].message != 0; i++) {
        if (m_mapFunc.insert(std::pair<UINT, MSGFUNC>(funcs[i].message, funcs[i].func)).second == false)
            TRACE("插入失败，消息值：%d 函数值：%08X 序号：%d\r\n", funcs[i].message, funcs[i].func, i);

    }
};

CClientSocket::CClientSocket(const CClientSocket& ss) {
    m_sock = ss.m_sock;
    m_nIP = ss.m_nIP;
    m_nPort = ss.m_nPort;
    m_bAutoClose = ss.m_bAutoClose;
    m_hThread = INVALID_HANDLE_VALUE;
    std::map<UINT,CClientSocket::MSGFUNC>::const_iterator it= ss.m_mapFunc.begin();

    for (; it != ss.m_mapFunc.end(); it++) {
        m_mapFunc.insert(std::pair<UINT, MSGFUNC>(it->first, it->second));
    
    }

};

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

unsigned CClientSocket::threadEntry(void* arg) {
    CClientSocket* thiz = (CClientSocket*)arg;
    thiz->threadFunc2();
    _endthreadex(0);
    return 0;
}

void CClientSocket::threadFunc2()
{
    SetEvent(m_eventInvoke);
    MSG msg;
    while (::GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        TRACE("Get Message:%08X\r\n", msg.message);
        if (m_mapFunc.find(msg.message) != m_mapFunc.end()) {
            (this->*m_mapFunc[msg.message])(msg.message,msg.wParam, msg.lParam);
        
        }
    }

}

bool CClientSocket::Send(const CPacket& pack) {
    TRACE("m_sock = %d\r\n", m_sock);
    if (m_sock == -1) return false;
    std::string strOut;
    pack.Data(strOut);
    return send(m_sock, strOut.c_str(), strOut.size(), 0) > 0;
}

//定义一个消息数据结构（数据和数据长度，模式）回调函数的数据结构（HWND）
void CClientSocket::SendPack(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
    PACKET_DATA data=*(PACKET_DATA*)wParam;
    delete (PACKET_DATA*)wParam;
    HWND hWnd = (HWND)lParam;

    size_t nTemp = data.strData.size();
    CPacket current((BYTE*)data.strData.c_str(), nTemp);

    if (InitSocket() == true) {
        int ret = send(m_sock, (char*)data.strData.c_str(), (int)data.strData.size(), 0);
        if (ret > 0) {
            size_t index = 0;
            std::string strBuffer;
            strBuffer.resize(BUFFER_SIZE);
            char* pBuffer = (char*)strBuffer.c_str();
            while (m_sock != INVALID_SOCKET) {
                int length = recv(m_sock, pBuffer + index, BUFFER_SIZE - index, 0);
                if (length > 0||(index > 0)) {
                    index += (size_t)length;
                    size_t nLen = index;
                    CPacket pack((BYTE*)pBuffer, nLen);
                    if (nLen > 0) {
                        ::SendMessage(hWnd, WM_SEND_PACK_ACK, (WPARAM)new CPacket(pack), data.wParam);
                        if (data.nMode & CSM_AUTOCLOSE) {
                            CloseSocket();
                            return;
                        }
                        index -= nLen;
                        memmove(pBuffer, pBuffer + nLen, index);
                    }
                }
                else {
                    TRACE("接收失败,length=%d,index=%d，cmd=%d\r\n",length,index,current.sCmd);
                    CloseSocket();
              

                    ::SendMessage(hWnd, WM_SEND_PACK_ACK, (WPARAM)new CPacket(current.sCmd,NULL,0), 1);

                }
            }
        }
        else { 
            CloseSocket(); 
        ::SendMessage(hWnd, WM_SEND_PACK_ACK, NULL, -1);

        }
    
    }
    else {
        ::SendMessage(hWnd, WM_SEND_PACK_ACK, NULL, -2);

    }

}


bool CClientSocket::InitSocket() {
    if (m_sock != INVALID_SOCKET) { CloseSocket(); }
    m_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (m_sock == -1) { return false; }

    sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(m_nIP);
    serv_addr.sin_port = htons(m_nPort);
    if (serv_addr.sin_addr.s_addr == INADDR_NONE) {
        AfxMessageBox("IP地址不存在！");
        return false;
    }

    int ret = connect(m_sock, (sockaddr*)&serv_addr, sizeof(serv_addr));

    if (ret == -1) {
        AfxMessageBox("连接失败");
        TRACE("连接失败：%d %s\r\n", WSAGetLastError(), GetErrInfo(WSAGetLastError()).c_str());
        return false;
    }
    TRACE("sock init done!\r\n");
    return true;
}

bool CClientSocket::SendPacket(HWND hWnd, const CPacket& pack, bool isAutoClosed,WPARAM wParam) {

    
    UINT nMode = isAutoClosed ? CSM_AUTOCLOSE : 0;
    std::string strOut;
    pack.Data(strOut);
    PACKET_DATA* pData = new PACKET_DATA(strOut.c_str(), strOut.size(), nMode, wParam);
    bool ret= PostThreadMessage(m_nThreadID, WM_SEND_PACK, (WPARAM)pData, (LPARAM)hWnd);
   
    if (ret == false) {
        delete pData;
    }
    return ret;
}


