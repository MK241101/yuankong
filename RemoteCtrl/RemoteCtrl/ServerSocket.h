#pragma once

#include "pch.h"
#include "framework.h"

#pragma pack(push)
#pragma pack(1)
#define BUFFER_SIZE 4096
class CPacket   //数据包结构
{
public:
    CPacket() :sHead(0), nLength(0), sCmd(0), sSum(0) {}  //
    CPacket(const CPacket& pack) {
        sHead = pack.sHead;
        nLength = pack.nLength;
        sCmd = pack.sCmd;
        strData = pack.strData;
        sSum = pack.sSum;
    }
    CPacket& operator=(const CPacket& pack) {
        if (this != &pack) {
            sHead = pack.sHead;
            nLength = pack.nLength;
            sCmd = pack.sCmd;
            strData = pack.strData;
            sSum = pack.sSum;
        }
        return *this;
    }
    CPacket(WORD nCmd, const BYTE* pData, size_t nSize) {     //打包数据
        sHead = 0xFEFF;
        nLength = nSize + 4;
        sCmd = nCmd;
        strData.resize(nSize);
        memcpy((void*)strData.c_str(), pData, nSize);
        sSum = 0;
        for (size_t j = 0; j < strData.size(); j++)
        {
            sSum += BYTE(strData[j]) & 0xFF;
        }
    }

    CPacket(const BYTE* pData, size_t nSize) {                 //从数据块中解析出数据包
        size_t i = 0;
        for (; i < nSize; i++) {
            if (*(WORD*)(pData + i) == 0xFEFF) {
                sHead = *(WORD*)(pData + i);
                i += 2;
                break;
            }
        }

        if (i + 8 > nSize) {             //包头+包长度+命令字+校验和最小8字节，包未完整接收，解析失败
            nSize = 0;
            return;
        }

        nLength = *(DWORD*)(pData + i); i += 4;
        if (nLength + i > nSize) {              //包未完整接收，解析失败
            nSize = 0;
            return;
        }

        sCmd = *(WORD*)(pData + i); i += 2;

        if (nLength > 4) {
            strData.resize(nLength - 4);
            memcpy((void*)strData.c_str(), pData + i, nLength - 4);
            i += (nLength - 4);
        }

        sSum = *(WORD*)(pData + i); i += 2;

        WORD sum = 0;
        for (size_t j = 0; j < strData.size(); j++)
        {
            sum += BYTE(strData[j]) & 0xFF;
        }

        if (sum == sSum) {
            nSize = i;
            return;
        }
        nSize = 0;
    }

    ~CPacket() {}

    int Size() { //包数据的大小
        return nLength + 6;
    }

    const char* Data() {
        strOut.resize(nLength + 6);
        BYTE* pData = (BYTE*)strOut.c_str();
        *(WORD*)pData = sHead; pData += 2;
        *(DWORD*)(pData) = nLength; pData += 4;
        *(WORD*)pData = sCmd; pData += 2;
        memcpy(pData, strData.c_str(), strData.size()); pData += strData.size();
        *(WORD*)pData = sSum;
        return strOut.c_str();
    }

public:
    WORD sHead;   //包头
    DWORD nLength;  //包长度（从控制命令开始到和校验结束）
    WORD sCmd;   //命令字
    std::string strData; //数据体
    WORD sSum;  //校验和
    std::string strOut;  //整个包的数据
};

#pragma pack(pop)


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
        if (m_client == -1) { return -1; }
		char* buffer = new char[BUFFER_SIZE];
		memset(buffer, 0, BUFFER_SIZE);
		size_t index = 0;
        while (true) {
            size_t len = recv(m_client, buffer+ index, BUFFER_SIZE -index, 0);
            if (len <= 0) { 
                return -1; 
            }
			index += len;
            len = index;
            m_packet = CPacket((BYTE*)buffer, len);  //通过构造函数解析数据包
            if (len > 0) {
                memmove(buffer, buffer + len, BUFFER_SIZE -len);
				index -= len;
                return m_packet.sCmd;
            }
        }
        return -1;
    }

    bool Send(const char* pData, int nSize) {
        if (m_client == -1) { return false; }
        return send(m_client, pData, nSize, 0) > 0;
    }

    bool Send(CPacket& pack) {
        if (m_client == -1) return false;
        return send(m_client, pack.Data(), pack.Size(), 0) > 0;
    }

    bool GetFilePath(std::string& strPath) {
        if (m_packet.sCmd == 2) {
            strPath = m_packet.strData;
            return true;
        }
        return false;
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
};


