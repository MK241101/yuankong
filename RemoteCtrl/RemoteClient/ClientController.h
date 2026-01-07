#pragma once
#include "ClientSocket.h"
#include "CWatchDialog.h"
#include "RemoteClientDlg.h"
#include "StatusDlg.h"
#include <map>
#include"resource.h"
#include"EdoyunTool.h"

#define WM_SHOW_STATUS (WM_USER + 3)  //展示状态
#define WM_SHOW_WATCH (WM_USER + 4)  //远程监控
#define WM_SEND_MESSAGE (WM_USER + 0x1000)  //通用消息转发


class CClientController
{

public:
	static CClientController* getInstance();
	int InitController();            //创建工作线程
	int Invoke(CWnd* pMainWin);     //启动客户端主对话框

	void UpdateAddress(int nIP, int nPort) {
		CClientSocket::getInstance()->UpdateAddress(nIP, nPort);

	}

	int DealCommand() {
		return CClientSocket::getInstance()->DealCommand();

	}

	void CloseSocket() {
		CClientSocket::getInstance()->CloseSocket();

	}

	

	//1 查看磁盘分区
	//2 查看指定目录下的文件
	//3 打开文件
	//4 下载文件
	//5 鼠标操作
	//6 发送屏幕内容
	//7 锁机
	//8 解锁
	//9 删除文件
	//1981 测试连接
	//返回值：是状态 
	bool SendCommandPacket(HWND hWnd, int nCmd, bool bAutoClose = true, BYTE* pData = NULL, size_t nLength = 0,WPARAM wParam=0);
	
	int GetImage(CImage& image) {
		CClientSocket* pClient = CClientSocket::getInstance();
		return CEdoyunTool::Bytes2Image(image, pClient->GetPacket().strData);

	}

	void DownloadEnd();

	int DownFile(CString strPath);

	void StartWatchScreen();

protected:

	void threadWatchScreen();
	static void threadWatchEntry(void* arg);


	CClientController():
		m_statusDlg(&m_remoteDlg), m_watchDlg(&m_remoteDlg) 
	{
		m_isClosed = true;
		m_hThreadWatch = INVALID_HANDLE_VALUE;
		m_hThread = INVALID_HANDLE_VALUE; // 初始化线程句柄为无效
		m_nThreadID = -1;                 // 初始化线程ID为-1
	}
		
	~CClientController() {WaitForSingleObject(m_hThread, 100);}  //等待工作线程退出，避免线程泄漏

	void threadFunc();           //工作线程的核心函数：消息循环 + 消息处理

	static unsigned __stdcall threadEntry(void* arg);  //工作线程入口函数
	
	static void releaseInstance() {
		if (m_instance != NULL) {
			delete m_instance;
			TRACE("ClientController released!!!\r\n");
			m_instance = NULL;
		}
	}

	//消息处理回调函数
	
	LRESULT OnShowStatus(UINT nMsg, WPARAM wParam, LPARAM lParam);  // 处理“展示状态”消息
	LRESULT OnShowWatcher(UINT nMsg, WPARAM wParam, LPARAM lParam); // 处理“展示监控”消息

private:
	typedef struct MsgInfo{     // 消息信息结构体：封装MSG、处理结果、事件（用于同步）
		MSG msg;                // 原始消息
		LRESULT result;			// 处理结果
		HANDLE hEvent;			// 同步事件（用于等待处理完成）
		MsgInfo(MSG m) {
			result = 0;
			memcpy(&msg, &m, sizeof(MSG));
		}

		MsgInfo(const MsgInfo& m) {
			result = m.result;
			memcpy(&msg, &m.msg, sizeof(MSG));
		}

		MsgInfo& operator=(const MsgInfo& m) {
			if (this != &m) {
				result = m.result;
				memcpy(&msg, &m.msg, sizeof(MSG));
			}
			return *this;
		}
	}MSGINFO;

	typedef LRESULT (CClientController::* MSGFUNC)(UINT nMsg, WPARAM wParam, LPARAM lParam);  //匹配消息处理函数的签名
	static std::map<UINT, MSGFUNC> m_mapFunc;  //消息处理函数映射表
	CWatchDialog m_watchDlg;      // 远程监控对话框
	CRemoteClientDlg m_remoteDlg; // 远程客户端主对话框
	CStatusDlg m_statusDlg;       // 状态展示对话框

	HANDLE m_hThread;            // 工作线程句柄
	unsigned m_nThreadID;        // 工作线程ID

	HANDLE m_hThreadWatch;
	bool m_isClosed; //监视是否关闭

	CString m_strRemote;   //下载文件的远程路径
	CString m_strLocal;    // 下载文件的本地路径

	static CClientController* m_instance;

	class CHelper {              //确保程序退出时销毁单例
	public:
		CHelper() {}
		~CHelper() { CClientController::releaseInstance(); }
	};

	static CHelper m_helper;



};

