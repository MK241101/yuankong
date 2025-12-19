#pragma once

#include "pch.h"
#include "framework.h"


class CServerSocket
{
public:
    CServerSocket() {
        if (InitSockEnv() == FALSE) {
			MessageBox(NULL, _T("初始化Socket环境失败"), _T("初始化错误"), MB_OK | MB_ICONERROR);
            exit(0);
        }
    };
    ~CServerSocket() {
        WSACleanup();     // 清理 Winsock 库
    
    };
    
    BOOL InitSockEnv() {
        WSADATA data;
        if (WSAStartup(MAKEWORD(1, 1), &data) != 0) {
            return FALSE; 
        }  
        return TRUE;          // 成功初始化
    
    }

private:
    
};


extern CServerSocket server;

