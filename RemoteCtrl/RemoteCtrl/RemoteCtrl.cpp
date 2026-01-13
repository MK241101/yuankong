// RemoteCtrl.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include "framework.h"
#include "RemoteCtrl.h"
#include "ServerSocket.h"
#include <atlimage.h>
#include "Command.h"
#include <conio.h>
#include "CEdoyunQueue.h"
#include <MSWSock.h>
#include "EdoyunServer.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define INVOKE_PATH _T("C:\\Users\\MK\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\RemoteCtrl.exe")

// 唯一的应用程序对象

CWinApp theApp;
using namespace std;

bool ChooseAutoInvoke(const CString& strPath) {
    TCHAR wcsSystem[MAX_PATH] = _T("");
    
    if (PathFileExists(strPath)) { return true; }

    CString strInfo = _T("该程序只允许用于合法的用途！\n");
    strInfo += _T("继续运行该程序，将使得这台机器处于被监控状态！\n");
    strInfo += _T("如果你不希望这样，请按“取消”按钮，退出程序。\n");
    strInfo += _T("按下“是”按钮，该程序将被复制到你的机器上，并随系统启动而自动运行！\n");
    strInfo += _T("按下“否”按钮，程序只运行一次，不会在系统内留下任何东西！\n");
    int ret = MessageBox(NULL, strInfo, _T("警告"), MB_YESNOCANCEL | MB_ICONWARNING | MB_TOPMOST);
    if (ret == IDYES) {
        //WriteRegisterTable(strPath);

        if (!CEdoyunTool::WriteStartupDir(strPath)) {
            MessageBox(NULL, _T("复制文件失败，是否权限不足？\r\n"), _T("错误"), MB_ICONERROR | MB_TOPMOST);
            return false;
        
        }
    }
    else if (ret == IDCANCEL) {
        return false;
    }
    return true;
}

void test() {

    //printf("按任意键退出程序！\r\n");

    CEdoyunQueue<std::string> lstStrings;    //实例化：存储string类型的线程安全队列
    ULONGLONG tick0 = GetTickCount64(), tick = GetTickCount64(), total= GetTickCount64();

    while (GetTickCount64()-total <= 1000) { 
        if (GetTickCount64() - tick0 > 13) 
        {
            lstStrings.PushBack("hello world");       //每1300毫秒 执行一次【入队】：向队列中添加字符串"hello world"
            tick0 = GetTickCount64();
        }
        if (GetTickCount64() - tick > 20) 
        {
            std::string str;
            lstStrings.PopFront(str);                 // 每2000毫秒 执行一次【出队】：从队列中取出元素并打印
            tick = GetTickCount64();
            printf("pop from queue:%s\r\n", str.c_str());
        }
        Sleep(1);
    }

    printf("exit done!size %d\r\n", lstStrings.Size());
    lstStrings.Clear();
    printf("exit done!size %d\r\n", lstStrings.Size());
}

class COverlapped {
public:
    OVERLAPPED m_overlapped;
    DWORD m_operator;
    char m_buffer[4096];
    COverlapped() {
        m_operator = 0;
        memset(&m_overlapped, 0, sizeof(m_overlapped));
        memset(m_buffer, 0, sizeof(m_buffer));
    }
};

/**/
void iocp() {
    EdoyunServer server;
    server.StartService();
    getchar();
}


int main()
{
    if (!CEdoyunTool::Init())return 1;

    

    

    //::exit(0);

    return 0;
    /*
    if (CEdoyunTool::IsAdmin()) {
        if (!CEdoyunTool::Init())return 1;

        //if(!ChooseAutoInvoke(INVOKE_PATH)){::exit(0)};    //开机自启动（有bug）
       
        CCommand cmd;
        int ret = CServerSocket::getInstance()->Run(&CCommand::RunCommand, &cmd);
        switch (ret) {
        case -1:
            MessageBox(NULL, _T("无法正常接入用户，结束程序！"), _T("接入用户失败！"), MB_OK | MB_ICONERROR);
            break;

        case -2:
            MessageBox(NULL, _T("多次无法正常接入用户，结束程序！"), _T("接入用户失败！"), MB_OK | MB_ICONERROR);
            break;

        }
    }
    else {
        if (CEdoyunTool::RunAsAdmin() == false) { 
            CEdoyunTool::ShowError(); 
            return 1;
        };
        MessageBox(NULL, _T("提权成功,当前程序以管理员权限运行！！"), _T("用户状态"), 0);
    }
    return 0;
    */
}
