// CWatchDialog.cpp: 实现文件
//

#include "pch.h"
#include "RemoteClient.h"
#include "afxdialogex.h"
#include "CWatchDialog.h"
#include "ClientController.h"

#ifndef WM_SEND_PACK_ACK
#define WM_SEND_PACK_ACK (WM_USER+2) //发送包数据应答
#endif 

// CWatchDialog 对话框

IMPLEMENT_DYNAMIC(CWatchDialog, CDialog)

CWatchDialog::CWatchDialog(CWnd* pParent /*=nullptr*/)
	: CDialog(IDD_DLG_WATCH, pParent)
{
	m_isFull = false;//表示一开始缓存中没有数据，直到获得数据之后才能是 true 
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


BEGIN_MESSAGE_MAP(CWatchDialog, CDialog)
	ON_WM_TIMER()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_RBUTTONDBLCLK()
	ON_WM_MOUSEMOVE()
	ON_STN_CLICKED(IDC_WATCH, &CWatchDialog::OnStnClickedWatch)
	ON_BN_CLICKED(IDC_BTN_LOCK, &CWatchDialog::OnBnClickedBtnLock)
	ON_BN_CLICKED(IDC_BTN_UNLOCK, &CWatchDialog::OnBnClickedBtnUnlock)
	ON_MESSAGE(WM_SEND_PACK_ACK, &CWatchDialog::OnSendPackAck)

END_MESSAGE_MAP()


// CWatchDialog 消息处理程序


BOOL CWatchDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_isFull = false;

	//SetTimer(0, 45, NULL);
	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}


void CWatchDialog::OnTimer(UINT_PTR nIDEvent)
{
	//// TODO: 在此添加消息处理程序代码和/或调用默认值
	//if (nIDEvent == 0) {
	//	CClientController* pParent = CClientController::getInstance();
	//	//指针指向父类对象，方便调用父类的函数
	//	//CRemoteClientDlg* pParent = (CRemoteClientDlg*)GetParent();
	//	//下面条件为 true 表示有新的屏幕数据，执行图像更新逻辑
	//	if (m_isFull) {

	//		TRACE("更新图片完成 %d %d \r\n",m_nObjWidth,m_nObjHeight);
	//		CRect rect;
	//		m_picture.GetWindowRect(rect);
	//		
	//		//CImage image;
	//		//pParent->GetImage(image);
	//		m_nObjWidth = m_image.GetWidth();
	//		m_nObjHeight = m_image.GetHeight();

	//		//将 m_image (从被监控的电脑上发送的图片) 渲染到 m_picture （对话框中的图像） 控件
	//		m_image.StretchBlt(
	//				m_picture.GetDC()->GetSafeHdc(), 0, 0, rect.Width(), rect.Height(), SRCCOPY);

	//		m_picture.InvalidateRect(NULL);
	//		m_image.Destroy();
	//		m_isFull = false;//默认是false，才能进入线程更新
	//		TRACE("更新图片完成 %d %d  %08X \r\n", m_nObjWidth, m_nObjHeight,(HBITMAP)m_image);
	//	}
	//}
	//CDialog::OnTimer(nIDEvent);
}

CPoint CWatchDialog::UserPointToRemoteScreenPoint(CPoint& point, bool isScreen)
{

	CRect clientRect;
	if (isScreen)ScreenToClient(&point);//全局坐标到客户区域坐标

	//本地坐标到远程坐标
	m_picture.GetWindowRect(clientRect);
//	TRACE("x = %d y = %d  \r\n", clientRect.Width(), clientRect.Height());
	int width0 = clientRect.Width();
	int height0 = clientRect.Height();
	//远程桌面的大小
	//int width = 1920, height = 1080;
	int width = m_nObjWidth, height = m_nObjHeight;
	int x = point.x * width / width0;
	int y = point.y * height / height0;
	return CPoint(x, y);

}

void CWatchDialog::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1)) {
		CPoint remote = UserPointToRemoteScreenPoint(point);
		// 封装
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 0;//左键
		event.nAction = 1;//双击
		//获取父类对象的指针，从而调用父类的函数
		CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)& event, sizeof(event));
	}
	CDialog::OnLButtonDblClk(nFlags, point);
}

void CWatchDialog::OnLButtonDown(UINT nFlags, CPoint point)
{
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1))
	{
//		TRACE(" x=%d  y=%d \r\n", point.x, point.y);
		CPoint remote = UserPointToRemoteScreenPoint(point);
//		TRACE(" x=%d  y=%d \r\n", point.x, point.y);
		// 封装
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 0;//左键
		event.nAction = 2;//按下

		//不能直接发送消息，因为这个界面是在线程中运行的
		//CClientSocket* pClient = CClientSocket::getInstance();
		//CPacket pack(5, (BYTE*)&event, sizeof(event));
		//pClient->Send(pack);
		CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));
	}

	CDialog::OnLButtonDown(nFlags, point);
}

void CWatchDialog::OnLButtonUp(UINT nFlags, CPoint point)
{
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1))
	{
		CPoint remote = UserPointToRemoteScreenPoint(point);
		// 封装
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 0;//左键
		event.nAction = 3;//弹起

		CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));
	}

	CDialog::OnLButtonUp(nFlags, point);
}

void CWatchDialog::OnRButtonDblClk(UINT nFlags, CPoint point)
{
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1))
	{
		CPoint remote = UserPointToRemoteScreenPoint(point);
		// 封装
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 1;//右键
		event.nAction = 1;//双击

		CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));
	}

	CDialog::OnRButtonDblClk(nFlags, point);
}

void CWatchDialog::OnRButtonDown(UINT nFlags, CPoint point)
{
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1))
	{
		CPoint remote = UserPointToRemoteScreenPoint(point);
		// 封装
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 1;//右键
		event.nAction = 2;//按下

		CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));
	}

	CDialog::OnRButtonDown(nFlags, point);
}

void CWatchDialog::OnRButtonUp(UINT nFlags, CPoint point)
{
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1))
	{
		CPoint remote = UserPointToRemoteScreenPoint(point);
		// 封装
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 1;//右键
		event.nAction = 3;//弹起

		CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));
	}

	CDialog::OnRButtonUp(nFlags, point);
}

void CWatchDialog::OnMouseMove(UINT nFlags, CPoint point)
{
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1))
	{
		CPoint remote = UserPointToRemoteScreenPoint(point);
		// 封装
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 8;//没有按键
		event.nAction = 0;//放开（随便）

		CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));
	}

	CDialog::OnMouseMove(nFlags, point);
}

void CWatchDialog::OnStnClickedWatch()
{

	if ((m_nObjWidth != -1) && (m_nObjHeight != -1)) {
		CPoint point;
		GetCursorPos(&point);//拿到鼠标的位置（整个屏幕的坐标）

		CPoint remote = UserPointToRemoteScreenPoint(point, true);
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 0;//左键
		event.nAction = 0;//单击

		CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(),5, true, (BYTE*)&event, sizeof(event));
	}
}

void CWatchDialog::OnOK()
{
	//CDialog::OnOK();
}

void CWatchDialog::OnBnClickedBtnLock()
{
	CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 7);
}


void CWatchDialog::OnBnClickedBtnUnlock()
{
	CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 8);
}


LRESULT CWatchDialog::OnSendPackAck(WPARAM wParam, LPARAM lParam)
{
	if ( lParam == -1 || lParam == -2) {
		//错误处理

	}
	else if (lParam == 1) {
		//对方关闭套接字

	}
	else 
	{
		CPacket* pPacket = (CPacket*)wParam;//发送回来的数据都会存入 wParam	
		if (pPacket != NULL) {
			CPacket head = *pPacket;
			delete pPacket;
			switch (head.sCmd) {
			case 6://更新屏幕状态
			{
				CTool::BytesToImage(m_image, head.strData);
				//将定时器中的逻辑放到这里
				CRect rect;
				m_picture.GetWindowRect(rect);
				m_nObjWidth = m_image.GetWidth();
				m_nObjHeight = m_image.GetHeight();
				m_image.StretchBlt(
					m_picture.GetDC()->GetSafeHdc(), 0, 0, rect.Width(), rect.Height(), SRCCOPY);//用来缩放
				m_picture.InvalidateRect(NULL);
				TRACE("更新图片完成%d %d %08X\r\n", m_nObjWidth, m_nObjHeight, (HBITMAP)m_image);
				m_image.Destroy();

				break;
			}
			case 5://鼠标操作
				TRACE("远程端应答了鼠标操作\r\n");
				break;
			case 7://7.8是上锁和解锁，不需要什么处理
			case 8:
			default:
				TRACE("unknow data received!%d\r\n", head.sCmd);
				break;
			}
		}
	}
	return 0;
}