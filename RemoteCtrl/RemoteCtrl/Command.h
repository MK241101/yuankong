#pragma once
#include <map>
#include "Packet.h"
#include <atlimage.h>
#include <direct.h>
#include "EdoyunTool.h"
#include <stdio.h>
#include <io.h>
#include<list>
#include "LockDialog.h"
#include "Resource.h"
class CCommand
{
public:
	CCommand();
	~CCommand();
	int ExcuteCommand(int nCmd, std::list<CPacket>& lstPacket,CPacket& inPacket);
    static void RunCommand(void* arg, int status, std::list<CPacket>& lstPacket, CPacket& inPacket) {
        CCommand* thiz=(CCommand*)arg;
        if (status > 0) {
            int ret = thiz->ExcuteCommand(status, lstPacket,inPacket);
            if (ret != 0) { TRACE("执行命令失败，%d ret=%d\r\n", status,ret); }
        }
        else {
            MessageBox(NULL, _T("网络初始化异常，未能成功初始，请检查网络状态！"), _T("网络初始化失败"), MB_OK | MB_ICONERROR);
        }
    }

protected:
	typedef int(CCommand::* CMDFUNC)(std::list<CPacket>&, CPacket& inPacket);   //成员函数指针
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

    int MakeDriverInfo(std::list<CPacket>& lstPacket, CPacket& inPacket){
        std::string result;
        for (int i = 1; i <= 26; i++) {
            if (_chdrive(i) == 0) {
                if (result.size() > 0)
                    result += ',';
                result += 'A' + i - 1;


            }
        }
        lstPacket.push_back(CPacket(1, (BYTE*)result.c_str(), result.size()));
        return 0;
    }

    int MakeDirectoryInfo(std::list<CPacket>& lstPacket, CPacket& inPacket) {
        std::string strPath=inPacket.strData;  //存储要查询的目标目录路径

        if (_chdir(strPath.c_str()) != 0) {
            FILEINFO finfo;
            finfo.HasNext = FALSE;
            lstPacket.push_back(CPacket(2, (BYTE*)&finfo, sizeof(finfo)));
            OutputDebugString(_T("没有权限访问目录！！"));
            return -2;
        }

        _finddata_t fdata;       //遍历目录下的文件 / 文件夹
        int hfind = 0;
        if ((hfind = _findfirst("*", &fdata)) == -1) {
            OutputDebugString(_T("没有找到任何文件！！"));
            FILEINFO finfo;           //发送 “遍历结束” 标记
            finfo.HasNext = FALSE;
            lstPacket.push_back(CPacket(2, (BYTE*)&finfo, sizeof(finfo)));

            return -3;
        }

        int count = 0;
        do {                  //循环发送所有文件 / 文件夹信息
            FILEINFO finfo;
            finfo.IsDirectory = (fdata.attrib & _A_SUBDIR) != 0;
            memcpy(finfo.szFileName, fdata.name, strlen(fdata.name));
            TRACE("%s\r\n", finfo.szFileName);
            lstPacket.push_back(CPacket(2, (BYTE*)&finfo, sizeof(finfo)));
            count++;
        } while (!_findnext(hfind, &fdata));
        TRACE("server: count=%d\r\n", count);

        FILEINFO finfo;           //发送 “遍历结束” 标记
        finfo.HasNext = FALSE;
        lstPacket.push_back(CPacket(2, (BYTE*)&finfo, sizeof(finfo)));
        return 0;
    }

    int RunFile(std::list<CPacket>& lstPacket, CPacket& inPacket) {
        std::string strPath = inPacket.strData;
        ShellExecuteA(NULL, NULL, strPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
        lstPacket.push_back(CPacket(3, NULL, 0));
        return 0;
    }

    int DownloadFile(std::list<CPacket>& lstPacket, CPacket& inPacket) {
        std::string strPath = inPacket.strData;
        long long data = 0;
        FILE* pFile = NULL;
        errno_t err = fopen_s(&pFile, strPath.c_str(), "rb");
        if (err != 0) {
            lstPacket.push_back(CPacket(4, (BYTE*)&data, 8));
            return -1;
        }
        if (pFile != NULL) {
            fseek(pFile, 0, SEEK_END);
            data = _ftelli64(pFile);
            lstPacket.push_back(CPacket(4, (BYTE*)&data, 8));
            fseek(pFile, 0, SEEK_SET);
            char buffer[1024] = "";
            size_t rlen = 0;
            do {
                rlen = fread(buffer, 1, 1024, pFile);
                lstPacket.push_back(CPacket(4, (BYTE*)buffer, rlen));
            } while (rlen >= 1024);
            fclose(pFile);
        }
        else {
            lstPacket.push_back(CPacket(4, (BYTE*)&data, 8));
        }
        
        return 0;
    }

    int MouseEvent(std::list<CPacket>& lstPacket, CPacket& inPacket) {
        MOUSEEV mouse;
        memcpy(&mouse, inPacket.strData.c_str(), sizeof(MOUSEEV));
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
        default:
            break;
        }
        lstPacket.push_back(CPacket(5, NULL, 0));
        return 0;
    }

    int SendScreen(std::list<CPacket>& lstPacket, CPacket& inPacket) {
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

            lstPacket.push_back(CPacket(6, pData, nSize));
            GlobalUnlock(hMem);
        }
        pStream->Release();  //释放资源
        GlobalFree(hMem);
        screen.ReleaseDC();
        return 0;
    }

    int LockMachine(std::list<CPacket>& lstPacket, CPacket& inPacket) {
        if ((dlg.m_hWnd == NULL) || (dlg.m_hWnd == INVALID_HANDLE_VALUE)) {
            _beginthreadex(NULL, 0, &CCommand::threadLockDlg, this, 0, &threadid);
            TRACE("Thread ID: %d\n", threadid);

        }
        lstPacket.push_back(CPacket(7, NULL, 0));
        return 0;

    }

    int UnlockMachine(std::list<CPacket>& lstPacket, CPacket& inPacket)
    {
        //dlg.SendMessage(WM_KEYDOWN, VK_ESCAPE, 0x01E0001);  
        //::SendMessage(dlg.m_hWnd, WM_KEYDOWN, VK_ESCAPE, 0x01E0001);

        /*
        * 解锁的时候，上面两种方法都不可行，必须要根据线程ID发送消息，并不是说根据窗口句柄发送消息
        * 因为MFC有基类，基类本质是一个线程，当使用上面的那两种方法的时候，没有对应的线程，所有MFC窗口不会响应
        */
        PostThreadMessage(threadid, WM_KEYDOWN, VK_ESCAPE, 0);  //向线程发送消息，模拟按下ESC键
        lstPacket.push_back(CPacket(8, NULL, 0));
        return 0;
    }

    int TestConnect(std::list<CPacket>& lstPacket, CPacket& inPacket) {
        lstPacket.push_back(CPacket(1981, NULL, 0));
 
        return 0;
    }

    int DeleteLocalFile(std::list<CPacket>& lstPacket, CPacket& inPacket) {

        std::string strPath = inPacket.strData;

        TCHAR sPath[MAX_PATH] = _T("");
        MultiByteToWideChar(CP_ACP, 0, strPath.c_str(), strPath.size(), sPath, sizeof(sPath) / sizeof(TCHAR));
        DeleteFileA(strPath.c_str());
        lstPacket.push_back(CPacket(9, NULL, 0));

        return 0;

    }
};

