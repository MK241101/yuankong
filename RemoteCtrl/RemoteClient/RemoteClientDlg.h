
// RemoteClientDlg.h: 头文件
//

#pragma once
#include "ClientSocket.h"
#include "StatusDlg.h"

#ifndef WM_SEND_PACK_ACK
#define WM_SEND_PACK_ACK (WM_USER + 2)
#endif // 


// CRemoteClientDlg 对话框
class CRemoteClientDlg : public CDialogEx
{
// 构造
public:
	CRemoteClientDlg(CWnd* pParent = nullptr);	// 标准构造函数
	

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_REMOTECLIENT_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持

public:
	void LoadFileInfo();    // 核心方法：加载指定远程目录的文件/子目录信息

private:
	bool m_isClosed;        // 监状态标记：监视客户端与服务端的连接是否关闭

private:
	void DealCommand(WORD nCmd, const std::string& strData,LPARAM lParam);  // 命令处理核心方法：根据服务端返回的命令码，分发处理不同业务逻辑
	void InitUIData();      // 初始化界面数据：IP、端口、控件状态等
	void LoadFileCurrent();    // 加载当前选中目录的文件
	void Str2Tree(const std::string& driver, CTreeCtrl& tree);       // 将服务端返回的驱动器字符串解析为树形控件节点
	void UpdateFileInfo(const FILEINFO& finfo, HTREEITEM hParent);   // 根据服务端返回的文件信息，更新树形控件（子目录）和列表控件（文件）
	void UpdateDownloadFile(const std::string& strData,FILE* pFile);   // 处理文件下载：将服务端分块返回的文件数据写入本地文件
	CString GetPath(HTREEITEM hTree);          // 根据树形控件节点句柄，递归拼接远程目录的完整路径
	void DeleteTreeChildrenItem(HTREEITEM hItem);   // 删除树形控件指定节点的所有子节点（刷新目录前清空旧数据）

// 实现
protected:
	HICON m_hIcon;            // 程序图标句柄
	CStatusDlg m_dlgStatus;   // 状态对话框实例

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();   // 对话框初始化
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);   // 系统命令处理
	afx_msg void OnPaint();        // 窗口绘制
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedBtnTest();     // 【连接测试】按钮点击事件
	DWORD m_server_address;     // 存储远程服务端IP（32位无符号整数格式）
	CString m_nPort;            // 存储远程服务端端口（字符串格式）
	afx_msg void OnBnClickedBtnFileinfo();  // 【获取文件信息】按钮点击事件
	CTreeCtrl m_Tree;           // 绑定树形控件：展示远程目录/驱动器结构
	afx_msg void OnNMDblclkTreeDir(NMHDR* pNMHDR, LRESULT* pResult);  // 树形控件双击事件：加载双击节点对应的远程目录信息
	afx_msg void OnNMClickTreeDir(NMHDR* pNMHDR, LRESULT* pResult);	  // 树形控件单击事件：加载单击节点对应的远程目录信息
	
	CListCtrl m_List;    //展示远程目录下的文件列表
	afx_msg void OnNMRClickListFile(NMHDR* pNMHDR, LRESULT* pResult);  // 文件列表右键事件：弹出操作菜单（下载、删除、运行）
	afx_msg void OnDownloadFile();   // 【下载文件】菜单命令处理
	afx_msg void OnDeleteFile();     // 【删除文件】菜单命令处理
	afx_msg void OnRunFile();        // 【运行文件】菜单命令处理

	afx_msg void OnBnClickedBtnStartWatch();   // 【开始监视】按钮点击事件（屏幕监视）
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnIpnFieldchangedIpaddressserv(NMHDR* pNMHDR, LRESULT* pResult);  //实时更新服务端IP配置

	afx_msg void OnEnChangeEditPort();  //实时更新服务端端口配置
	 
	afx_msg LRESULT OnSendPackAck(WPARAM wParam, LPARAM lParam);  // 自定义消息WM_SEND_PACK_ACK的处理函数：接收服务端数据包并分发

};
