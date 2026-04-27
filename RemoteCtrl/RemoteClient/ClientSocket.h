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
#define BUFFER_SIZE 4096000  //缓冲区大小

#pragma pack(push)       // 保存当前编译器的默认字节对齐方式，用于后续恢复
#pragma pack(1)          // 设置字节对齐为1字节（网络传输强制要求），禁止编译器自动填充空字节

class CPacket            //数据包结构
{
public:
    CPacket() :sHead(0), nLength(0), sCmd(0), sSum(0) {}  //
    
    CPacket(WORD nCmd, const BYTE* pData, size_t nSize) {     //打包构造函数：将业务命令+原始数据封装成标准CPacket数据包
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
    
    CPacket(const CPacket& pack) {
        sHead = pack.sHead;
        nLength = pack.nLength;
        sCmd = pack.sCmd;
        strData = pack.strData;
        sSum = pack.sSum;
    }

    CPacket(const BYTE* pData, size_t& nSize) {  //构造函数：将标准CPacket数据包解包成业务命令+原始数据
        size_t i = 0;

        // 第一步：帧同步 - 遍历字节流，查找包头标识0xFEFF，定位数据包起始位置
        for (; i < nSize; i++) {       
            if (*(WORD*)(pData + i) == 0xFEFF) {
                sHead = *(WORD*)(pData + i);
                i += 2;
                break;
            }
        }
        // 第二步：校验数据包完整性 - 检查剩余字节是否满足最小包结构（包头2+长度4+命令2+校验2）
        if (i + 4 + 2 + 2 > nSize) {
            nSize = 0;
            return;
        }
        // 第三步：解析固定字段（包长度、命令字）
        nLength = *(DWORD*)(pData + i); i += 4;
        if (nLength + i > nSize) {//包未完全接收到，就返回，解析失败
            nSize = 0;
            return;
        }
        sCmd = *(WORD*)(pData + i); i += 2;    // 读取2字节命令字，指针后移
        // 第四步：解析数据体（业务数据）
        if (nLength > 4) {
            strData.resize(nLength - 2 - 2);   // 数据体长度=包长度-命令-校验和
            memcpy((void*)strData.c_str(), pData + i, nLength - 4);
            TRACE("%s\r\n", strData.c_str());
            i += nLength - 4;
        }
        // 第五步：解析校验和并验证数据完整性
        sSum = *(WORD*)(pData + i); i += 2;
        WORD sum = 0;
        for (size_t j = 0; j < strData.size(); j++)
        {
            sum += BYTE(strData[j]) & 0xFF;  //把string中的每个字符当作一个 0～255 的无符号字节来累加，用于校验和计算
        }
        if (sum == sSum) {    // 校验和一致：数据完整，解析成功
            nSize = i;//head2 length4 data...
            return;
        }
        nSize = 0;
    }
    ~CPacket() {}

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

    int Size() { //包数据的大小
        return nLength + 6;
    }

    const char* Data(std::string& strOut) const{     // 将CPacket对象转换为连续的字节流
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

#pragma pack(pop)   // 恢复编译器默认的字节对齐方式，不影响后续代码编译

typedef struct MouseEvent {    //鼠标操作数据结构
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

typedef struct file_info {       // 文件/目录信息数据结构
    file_info() {
        IsInvalid = FALSE;
        IsDirectory = -1;       //是否是目录
        HasNext = TRUE;         //是否有下一个文件/目录
        memset(szFileName, 0, sizeof(szFileName));
    }
    BOOL IsInvalid;   //是否有效
    BOOL IsDirectory;   //是否是目录
    BOOL HasNext;   //是否有下一个
    char szFileName[256];  //文件名

}FILEINFO, * PFILEINFO;

enum {
    CSM_AUTOCLOSE = 1,  //自动关闭模式
};

typedef struct PacketData {      // 数据包传输附加信息结构体：封装待发送数据的「原始数据+发送模式+扩展参数」

    std::string strData;
    UINT nMode;
    WPARAM wParam;
    PacketData(const char* pData, size_t nLen, UINT mode, WPARAM nParam = 0) {
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

//把 Windows 网络编程中（WSA）的数字错误码（比如 10060 表示连接超时、10061 表示连接被拒绝），
//转换成人类可读的文本描述
std::string GetErrInfo(int wsaErrCode);

void Dump(BYTE* pData, size_t nSize);


class CClientSocket
{
public:
    static CClientSocket* getInstance() {   //获取全局唯一实例（懒汉式，首次调用创建）
        if (m_instance == NULL) {
            m_instance = new CClientSocket();
            TRACE("CClientSocket size is %d\r\n",sizeof(*m_instance));

        }
        return m_instance;
    }
     
    bool InitSocket();    // 初始化Socket：创建Socket句柄、连接被控端IP/端口

    int DealCommand() {   // 循环接收Socket数据，解析CPacket数据包，成功解析返回数据包命令字
        if (m_sock == -1) { return -1; }
        char* buffer = m_buffer.data();
        static size_t index = 0;  // 静态偏移量：记录缓冲区中未解析的有效数据起始位置
        while (true) {
            size_t len = recv(m_sock, buffer + index, BUFFER_SIZE - index, 0);  // 接收数据：从index位置开始写入，避免覆盖未解析数据
            if ((len <= 0) && (index<=0)) {
                return -1;
            }

            index += len;
            len = index;

            m_packet = CPacket((BYTE*)buffer, len);  //通过构造函数解析数据包

            if (len > 0) {
                memmove(buffer, buffer + len, index - len);
                index -= len;
                
                return m_packet.sCmd;
            }
        }
        
        return -1;
    }

    // 封装发送数据包：供上层调用，将CPacket打包为PacketData，跨线程触发发送
    bool SendPacket(HWND hWnd ,const CPacket& pack, bool isAutoClosed=true,WPARAM wParam=0);

    bool GetFilePath(std::string& strPath) {      // 获取文件路径：从解析后的数据包中提取文件/目录路径
        if ((m_packet.sCmd >= 2) && (m_packet.sCmd <= 4)) {
        //指令2 3 4是和文件相关的
            strPath = m_packet.strData;
            return true;
        }
        return false;
    }

    bool GetMouseEvent(MOUSEEV& mouse) {   //获取鼠标操作数据：从解析后的数据包中提取鼠标操作参数（命令字5：鼠标操作）
        if (m_packet.sCmd == 5) {
            memcpy(&mouse, m_packet.strData.c_str(), sizeof(MOUSEEV));
            return true;
        }
        return false;
    }
    
    CPacket GetPacket() {       // 获取当前解析后的完整数据包对象，供上层业务模块使用
        return m_packet;
    }
   
    void CloseSocket() {
        closesocket(m_sock);
        m_sock= INVALID_SOCKET;
    }

    void UpdateAddress(INT nIP, int nPort) {      // 更新被控端地址：修改IP和端口
        if ((m_nIP != nIP) || (m_nPort != nPort)) {
            m_nIP = nIP;
            m_nPort = nPort;
        }
    }


private:
    HANDLE m_eventInvoke;//启动事件
    UINT m_nThreadID;    //网络线程ID：用于PostThreadMessage跨线程发送消息
    typedef void(CClientSocket::* MSGFUNC)(UINT nMsg, WPARAM wParam, LPARAM lParam);  // 消息处理函数指针类型
    std::map<UINT, MSGFUNC> m_mapFunc;   // 消息处理映射表：自定义消息ID → 对应处理函数（消息驱动核心）
    HANDLE m_hThread;
    bool m_bAutoClose;

    /*
    std::mutex m_lock;
    std::list<CPacket> m_lstSend;
    std::map<HANDLE, std::list<CPacket>&> m_mapAck;
    std::map<HANDLE, bool> m_mapAutoClosed;
    */
    
    int m_nIP;//地址
    int m_nPort;//端口
    std::vector<char> m_buffer;
    SOCKET m_sock;    // 核心Socket句柄：与被控端通信的唯一标识
    CPacket m_packet;


    CClientSocket& operator=(const CClientSocket& ss) {}
    CClientSocket(const CClientSocket& ss);
    CClientSocket();
    ~CClientSocket() {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
        WSACleanup();
    }
    static unsigned __stdcall threadEntry(void* arg);
    //void threadFunc();
    void threadFunc2();   // 实际的网络线程消息处理函数：循环处理Windows消息
     
    BOOL InitSockEnv() {    // 初始化Socket环境：加载WSA库，初始化Socket运行环境
        WSADATA data;
        if (WSAStartup(MAKEWORD(1, 1), &data) != 0) {
            return FALSE;
        }
        return TRUE;
    }

    bool Send(const char* pData, int nSize) {    //底层发送方法：直接调用socket::send发送字节流
        if (m_sock == -1) return false;
        return send(m_sock, pData, nSize, 0) > 0;
    }

    bool Send(const CPacket& pack);   // 底层发送方法：发送标准化CPacket数据包

    void SendPack(UINT nMsg, WPARAM wParam/*缓冲区的值*/, LPARAM lParam/*缓冲区的长度*/);   //核心消息处理函数：处理WM_SEND_PACK消息，执行实际的数据包发送 + 接收应答
    
    static void releaseInstance() {
        TRACE("CClientSocket has been called!\r\n");
        if (m_instance != NULL) {
            CClientSocket* tmp = m_instance;
            m_instance = NULL;
            delete tmp;
            TRACE("CClientSocket has released!\r\n");
        }
    }
    static CClientSocket* m_instance;
    class CHelper {
    public:
        CHelper() {
            CClientSocket::getInstance();
        }
        ~CHelper() {
            CClientSocket::releaseInstance();
        }
    };
    static CHelper m_helper;  // 全局辅助对象，程序启动时创建，退出时析构
};



