// RemoteCtrl.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include "framework.h"
#include "RemoteCtrl.h"
#include "ServerSocket.h"
#include <atlimage.h>
#include "Command.h"
#include<conio.h>


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

#define IOCP_LIST_PUSH 1
#define IOCP_LIST_POP 2
#define IOCP_LIST_EMPTY 0

// 定义IOCP的三种操作指令枚举
enum {
    IocpListEmpty,  // 指令：清空字符串列表  0
    IocpListPush,   // 指令：添加字符串数据  1
    IocpListPop     // 指令：取出字符串数据  2
};


// IOCP投递的「任务参数结构体」，所有要执行的任务数据都存在这里
typedef struct IocpParam {
    int nOperator;                    // 核心：要执行的操作类型（对应上面的枚举）
    std::string strData;              // 操作携带的字符串数据
    _beginthread_proc_type cbFunc;    // 回调函数：执行完Pop操作后，调用此函数处理结果

    IocpParam(int op, const char* sData, _beginthread_proc_type cb=NULL) {
        nOperator = op;
        strData = sData;
        cbFunc = cb;
    }
    IocpParam() {
        nOperator = -1;
    }
}IOCP_PARAM;


// IOCP的【工作线程函数】：线程的核心执行逻辑，阻塞等待并处理投递过来的所有任务
void threadQueueEntry(HANDLE hIOCP) {
    
    std::list<std::string> lstString;   // 工作线程内部维护的一个字符串链表：存储所有Push进来的字符串
    DWORD dwTransferred = 0;            // 传输字节数（本代码中无实际意义，是IOCP的标准参数）
    ULONG_PTR CompletionKey = 0;        // 完成键【核心】：存放我们的IOCP_PARAM任务结构体指针
    OVERLAPPED* pOverlapped = NULL;     // 重叠结构指针（本代码中无实际意义，标准IOCP用于异步I/O，这里传NULL）
    while (GetQueuedCompletionStatus(hIOCP, &dwTransferred, &CompletionKey, &pOverlapped, INFINITE)) {     // 死循环,在这里阻塞,从 IOCP 队列取任务
        if ((dwTransferred == 0) || (CompletionKey == NULL)) {
            printf("线程准备退出！\r\n");
            break;
        }
        IOCP_PARAM* pParam = (IOCP_PARAM*)CompletionKey;     // 把完成键强转回我们的任务结构体指针，拿到本次要执行的任务
        if (pParam->nOperator == IocpListPush) {
            lstString.push_back(pParam->strData);            // 把任务中的字符串添加到链表尾部
        
        }
        else if (pParam->nOperator == IocpListPop) {
            std::string* pStr = NULL;

            if (lstString.size() > 0) {
                pStr = new std::string(lstString.front());    // 如果链表不为空，取出头部第一个字符串，用堆内存保存结果
                lstString.pop_front();
            }
            if (pParam->cbFunc) {                             // 如果设置了回调函数，调用回调函数处理取出的结果
                pParam->cbFunc(pStr);
            }

        }
        else if (pParam->nOperator == IocpListEmpty) {
            lstString.clear();
        
        }
        delete pParam;
    }
    _endthread();
}


// Pop操作的【回调处理函数】：处理从链表中取出的字符串数据
void func(void* arg) {
    std::string* pstr=(std::string*)arg;
    if (pstr == NULL) {
        printf("pop from list:%s\r\n", pstr->c_str());
        delete pstr;
    }
    else {
    
    
    }
    
}




int main()
{
    if (!CEdoyunTool::Init())return 1;
    printf("按任意键退出程序！\r\n");

    HANDLE hIOCP = INVALID_HANDLE_VALUE; //   IOCP
    hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 1);  //创建IOCP完成端口
    HANDLE hThread=(HANDLE)_beginthread(threadQueueEntry, 0, hIOCP);   //创建工作线程
    
    ULONGLONG tick = GetTickCount64();             // 记录初始时间戳，用于定时投递任务
    while (_kbhit() == 0) {                        // 死循环：只要【没有按下任意键】，就持续运行投递任务
        if (GetTickCount64() - tick > 1300) {      // 每1300毫秒，投递【POP取数据】任务到IOCP队列
            PostQueuedCompletionStatus(hIOCP, sizeof(IOCP_PARAM), (ULONG_PTR)new IOCP_PARAM(IocpListPop, "hello world"), NULL);
        
        }
        if (GetTickCount64() - tick > 2000) {      // 每2000毫秒，投递【PUSH加数据】任务到IOCP队列，并重置时间戳
            PostQueuedCompletionStatus(hIOCP, sizeof(IOCP_PARAM), (ULONG_PTR)new IOCP_PARAM(IocpListPush, "hello world"), NULL);
            tick = GetTickCount64();
        }
        Sleep(1);     // 让出CPU，避免主线程空转占用100%CPU
    
    }
    if (hIOCP != NULL) {    //程序退出
        PostQueuedCompletionStatus(hIOCP, 0, NULL, NULL);
        WaitForSingleObject(hThread, INFINITE);      //等待线程结束
    }
    CloseHandle(hIOCP);

    printf("执行完成!!!\r\n");
    ::exit(0);
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
