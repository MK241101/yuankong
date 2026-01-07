
// RemoteClientDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "RemoteClient.h"
#include "RemoteClientDlg.h"
#include "afxdialogex.h"
#include "ClientController.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#endif
#include "CWatchDialog.h"

#ifndef WM_SEND_PACK_ACK
#define WM_SEND_PACK_ACK (WM_USER + 2)
#endif

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
public:
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
	ON_BN_CLICKED(IDC_BTN_START_WATCH, &CRemoteClientDlg::OnBnClickedBtnStartWatch)
	ON_WM_TIMER()
	ON_NOTIFY(IPN_FIELDCHANGED, IDC_IPADDRESS_serv, &CRemoteClientDlg::OnIpnFieldchangedIpaddressserv)
	ON_EN_CHANGE(IDC_EDIT_PORT, &CRemoteClientDlg::OnEnChangeEditPort)
	ON_MESSAGE(WM_SEND_PACK_ACK, &CRemoteClientDlg::OnSendPackAck)

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
	m_server_address=0xC0A80A6A;//192.168.10.106
	m_nPort = _T("9527");
	CClientController* pController = CClientController::getInstance();
	pController->UpdateAddress(m_server_address, atoi((LPCTSTR)m_nPort));
	UpdateData(FALSE);

	m_dlgStatus.Create(IDD_DLG_STATUS, this);
	m_dlgStatus.ShowWindow(SW_HIDE);
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
	CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 1981);
}

void CRemoteClientDlg::OnBnClickedBtnFileinfo()
{
	std::list<CPacket> lstPackets;
	int ret= CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 1,true,NULL,0);
	if (ret == 0 ) {
		AfxMessageBox(_T("命令处理失败"));
		return;
	}
	
}

void CRemoteClientDlg::LoadFileCurrent()
{
	HTREEITEM hTree= m_Tree.GetSelectedItem();  //获取树形控件中当前选中的节点（要加载的目标目录）
	CString strPath = GetPath(hTree);  // 调用GetPath函数，获取双击节点的完整路径

	m_List.DeleteAllItems();    //清空列表控件原有所有项（避免旧数据残留）

	int nCmd = CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 2, false, (BYTE*)(LPCTSTR)strPath, strPath.GetLength()); //发送命令包：命令码2，路径字符串作为数据

	PFILEINFO pInfo = (PFILEINFO)CClientSocket::getInstance()->GetPacket().strData.c_str();  //处理服务端返回的文件信息

	while (pInfo->HasNext) {       // 循环解析服务端返回的文件信息
		TRACE("[%s] isdir %d\r\n", pInfo->szFileName, pInfo->IsDirectory);
		if (!pInfo->IsDirectory) {   // 仅将“非目录”的文件插入列表控件
			m_List.InsertItem(0, pInfo->szFileName);
		}

		int cmd = CClientController::getInstance()->DealCommand();
		TRACE("ack:%d\n", cmd);
		if (cmd < 0) { break; }
		pInfo = (PFILEINFO)CClientSocket::getInstance()->GetPacket().strData.c_str();
	}
	//CClientController::getInstance()->CloseSocket();
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

	TRACE("hTreeSelected %08X\r\n", hTreeSelected);

	CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 2, false, (BYTE*)(LPCTSTR)strPath, strPath.GetLength(),(WPARAM)hTreeSelected); //发送命令包：命令码2，路径字符串作为数据

	

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
	int nListSelected = m_List.GetSelectionMark();      //获取列表控件中选中项的索引
	CString strFile = m_List.GetItemText(nListSelected, 0);   //从列表控件中获取选中项的文件名
	HTREEITEM hSelevted = m_Tree.GetSelectedItem();     //获取树形控件中选中的节点
	strFile = GetPath(hSelevted) + strFile;     // 拼接远程文件的完整路径（树形路径 + 列表文件名）
	int ret=CClientController::getInstance()->DownFile(strFile);

	if (ret != 0) {
		MessageBox(_T("下载文件失败！！"));
		TRACE("下载失败 ret=%d\r\n", ret);
	}

}

void CRemoteClientDlg::OnDeleteFile()
{
	HTREEITEM hSelevted = m_Tree.GetSelectedItem();     //获取树形控件中选中的节点
	CString strPath = GetPath(hSelevted);        //获取树形控件中选中的节点路径
	int nSelected = m_List.GetSelectionMark();      //获取列表控件中选中项的索引
	CString strFile = m_List.GetItemText(nSelected, 0);   //从列表控件中获取选中项的文件名
	strFile = strPath + strFile;

	int ret = CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 9, false, (BYTE*)(LPCTSTR)strFile, strFile.GetLength());
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

    int ret = CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 3, false, (BYTE*)(LPCTSTR)strFile, strFile.GetLength());
	if (ret < 0) {
		AfxMessageBox("打开文件命令失败！！");
	}

}

void CRemoteClientDlg::OnBnClickedBtnStartWatch()
{
	CClientController::getInstance()->StartWatchScreen();
	

}

void CRemoteClientDlg::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值

	CDialogEx::OnTimer(nIDEvent);
}

void CRemoteClientDlg::OnIpnFieldchangedIpaddressserv(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMIPADDRESS pIPAddr = reinterpret_cast<LPNMIPADDRESS>(pNMHDR);
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;

	UpdateData();
	CClientController* pController = CClientController::getInstance();
	pController->UpdateAddress(m_server_address, atoi((LPCTSTR)m_nPort));
}

void CRemoteClientDlg::OnEnChangeEditPort()
{
	UpdateData();
	CClientController* pController = CClientController::getInstance();
	pController->UpdateAddress(m_server_address, atoi((LPCTSTR)m_nPort));
}

LRESULT CRemoteClientDlg::OnSendPackAck(WPARAM wParam, LPARAM lParam) {
	if ((lParam == -1) || (lParam == -2)) {
	}
	else if (lParam == 1) {
	}
	else  {
		if (wParam != NULL) {
			CPacket head = *(CPacket*)wParam;
			delete (CPacket*)wParam;

			switch (head.sCmd) {
			case 1:       //获取驱动器列表
			{
				std::string drivers = head.strData;
				std::string dr;
				m_Tree.DeleteAllItems();
				for (size_t i = 0; i < drivers.size(); i++) {
					if (drivers[i] == ',') {
						dr += ":";
						HTREEITEM hTemp = m_Tree.InsertItem(dr.c_str(), TVI_ROOT, TVI_LAST);
						m_Tree.InsertItem(NULL, hTemp, TVI_LAST);
						dr.clear();
						continue;
					}
					dr += drivers[i];
				}
				if (dr.size() > 0) {
					dr += ":";
					HTREEITEM hTemp = m_Tree.InsertItem(dr.c_str(), TVI_ROOT, TVI_LAST);
					m_Tree.InsertItem(NULL, hTemp, TVI_LAST);
				}
			}
				break;


			case 2:  //获取文件列表
			{
				PFILEINFO pInfo = (PFILEINFO)head.strData.c_str();  //处理服务端返回的文件信息
				TRACE("hasnext %d isdirectory %d %s\r\n",pInfo->HasNext,pInfo->IsDirectory,pInfo->szFileName);

				if (pInfo->HasNext == FALSE)break;
				if (pInfo->IsDirectory) {
					if ((CString(pInfo->szFileName) == ".") || (CString(pInfo->szFileName) == "..")) {
						break;
					}
					TRACE("hselected %08X\r\n", lParam,m_Tree.GetSelectedItem());

					HTREEITEM hTemp = m_Tree.InsertItem(pInfo->szFileName, (HTREEITEM)lParam);  // 将文件名插入到树形控件中
					m_Tree.InsertItem("", hTemp, TVI_LAST);
					m_Tree.Expand((HTREEITEM)lParam, TVE_EXPAND);  // 展开当前节点

				}
				else { m_List.InsertItem(0, pInfo->szFileName); }


			}
			break;
			case 3:
				TRACE("运行文件!!\r\n");

				break;
			case 4:
			{
				static LONGLONG length = 0,index=0;
				TRACE("length %d index %d\r\n", length, index);
				if (length == 0) {
					length = *(long long*)head.strData.c_str();
					if (length == 0) {
						AfxMessageBox("文件长度为零或者无法读取文件！！！");
						CClientController::getInstance()->DownloadEnd();
						break;
					}
				}
				else if (length > 0 && (index >= length)) {
					fclose((FILE*)lParam);
					length = 0;
					index = 0;
					CClientController::getInstance()->DownloadEnd();

				}
				else {
					FILE* pFile=(FILE*)lParam;
					fwrite(head.strData.c_str(), 1, head.strData.size(), pFile);
					index += head.strData.size();
					TRACE("index=%d\r\n", index);
					if (index >= length) {
						fclose((FILE*)lParam);
						length = 0;
						index = 0;
						CClientController::getInstance()->DownloadEnd();
					}
				}
			
			}
			break;
			case 9:
				TRACE("删除文件成功\r\n");
				break;
			case 1981:
				TRACE("测试连接成功\r\n");
				break;
			default:
				TRACE("未知命令：%d\r\n", head.sCmd);
				break;

			}
		}


	}
	
	return 0;
}