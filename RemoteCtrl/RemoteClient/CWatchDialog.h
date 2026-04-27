#pragma once
#include "afxdialogex.h"


#ifndef WM_SEND_PACK_ACK
#define WM_SEND_PACK_ACK (WM_USER + 2)
#endif


// CWatchDialog 对话框

class CWatchDialog : public CDialog
{
	DECLARE_DYNAMIC(CWatchDialog)

public:
	CWatchDialog(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CWatchDialog();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DLG_WATCH };
#endif

public:
	int m_nObjWidth;     // 远程屏幕的实际宽度
	int m_nObjHeight;    // 远程屏幕的实际高度
	CImage m_image;      // MFC图像对象，用于存储接收到的远程屏幕图像数据

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持
	bool m_isFull;   //缓存区是否已满

	DECLARE_MESSAGE_MAP()
public:

	CImage& GetImage() { return m_image; }   // 获取远程屏幕图像对象的引用
	void SetImageStatus(bool isFull = false) { m_isFull = isFull; }  // 设置缓存区状态
	bool isFull() const { return m_isFull; }   // 获取缓存区状态
	
	CPoint UserPoint2RemoteScreenPoint(CPoint& point,bool isScreen = false);  //本地鼠标坐标转换为远程屏幕坐标
	
	virtual BOOL OnInitDialog();        // 对话框初始化函数
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	CStatic m_picture;                 // 静态图片控件，用于显示远程屏幕图像（绑定IDC_WATCH）
	afx_msg LRESULT OnSendPackAck(WPARAM wParam, LPARAM lParam); // 自定义消息WM_SEND_PACK_ACK处理
	
	// 鼠标操作消息处理：左键/右键 双击/按下/弹起、鼠标移动
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);

	afx_msg void OnStnClickedWatch();    // 图片控件IDC_WATCH单击事件处理
	virtual void OnOK();                 // 重写基类OK按钮事件（屏蔽默认关闭功能）
	
	afx_msg void OnBnClickedBtnLock();   // 锁定远程端按钮单击事件
	afx_msg void OnBnClickedBtnUnlock(); // 解锁远程端按钮单击事件
};
