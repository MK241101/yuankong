// RemoteCtrl.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include "framework.h"
#include "RemoteCtrl.h"
#include "ServerSocket.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 唯一的应用程序对象

CWinApp theApp;

using namespace std;

int main()
{
    int nRetCode = 0;

    HMODULE hModule = ::GetModuleHandle(nullptr);

    if (hModule != nullptr)
    {
        // 初始化 MFC 并在失败时显示错误888999
        if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0))
        {
            // TODO: 在此处为应用程序的行为编写代码。
            wprintf(L"错误: MFC 初始化失败\n");
            nRetCode = 1;
        }
        else
        {
           // server;
            
            SOCKET serv_sock= socket(PF_INET, SOCK_STREAM, 0);

			sockaddr_in serv_addr,client_adr;
			memset(&serv_addr, 0, sizeof(serv_addr));
			serv_addr.sin_family = AF_INET;
			serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
			serv_addr.sin_port = htons(9527);
			
			bind(serv_sock, (sockaddr*)&serv_addr, sizeof(serv_addr));
			listen(serv_sock, 1);

			char buffer[1024];

			int cli_sz = sizeof(client_adr);
			//SCOKET client=accept(serv_sock, (sockaddr*)&client_adr, &cli_sz);
            //recv(client, buffer, sizeof(buffer), 0);
			//send(client, buffer, sizeof(buffer), 0);

			closesocket(serv_sock);
			
        }
    }
    else
    {
        // TODO: 更改错误代码以符合需要
        wprintf(L"错误: GetModuleHandle 失败\n");
        nRetCode = 1;
    }

    return nRetCode;
}
