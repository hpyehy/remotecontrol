#include "pch.h"
#include "ClientController.h"
#include "ClientSocket.h"

//和服务器那边一样，静态成员变量要类内声明，类外实现
std::map<UINT, CClientController::MSGFUNC> CClientController::m_mapFunc;

CClientController* CClientController::m_instance = NULL;
// 创建一个 CHelper 的对象，触发 CHelper 的构造函数，保证程序启动时创建单例实例
CClientController::CHelper CClientController::m_helper;

CClientController* CClientController::getInstance()
{
	if (m_instance == NULL) {
		m_instance = new CClientController();
		TRACE("CClientController size is %d\r\n", sizeof(*m_instance));;
		struct { UINT nMsg; MSGFUNC func; }MsgFuncs[] =
		{
			{WM_SHOW_STATUS,&CClientController::OnShowStatus},
			{WM_SHOW_WATCH,&CClientController::OnShowWatch},
			{(UINT)-1,NULL}
		};
		for (int i = 0; MsgFuncs[i].func != NULL; i++) {
			m_mapFunc.insert(std::pair<UINT, MSGFUNC>(MsgFuncs
				[i].nMsg, MsgFuncs[i].func));
		}
	}
	return m_instance;
	//return nullptr;
}

int CClientController::Invoke(CWnd* pMainWnd)
{
	//m_remoteDlg 是 CRemoteClientDlg 类的对象，用于创建该窗口
	pMainWnd = &m_remoteDlg;
	return m_remoteDlg.DoModal();
	//return 0;
}

int CClientController::InitController()
{
	m_hThread = (HANDLE)_beginthreadex(NULL, 0,
		&CClientController::threadEntry, this, 0, &m_nThreadID);
	m_statusDlg.Create(IDD_DLG_STATUS,&m_remoteDlg);
	return 0;
}

//启动 threadFunc() 进入 消息循环，确保后台线程持续运行
unsigned _stdcall CClientController::threadEntry(void* arg)
{
	CClientController* thiz=(CClientController*)arg;
	thiz->threadFunc();
	_endthreadex(0);
	return 0;
}

//在主线程和后台线程之间传递消息
//LRESULT CClientController::SendMessage(MSG msg)
//{
//	HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
//	if (hEvent == NULL)return -2;
//	MSGINFO info(msg);
//	PostThreadMessage(m_nThreadID, WM_SEND_MESSAGE, (WPARAM)&info, (LPARAM)&hEvent);
//	//WaitForSingleObject(hEvent, -1) 和 SetEvent(hEvent) 是一对
//	//阻塞，直到 SetEvent(hEvent) 触发
//	WaitForSingleObject(hEvent, INFINITE);
//	CloseHandle(hEvent);
//	return info.result;
//}

//监听消息队列 GetMessage(&msg, NULL, 0, 0)，处理 WM_SEND_MESSAGE 消息
void CClientController::threadFunc()
{
	MSG msg;
	//建立消息循环，GetMessage监听消息（阻塞）
	while (::GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		//说明收到了来自 SendMessage() 的自定义消息
		if (msg.message == WM_SEND_MESSAGE) {
			MsgInfo* pmsg = (MsgInfo*)msg.wParam;//指向 MSGINFO 结构体的指针(消息内容)
			HANDLE hEvent = (HANDLE)msg.lParam;//指向 HANDLE 的指针（用于同步）
			std::map<UINT, MSGFUNC>::iterator it = m_mapFunc.find(msg.message);
			//执行对应的消息
			if (it != m_mapFunc.end()) {
				pmsg->result = (this->*it->second)(pmsg->msg.message, 
					pmsg->msg.wParam, pmsg->msg.lParam);
			}
			else {
				pmsg->result = -1;
			}
			// 通知 SendMessage() 处理完成
			SetEvent(hEvent);
		}
		else {
			// 处理其他普通消息，即不是由SendMessage()发送的
			std::map<UINT, MSGFUNC>::iterator it = m_mapFunc.find(msg.message);
			if (it != m_mapFunc.end()) {
				(this->*it->second)(msg.message, msg.wParam, msg.lParam);
			}
		}
	}
}

//void CClientController::threadDownloadEntry(void* arg)
//{
//	CClientController* thiz = (CClientController*)arg;
//	thiz->threadDownloadFile();
//	_endthread();
//}
//
//void CClientController::threadDownloadFile()
//{
//	FILE* pFile = fopen(m_strLocal, "wb+");//将 pFile 作为参数传入处理函数
//	if (pFile == NULL) {
//		AfxMessageBox(_T("本地没有权限保存该文件，或者文件无法创建！！！"));
//		m_statusDlg.ShowWindow(SW_HIDE);
//		m_remoteDlg.EndWaitCursor();
//		return ;
//	}
//	SendCommandPacket(m_remoteDlg ,4, false, (BYTE*)(LPCSTR)m_strRemote,m_strRemote.GetLength(),(WPARAM)pFile);
//}

int CClientController::DownFile(CString strPath)
{
	//dlg作用是什么
	CFileDialog dlg(FALSE, NULL, strPath,
		OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,//隐藏只读，
		NULL, &m_remoteDlg);

	if (dlg.DoModal() == IDOK) {
		m_strRemote = strPath;
		m_strLocal = dlg.GetPathName();

		FILE* pFile = fopen(m_strLocal, "wb+");//将 pFile 作为参数传入处理函数

		if (pFile == NULL) {
			AfxMessageBox(_T("本地没有权限保存该文件，或者文件无法创建！！！"));
			//m_statusDlg.ShowWindow(SW_HIDE);
			//m_remoteDlg.EndWaitCursor();
			return -1;
		}

		SendCommandPacket(m_remoteDlg, 4, false,
			(BYTE*)(LPCSTR)m_strRemote, m_strRemote.GetLength(), (WPARAM)pFile);
		//m_hThreadDownload = (HANDLE)_beginthread(&CClientController::threadDownloadEntry, 0, this); // 开启线程
		//if (WaitForSingleObject(m_hThreadDownload, 0) != WAIT_TIMEOUT) {
		//	return -1;
		//}
		
		m_remoteDlg.BeginWaitCursor();//光标变成等待状态
		m_statusDlg.m_info.SetWindowTextA(_T("命令正在执行中！"));
		m_statusDlg.ShowWindow(SW_SHOW); // 显示状态对话框
		m_statusDlg.CenterWindow(&m_remoteDlg);
		m_statusDlg.SetActiveWindow(); // 使窗口获得焦点        
	}

	return 0;
}


void CClientController::DownloadEnd()
{
	m_statusDlg.ShowWindow(SW_HIDE);
	m_remoteDlg.EndWaitCursor();
	m_remoteDlg.MessageBox(_T("下载完成！！"), _T("完成"));
}

bool CClientController::SendCommandPacket(
	HWND hWnd, 
	int nCmd, 
	bool bAutoClose,
	BYTE* pData, 
	size_t nLength, 
	WPARAM wParam)
{
	TRACE("SendCommandPacket：cmd:%d      %s   start %lld \r\n",nCmd, __FUNCTION__, GetTickCount64());
	CClientSocket* pClient=CClientSocket::getInstance();
	//SendPacket在CClientSocket构造函数里面初始化了，所以之后向线程发消息的时候肯定没问题
	bool ret = pClient->SendPacket(hWnd,CPacket(nCmd, pData, nLength), bAutoClose, wParam);

	return ret;
}



void CClientController::StartWatchScreen()
{
	m_isClosed = false;
	//m_watchDlg.SetParent(&m_remoteDlg);

	//CWatchDialog dlg(&m_remoteDlg);//设置 parent 为 m_remoteDlg
	HANDLE hThread = (HANDLE)_beginthread(CClientController::threadWatchScreenEntry, 0, this);

	//dlg.DoModal();//设置监视窗口模态
	m_watchDlg.DoModal();
	m_isClosed = true;
	WaitForSingleObject(hThread, 500);
}

void CClientController::threadWatchScreenEntry(void* arg)
{
	CClientController* thiz = (CClientController*)arg;
	thiz->threadWatchScreen();
	_endthread();
}

void CClientController::threadWatchScreen()
{
	Sleep(50);
	ULONGLONG nTick = GetTickCount64();
	while (m_isClosed == false)//窗口未关闭状态
	{
		// true ：表示当前已存有一帧图像，等待 UI 处理，不应继续请求数据 
		if (m_watchDlg.isFull() == false)
		{
			ULONGLONG time = GetTickCount64() - nTick;
			if (time < 200)
				Sleep(200 - time);
			nTick = GetTickCount64();
			//程序会在这里阻塞，直到 lstPacks 中获取所需的数据
 			int ret = SendCommandPacket(m_watchDlg.GetSafeHwnd(), 6, true, NULL );
			if (ret == 1)
			{
				TRACE("成功发请求图片命令！\r\n");
			}
			else {
				TRACE("获取图片失败！ret =%d\r\n",ret);
				//Sleep(1);//如果服务器未返回数据，Sleep(1)避免 CPU 100% 占用
			}
		}
		Sleep(1);
	}
}


LRESULT CClientController::OnShowStatus(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	return m_statusDlg.ShowWindow(SW_SHOW);
}

LRESULT CClientController::OnShowWatch(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	return m_watchDlg.DoModal();//阻塞 
}
