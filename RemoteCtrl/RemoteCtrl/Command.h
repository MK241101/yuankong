#pragma once
#include <map>
#include "ServerSocket.h"
#include <atlimage.h>
#include <direct.h>
#include "EdoyunTool.h"
#include <stdio.h>
#include <io.h>
#include "LockDialog.h"
#include "Resource.h"
class CCommand
{
public:
	CCommand();
	~CCommand();
	int ExcuteCommand(int nCmd);

protected:
	typedef int(CCommand::* CMDFUNC)();   //成员函数指针
	std::map<int, CMDFUNC> m_mapFunction;
    CLockDialog dlg;
    unsigned threadid;

protected:
    static unsigned __stdcall threadLockDlg(void* arg){
        CCommand* thiz=(CCommand*)arg;
        thiz->threadLockDlgMain();
        _endthreadex(0);
        return 0;
    }

    void threadLockDlgMain() {
        TRACE("%s(%d):%d\r\n", __FUNCTION__, __LINE__, GetCurrentThreadId());
        dlg.Create(IDD_DIALOG_INFO, NULL);  //创建非模态对话框
        dlg.ShowWindow(SW_SHOW);

        CRect rect;
        rect.left = 0;
        rect.right = GetSystemMetrics(SM_CXFULLSCREEN);
        rect.top = 0;
        rect.bottom = GetSystemMetrics(SM_CYFULLSCREEN);
        rect.bottom = LONG(rect.bottom * 1.10);

        dlg.MoveWindow(rect);  //设置对话框大小
        CWnd* pText = dlg.GetDlgItem(IDC_STATIC);
        if (pText) {
            CRect rtText;
            pText->GetWindowRect(rtText);
            int nWidth = rtText.Width();
            int x = (rect.right - nWidth) / 2;
            int nHeight = rtText.Height();
            int y = (rect.bottom - nHeight) / 2;
            pText->MoveWindow(x, y, rtText.Width(), rtText.Height());
        }

        dlg.SetWindowPos(&dlg.wndTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);  //设置对话框为置顶窗口（始终在所有窗口最上层）

        ::ShowWindow(::FindWindow(_T("Shell_TrayWnd"), NULL), SW_HIDE);  //隐藏任务栏

        ClipCursor(rect);  //限制鼠标活动范围，使鼠标只能在对话框内活动

        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {  //自定义消息循环：持续获取并处理系统消息，直到ESC键被按下
            TranslateMessage(&msg);   // 翻译虚拟键消息（如将WM_KEYDOWN转为字符消息）
            DispatchMessage(&msg);    //将消息分发到对应的窗口过程函数
            if (msg.message == WM_KEYDOWN) {
                if (msg.wParam == VK_ESCAPE) {   //监听键盘按下消息，判断是否为ESC键
                    break;
                }
            }
        }
        ClipCursor(NULL);
        ::ShowWindow(::FindWindow(_T("Shell_TrayWnd"), NULL), SW_SHOW);  //恢复任务栏
        dlg.DestroyWindow();

    
    }

    int MakeDriverInfo(){
        std::string result;
        for (int i = 1; i <= 26; i++) {
            if (_chdrive(i) == 0) {
                if (result.size() > 0)
                    result += ',';
                result += 'A' + i - 1;


            }
        }
        CPacket pack(1, (BYTE*)result.c_str(), result.size());
        CEdoyunTool::Dump((BYTE*)pack.Data(), pack.Size());
        CServerSocket::getInstance()->Send(pack);

        return 0;
    }

    int MakeDirectoryInfo() {
        std::string strPath;  //存储要查询的目标目录路径

        if (CServerSocket::getInstance()->GetFilePath(strPath) == false) {
            OutputDebugString(_T("当前的命令，不是获取文件列表，命令解析错误！！"));
            return -1;
        }

        if (_chdir(strPath.c_str()) != 0) {
            FILEINFO finfo;
            finfo.HasNext = FALSE;

            CPacket pack(2, (BYTE*)&finfo, sizeof(finfo));
            CServerSocket::getInstance()->Send(pack);
            OutputDebugString(_T("没有权限访问目录！！"));
            return -2;
        }

        _finddata_t fdata;       //遍历目录下的文件 / 文件夹
        int hfind = 0;
        if ((hfind = _findfirst("*", &fdata)) == -1) {
            OutputDebugString(_T("没有找到任何文件！！"));
            FILEINFO finfo;           //发送 “遍历结束” 标记
            finfo.HasNext = FALSE;
            CPacket pack(2, (BYTE*)&finfo, sizeof(finfo));
            CServerSocket::getInstance()->Send(pack);
            return -3;
        }

        int count = 0;
        do {                  //循环发送所有文件 / 文件夹信息
            FILEINFO finfo;
            finfo.IsDirectory = (fdata.attrib & _A_SUBDIR) != 0;
            memcpy(finfo.szFileName, fdata.name, strlen(fdata.name));
            TRACE("%s\r\n", finfo.szFileName);
            CPacket pack(2, (BYTE*)&finfo, sizeof(finfo));
            CServerSocket::getInstance()->Send(pack);
            count++;

        } while (!_findnext(hfind, &fdata));
        TRACE("server: count=%d\r\n", count);

        FILEINFO finfo;           //发送 “遍历结束” 标记
        finfo.HasNext = FALSE;
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
        CPacket head(4, (BYTE*)&data, 8);  //构造 “文件大小数据包”
        CServerSocket::getInstance()->Send(head);
        fseek(pFile, 0, SEEK_SET);  //把文件的读取指针移到文件开头（目的是从文件开头开始读取文件）

        char buffer[1024] = "";    //分块读取并发送文件内容
        size_t rlen = 0;
        do {
            rlen = fread(buffer, 1, 1024, pFile);
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

            TRACE("mouse event:%08X x:%d y:%d\r\n", nFlags, mouse.ptXY.x, mouse.ptXY.y);

            switch (nFlags)
            {
            case 0x11://左键单击
                mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
                mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
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
                mouse_event(MOUSEEVENTF_MOVE, mouse.ptXY.x, mouse.ptXY.y, 0, GetMessageExtraInfo());
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
        HDC hScreen = ::GetDC(NULL);   //获取整个屏幕的设备上下文
        int nBitPerPixel = GetDeviceCaps(hScreen, BITSPIXEL);
        int nWidth = GetDeviceCaps(hScreen, HORZRES);
        int nHeight = GetDeviceCaps(hScreen, VERTRES);
        screen.Create(nWidth, nHeight, nBitPerPixel);  //创建和屏幕尺寸、颜色深度一致的图像对象，用于存放截图

        BitBlt(screen.GetDC(), 0, 0, nWidth, nHeight, hScreen, 0, 0, SRCCOPY); //将屏幕DC的内容拷贝到screen的DC中
        ReleaseDC(NULL, hScreen);

        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, 0);  //分配可移动的全局内存块（用于存储PNG格式的图像数据）
        if (hMem == NULL) { return -1; }

        IStream* pStream = NULL;     // 内存流对象，用于将图像保存为PNG
        HRESULT ret = CreateStreamOnHGlobal(hMem, TRUE, &pStream);   //创建基于全局内存的流对象（后续将图像写入该流）
        if (ret == S_OK) {
            screen.Save(pStream, Gdiplus::ImageFormatPNG);  // 将screen中的截图保存到内存流中，流的 “读取指针” 会停在数据末尾

            LARGE_INTEGER bg = { 0 };  // 将流的读取指针重置到起始位置（准备读取流中的PNG数据）
            pStream->Seek(bg, STREAM_SEEK_SET, NULL);

            PBYTE pData = (PBYTE)GlobalLock(hMem);  // 锁定全局内存块，获取PNG数据的起始指针
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

    int LockMachine() {
        if ((dlg.m_hWnd == NULL) || (dlg.m_hWnd == INVALID_HANDLE_VALUE)) {
            _beginthreadex(NULL, 0, &CCommand::threadLockDlg, this, 0, &threadid);
            TRACE("Thread ID: %d\n", threadid);

        }
        CPacket pack(7, NULL, 0);
        CServerSocket::getInstance()->Send(pack);
        return 0;

    }

    int UnlockMachine()
    {
        //dlg.SendMessage(WM_KEYDOWN, VK_ESCAPE, 0x01E0001);  
        //::SendMessage(dlg.m_hWnd, WM_KEYDOWN, VK_ESCAPE, 0x01E0001);

        /*
        * 解锁的时候，上面两种方法都不可行，必须要根据线程ID发送消息，并不是说根据窗口句柄发送消息
        * 因为MFC有基类，基类本质是一个线程，当使用上面的那两种方法的时候，没有对应的线程，所有MFC窗口不会响应
        */
        PostThreadMessage(threadid, WM_KEYDOWN, VK_ESCAPE, 0);  //向线程发送消息，模拟按下ESC键
        CPacket pack(8, NULL, 0);
        CServerSocket::getInstance()->Send(pack);
        return 0;
    }

    int TestConnect() {
        CPacket pack(1981, NULL, 0);
        bool ret = CServerSocket::getInstance()->Send(pack);
        TRACE("Send ret=%d\r\n", ret);
        return 0;
    }

    int DeleteLocalFile() {

        std::string strPath;     //存储要下载的文件路径
        CServerSocket::getInstance()->GetFilePath(strPath);
        TCHAR sPath[MAX_PATH] = _T("");
        MultiByteToWideChar(CP_ACP, 0, strPath.c_str(), strPath.size(), sPath, sizeof(sPath) / sizeof(TCHAR));
        DeleteFileA(strPath.c_str());
        CPacket pack(9, NULL, 0);
        bool ret = CServerSocket::getInstance()->Send(pack);
        TRACE("Send ret=%d\r\n", ret);

        return 0;

    }
};

