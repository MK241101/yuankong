#pragma once

#include "pch.h"
#include "framework.h"
#include <list>
#include "Packet.h"


#define BUFFER_SIZE 4096   // 接收数据缓冲区大小 4096 字节
typedef void(*SOCKET_CALLBACK)(void* ,int, std::list<CPacket>&, CPacket&);  // 回调函数类型定义：收到消息后调用的业务函数

class CServerSocket
{
public:
    static CServerSocket* getInstance() {   // 获取单例实例（全局只有一个服务器对象）
        if (m_instance == NULL) {
            m_instance = new CServerSocket();
        }
        return m_instance;
    }

    bool InitSocket(short port) {     // 初始化 Socket：绑定端口 + 开始监听
        sockaddr_in serv_addr, client_adr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = htons(port);

        bind(m_sock, (sockaddr*)&serv_addr, sizeof(serv_addr)); 
        if (listen(m_sock, 1) == -1) { return false; };   
        
        // listen 的第二个参数叫 半连接队列 / 全连接队列，
        // 控制有多少个客户端可以 “已经连接成功，但服务器还没来得及调用 accept”

        return true;
    }
    
    int Run(SOCKET_CALLBACK callback, void* arg, short port=9527) {   // 服务器主循环：启动服务
        bool ret = InitSocket(port);
        if(ret==false) { return -1; }

        std::list<CPacket> lstPacket;      // 待发送消息队列
        m_callback = callback;             // 保存业务回调函数
        m_arg = arg;
        int count = 0;

        while (true) {             // 死循环：接受客户端 -> 接收数据 -> 处理 -> 关闭
            if (AcceptClient() == false) {
                if (count >= 3) { return -2; }
                count++;
            }

            int ret=DealCommand();  // 接收并解析客户端数据（阻塞）
            if (ret > 0) {
                m_callback(m_arg, ret,lstPacket,m_packet);

                if (lstPacket.size() > 0) {        // 如果有要回复的消息，发送给客户端
                    Send(lstPacket.front());
                    lstPacket.pop_front();
                }
            }
            CloseClient();       // 处理完立刻关闭客户端
        }
        return 0;
        
    }
protected:
    
   
    bool AcceptClient()   // 接受客户端连接（阻塞式）
    {
        TRACE("enter AcceptClient\r\n");
        sockaddr_in client_adr;
		int cli_sz = sizeof(client_adr);

        m_client = accept(m_sock, (sockaddr*)&client_adr, &cli_sz);   // accept 阻塞等待客户端连接
        TRACE("m_client=%d\r\n", m_client);
        if (m_client == -1) { return false; }
		return true;    

    }

    int DealCommand() {                    // 接收客户端数据并解析（阻塞）
        if (m_client == -1) { return -1; }
		char* buffer = new char[BUFFER_SIZE];
        if (buffer == NULL) {
            TRACE("内存不足！\r\n");
            return -2;
        }
		memset(buffer, 0, BUFFER_SIZE);
		size_t index = 0;

        while (true) {     // 循环接收数据
            size_t len = recv(m_client, buffer+ index, BUFFER_SIZE -index, 0);  // recv 阻塞等待数据到来
            if (len <= 0) { 
                delete[] buffer;
                return -1; 
            }
            TRACE("recv %d\r\n", len);

			index += len;
            len = index;
            m_packet = CPacket((BYTE*)buffer, len);  //通过构造函数解析数据包
            if (len > 0) {
                memmove(buffer, buffer + len, BUFFER_SIZE -len);   // 缓冲区数据前移
				index -= len;
                delete[] buffer;
                return m_packet.sCmd;   // 返回命令号
            }
        }
        delete[] buffer;
        return -1;
    }

    // 发送原始数据
    bool Send(const char* pData, int nSize) {
        if (m_client == -1) { return false; }
        return send(m_client, pData, nSize, 0) > 0;
    }

    // 发送数据包
    bool Send(CPacket& pack) { 
        if (m_client == -1) return false;
        return send(m_client, pack.Data(), pack.Size(), 0) > 0;
    }

    // 关闭客户端连接
    void CloseClient() {
        if (m_client != INVALID_SOCKET) {
            closesocket(m_client);
            m_client = INVALID_SOCKET;
        }
        
    }

private:
    // 拷贝构造私有化（单例禁止拷贝）
    CServerSocket(const CServerSocket& ss) {
        m_sock = ss.m_sock;
        m_client = ss.m_client;
    
    };
    CServerSocket& operator=(const CServerSocket& ss) {};
    
    CServerSocket() {   // 构造函数：初始化 Winsock + 创建监听 Socket
        m_client = INVALID_SOCKET;
        if (InitSockEnv() == FALSE) {
            MessageBox(NULL, _T("初始化Socket环境失败"), _T("初始化错误"), MB_OK | MB_ICONERROR);
            exit(0);
        }
        m_sock=socket(PF_INET, SOCK_STREAM, 0);
    };

    ~CServerSocket() {         // 析构函数：关闭 Socket + 清理网络库
        closesocket(m_sock);
        WSACleanup();     // 清理 Winsock 库

    };

    // 初始化 Windows 网络库
    BOOL InitSockEnv() {
        WSADATA data;
        if (WSAStartup(MAKEWORD(2, 0), &data) != 0) {
            return FALSE;
        }
        return TRUE;          // 成功初始化

    }

    
    // 释放单例
    static void releaseInstance() {
        if (m_instance != NULL) {
            CServerSocket* tmp = m_instance;
            m_instance = NULL;
            delete tmp;
        }
    }
    
    // 辅助类：自动释放单例
    class CHelper {
    public:
        CHelper() { CServerSocket::getInstance(); }
        ~CHelper() { CServerSocket::releaseInstance(); }
    };

    static CHelper m_helper;
    static CServerSocket* m_instance;

    SOCKET m_sock;       // 监听 Socket
    SOCKET m_client;     // 当前客户端 Socket
    CPacket m_packet;    // 接收的数据包

    SOCKET_CALLBACK m_callback;  // 业务回调函数
    void* m_arg;                  // 回调参数
};


