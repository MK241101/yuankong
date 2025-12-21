// RemoteCtrl.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include "framework.h"
#include "RemoteCtrl.h"
#include "ServerSocket.h"
#include <direct.h>
#include <io.h>
#include <atlimage.h>
#include "LockDialog.h"



#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 唯一的应用程序对象

CWinApp theApp;

using namespace std;

void Dump(BYTE* pData, size_t nSize)
{
    std::string strOut;
    for (size_t i = 0; i < nSize; i++)
    {
        char buf[8] = "";
        if (i > 0 && (i % 16 == 0)) strOut += "\n";
        snprintf(buf, sizeof(buf), "%02X ", pData[i] & 0xFF);
        strOut += buf;
    }
    strOut += "\n";
    OutputDebugStringA(strOut.c_str());
}

int MakeDriverInfo() {
    std::string result;
    for (int i = 1; i <= 26; i++) {
        if (_chdrive(i) == 0) {
            if (result.size() > 0)
                result += ',';
            result += 'A' + i - 1;
        }
    }
    CPacket pack(1, (BYTE*)result.c_str(), result.size());
    Dump((BYTE*)pack.Data(), pack.Size());
    //CServerSocket::getInstance()->Send(pack);

    return 0;
}

typedef struct file_info{
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

}FILEINFO, *PFILEINFO;

int MakeDirectoryInfo() {
    std::string strPath;  //存储要查询的目标目录路径
    
    if (CServerSocket::getInstance()->GetFilePath(strPath) == false) {
        OutputDebugString(_T("当前的命令，不是获取文件列表，命令解析错误！！"));
        return -1;
    }

    if (_chdir(strPath.c_str()) != 0) {
        FILEINFO finfo;
        finfo.IsInvalid = TRUE;
        finfo.IsDirectory = TRUE;
        finfo.HasNext = FALSE;
        memcpy(finfo.szFileName, strPath.c_str(), strPath.size());  //把目标目录路径拷贝到FILEINFO的文件名字段；
        
        CPacket pack(2, (BYTE*)&finfo, sizeof(finfo));
        CServerSocket::getInstance()->Send(pack);
        OutputDebugString(_T("没有权限访问目录！！"));
        return -2;
    }

    _finddata_t fdata;       //遍历目录下的文件 / 文件夹
    int hfind = 0;
    if ((hfind = _findfirst("*", &fdata)) == -1) {
        OutputDebugString(_T("没有找到任何文件！！"));
        return -3;
    }
     
    do {                  //循环发送所有文件 / 文件夹信息
        FILEINFO finfo;
        finfo.IsDirectory = (fdata.attrib & _A_SUBDIR)!=0;
        memcpy(finfo.szFileName,fdata.name, strlen(fdata.name));
        CPacket pack(2, (BYTE*)&finfo, sizeof(finfo));
        CServerSocket::getInstance()->Send(pack);

    } while (!_findnext(hfind, &fdata));

    FILEINFO finfo;           //发送 “遍历结束” 标记
    finfo.HasNext=FALSE;
    CPacket pack(2, (BYTE*)&finfo, sizeof(finfo));
    CServerSocket::getInstance()->Send(pack);

    return 0;
}


int RunFile() {
    std::string strPath;
    CServerSocket::getInstance()->GetFilePath(strPath);
    ShellExecuteA(NULL, NULL, strPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
    CPacket pack(3, NULL, 0);
    CServerSocket::getInstance()->Send(pack);
    return 0;
}

int DownloadFile() {
    std::string strPath;   //存储要下载的文件路径
    CServerSocket::getInstance()->GetFilePath(strPath);

    long long data = 0;   //打开文件并校验
    FILE* pFile = fopen(strPath.c_str(), "rb");
    if (pFile == NULL) {
        CPacket pack(4, (BYTE*)&data, 8);
        CServerSocket::getInstance()->Send(pack);
        return -1;
    }


    fseek(pFile, 0, SEEK_END);  //把文件的读取指针移到文件末尾（目的是获取文件总长度）
    data = _ftelli64(pFile);    //获取当前读取指针的位置
    CPacket head(4,(BYTE*)&data,8);  //构造 “文件大小数据包”
    fseek(pFile, 0, SEEK_SET);  //把文件的读取指针移到文件开头（目的是从文件开头开始读取文件）

    char buffer[1024] = "";    //分块读取并发送文件内容
    size_t rlen = 0;
    do {
        rlen=fread(buffer, 1, 1024, pFile);
        CPacket pack(4, (BYTE*)buffer, rlen);
        CServerSocket::getInstance()->Send(pack);

    } while (rlen >= 1024);
    CPacket pack(4, NULL, 0);
    CServerSocket::getInstance()->Send(pack);
    fclose(pFile);
}

int MouseEvent() {
    MOUSEEV mouse;
    if (CServerSocket::getInstance()->GetMouseEvent(mouse)) {
        DWORD nFlags = 0;

        switch (mouse.nButton) {
        case 0://左键
            nFlags = 1;
            break;
        case 1://右键
            nFlags = 2;
            break;
        case 2://中键
            nFlags = 4;
            break;
        case 4: //没有按键
            nFlags = 8;
            break;
        }
        if (nFlags != 8) { SetCursorPos(mouse.ptXY.x, mouse.ptXY.y); }  //瞬间把鼠标光标定位到屏幕的 (x,y) 坐标位置

        switch (mouse.nAction)
        {
        case 0://单击
            nFlags |= 0x10;
            break;
        case 1://双击
            nFlags |= 0x20;
            break;
        case 2://按下
            nFlags |= 0x40;
            break;
        case 3://放开
            nFlags |= 0x80;
            break;
        default:
                break;
        }

        switch (nFlags)
        {
        case 0x11://左键单击
            mouse_event(MOUSEEVENTF_LEFTDOWN,0,0,0,GetMessageExtraInfo());
            mouse_event(MOUSEEVENTF_LEFTUP,0,0,0,GetMessageExtraInfo());
            break;
        case 0x21://左键双击
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x41://左键按下
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x81://左键放开
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x12://右键单击
            mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
            mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x22://右键双击
            mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
            mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
            mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
            mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x42://右键按下
            mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x82://右键放开
            mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x14://中键单击
            mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
            mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x24://中键双击
            mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
            mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
            mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
            mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x44://中键按下
            mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x84://中键放开
            mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x08://鼠标移动
            mouse_event(MOUSEEVENTF_MOVE, mouse.ptXY.x, mouse.ptXY.y, 0,GetMessageExtraInfo());
            break;
        }

        CPacket pack(5, NULL, 0);
        CServerSocket::getInstance()->Send(pack);
    }
    else {
        OutputDebugString(_T("获取鼠标事件失败！！"));
        return -1;
    }
    return 0;
}

int SendScreen() {
    CImage screen;    //存储屏幕截图的图像
    HDC hScreen= ::GetDC(NULL);   //获取整个屏幕的设备上下文
    int nBitPerPixel=GetDeviceCaps(hScreen, BITSPIXEL);
    int nWidth=GetDeviceCaps(hScreen, HORZRES);
    int nHeight=GetDeviceCaps(hScreen, VERTRES);
    screen.Create(nWidth, nHeight, nBitPerPixel);  //创建和屏幕尺寸、颜色深度一致的图像对象，用于存放截图

    BitBlt(screen.GetDC(),0,0,2560,1550, hScreen,0,0,SRCCOPY); //将屏幕DC的内容拷贝到screen的DC中
    ReleaseDC(NULL, hScreen);

    HGLOBAL hMem= GlobalAlloc(GMEM_MOVEABLE,0);  //分配可移动的全局内存块（用于存储PNG格式的图像数据）
    if (hMem == NULL) {return -1;}

    IStream* pStream = NULL;     // 内存流对象，用于将图像保存为PNG
    HRESULT ret= CreateStreamOnHGlobal(hMem, TRUE, &pStream);   //创建基于全局内存的流对象（后续将图像写入该流）
    if (ret == S_OK)
    {
        screen.Save(pStream, Gdiplus::ImageFormatPNG);  // 将screen中的截图保存到内存流中，流的 “读取指针” 会停在数据末尾

        LARGE_INTEGER bg = { 0 };  // 将流的读取指针重置到起始位置（准备读取流中的PNG数据）
        pStream->Seek(bg,STREAM_SEEK_SET, NULL);

        PBYTE pData=(PBYTE)GlobalLock(hMem);  // 锁定全局内存块，获取PNG数据的起始指针
        SIZE_T nSize = GlobalSize(hMem);  // 获取全局内存块的大小（即PNG图像数据的总字节数）
        CPacket pack(6, pData, nSize);
        CServerSocket::getInstance()->Send(pack);

        GlobalUnlock(hMem);

    }
    pStream->Release();  //释放资源
    GlobalFree(hMem);
    screen.ReleaseDC();
    return 0;
}

CLockDialog dlg;

int LockMachine() {
    dlg.Create(IDD_DIALOG_INFO, NULL);
    dlg.ShowWindow(SW_SHOW);
    dlg.SetWindowPos(&dlg.wndTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if (msg.message == WM_KEYDOWN) {
            if (msg.wParam == VK_ESCAPE) {   //按下ESC键 退出
                break;
            }
        }
    }
    dlg.DestroyWindow();
    return 0;
}

int UnlockMachine() {

    return 0;

}


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
           
            /*CServerSocket* pserver = CServerSocket::getInstance();
            int count = 0;
            if (pserver->InitSocket() == false) {
                MessageBox(NULL, _T("网络初始化异常，未能成功初始hi，请检查网络状态！"), _T("网络初始化失败"), MB_OK | MB_ICONERROR);
                exit(0);
            }
            while (CServerSocket::getInstance() != NULL) {
                if (pserver->AcceptClient() == false) {
                    if (count >= 3) {
                        MessageBox(NULL, _T("多次无法正常接入用户，结束程序！"), _T("接入用户失败！"), MB_OK | MB_ICONERROR);
                        exit(0);
                    }
                    MessageBox(NULL, _T("无法正常接入用户，自动重试"), _T("接入用户失败！"), MB_OK | MB_ICONERROR);
                    count++;
                }
            }
            int ret = pserver->DealCommand();*/


            int nCmd = 7;
            switch (nCmd) {
            case 1://查看磁盘分区
                MakeDriverInfo();
                break;
            case 2://查看指定目录下的文件
                MakeDirectoryInfo();
                break;
            case 3://打开文件
                RunFile();
                break;
            case 4://下载文件
                DownloadFile();
                break;
            case 5:
                MouseEvent();
                break;
            case 6:
                SendScreen();
                break;
            case 7:
                LockMachine();
                break;
            case 8:
                UnlockMachine();
                break;
            }
			
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
