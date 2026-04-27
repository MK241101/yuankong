#include "pch.h"
#include "ClientController.h"
#include "ClientSocket.h"

std::map<UINT, CClientController::MSGFUNC> CClientController::m_mapFunc;   //静态映射表：消息ID -> 对应的处理函数
CClientController* CClientController::m_instance = NULL;    //单例实例初始化为空（懒汉式）
CClientController::CHelper CClientController::m_helper;     // 静态帮助类，用于初始化静态成员变量


CClientController* CClientController::getInstance() {
	if (m_instance == NULL) {
		m_instance = new CClientController();   //懒汉式，调用的时候才初始化（高效分配）

		struct { UINT nMsg; MSGFUNC func; }MsgFuncs[] = {    // 构建消息-处理函数映射结构体
			{WM_SHOW_STATUS,&CClientController::OnShowStatus},
			{WM_SHOW_WATCH,&CClientController::OnShowWatcher},
			{(UINT)- 1,NULL}
		};
		for (int i = 0; MsgFuncs[i].func != NULL; i++) {     // 遍历映射结构体，将消息和函数存入m_mapFunc（后续通过消息ID找函数）
			m_mapFunc.insert(std::pair<UINT, MSGFUNC>(MsgFuncs[i].nMsg, MsgFuncs[i].func));
		
		}

	}
	return m_instance;
}

int CClientController::InitController()   // 初始化控制器：创建工作线程（用于异步处理消息，避免阻塞UI）
{
	m_hThread = (HANDLE)_beginthreadex(NULL, 0, &CClientController::threadEntry, this, 0, &m_nThreadID);
	m_statusDlg.Create(IDD_DLG_STATUS, &m_remoteDlg); // 创建非模态的状态对话框（父窗口为主对话框，创建后不显示）
	return 0;
}

int CClientController::Invoke(CWnd*& pMainWin) {   // 启动远程客户端主对话框
	
	pMainWin = &m_remoteDlg;
	return m_remoteDlg.DoModal();

}

bool CClientController::SendCommandPacket(HWND hWnd,int nCmd, bool bAutoClose, BYTE* pData, size_t nLength, WPARAM wParam)
{
	TRACE("cmd=%d ,%s strat %lld\r\n", nCmd, __FUNCTION__, GetTickCount64());

	CClientSocket* pClient = CClientSocket::getInstance(); // 获取网络层单例
	bool ret=pClient->SendPacket(hWnd,CPacket(nCmd, pData, nLength), bAutoClose,wParam);   //调用网络层发送方法
	return ret;
}

inline int CClientController::GetImage(CImage& image) {
	CClientSocket* pClient = CClientSocket::getInstance();
	return CEdoyunTool::Bytes2Image(image, pClient->GetPacket().strData);

}

void CClientController::DownloadEnd()
{
	m_statusDlg.ShowWindow(SW_HIDE);
	m_remoteDlg.EndWaitCursor();
	m_remoteDlg.MessageBox(_T("下载完成"), _T("完成"));
}

int CClientController::DownFile(CString strPath/*strPath为远程文件地址*/)
{
	CFileDialog dlg(FALSE, NULL, strPath, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, NULL, &m_remoteDlg); // 弹出文件保存对话框
	if (dlg.DoModal() == IDOK) {

		m_strRemote = strPath;   // 保存远程文件路径
		m_strLocal = dlg.GetPathName();   //保存本地文件路径

		FILE* pFile = fopen(m_strLocal, "wb+");
		if (pFile == NULL) {
			AfxMessageBox(_T("本地没有权限保存文件，或者文件无法创建！！"));
			return -1;
		}

		//发送下载指令
		SendCommandPacket(m_remoteDlg, 4, false, (BYTE*)(LPCSTR)m_strRemote, m_strRemote.GetLength(), (WPARAM)pFile);

		m_remoteDlg.BeginWaitCursor();
		m_statusDlg.m_info.SetWindowText(_T("命令正在执行中！"));
		m_statusDlg.ShowWindow(SW_SHOW);
		m_statusDlg.CenterWindow(&m_remoteDlg);
		m_statusDlg.SetActiveWindow();
	}
	
	return 0;
}

void CClientController::StartWatchScreen()
{
	m_isClosed = false;     // 置为false，允许监控线程循环
	m_hThreadWatch = (HANDLE)_beginthread(&CClientController::threadWatchEntry, 0, this);
	m_watchDlg.DoModal();   // 显示监控模态对话框
	m_isClosed = true;      // 对话框关闭，置为true，退出监控循环
	WaitForSingleObject(m_hThreadWatch, 500);       // 等待监控线程退出（超时500ms）

	/*
	这里的代码有问题，_beginthread并不能使用WaitForSingleObject，应该采用_beginthreadex
	*/
}

void CClientController::threadWatchScreen()
{
	Sleep(50);
	ULONGLONG nTick = GetTickCount64();
	while (!m_isClosed) {
		if (m_watchDlg.isFull() == false) {
			if (GetTickCount64() - nTick < 200) {
				Sleep(200 - DWORD(GetTickCount64() - nTick));
			}
			nTick= GetTickCount64();
			int ret = SendCommandPacket(m_watchDlg.GetSafeHwnd(), 6, true, NULL, 0);  // 发送屏幕采集指令（nCmd=6），返回1表示成功

			if (ret == 1) {
				

			}
			else { TRACE("1111获取图片失败! ret=%d\r\n", ret); }

		}
		Sleep(1);
	}
	
}

void CClientController::threadWatchEntry(void* arg)
{
	CClientController* thiz = (CClientController*)arg;
	thiz->threadWatchScreen();
	_endthread();

}


void CClientController::threadFunc()   //消息循环 + 消息分发处理
{
	MSG msg;
	while (::GetMessage(&msg, NULL, 0, 0)) {  // 线程消息循环（GetMessage：阻塞等待消息，直到收到WM_QUIT）
		TranslateMessage(&msg);       //处理键盘相关的 “硬件消息→字符消息” 转换
		DispatchMessage(&msg);        // 分发消息
		
		// 模式1：处理通用同步转发消息（WM_SEND_MESSAGE）
		// 自定义消息 WM_SHOW_STATUS / WM_SHOW_WATCH 不能直接发给工作线程，必须用 WM_SEND_MESSAGE 做一层包装

		if (msg.message == WM_SEND_MESSAGE) {     
			MSGINFO* pmsg = (MSGINFO*)msg.wParam;     // 解析参数：消息信息指针 + 同步事件句柄
			HANDLE hEvent = (HANDLE)msg.lParam;    

			std::map<UINT, MSGFUNC>::iterator it=m_mapFunc.find(msg.message);    // 根据原始消息ID查找处理函数
			if (it != m_mapFunc.end()) {
				pmsg->result = (this->*it->second)(pmsg->msg.message, pmsg->msg.wParam, pmsg->msg.lParam);
			}
			else { pmsg->result = -1; }
			SetEvent(hEvent);        // 设置事件为有信号：通知主线程处理完成
		}
		else {             // 模式2：处理直接投递到线程的自定义消息（异步，无需等待）
			std::map<UINT, MSGFUNC>::iterator it = m_mapFunc.find(msg.message);
			if (it != m_mapFunc.end()) { (this->*it->second)(msg.message, msg.wParam, msg.lParam); }
		}
	}
}

unsigned __stdcall CClientController::threadEntry(void* arg) {
	/*我们创建线程时，把 this 传了进去，系统只能以 void* 类型传递，到了入口函数，不知道类型是什么
		所以强制转回 CClientController*     在静态函数里，拿到类对象指针，从而调用非静态成员函数
	*/
	
	CClientController* thiz = (CClientController*)arg;
	thiz->threadFunc();
	_endthreadex(0);
	return 0;
}

LRESULT CClientController::OnShowStatus(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	return m_statusDlg.ShowWindow(SW_SHOW);
}

LRESULT CClientController::OnShowWatcher(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	return m_watchDlg.DoModal();
}
