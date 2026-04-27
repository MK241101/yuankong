// CWatchDialog.cpp: 实现文件

#include "pch.h"
#include "RemoteClient.h"
#include "afxdialogex.h"
#include "CWatchDialog.h"
#include "ClientController.h"

// CWatchDialog 对话框

IMPLEMENT_DYNAMIC(CWatchDialog, CDialog)

CWatchDialog::CWatchDialog(CWnd* pParent /*=nullptr*/)
	: CDialog(IDD_DLG_WATCH, pParent)
{
	m_isFull = false;
	m_nObjWidth = -1;
	m_nObjHeight = -1;
}

CWatchDialog::~CWatchDialog()
{
}

void CWatchDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_WATCH, m_picture);
}

// 消息映射表：MFC核心，将窗口消息/控件事件与对应的处理函数绑定
BEGIN_MESSAGE_MAP(CWatchDialog, CDialog)
	ON_WM_TIMER()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONDBLCLK()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_STN_CLICKED(IDC_WATCH, &CWatchDialog::OnStnClickedWatch)
	ON_BN_CLICKED(IDC_BTN_LOCK, &CWatchDialog::OnBnClickedBtnLock)
	ON_BN_CLICKED(IDC_BTN_UNLOCK, &CWatchDialog::OnBnClickedBtnUnlock)
	ON_MESSAGE(WM_SEND_PACK_ACK, &CWatchDialog::OnSendPackAck)
END_MESSAGE_MAP()


// CWatchDialog 消息处理程序

CPoint CWatchDialog::UserPoint2RemoteScreenPoint(CPoint& point, bool isScreen)  // 本地鼠标坐标转换为远程屏幕坐标
{
	CRect clientRect;        // 矩形对象，用于存储控件/屏幕矩形
	if (!isScreen) { ClientToScreen(&point); }
	
	m_picture.ScreenToClient(&point); //将屏幕坐标转换为图片控件的客户区坐标
	TRACE("x=%d,y=%d\r\n", point.x, point.y);

	m_picture.GetWindowRect(clientRect);  //本地坐标到远程坐标
	TRACE("x=%d,y=%d\r\n", clientRect.Width(), clientRect.Height());

	//保证本地鼠标在控件上的相对位置，与远程鼠标在屏幕上的相对位置一致
	return CPoint(point.x*m_nObjWidth/clientRect.Width(), point.y*m_nObjHeight/clientRect.Height());
}

BOOL CWatchDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  在此添加额外的初始化
    m_isFull = false;
	//SetTimer(0, 45, NULL);
	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

void CWatchDialog::OnTimer(UINT_PTR nIDEvent)
{
	CDialog::OnTimer(nIDEvent);
}

/*
OnSendPackAck()
核心自定义消息处理：WM_SEND_PACK_ACK（网络数据包应答/图像更新）
功能：接收网络层传递的数据包，解析并处理（核心：远程屏幕图像更新、鼠标操作应答）
参数：wParam-数据包指针；lParam-数据包状态标记（-1/-2=异常，1=普通应答，其他=有效数据包）
返回：LRESULT-消息处理结果（默认返回0）
*/
LRESULT CWatchDialog::OnSendPackAck(WPARAM wParam, LPARAM lParam)
{
	if ((lParam == -1) || (lParam == -2)) {   //   -1/-2=异常
	} 
	else if (lParam == 1) {      //  1=普通应答
	}
	else{
		CPacket* pPacket=(CPacket*)wParam;
		if (pPacket != NULL) {
			CPacket head = *(CPacket*)wParam;  //把堆上的CPacket对象，复制一份到栈上的head对象中，后续对head的操作都和原堆对象无关
			delete (CPacket*)wParam;   

			switch (head.sCmd) {
			case 6:    // 命令字6：远程屏幕图像数据（核心处理）
			{
				CEdoyunTool::Bytes2Image(m_image, head.strData);   // 工具类转换：将数据包中的字节流数据转换为MFC CImage图像
				CRect rect;           // 存储图片控件的显示矩形
				m_picture.GetWindowRect(rect);   // 获取控件的屏幕矩形

				m_nObjWidth = m_image.GetWidth();    // 记录远程屏幕的实际宽度
				m_nObjHeight = m_image.GetHeight();  // 记录远程屏幕的实际高度

				// 图像绘制：将CImage拉伸绘制到图片控件的设备上下文（DC）
				m_image.StretchBlt(m_picture.GetDC()->GetSafeHdc(), 0, 0, rect.Width(), rect.Height(), SRCCOPY);
				m_picture.InvalidateRect(NULL);      // 刷新控件：让绘制的图像立即显示
				TRACE("更新图片完成%d\r\n", m_nObjHeight);
				m_image.Destroy();    // 销毁CImage对象，释放图像内存
				
				break;

			}

			case 5:
				TRACE("远程端应答了鼠标操作\r\n");
				break;
			case 7:
			case 8:
			default:
				break;
			
			}
		}


	}
	return 0;
}

/*
*   OnLButtonDblClk
	左键双击消息处理：将本地左键双击操作转发到远程端
	nFlags - 鼠标按键标志；point - 鼠标点击的本地客户区坐标
*/
void CWatchDialog::OnLButtonDblClk(UINT nFlags, CPoint point){

	if ((m_nObjWidth != -1) && (m_nObjHeight != -1)) {       // 判空：确保已获取远程屏幕宽高
		CPoint remote = UserPoint2RemoteScreenPoint(point);  // 本地坐标转远程坐标
		
		MOUSEEV event;         // 封装鼠标事件结构体
		event.ptXY = remote;   // 转换后的远程屏幕坐标
		event.nButton = 0;     //左键
		event.nAction = 2;     //双击

		// 单例控制器发送鼠标操作数据包：命令字5，携带MOUSEEV结构体
		CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));
	}
	CDialog::OnLButtonDblClk(nFlags, point);
}

// 左键按下消息处理：将本地左键按下操作转发到远程端
void CWatchDialog::OnLButtonDown(UINT nFlags, CPoint point){
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1)) {
		TRACE("x=%d,y=%d\r\n", point.x, point.y);     // 调试输出本地原始坐标
		CPoint remote = UserPoint2RemoteScreenPoint(point);
		TRACE("x=%d,y=%d\r\n", point.x, point.y);
		TRACE("remote.x=%d,remote.y=%d\r\n", remote.x, remote.y);
		//封装
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 0;//左键
		event.nAction = 2;//按下
		CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(),5, true, (BYTE*)&event, sizeof(event));

	}
	CDialog::OnLButtonDown(nFlags, point);
}

// 左键弹起消息处理：将本地左键弹起操作转发到远程端
void CWatchDialog::OnLButtonUp(UINT nFlags, CPoint point){
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1)) {
		CPoint remote = UserPoint2RemoteScreenPoint(point);   //坐标转换
		
		MOUSEEV event;         //封装
		event.ptXY = remote;
		event.nButton = 0;     //左键
		event.nAction = 3;     //弹起
		CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(),5, true, (BYTE*)&event, sizeof(event));
	}
	CDialog::OnLButtonUp(nFlags, point);
}

// 右键双击消息处理：将本地右键双击操作转发到远程端
void CWatchDialog::OnRButtonDblClk(UINT nFlags, CPoint point)
{
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1)) {
		//坐标转换
		CPoint remote = UserPoint2RemoteScreenPoint(point);
		//封装
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 1;//右键
		event.nAction = 1;//双击

		CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(),5, true, (BYTE*)&event, sizeof(event));
	
	}
	CDialog::OnRButtonDblClk(nFlags, point);
}

// 右键按下消息处理：将本地右键按下操作转发到远程端
void CWatchDialog::OnRButtonDown(UINT nFlags, CPoint point)
{
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1)) {
		//坐标转换
		CPoint remote = UserPoint2RemoteScreenPoint(point);
		//封装
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 1;//右键
		event.nAction = 2;//按下
		CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));

	}
	CDialog::OnRButtonDown(nFlags, point);
}

// 右键弹起消息处理：将本地右键弹起操作转发到远程端
void CWatchDialog::OnRButtonUp(UINT nFlags, CPoint point)
{
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1)) {

		//坐标转换
		CPoint remote = UserPoint2RemoteScreenPoint(point);
		//封装
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 1;//右键
		event.nAction = 4;//弹起
		CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));

	}
	CDialog::OnRButtonUp(nFlags, point);
}

// 鼠标移动消息处理：将本地鼠标移动操作转发到远程端
void CWatchDialog::OnMouseMove(UINT nFlags, CPoint point)
{
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1)) {

		//坐标转换
		CPoint remote = UserPoint2RemoteScreenPoint(point);
		//封装
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 8;//鼠标移动
		event.nAction = 0;//移动
		CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));

	}
	CDialog::OnMouseMove(nFlags, point);
}

// 图片控件IDC_WATCH单击事件处理：处理控件区域的左键单击操作
void CWatchDialog::OnStnClickedWatch()    //TODO:为什么上面有很多鼠标的操作了  这里还需要这个函数
{
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1)) {
		CPoint point;
		GetCursorPos(&point);        //将当前鼠标光标的屏幕坐标写入传入的CPoint结构体中
		CPoint remote = UserPoint2RemoteScreenPoint(point, true);
		//封装
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 0;//左键
		event.nAction = 0;//单击
		CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));
	}
}

/*
	其他鼠标消息管 “对话框空白处的鼠标动作”，OnStnClickedWatch 管 “远程屏幕图片上的单击动作”
*/


void CWatchDialog::OnOK()
{
	// TODO: 在此添加专用代码和/或调用基类

	//CDialog::OnOK();
}

// 锁定远程端按钮单击事件：发送锁定命令数据包（命令字7）
void CWatchDialog::OnBnClickedBtnLock()
{
	CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 7);

}

// 解锁远程端按钮单击事件：发送解锁命令数据包（命令字8）
void CWatchDialog::OnBnClickedBtnUnlock()
{
	CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 8);

}
