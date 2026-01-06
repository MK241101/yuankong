#pragma once

#include <string>
#include "pch.h"
#include "framework.h"
#include <vector>
#include<list>
#include<map>
#include<mutex>

#define WM_SEND_PACK (WM_USER + 1)  //发送数据包
#define WM_SEND_PACK_ACK (WM_USER + 2)  //发送数据包应答


#pragma pack(push)
#pragma pack(1)
#define BUFFER_SIZE 4096000
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

    CPacket(const BYTE* pData, size_t& nSize){
        size_t i = 0;
        for (; i < nSize; i++) {
            if (*(WORD*)(pData + i) == 0xFEFF) {
                sHead = *(WORD*)(pData + i);
                i += 2;
                break;
            }
        }
        if (i + 4 + 2 + 2 > nSize) {//包数据可能不全，或者包头未能全部接收到
            nSize = 0;
            return;
        }
        nLength = *(DWORD*)(pData + i); i += 4;
        if (nLength + i > nSize) {//包未完全接收到，就返回，解析失败
            nSize = 0;
            return;
        }
        sCmd = *(WORD*)(pData + i); i += 2;
        if (nLength > 4) {
            strData.resize(nLength - 2 - 2);
            memcpy((void*)strData.c_str(), pData + i, nLength - 4);
            TRACE("%s\r\n", strData.c_str() + 12);
            i += nLength - 4;
        }
        sSum = *(WORD*)(pData + i); i += 2;
        WORD sum = 0;
        for (size_t j = 0; j < strData.size(); j++)
        {
            sum += BYTE(strData[j]) & 0xFF;
        }
        if (sum == sSum) {
            nSize = i;//head2 length4 data...
            return;
        }
        nSize = 0;
    }

    ~CPacket() {}

    int Size() { //包数据的大小
        return nLength + 6;
    }

    const char* Data(std::string& strOut) const{
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

void Dump(BYTE* pData, size_t nSize);

enum {
    CSM_AUTOCLOSE=1,  //自动关闭模式



};


typedef struct PacketData{

    std::string strData;
    UINT nMode;
    WPARAM wParam;
    PacketData(const char* pData, size_t nLen, UINT mode, WPARAM nParam=0) {
        strData.resize(nLen);
        memcpy((char*)strData.c_str(), pData, nLen);
        nMode = mode;
        wParam = nParam;
    }

    PacketData(const PacketData& data) {
        strData = data.strData;
        nMode = data.nMode;
        wParam = data.wParam;

    }

    PacketData& operator=(const PacketData& data) {
        if (this != &data) {
            strData = data.strData;
            nMode = data.nMode;
            wParam = data.wParam;

        }
        return *this;
    }
}PACKET_DATA;




class CClientSocket
{
public:
    static CClientSocket* getInstance() {
        if (m_instance == NULL) {
            m_instance = new CClientSocket();
            TRACE("CClientSocket size is %d\r\n",sizeof(*m_instance));

        }
        return m_instance;
    }

    bool InitSocket();

    
    int DealCommand() {
        if (m_sock == -1) { return -1; }
        char* buffer = m_buffer.data();
        static size_t index = 0;
        while (true) {
            size_t len = recv(m_sock, buffer + index, BUFFER_SIZE - index, 0);
            if (((int)len <= 0) && ((int)index<=0)) {
                return -1;
            }
            //Dump((BYTE*)buffer, index);

            TRACE("recv len=%d(0x%08x) index=%d(0x%08x)\r\n", len, len, index, index);

            index += len;
            len = index;
            TRACE("recv len=%d(0x%08x) index=%d(0x%08x)\r\n", len, len, index, index);

            m_packet = CPacket((BYTE*)buffer, len);  //通过构造函数解析数据包
            TRACE("command %d\r\n",m_packet.sCmd);

            if (len > 0) {
                memmove(buffer, buffer + len, index - len);
                index -= len;
                
                return m_packet.sCmd;
            }
        }
        
        return -1;
    }

    

    bool SendPacket(HWND hWnd ,const CPacket& pack, bool isAutoClosed=true,WPARAM wParam=0);

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
        m_sock=INADDR_ANY;
    }

    void UpdateAddress(INT nIP, int nPort) {
        if ((m_nIP != nIP) || (m_nPort != nPort)) {
            m_nIP = nIP;
            m_nPort = nPort;
        }
    
    }


private:
    CClientSocket(const CClientSocket& ss);

    CClientSocket& operator=(const CClientSocket& ss) {};

    CClientSocket();

    ~CClientSocket() {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
        WSACleanup();     // 清理 Winsock 库

    };




    static unsigned __stdcall threadEntry(void* arg);

   // void threadFunc();
    void threadFunc2();

    BOOL InitSockEnv() {
        WSADATA data;
        if (WSAStartup(MAKEWORD(1, 1), &data) != 0) {
            return FALSE;
        }
        return TRUE;          // 成功初始化

    }

    static void releaseInstance() {
        TRACE("CClientSocket has called!!!\r\n");

        if (m_instance != NULL) {
            CClientSocket* tmp = m_instance;
            m_instance = NULL;
            delete tmp;
            TRACE("CClientSocket has released\r\n"); 
        }
    }


    bool Send(const char* pData, int nSize) {
        if (m_sock == -1) { return false; }
        return send(m_sock, pData, nSize, 0) > 0;
    }

    bool Send(const CPacket& pack);

    void SendPack(UINT nMsg, WPARAM wParam/*缓冲区的值*/, LPARAM lParam/*缓冲区的长度*/);


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
    int m_nPort;
    int m_nIP;

    std::map<HANDLE, bool> m_mapAutoClosed;
    std::map<HANDLE,std::list<CPacket>&> m_mapAck;
    std::list<CPacket> m_lstSend;

    bool m_bAutoClose;
    std::mutex m_lock;

    HANDLE m_hThread;
    typedef void(CClientSocket::* MSGFUNC)(UINT nMsg, WPARAM wParam, LPARAM lParam);
    std::map<UINT, MSGFUNC> m_mapFunc;

    UINT m_nThreadID;
};



