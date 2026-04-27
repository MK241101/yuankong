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

CClientSocket::CClientSocket(const CClientSocket& ss) {

    m_sock = ss.m_sock;
    m_nIP = ss.m_nIP;
    m_nPort = ss.m_nPort;
    m_bAutoClose = ss.m_bAutoClose;
    m_hThread = INVALID_HANDLE_VALUE;  //这里采用硬编码，因为线程句柄不能复制
    
    std::map<UINT, CClientSocket::MSGFUNC>::const_iterator it = ss.m_mapFunc.begin();  // 拷贝消息处理映射表
    for (; it != ss.m_mapFunc.end(); it++) {
        m_mapFunc.insert(std::pair<UINT, MSGFUNC>(it->first, it->second));
    }
};

CClientSocket::CClientSocket():m_nIP(INADDR_ANY), m_nPort(0), m_sock(INVALID_SOCKET), m_bAutoClose(true), m_hThread(INVALID_HANDLE_VALUE) {
    if (InitSockEnv() == FALSE) {
        MessageBox(NULL, _T("初始化Socket环境失败"), _T("初始化错误"), MB_OK | MB_ICONERROR);
        exit(0);
    }
    //事件的核心作用：让主线程「阻塞等待」网络线程初始化完成
    m_eventInvoke=CreateEvent(NULL, TRUE, FALSE, NULL);   // 创建手动重置的无信号事件：用于网络线程启动同步
    m_hThread = (HANDLE)_beginthreadex(NULL, 0, &CClientSocket::threadEntry, this, 0, &m_nThreadID);
    
    if (WaitForSingleObject(m_eventInvoke, 100) == WAIT_TIMEOUT) {
        TRACE("网络消息处理线程启动失败!!\r\n");
    
    }
    CloseHandle(m_eventInvoke);
    //TODO 为什么这里要关闭呢
    //当线程初始化同步完成后，事件对象已经完成了它的使命，不再需要使用，
    //因此主动关闭内核句柄，释放系统资源，避免句柄泄漏。

    m_buffer.resize(BUFFER_SIZE);
    memset(m_buffer.data(), 0, BUFFER_SIZE);

    struct {
        UINT message;
        MSGFUNC func;
    }funcs[] = {
        {WM_SEND_PACK,&CClientSocket::SendPack},    // WM_SEND_PACK → 调用SendPack处理
        {0,NULL}

    };

    // 将消息映射关系插入map
    for (int i = 0; funcs[i].message != 0; i++) {
        if (m_mapFunc.insert(std::pair<UINT, MSGFUNC>(funcs[i].message, funcs[i].func)).second == false)
            TRACE("插入失败，消息值：%d 函数值：%08X 序号：%d\r\n", funcs[i].message, funcs[i].func, i);
    }
};

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
    TRACE("sock init done!网络连接成功！\r\n");
    return true;
}

/*
    SendPacket()接收上层传递的「窗口句柄、待发送 CPacket、是否自动关闭、扩展参数」，将 CPacket 转为网络字节流，
    封装成PACKET_DATA对象，通过PostThreadMessage向网络线程发送WM_SEND_PACK消息，
    把PACKET_DATA指针和窗口句柄通过WPARAM/LPARAM传递。
    仅完成 “参数封装 + 跨线程消息触发”，执行后立即返回，绝不阻塞上层线程。
*/
bool CClientSocket::SendPacket(HWND hWnd, const CPacket& pack, bool isAutoClosed, WPARAM wParam) {

    UINT nMode = isAutoClosed ? CSM_AUTOCLOSE : 0;
    std::string strOut;
    pack.Data(strOut);   // 将CPacket对象转换为连续的字节流

    PACKET_DATA* pData = new PACKET_DATA(strOut.c_str(), strOut.size(), nMode, wParam);  // 创建PacketData对象
    bool ret = PostThreadMessage(m_nThreadID, WM_SEND_PACK, (WPARAM)pData, (LPARAM)hWnd);  // 给本机网络线程发送消息

    
    if (ret == false) {
        delete pData;
    }
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
    SetEvent(m_eventInvoke);    // 设置事件为有信号，通知主线程：网络线程初始化完成
    MSG msg;
    while (::GetMessage(&msg, NULL, 0, 0)) {  // 【死循环】一直等待、获取线程消息
        TranslateMessage(&msg);
        DispatchMessage(&msg);  

        TRACE("Get Message:%08X\r\n", msg.message);   // 打印收到什么消息

        // 核心：查找消息对应的处理函数
        if (m_mapFunc.find(msg.message) != m_mapFunc.end()) {

            // 找到就调用！
            (this->*m_mapFunc[msg.message])(msg.message,msg.wParam, msg.lParam);
        
        }
    }

}

// 实现内部CPacket发送方法：将CPacket转换为字节流，调用socket::send发送
bool CClientSocket::Send(const CPacket& pack) {
    TRACE("m_sock = %d\r\n", m_sock);
    if (m_sock == -1) return false;
    std::string strOut;
    pack.Data(strOut);
    return send(m_sock, strOut.c_str(), strOut.size(), 0) > 0;
}

/*
    SendPack()在独立的网络线程中执行，承接SendPacket的触发指令，完成从 “建立连接” 到 “反馈结果” 的全流程：
    （1）解析消息参数，释放PACKET_DATA堆内存；
    （2）调用InitSocket建立 / 重置 Socket 连接；
    （3）调用底层send（或复用Send()）发送网络字节流；
    （4）循环接收被控端应答数据包，处理粘包；
    （5）根据发送结果向上层窗口发送WM_SEND_PACK_ACK应答消息；
    （6）处理自动关闭（CSM_AUTOCLOSE）、关闭 Socket、释放资源；
    （7）处理各类异常（连接失败、发送失败、接收失败）。
*/
void CClientSocket::SendPack(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
    PACKET_DATA data = *(PACKET_DATA*)wParam;   // 解析PacketData指针，获取发送信息
    delete (PACKET_DATA*)wParam;                // 释放堆上的PacketData，避免内存泄漏
    HWND hWnd = (HWND)lParam;                   // 解析上层窗口句柄，用于应答反馈
    size_t nTemp = data.strData.size();         // 获取待发送字节流长度
    CPacket current((BYTE*)data.strData.c_str(), nTemp);    // 解析待发送字节流为CPacket
    
    
    if (InitSocket() == true) {      // 第一步：初始化Socket并连接被控端，连接成功则执行发送
        int ret = send(m_sock, (char*)data.strData.c_str(), (int)data.strData.size(), 0);  // 第二步：调用socket::send发送字节流
        
        if (ret > 0) {
            size_t index = 0;        // 接收应答的偏移量（处理粘包）
            std::string strBuffer;    // 应答接收缓冲区
            strBuffer.resize(BUFFER_SIZE);
            char* pBuffer = (char*)strBuffer.c_str();

            while (m_sock != INVALID_SOCKET) {      // 循环接收应答，直到Socket关闭
                TRACE("准备执行 recv，开始等待服务器数据...\n"); 
                int length = recv(m_sock, pBuffer + index, BUFFER_SIZE - index, 0); 
                TRACE("recv 返回：length=%d\n", length);
                if (length > 0 || (index > 0)) {
                    
                    index += (size_t)length;       // 偏移量后移，累计有效数据
                    size_t nLen = index;           // 当前有效数据总长度

                    CPacket pack((BYTE*)pBuffer, nLen);    // 解析应答数据包
                    if (nLen > 0) {
                        TRACE("ack pack %d to hWnd %08X %d %d\r\n", pack.sCmd, hWnd, index, nLen);
                        TRACE("%04X\r\n", *(WORD*)pBuffer + nLen);

                        // 发送应答消息给上层窗口：携带解析后的应答数据包，传递扩展参数
                        ::SendMessage(hWnd, WM_SEND_PACK_ACK, (WPARAM)new CPacket(pack), data.wParam);
                       
                        if (data.nMode & CSM_AUTOCLOSE) {   // 判断发送模式：自动关闭则关闭Socket并退出
                            CloseSocket();
                            return;
                        }

                        index -= nLen;    // 处理粘包：移动未解析数据，更新偏移量
                        memmove(pBuffer, pBuffer + nLen, index);
                    }
                }
                else { 
                    TRACE("recv failed length %d index %d cmd %d\r\n", length, index, current.sCmd);
                   
                    CloseSocket();
                    // 发送失败应答：携带空数据包，扩展参数置1（标识接收失败）
                    ::SendMessage(hWnd, WM_SEND_PACK_ACK, (WPARAM)new CPacket(current.sCmd, NULL, 0), 1);
                }
            }
        }
        else {
            CloseSocket();
            //网络终止处理
            ::SendMessage(hWnd, WM_SEND_PACK_ACK, NULL, -1);
        }
    }
    else {
        //TODO:错误处理
        ::SendMessage(hWnd, WM_SEND_PACK_ACK, NULL, -2);
    }
}

//void CClientSocket::SendPack(UINT nMsg, WPARAM wParam, LPARAM lParam)
//{
//    PACKET_DATA data = *(PACKET_DATA*)wParam;   // 解析PacketData指针，获取发送信息
//    delete (PACKET_DATA*)wParam;                // 释放堆上的PacketData，避免内存泄漏
//    HWND hWnd = (HWND)lParam;                   // 解析上层窗口句柄，用于应答反馈
//    size_t nTemp = data.strData.size();         // 获取待发送字节流长度
//    CPacket current((BYTE*)data.strData.c_str(), nTemp);    // 解析待发送字节流为CPacket
//
//    if (InitSocket() == true) {      // 第一步：初始化Socket并连接被控端，连接成功则执行发送
//        int ret = send(m_sock, (char*)data.strData.c_str(), (int)data.strData.size(), 0);  // 第二步：调用socket::send发送字节流
//
//        if (ret > 0) {
//            size_t index = 0;        // 接收应答的偏移量（处理粘包）
//            std::string strBuffer;    // 应答接收缓冲区
//            strBuffer.resize(BUFFER_SIZE);
//            char* pBuffer = (char*)strBuffer.c_str();
//
//            while (m_sock != INVALID_SOCKET) {      // 循环接收应答，直到Socket关闭
//                TRACE("准备执行 recv，开始等待服务器数据...\n");
//                int length = recv(m_sock, pBuffer + index, BUFFER_SIZE - index, 0);
//                TRACE("recv 返回：length=%d\n", length);
//
//                // ===================== 【核心修复 1】 =====================
//                // recv 返回 <=0 代表：服务器断开连接 或 出错
//                if (length <= 0) {
//                    TRACE("服务器已关闭连接，通信正常结束\n");
//
//                    // 服务器回复完成 → 给UI发送成功消息
//                    ::SendMessage(hWnd, WM_SEND_PACK_ACK, (WPARAM)new CPacket(current.sCmd, NULL, 0), 0);
//
//                    CloseSocket();
//                    return;
//                }
//
//                index += (size_t)length;       // 偏移量后移，累计有效数据
//                size_t nLen = index;           // 当前有效数据总长度
//
//                // ===================== 【核心修复 2】 =====================
//                // 必须先判断数据包是否完整，再解析
//                if (nLen >= 6) {  // 包头至少6字节
//                    CPacket pack((BYTE*)pBuffer, nLen);    // 解析应答数据包
//
//                    TRACE("ack pack %d to hWnd %08X %d %d\r\n", pack.sCmd, hWnd, index, nLen);
//
//                    // 发送应答消息给上层窗口
//                    ::SendMessage(hWnd, WM_SEND_PACK_ACK, (WPARAM)new CPacket(pack), data.wParam);
//
//                    if (data.nMode & CSM_AUTOCLOSE) {
//                        CloseSocket();
//                        return;
//                    }
//
//                    // 处理粘包：移除已处理的包
//                    index -= nLen;
//                    memmove(pBuffer, pBuffer + nLen, index);
//                }
//                // ==========================================================
//            }
//        }
//        else {
//            CloseSocket();
//            ::SendMessage(hWnd, WM_SEND_PACK_ACK, NULL, -1);
//        }
//    }
//    else {
//        ::SendMessage(hWnd, WM_SEND_PACK_ACK, NULL, -2);
//    }
//}

