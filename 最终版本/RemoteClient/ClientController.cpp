#include "pch.h"
#include "ClientController.h"
#include "ClientSocket.h"

//�ͷ������Ǳ�һ������̬��Ա����Ҫ��������������ʵ��
std::map<UINT, CClientController::MSGFUNC> CClientController::m_mapFunc;

CClientController* CClientController::m_instance = NULL;
// ����һ�� CHelper �Ķ��󣬴��� CHelper �Ĺ��캯������֤��������ʱ��������ʵ��
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
	//m_remoteDlg �� CRemoteClientDlg ��Ķ������ڴ����ô���
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

//���� threadFunc() ���� ��Ϣѭ����ȷ����̨�̳߳�������
unsigned _stdcall CClientController::threadEntry(void* arg)
{
	CClientController* thiz=(CClientController*)arg;
	thiz->threadFunc();
	_endthreadex(0);
	return 0;
}

//�����̺߳ͺ�̨�߳�֮�䴫����Ϣ
//LRESULT CClientController::SendMessage(MSG msg)
//{
//	HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
//	if (hEvent == NULL)return -2;
//	MSGINFO info(msg);
//	PostThreadMessage(m_nThreadID, WM_SEND_MESSAGE, (WPARAM)&info, (LPARAM)&hEvent);
//	//WaitForSingleObject(hEvent, -1) �� SetEvent(hEvent) ��һ��
//	//������ֱ�� SetEvent(hEvent) ����
//	WaitForSingleObject(hEvent, INFINITE);
//	CloseHandle(hEvent);
//	return info.result;
//}

//������Ϣ���� GetMessage(&msg, NULL, 0, 0)������ WM_SEND_MESSAGE ��Ϣ
void CClientController::threadFunc()
{
	MSG msg;
	//������Ϣѭ����GetMessage������Ϣ��������
	while (::GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		//˵���յ������� SendMessage() ���Զ�����Ϣ
		if (msg.message == WM_SEND_MESSAGE) {
			MsgInfo* pmsg = (MsgInfo*)msg.wParam;//ָ�� MSGINFO �ṹ���ָ��(��Ϣ����)
			HANDLE hEvent = (HANDLE)msg.lParam;//ָ�� HANDLE ��ָ�루����ͬ����
			std::map<UINT, MSGFUNC>::iterator it = m_mapFunc.find(msg.message);
			//ִ�ж�Ӧ����Ϣ
			if (it != m_mapFunc.end()) {
				pmsg->result = (this->*it->second)(pmsg->msg.message, 
					pmsg->msg.wParam, pmsg->msg.lParam);
			}
			else {
				pmsg->result = -1;
			}
			// ֪ͨ SendMessage() �������
			SetEvent(hEvent);
		}
		else {
			// ����������ͨ��Ϣ����������SendMessage()���͵�
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
//	FILE* pFile = fopen(m_strLocal, "wb+");//�� pFile ��Ϊ�������봦����
//	if (pFile == NULL) {
//		AfxMessageBox(_T("����û��Ȩ�ޱ�����ļ��������ļ��޷�����������"));
//		m_statusDlg.ShowWindow(SW_HIDE);
//		m_remoteDlg.EndWaitCursor();
//		return ;
//	}
//	SendCommandPacket(m_remoteDlg ,4, false, (BYTE*)(LPCSTR)m_strRemote,m_strRemote.GetLength(),(WPARAM)pFile);
//}

int CClientController::DownFile(CString strPath)
{
	//dlg������ʲô
	CFileDialog dlg(FALSE, NULL, strPath,
		OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,//����ֻ����
		NULL, &m_remoteDlg);

	if (dlg.DoModal() == IDOK) {
		m_strRemote = strPath;
		m_strLocal = dlg.GetPathName();

		FILE* pFile = fopen(m_strLocal, "wb+");//�� pFile ��Ϊ�������봦����

		if (pFile == NULL) {
			AfxMessageBox(_T("����û��Ȩ�ޱ�����ļ��������ļ��޷�����������"));
			//m_statusDlg.ShowWindow(SW_HIDE);
			//m_remoteDlg.EndWaitCursor();
			return -1;
		}

		SendCommandPacket(m_remoteDlg, 4, false,
			(BYTE*)(LPCSTR)m_strRemote, m_strRemote.GetLength(), (WPARAM)pFile);
		//m_hThreadDownload = (HANDLE)_beginthread(&CClientController::threadDownloadEntry, 0, this); // �����߳�
		//if (WaitForSingleObject(m_hThreadDownload, 0) != WAIT_TIMEOUT) {
		//	return -1;
		//}
		
		m_remoteDlg.BeginWaitCursor();//����ɵȴ�״̬
		m_statusDlg.m_info.SetWindowTextA(_T("��������ִ���У�"));
		m_statusDlg.ShowWindow(SW_SHOW); // ��ʾ״̬�Ի���
		m_statusDlg.CenterWindow(&m_remoteDlg);
		m_statusDlg.SetActiveWindow(); // ʹ���ڻ�ý���        
	}

	return 0;
}


void CClientController::DownloadEnd()
{
	m_statusDlg.ShowWindow(SW_HIDE);
	m_remoteDlg.EndWaitCursor();
	m_remoteDlg.MessageBox(_T("������ɣ���"), _T("���"));
}

bool CClientController::SendCommandPacket(
	HWND hWnd, 
	int nCmd, 
	bool bAutoClose,
	BYTE* pData, 
	size_t nLength, 
	WPARAM wParam)
{
	TRACE("SendCommandPacket��cmd:%d      %s   start %lld \r\n",nCmd, __FUNCTION__, GetTickCount64());
	CClientSocket* pClient=CClientSocket::getInstance();
	//SendPacket��CClientSocket���캯�������ʼ���ˣ�����֮�����̷߳���Ϣ��ʱ��϶�û����
	bool ret = pClient->SendPacket(hWnd,CPacket(nCmd, pData, nLength), bAutoClose, wParam);

	return ret;
}



void CClientController::StartWatchScreen()
{
	m_isClosed = false;
	//m_watchDlg.SetParent(&m_remoteDlg);

	//CWatchDialog dlg(&m_remoteDlg);//���� parent Ϊ m_remoteDlg
	HANDLE hThread = (HANDLE)_beginthread(CClientController::threadWatchScreenEntry, 0, this);

	//dlg.DoModal();//���ü��Ӵ���ģ̬
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
	while (m_isClosed == false)//����δ�ر�״̬
	{
		// true ����ʾ��ǰ�Ѵ���һ֡ͼ�񣬵ȴ� UI ������Ӧ������������ 
		if (m_watchDlg.isFull() == false)
		{
			ULONGLONG time = GetTickCount64() - nTick;
			if (time < 200)
				Sleep(200 - time);
			nTick = GetTickCount64();
			//�����������������ֱ�� lstPacks �л�ȡ���������
 			int ret = SendCommandPacket(m_watchDlg.GetSafeHwnd(), 6, true, NULL );
			if (ret == 1)
			{
				TRACE("�ɹ�������ͼƬ���\r\n");
			}
			else {
				TRACE("��ȡͼƬʧ�ܣ�ret =%d\r\n",ret);
				//Sleep(1);//���������δ�������ݣ�Sleep(1)���� CPU 100% ռ��
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
	return m_watchDlg.DoModal();//���� 
}
