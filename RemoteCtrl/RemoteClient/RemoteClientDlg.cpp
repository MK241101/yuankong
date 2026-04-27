
// RemoteClientDlg.cpp: 实现文件

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



CRemoteClientDlg::CRemoteClientDlg(CWnd* pParent /*=nullptr*/)  // 主对话框构造函数：初始化父类、IP、端口、程序图标
	: CDialogEx(IDD_REMOTECLIENT_DIALOG, pParent)
	, m_server_address(0)   // 初始IP为0
	, m_nPort(_T(""))       // 初始端口为空字符串
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
	ON_BN_CLICKED(IDC_BTN_TEST, &CRemoteClientDlg::OnBnClickedBtnTest)  // 测试按钮
	ON_BN_CLICKED(IDC_BTN_FILEINFO, &CRemoteClientDlg::OnBnClickedBtnFileinfo)   // 获取文件信息按钮
	ON_NOTIFY(NM_DBLCLK, IDC_TREE_DIR, &CRemoteClientDlg::OnNMDblclkTreeDir)     // 树形控件双击
	ON_NOTIFY(NM_CLICK, IDC_TREE_DIR, &CRemoteClientDlg::OnNMClickTreeDir)       // 树形控件单击
	ON_NOTIFY(NM_RCLICK, IDC_LIST_FILE, &CRemoteClientDlg::OnNMRClickListFile)   // 文件列表右键
	ON_COMMAND(ID_DOWNLOAD_FILE, &CRemoteClientDlg::OnDownloadFile)      // 下载文件命令
	ON_COMMAND(ID_DELETE_FILE, &CRemoteClientDlg::OnDeleteFile)          // 删除文件命令
	ON_COMMAND(ID_RUN_FILE, &CRemoteClientDlg::OnRunFile)                // 运行文件命令
	ON_BN_CLICKED(IDC_BTN_START_WATCH, &CRemoteClientDlg::OnBnClickedBtnStartWatch)   // 开始监视按钮
	ON_WM_TIMER()
	ON_NOTIFY(IPN_FIELDCHANGED, IDC_IPADDRESS_serv, &CRemoteClientDlg::OnIpnFieldchangedIpaddressserv)   // IP控件改变
	ON_EN_CHANGE(IDC_EDIT_PORT, &CRemoteClientDlg::OnEnChangeEditPort)   // 端口编辑框改变
	ON_MESSAGE(WM_SEND_PACK_ACK, &CRemoteClientDlg::OnSendPackAck)       // 自定义消息：服务端数据包响应

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

	InitUIData();
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

void CRemoteClientDlg::OnPaint()   // 窗口绘制函数：处理窗口重绘事件
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

// 【连接测试】按钮点击事件：向服务端发送测试命令（命令码1981）
void CRemoteClientDlg::OnBnClickedBtnTest()  
{
	TRACE("======== 点击了连接测试！========\n");
	CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 1981);
}

// 【获取文件信息】按钮点击事件：向服务端发送获取驱动器列表命令（命令码1）
void CRemoteClientDlg::OnBnClickedBtnFileinfo()
{
	//std::list<CPacket> lstPackets;
	int ret= CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(),1,true,NULL,0,0);
	if (ret == 0 ) {
		AfxMessageBox(_T("命令处理失败"));
		return;
	}
	
}

// 核心：命令分发处理函数——根据服务端返回的命令码，处理不同业务
void CRemoteClientDlg::DealCommand(WORD nCmd,const std::string& strData, LPARAM lParam)
{
	switch (nCmd) {
	case 1:
		Str2Tree(strData, m_Tree);    //获取驱动器列表
		break;
	case 2:
		UpdateFileInfo(*(PFILEINFO)strData.c_str(), (HTREEITEM)lParam);    //获取文件列表
		break;
	case 3:  // 命令码3：服务端返回“运行文件成功” → 弹出提示
		MessageBox(_T("打开文件完成！"), _T("打开文件"));
		break;
	case 4:  // 命令码4：服务端返回文件下载分块数据 → 写入本地文件
		UpdateDownloadFile(strData, (FILE*)lParam);
		break;
	case 9:   // 命令码9：服务端返回“删除文件成功” → 弹出提示
		MessageBox(_T("删除文件完成！"), _T("删除文件"));
		break;
	case 1981: // 命令码1981：服务端返回“连接测试成功” → 弹出提示
		MessageBox(_T("连接测试成功！"),_T("连接成功"));
		break;
	default:
		TRACE("未知命令：%d\r\n", nCmd);
		break;

	}
}

// 界面数据初始化：设置默认IP、端口，初始化控制层地址，创建状态对话框
void CRemoteClientDlg::InitUIData()
{
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标
	UpdateData();
	m_server_address = 0x7F000001;//;//127.0.0.1
	m_nPort = _T("9527");
	CClientController* pController = CClientController::getInstance();
	pController->UpdateAddress(m_server_address, atoi((LPCTSTR)m_nPort));
	/*
	（1）创建CClientController的实例对象pController
	（2）pController调用UpdateAddress()
	（3）创建CClientSocket的实例对象m_instance
	（4）调用m_instance的UpdateAddress()完成IP和端口的更新
	*/
	UpdateData(FALSE);

	m_dlgStatus.Create(IDD_DLG_STATUS, this);
	m_dlgStatus.ShowWindow(SW_HIDE);
}

// 加载当前选中目录的文件
void CRemoteClientDlg::LoadFileCurrent()
{
	HTREEITEM hTree= m_Tree.GetSelectedItem();  //（1）获取树形控件中【当前选中的目录节点】句柄
	CString strPath = GetPath(hTree);			//（2）调拼接选中节点的【完整远程路径】
	m_List.DeleteAllItems();					//（3）清空文件列表控件旧数据，避免新旧数据混合

	//（4）向服务端发送【获取目录文件】请求（核心通信步骤）
	int nCmd = CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 2, false, (BYTE*)(LPCTSTR)strPath, strPath.GetLength()); 
	
	//（5）获取服务端返回的【第一个文件信息数据包】
	PFILEINFO pInfo = (PFILEINFO)CClientSocket::getInstance()->GetPacket().strData.c_str();  

	while (pInfo->HasNext) {    // （6）循环处理服务端返回的【所有文件信息】
		TRACE("[%s] isdir \r\n", pInfo->szFileName, pInfo->IsDirectory);
		if (!pInfo->IsDirectory) {   // （7）筛选【非目录文件】并添加到列表控件
			if (CString(pInfo->szFileName) != "." || (CString(pInfo->szFileName) != ".."))
				//TODO:原代码这里是==，我改成了!=
				m_List.InsertItem(0, pInfo->szFileName);
		}
		int cmd = CClientController::getInstance()->DealCommand(); // （8）处理下一个文件信息，更新循环条件
		TRACE("ack:%d\r\n", cmd); 
		/*
		调用DealCommand() → 接收+解析下一个数据包 → 存储到CClientSocket::m_packet 
		→ 调用GetPacket()读取m_packet → 转换为pInfo
		*/

		if (cmd < 0) break;   // 命令处理失败，退出循环
		pInfo = (PFILEINFO)CClientSocket::getInstance()->GetPacket().strData.c_str();
	}
	
}

// 将服务端返回的驱动器字符串（如C,D,E）解析为树形控件的根节点
void CRemoteClientDlg::Str2Tree(const std::string& drivers,CTreeCtrl& tree)
{
	std::string dr;
	tree.DeleteAllItems();    // 清空树形控件原有所有节点

	// 遍历驱动器字符串，按逗号分割（服务端约定分隔符为,）
	for (size_t i = 0; i < drivers.size(); i++) {
		if (drivers[i] == ',') {
			dr += ":";
			HTREEITEM hTemp = tree.InsertItem(dr.c_str(), TVI_ROOT, TVI_LAST);   // 插入根节点（驱动器），并添加一个空子节点
			tree.InsertItem("", hTemp, TVI_LAST);
			dr.clear();
			continue;
		}
		dr += drivers[i];     // 拼接驱动器字符
	}
	if (dr.size() > 0) {    // 处理最后一个驱动器（字符串末尾无逗号的情况）
		dr += ":";
		HTREEITEM hTemp = tree.InsertItem(dr.c_str(), TVI_ROOT, TVI_LAST);
		tree.InsertItem("", hTemp, TVI_LAST);
	}
}

// 根据服务端返回的FILEINFO结构体，更新树形控件（子目录）和列表控件（文件）
void CRemoteClientDlg::UpdateFileInfo(const FILEINFO& finfo,HTREEITEM hParent)
{
	TRACE("hasnext %d isdirectory %d %s\r\n", finfo.HasNext, finfo.IsDirectory, finfo.szFileName);
	if (finfo.HasNext == FALSE)return;

	if (finfo.IsDirectory) {        // 如果是目录文件
		if ((CString(finfo.szFileName) == ".") || (CString(finfo.szFileName) == "..")) return;

		TRACE("hselected %08X %08X\r\n", hParent, m_Tree.GetSelectedItem());

		HTREEITEM hTemp = m_Tree.InsertItem(finfo.szFileName, (HTREEITEM)hParent);  // 将文件名插入到树形控件中
		m_Tree.InsertItem("", hTemp, TVI_LAST);
		m_Tree.Expand(hParent, TVE_EXPAND);  // 展开当前节点

	}
	else { m_List.InsertItem(0, finfo.szFileName); }    // 非目录文件，插入到列表控件中
}

// 处理文件下载：接收服务端分块数据，写入本地文件，完成后关闭文件
void CRemoteClientDlg::UpdateDownloadFile(const std::string& strData, FILE* pFile)
{
	static LONGLONG length = 0, index = 0;
	TRACE("length %d index %d\r\n", length, index);
	if (length == 0) {
		length = *(long long*)strData.c_str();     // 第一次接收数据：解析文件总长度（服务端第一个分块传递文件大小）
		if (length == 0) {
			AfxMessageBox("文件长度为零或者无法读取文件！！！");
			CClientController::getInstance()->DownloadEnd();
		}
	}
	else if (length > 0 && (index >= length)) {   // 已写入长度 >= 总长度：下载完成，关闭文件，重置状态
		fclose(pFile);
		length = 0;
		index = 0;
		CClientController::getInstance()->DownloadEnd();
	}
	else {
		fwrite(strData.c_str(), 1, strData.size(), pFile);    // 正常分块数据：写入本地文件
		index += strData.size();
		TRACE("index=%d\r\n", index);

		//检查是否下载完成，完成则关闭文件、重置状态
		if (index >= length) {
			fclose(pFile);
			length = 0;
			index = 0;
			CClientController::getInstance()->DownloadEnd();
		}
	}
}

// 根据鼠标点击/双击的树形节点，获取对应路径的文件/目录
void CRemoteClientDlg::LoadFileInfo()
{
	CPoint ptMouse;                            // 定义鼠标坐标变量
	GetCursorPos(&ptMouse);                    // 获取鼠标在屏幕坐标系下的位置
	m_Tree.ScreenToClient(&ptMouse);           // 将屏幕坐标转换为树形控件的客户区坐标（控件内部坐标）
	HTREEITEM hTreeSelected = m_Tree.HitTest(ptMouse, 0);  //检测鼠标双击位置对应的树形节点句柄
	if (hTreeSelected == NULL) return;            // 如果双击位置没有节点（空白处），直接返回

	DeleteTreeChildrenItem(hTreeSelected);     // 删除当前节点的所有子节点
	m_List.DeleteAllItems();                   // 清空文件列表旧数据

	CString strPath = GetPath(hTreeSelected);  // 调用GetPath函数，获取双击节点的完整路径
	TRACE("hTreeSelected %08X\r\n", hTreeSelected);

	// 发送命令码2，获取该路径下的文件/目录信息，传递节点句柄作为附加参数
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

// 删除树形控件指定节点的所有子节点（递归删除，确保清空）
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

// 树形控件单击事件：调用LoadFileInfo加载对应目录信息
void CRemoteClientDlg::OnNMClickTreeDir(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	LoadFileInfo();
}

// 文件列表右键事件：弹出上下文菜单（下载、删除、运行）
void CRemoteClientDlg::OnNMRClickListFile(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	*pResult = 0;

	CPoint ptMouse,ptList;
	GetCursorPos(&ptMouse);   // 获取屏幕鼠标位置
	ptList = ptMouse;

	m_List.ScreenToClient(&ptList);    // 转换为列表控件客户区坐标
	int ListSelected = m_List.HitTest(ptList);   // 检测鼠标是否命中列表项（选中有效文件）
	if (ListSelected < 0)return;
	
	CMenu menu;    // 加载右键菜单资源
	menu.LoadMenu(IDR_MENU_RCLICK);
	CMenu* pPupup = menu.GetSubMenu(0);   // 获取第一个子菜单（实际菜单项）
	if (pPupup == NULL) {
		pPupup->TrackPopupMenu(TPM_LEFTALIGN|TPM_RIGHTBUTTON, ptMouse.x, ptMouse.y, this);
	}
}

// 【下载文件】菜单命令：下载列表中选中的远程文件到本地
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

// 【删除文件】菜单命令：删除远程服务端指定文件
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

// 【运行文件】菜单命令：请求远程服务端运行指定文件
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

// 【开始监视】按钮点击事件：请求远程服务端开启屏幕监视
void CRemoteClientDlg::OnBnClickedBtnStartWatch()
{
	CClientController::getInstance()->StartWatchScreen();
}

void CRemoteClientDlg::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值

	CDialogEx::OnTimer(nIDEvent);
}

// IP地址控件内容改变事件：实时更新控制层的服务端IP配置
void CRemoteClientDlg::OnIpnFieldchangedIpaddressserv(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMIPADDRESS pIPAddr = reinterpret_cast<LPNMIPADDRESS>(pNMHDR);
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;

	UpdateData();   // 从界面同步IP到成员变量m_server_address
	CClientController* pController = CClientController::getInstance();
	pController->UpdateAddress(m_server_address, atoi((LPCTSTR)m_nPort));   // 更新控制层的服务端地址配置
}

// 端口编辑框内容改变事件：实时更新控制层的服务端端口配置
void CRemoteClientDlg::OnEnChangeEditPort()
{
	UpdateData();
	CClientController* pController = CClientController::getInstance();
	pController->UpdateAddress(m_server_address, atoi((LPCTSTR)m_nPort));
}

// 自定义消息WM_SEND_PACK_ACK处理函数：服务端数据包的最终接收入口
// wParam：服务端返回的数据包（CPacket*），lParam：附加状态/参数
LRESULT CRemoteClientDlg::OnSendPackAck(WPARAM wParam, LPARAM lParam) {
	if ((lParam == -1) || (lParam == -2)) {
		TRACE("套接字错误！！！ %d\r\n", lParam);
	}
	else if (lParam == 1) {
		TRACE("套接字已关闭！！\r\n");
	}
	else  {
		if (wParam != NULL) {
			CPacket head = *(CPacket*)wParam;
			delete (CPacket*)wParam;
			DealCommand(head.sCmd,head.strData, lParam);
		}
	}
	return 0;
}