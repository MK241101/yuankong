#pragma once
#include "pch.h"
#include "framework.h"


#pragma pack(push)
#pragma pack(1)


class CPacket   //数据包结构
{
public:
    CPacket() :sHead(0), nLength(0), sCmd(0), sSum(0) {}  //
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
    CPacket(const CPacket& pack) {
        sHead = pack.sHead;
        nLength = pack.nLength;
        sCmd = pack.sCmd;
        strData = pack.strData;
        sSum = pack.sSum;
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

        if (i + 4 + 2 + 2 > nSize) {             //包头+包长度+命令字+校验和最小8字节，包未完整接收，解析失败
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
            i += nLength - 4;
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