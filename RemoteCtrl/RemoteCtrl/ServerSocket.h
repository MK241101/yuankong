#pragma once

#include "pch.h"
#include "framework.h"
#include <list>
#include "Packet.h"


#define BUFFER_SIZE 4096
typedef void(*SOCKET_CALLBACK)(void* ,int, std::list<CPacket>&, CPacket&);

class CServerSocket
{
public:
    static CServerSocket* getInstance() {
        if (m_instance == NULL) {
            m_instance = new CServerSocket();

        }
        return m_instance;
    }

    

    int Run(SOCKET_CALLBACK callback, void* arg, short port=9527) {
        bool ret = InitSocket(port);
        if(ret==false) { return -1; }

        std::list<CPacket> lstPacket;
        m_callback = callback;
        m_arg = arg;
        int count = 0;

        while (true) {
            if (AcceptClient() == false) {
                if (count >= 3) { return -2; }
                count++;
            }

            int ret=DealCommand();
            if (ret > 0) {
                m_callback(m_arg, ret,lstPacket,m_packet);
                if (lstPacket.size() > 0) {
                    Send(lstPacket.front());
                    lstPacket.pop_front();
                }
            }
            CloseClient();
        }
        return 0;
        
    }
protected:
    bool InitSocket(short port) {

        sockaddr_in serv_addr, client_adr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = htons(port);

        bind(m_sock, (sockaddr*)&serv_addr, sizeof(serv_addr));
        if (listen(m_sock, 1) == -1) { return false; };

        return true;
    }
   
    bool AcceptClient()
    {
        TRACE("enter AcceptClient\r\n");
        sockaddr_in client_adr;
		int cli_sz = sizeof(client_adr);
        m_client = accept(m_sock, (sockaddr*)&client_adr, &cli_sz);
        TRACE("m_client=%d\r\n", m_client);
        if (m_client == -1) { return false; }
		return true;    

    }

    int DealCommand() {
        if (m_client == -1) { return -1; }
		char* buffer = new char[BUFFER_SIZE];
        if (buffer == NULL) {
            TRACE("内存不足！\r\n");
            return -2;
        }
		memset(buffer, 0, BUFFER_SIZE);
		size_t index = 0;
        while (true) {
            size_t len = recv(m_client, buffer+ index, BUFFER_SIZE -index, 0);
            if (len <= 0) { 
                delete[] buffer;

                return -1; 
            }
			index += len;
            len = index;
            m_packet = CPacket((BYTE*)buffer, len);  //通过构造函数解析数据包
            if (len > 0) {
                memmove(buffer, buffer + len, BUFFER_SIZE -len);
				index -= len;
                delete[] buffer;
                return m_packet.sCmd;
            }
        }
        delete[] buffer;
        return -1;
    }

    bool Send(const char* pData, int nSize) {
        if (m_client == -1) { return false; }
        return send(m_client, pData, nSize, 0) > 0;
    }

    bool Send(CPacket& pack) {
        if (m_client == -1) return false;
        //Dump((BYTE*)pack.Data(), pack.Size());
        return send(m_client, pack.Data(), pack.Size(), 0) > 0;
    }

    bool GetFilePath(std::string& strPath) {
        if (((m_packet.sCmd >= 2)&&(m_packet.sCmd<=4))||(m_packet.sCmd==9)) {
            strPath = m_packet.strData;
            return true;
        }
        return false;
    }

    bool GetMouseEvent(MOUSEEV& mouse) {
        if (m_packet.sCmd == 5) {
            memcpy(&mouse, m_packet.strData.c_str(), sizeof(MOUSEEV));
            return true;
        }
        return false;
    }

    CPacket GetPacket() {
        return m_packet;
    
    }

    void CloseClient() {
        if (m_client != INVALID_SOCKET) {
            closesocket(m_client);
            m_client = INVALID_SOCKET;
        }
        
    }

private:
    CServerSocket(const CServerSocket& ss) {
        m_sock = ss.m_sock;
        m_client = ss.m_client;
    
    };
    CServerSocket& operator=(const CServerSocket& ss) {};
    CServerSocket() {
        m_client = -1;
        if (InitSockEnv() == FALSE) {
            MessageBox(NULL, _T("初始化Socket环境失败"), _T("初始化错误"), MB_OK | MB_ICONERROR);
            exit(0);
        }
        m_sock=socket(PF_INET, SOCK_STREAM, 0);
    };

    ~CServerSocket() {
        closesocket(m_sock);

        WSACleanup();     // 清理 Winsock 库

    };

    BOOL InitSockEnv() {
        WSADATA data;
        if (WSAStartup(MAKEWORD(1, 1), &data) != 0) {
            return FALSE;
        }
        return TRUE;          // 成功初始化

    }

    

    static void releaseInstance() {
        if (m_instance != NULL) {
            CServerSocket* tmp = m_instance;
            m_instance = NULL;
            delete tmp;
        }
    }
    
    class CHelper {
    public:
        CHelper() { CServerSocket::getInstance(); }
        ~CHelper() { CServerSocket::releaseInstance(); }
    };

    static CHelper m_helper;
    static CServerSocket* m_instance;

    SOCKET m_sock;
    SOCKET m_client;
    CPacket m_packet;

    SOCKET_CALLBACK m_callback;
    void* m_arg;
};


