#pragma once

#include <string>
#include "pch.h"
#include "framework.h"
#include <vector>


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
        if (nSize > 0) {
            strData.resize(nSize);
            memcpy((void*)strData.c_str(), pData, nSize);
        }
        else {
            strData.clear();
        }

        sSum = 0;
        for (size_t j = 0; j < strData.size(); j++)
        {
            sSum += BYTE(strData[j]) & 0xFF;
        }
    }

    CPacket(const BYTE* pData, size_t& nSize) {                 //从数据块中解析出数据包
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



typedef struct MouseEvent {
    MouseEvent() {
        nAction = 0;
        nButton = -1;
        ptXY.x = 0;
        ptXY.y = 0;
    }
    WORD nAction;    // 操作类型：点击、移动、双击等
    WORD nButton;    // 鼠标按键：左键、右键、中键等
    POINT ptXY;      // 鼠标坐标
} MOUSEEV, * PMOUSEEV;

typedef struct file_info {
    file_info() {
        IsInvalid = FALSE;
        IsDirectory = -1;
        HasNext = TRUE;
        memset(szFileName, 0, sizeof(szFileName));
    }
    BOOL IsInvalid;   //是否有效
    BOOL IsDirectory;   //是否是目录
    BOOL HasNext;   //是否有下一个
    char szFileName[256];  //文件名

}FILEINFO, * PFILEINFO;


//把 Windows 网络编程中（WSA）的数字错误码（比如 10060 表示连接超时、10061 表示连接被拒绝），
//转换成人类可读的文本描述
std::string GetErrInfo(int wsaErrCode);


class CClientSocket
{
public:
    static CClientSocket* getInstance() {
        if (m_instance == NULL) {
            m_instance = new CClientSocket();

        }
        return m_instance;
    }

    bool InitSocket(int nIP,int nPort) {
        if (m_sock != INVALID_SOCKET) { CloseSocket(); }
        m_sock = socket(PF_INET, SOCK_STREAM, 0);
        if (m_sock == -1) { return false; }

        sockaddr_in serv_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = htonl(nIP);
        serv_addr.sin_port = htons(nPort);
        if (serv_addr.sin_addr.s_addr == INADDR_NONE) {
            AfxMessageBox("IP地址不存在！");
            return false;
        }

        int ret = connect(m_sock, (sockaddr*)&serv_addr, sizeof(serv_addr));

        if (ret == -1) {
            AfxMessageBox("连接服务器失败");
            TRACE("连接服务器失败：%d %s\r\n",WSAGetLastError(),GetErrInfo(WSAGetLastError()).c_str());
            return false;
        }
        return true;
    }

    
    int DealCommand() {
        if (m_sock == -1) { return -1; }
        char* buffer = m_buffer.data();
        memset(buffer, 0, BUFFER_SIZE);
        size_t index = 0;
        while (true) {
            size_t len = recv(m_sock, buffer + index, BUFFER_SIZE - index, 0);
            if (len <= 0) {
                return -1;
            }
            index += len;
            len = index;
            m_packet = CPacket((BYTE*)buffer, len);  //通过构造函数解析数据包
            if (len > 0) {
                memmove(buffer, buffer + len, BUFFER_SIZE - len);
                index -= len;
                
                return m_packet.sCmd;
            }
        }
        
        return -1;
    }

    bool Send(const char* pData, int nSize) {
        if (m_sock == -1) { return false; }
        return send(m_sock, pData, nSize, 0) > 0;
    }

    bool Send(CPacket& pack) {
        if (m_sock == -1) return false;
        return send(m_sock, pack.Data(), pack.Size(), 0) > 0;
    }

    bool GetFilePath(std::string& strPath) {
        if ((m_packet.sCmd >= 2) && (m_packet.sCmd <= 4)) {
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
    void CloseSocket() {
        closesocket(m_sock);
        m_sock=INVALID_SOCKET;
    }

private:
    CClientSocket(const CClientSocket& ss) {
        m_sock = ss.m_sock;
    };
    CClientSocket& operator=(const CClientSocket& ss) {};
    CClientSocket() {

        if (InitSockEnv() == FALSE) {
            MessageBox(NULL, _T("初始化Socket环境失败"), _T("初始化错误"), MB_OK | MB_ICONERROR);
            exit(0);
        }
        m_buffer.resize(BUFFER_SIZE);
    };

    ~CClientSocket() {
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
            CClientSocket* tmp = m_instance;
            m_instance = NULL;
            delete tmp;
        }
    }

    class CHelper {
    public:
        CHelper() { CClientSocket::getInstance(); }
        ~CHelper() { CClientSocket::releaseInstance(); }
    };

    static CHelper m_helper;
    static CClientSocket* m_instance;

    SOCKET m_sock;
    CPacket m_packet;
    std::vector<char> m_buffer;
};



