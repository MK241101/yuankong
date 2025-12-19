#pragma once

#include "pch.h"
#include "framework.h"


class CServerSocket
{
public:
    static CServerSocket* getInstance() {
        if (m_instance == NULL) {
            m_instance = new CServerSocket();

        }
        return m_instance;
    }

    bool InitSocket() {

        sockaddr_in serv_addr, client_adr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        serv_addr.sin_port = htons(9527);

        bind(m_sock, (sockaddr*)&serv_addr, sizeof(serv_addr));
        if (listen(m_sock, 1) == -1) { return false; };
        
        return true;
    }

    bool AcceptClient()
    {
        sockaddr_in client_adr;
		int cli_sz = sizeof(client_adr);
        m_client = accept(m_sock, (sockaddr*)&client_adr, &cli_sz);
       
		return true;    

    }

    int DealCommand() {
        if (m_client == -1) { return false; }
        char buffer[1024] = "";
        while (true) {
            int len = recv(m_client, buffer, sizeof(buffer), 0);
            if (len <= 0) { 
                return -1; 
            }


        }
    
    }

    bool Send(const char* pData, int nSize) {
        if (m_client == -1) { return false; }
        return send(m_client, pData, nSize, 0) > 0;
    
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

};


