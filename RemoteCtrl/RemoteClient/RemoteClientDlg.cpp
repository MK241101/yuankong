
// RemoteClientDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "RemoteClient.h"
#include "RemoteClientDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif
#include "CWatchDialog.h"



// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CRemoteClientDlg 对话框



CRemoteClientDlg::CRemoteClientDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_REMOTECLIENT_DIALOG, pParent)
	, m_server_address(0)
	, m_nPort(_T(""))
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CRemoteClientDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_IPAddress(pDX, IDC_IPADDRESS_serv, m_server_address);
	DDX_Text(pDX, IDC_EDIT_PORT, m_nPort);
	DDX_Control(pDX, IDC_TREE_DIR, m_Tree);
	DDX_Control(pDX, IDC_LIST_FILE, m_List);
}

int CRemoteClientDlg::SendCommandPacket(int nCmd, bool bAutoClose, BYTE* pData, int nLength)
{
	UpdateData();
	CClientSocket* pClient = CClientSocket::getInstance();
	bool ret = pClient->InitSocket(m_server_address, atoi(LPCTSTR(m_nPort)));
	if (!ret) {
		AfxMessageBox("网络初始化失败");
		return -1;
	}

	CPacket pack(nCmd, pData, nLength);
	ret = pClient->Send(pack);
	TRACE("send ret:%d\n", ret);
	int cmd = pClient->DealCommand();
	TRACE("ack:%d\n", cmd);

	if (bAutoClose) { pClient->CloseSocket(); }
	
	return cmd;
}

BEGIN_MESSAGE_MAP(CRemoteClientDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BTN_TEST, &CRemoteClientDlg::OnBnClickedBtnTest)
	ON_BN_CLICKED(IDC_BTN_FILEINFO, &CRemoteClientDlg::OnBnClickedBtnFileinfo)
	ON_NOTIFY(NM_DBLCLK, IDC_TREE_DIR, &CRemoteClientDlg::OnNMDblclkTreeDir)
	ON_NOTIFY(NM_CLICK, IDC_TREE_DIR, &CRemoteClientDlg::OnNMClickTreeDir)
	ON_NOTIFY(NM_RCLICK, IDC_LIST_FILE, &CRemoteClientDlg::OnNMRClickListFile)
	ON_COMMAND(ID_DOWNLOAD_FILE, &CRemoteClientDlg::OnDownloadFile)
	ON_COMMAND(ID_DELETE_FILE, &CRemoteClientDlg::OnDeleteFile)
	ON_COMMAND(ID_RUN_FILE, &CRemoteClientDlg::OnRunFile)
	ON_MESSAGE(WM_SEND_PACKET, &CRemoteClientDlg::OnSendPacket)
	ON_BN_CLICKED(IDC_BTN_START_WATCH, &CRemoteClientDlg::OnBnClickedBtnStartWatch)
	ON_WM_TIMER()
END_MESSAGE_MAP()


// CRemoteClientDlg 消息处理程序

BOOL CRemoteClientDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码
	UpdateData();
	m_server_address=0xC0A80A67;//192.168.10.103
	m_nPort = _T("9527");
	UpdateData(FALSE);

	m_dlgStatus.Create(IDD_DLG_STATUS, this);
	m_dlgStatus.ShowWindow(SW_HIDE);
	m_isFull = false;
	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CRemoteClientDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CRemoteClientDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CRemoteClientDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CRemoteClientDlg::OnBnClickedBtnTest()
{
	SendCommandPacket(1981);
}

void CRemoteClientDlg::OnBnClickedBtnFileinfo()
{
	int ret=SendCommandPacket(1);
	if (ret == -1) {
		AfxMessageBox(_T("命令处理失败"));
		return;
	}
	CClientSocket* pClient = CClientSocket::getInstance();
	std::string drivers = pClient->GetPacket().strData;
	std::string dr;
	m_Tree.DeleteAllItems();
	for (size_t i = 0; i < drivers.size(); i++) {
		if (drivers[i] == ',') {
			dr += ":";
			HTREEITEM hTemp = m_Tree.InsertItem(dr.c_str(),TVI_ROOT, TVI_LAST);
			m_Tree.InsertItem(NULL, hTemp, TVI_LAST);
			dr.clear();
			continue;
		}
		dr += drivers[i];
	}
}

void CRemoteClientDlg::threadEntryForWatchData(void* arg)
{
	CRemoteClientDlg* thiz = (CRemoteClientDlg*)arg;
	thiz->threadWatchData();
	_endthread();
}

void CRemoteClientDlg::threadWatchData()
{
	Sleep(50);
	CClientSocket* pClient = NULL;
	do {
		pClient = CClientSocket::getInstance();
	} while (pClient == NULL);

	while(!m_isClosed){
		if (m_isFull == false) {
			int ret=SendMessage(WM_SEND_PACKET, 6<<1|1);
			if (ret==6) {
				BYTE* pData = (BYTE*)pClient->GetPacket().strData.c_str();  //获取返回的数据包数据
				HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, 0);  //分配可移动的全局内存块
				if (hMem == NULL) {
					TRACE("内存不足了！");
					Sleep(1);
					continue;
				}

				IStream* pStream = NULL;
				HRESULT hRet = CreateStreamOnHGlobal(hMem, TRUE, &pStream);  //创建基于全局内存的IStream流对象
				if (hRet == S_OK) {
					ULONG length = 0;
					pStream->Write(pData, pClient->GetPacket().strData.size(), &length); //将Socket接收的二进制图片数据写入流中
					LARGE_INTEGER bg = { 0 };        // 设置流的读取位置到起始处（确保图片加载时从开头读取）
					pStream->Seek(bg, STREAM_SEEK_SET, NULL);

					if ((HBITMAP)m_image != NULL) { m_image.Destroy(); }
					m_image.Load(pStream);               // 将流中的图片数据加载到m_image对象
					m_isFull = true;
				}

			}
			
			else { Sleep(1); }
		}
		else { Sleep(1); }
	}

}

void CRemoteClientDlg::threadEntryForDownFile(void* arg)
{
	CRemoteClientDlg* thiz = (CRemoteClientDlg*)arg;
	thiz->threadDownFile();
	_endthread();

}

void CRemoteClientDlg::threadDownFile()
{
	int nListSelected = m_List.GetSelectionMark();      //获取列表控件中选中项的索引
	CString strFile = m_List.GetItemText(nListSelected, 0);   //从列表控件中获取选中项的文件名
	CFileDialog dlg(FALSE, "*", strFile, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, NULL, this); // 创建文件保存对话框

	if (dlg.DoModal() == IDOK) {    //在保存对话框中点击“确定”
		FILE* pFile = fopen(dlg.GetPathName(), "wb+");  //打开本地文件
		if (pFile == NULL) {
			AfxMessageBox("本地没有权限保存文件，或者文件无法创建！！");
			m_dlgStatus.ShowWindow(SW_HIDE);
			EndWaitCursor();

			return;
		}

		HTREEITEM hSelevted = m_Tree.GetSelectedItem();     //获取树形控件中选中的节点
		strFile = GetPath(hSelevted) + strFile;     // 拼接远程文件的完整路径（树形路径 + 列表文件名）
		TRACE("%s\r\n", LPCSTR(strFile));
		CClientSocket* pClient = CClientSocket::getInstance();
		do {
			
			//int ret = SendCommandPacket(4, false, (BYTE*)(LPCTSTR)strFile, strFile.GetLength());
			int ret =SendMessage(WM_SEND_PACKET, 4<<1|0, (LPARAM)(LPCTSTR)strFile);
			
			if (ret < 0) {                             // 检查下载命令是否发送成功
				AfxMessageBox("执行下载命令失败！！");
				TRACE("执行下载失败：ret=%d\r\n", ret);
				break;
			}
			//TODO: 为什么这里GetPacket().strData返回的是文件总长度  strData应该是数据内容吧
			long long nLength = *(long long*)pClient->GetPacket().strData.c_str();  //读取服务端返回的文件总长度
			if (nLength == 0) {
				AfxMessageBox("文件长度为零或者无法读取文件！！");
				break;
			}

			long long nCount = 0;

			while (nCount < nLength) {          // 循环接收文件数据，直到接收完所有内容
				ret = pClient->DealCommand();
				if (ret < 0) {
					AfxMessageBox("传输失败！！");
					TRACE("传输失败：ret=%d\r\n", ret);
					break;
				}
				// 将接收到的数据包内容写入本地文件
				fwrite(pClient->GetPacket().strData.c_str(), 1, pClient->GetPacket().strData.size(), pFile);
				nCount += pClient->GetPacket().strData.size();
			}
		} while (false);
		fclose(pFile);
		pClient->CloseSocket();
	}
	m_dlgStatus.ShowWindow(SW_HIDE);
	EndWaitCursor();
	MessageBox(_T("文件下载完成"));
}

void CRemoteClientDlg::LoadFileCurrent()
{
	HTREEITEM hTree= m_Tree.GetSelectedItem();  //获取树形控件中当前选中的节点（要加载的目标目录）
	CString strPath = GetPath(hTree);  // 调用GetPath函数，获取双击节点的完整路径

	m_List.DeleteAllItems();    //清空列表控件原有所有项（避免旧数据残留）
	int nCmd = SendCommandPacket(2, false, (BYTE*)(LPCTSTR)strPath, strPath.GetLength()); //发送命令包：命令码2，路径字符串作为数据

	PFILEINFO pInfo = (PFILEINFO)CClientSocket::getInstance()->GetPacket().strData.c_str();  //处理服务端返回的文件信息
	CClientSocket* pClient = CClientSocket::getInstance();

	while (pInfo->HasNext) {       // 循环解析服务端返回的文件信息
		TRACE("[%s] isdir %d\r\n", pInfo->szFileName, pInfo->IsDirectory);
		if (!pInfo->IsDirectory) {   // 仅将“非目录”的文件插入列表控件
			m_List.InsertItem(0, pInfo->szFileName);
		}

		int cmd = pClient->DealCommand();
		TRACE("ack:%d\n", cmd);
		if (cmd < 0) { break; }
		pInfo = (PFILEINFO)CClientSocket::getInstance()->GetPacket().strData.c_str();
	}
	pClient->CloseSocket();
}

void CRemoteClientDlg::LoadFileInfo()
{
	CPoint ptMouse;                            // 定义鼠标坐标变量
	GetCursorPos(&ptMouse);                    // 获取鼠标在屏幕坐标系下的位置
	m_Tree.ScreenToClient(&ptMouse);           // 将屏幕坐标转换为树形控件的客户区坐标（控件内部坐标）
	HTREEITEM hTreeSelected = m_Tree.HitTest(ptMouse, 0);  //检测鼠标双击位置对应的树形节点句柄
	if (hTreeSelected == NULL) return;            // 如果双击位置没有节点（空白处），直接返回
	if (m_Tree.GetChildItem(hTreeSelected) == NULL) return;  // 如果双击的节点没有子节点，直接返回
	DeleteTreeChildrenItem(hTreeSelected);  // 删除当前节点的所有子节点

	m_List.DeleteAllItems();
	CString strPath = GetPath(hTreeSelected);  // 调用GetPath函数，获取双击节点的完整路径
	int nCmd = SendCommandPacket(2, false, (BYTE*)(LPCTSTR)strPath, strPath.GetLength()); //发送命令包：命令码2，路径字符串作为数据

	PFILEINFO pInfo = (PFILEINFO)CClientSocket::getInstance()->GetPacket().strData.c_str();  //处理服务端返回的文件信息
	CClientSocket* pClient = CClientSocket::getInstance();

	int Count = 0;
	while (pInfo->HasNext) {
		TRACE("[%s] isdir %d\r\n", pInfo->szFileName, pInfo->IsDirectory);
		if (pInfo->IsDirectory) {
			if ((CString(pInfo->szFileName) == ".") || (CString(pInfo->szFileName) == "..")) {
				int cmd = pClient->DealCommand();
				TRACE("ack:%d\n", cmd);
				if (cmd < 0) { break; }
				pInfo = (PFILEINFO)CClientSocket::getInstance()->GetPacket().strData.c_str();
				continue;
			}
			HTREEITEM hTemp = m_Tree.InsertItem(pInfo->szFileName, hTreeSelected, TVI_LAST);  // 将文件名插入到树形控件中
			m_Tree.InsertItem("", hTemp, TVI_LAST);

		}
		else { m_List.InsertItem(0, pInfo->szFileName); }
		
		Count++;
		int cmd = pClient->DealCommand();
		
		if (cmd < 0) { break; }
		pInfo = (PFILEINFO)CClientSocket::getInstance()->GetPacket().strData.c_str();
	}

	pClient->CloseSocket();
	TRACE("Count=%d\r\n", Count);
}

// 根据树形控件的节点句柄，递归向上拼接完整路径（从根节点到当前节点）
CString CRemoteClientDlg::GetPath(HTREEITEM hTree) {

	CString strRet, strTmp;
	do {
		strTmp = m_Tree.GetItemText(hTree);   // 获取当前节点的文本（文件夹/文件名称）
		strRet = strTmp+ '\\' + strRet;       // 拼接路径：当前节点名称 + 反斜杠 + 已拼接的上级路径
		hTree = m_Tree.GetParentItem(hTree);  // 获取当前节点的父节点句柄，继续向上遍历
	}while(hTree != NULL);
	return strRet;
}

void CRemoteClientDlg::DeleteTreeChildrenItem(HTREEITEM hTree)
{
	HTREEITEM hSub = NULL;
	do {
		hSub = m_Tree.GetChildItem(hTree);  // 获取当前节点的第一个子节点
		if (hSub != NULL) { m_Tree.DeleteItem(hSub); }  // 删除子节点
	} while (hSub != NULL);
}

// 树形控件（文件夹目录）的双击事件处理函数
void CRemoteClientDlg::OnNMDblclkTreeDir(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	LoadFileInfo();
}

void CRemoteClientDlg::OnNMClickTreeDir(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	LoadFileInfo();
}

void CRemoteClientDlg::OnNMRClickListFile(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	*pResult = 0;

	CPoint ptMouse,ptList;
	GetCursorPos(&ptMouse);
	ptList = ptMouse;
	m_List.ScreenToClient(&ptList);
	int ListSelected = m_List.HitTest(ptList);
	if (ListSelected < 0)return;
	CMenu menu;
	menu.LoadMenu(IDR_MENU_RCLICK);
	CMenu* pPupup = menu.GetSubMenu(0);
	if (pPupup == NULL) {
		pPupup->TrackPopupMenu(TPM_LEFTALIGN|TPM_RIGHTBUTTON, ptMouse.x, ptMouse.y, this);
	
	}
}

void CRemoteClientDlg::OnDownloadFile()
{
	_beginthread(CRemoteClientDlg::threadEntryForDownFile, 0, this);
	BeginWaitCursor();
	m_dlgStatus.m_info.SetWindowText(_T("命令正在执行中！"));
	m_dlgStatus.ShowWindow(SW_SHOW);
	m_dlgStatus.CenterWindow(this);
	m_dlgStatus.SetActiveWindow();
}

void CRemoteClientDlg::OnDeleteFile()
{
	HTREEITEM hSelevted = m_Tree.GetSelectedItem();     //获取树形控件中选中的节点
	CString strPath = GetPath(hSelevted);        //获取树形控件中选中的节点路径
	int nSelected = m_List.GetSelectionMark();      //获取列表控件中选中项的索引
	CString strFile = m_List.GetItemText(nSelected, 0);   //从列表控件中获取选中项的文件名
	strFile = strPath + strFile;

	int ret = SendCommandPacket(9, false, (BYTE*)(LPCTSTR)strFile, strFile.GetLength());
	if (ret < 0) {
		AfxMessageBox("删除文件命令失败！！");
	}
	LoadFileCurrent();   //重新加载当前目录的文件列表（刷新列表，确认删除结果）



}

void CRemoteClientDlg::OnRunFile()
{
	HTREEITEM hSelevted = m_Tree.GetSelectedItem();     //获取树形控件中选中的节点
	CString strPath = GetPath(hSelevted);        //获取树形控件中选中的节点路径
	int nSelected = m_List.GetSelectionMark();      //获取列表控件中选中项的索引
	CString strFile = m_List.GetItemText(nSelected, 0);   //从列表控件中获取选中项的文件名
	strFile = strPath + strFile;

    int ret = SendCommandPacket(3, false, (BYTE*)(LPCTSTR)strFile, strFile.GetLength());
	if (ret < 0) {
		AfxMessageBox("打开文件命令失败！！");
	}

}

LRESULT CRemoteClientDlg::OnSendPacket(WPARAM wParam, LPARAM lParam)
{
	int ret = 0;
	int cmd = wParam >> 1;
	switch (cmd) {
	case 4:{
			CString strFile=(LPCSTR)lParam;
			ret = SendCommandPacket(cmd, wParam & 1, (BYTE*)(LPCSTR)strFile, strFile.GetLength());
		}
		break;

	case 5: {  //鼠标操作
			ret=SendCommandPacket(cmd, wParam & 1, (BYTE*)lParam, sizeof(MOUSEEV));
		}
		  break;
	case 6:
			ret=SendCommandPacket(cmd,wParam & 1);
			break;
    default:
			ret = -1;
	}
	return ret;
}

void CRemoteClientDlg::OnBnClickedBtnStartWatch()
{
	m_isClosed = false;
	CWatchDialog dlg(this);
	HANDLE hThread=(HANDLE)_beginthread(CRemoteClientDlg::threadEntryForWatchData, 0, this);
	dlg.DoModal();
	m_isClosed = true;
	WaitForSingleObject(hThread, 500);

}

void CRemoteClientDlg::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值

	CDialogEx::OnTimer(nIDEvent);
}
