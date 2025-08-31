#pragma once
#include "ClientSocket.h"
#include "CWatchDialog.h"
#include "RemoteClientDlg.h"
#include "StatusDlg.h"
#include <map>
#include "resource.h"
#include "CTool.h"


#define WM_SHOW_STATUS (WM_USER+3) //չʾ״̬
#define WM_SHOW_WATCH (WM_USER+4) //Զ�̼��
#define WM_SEND_MESSAGE (WM_USER+0x1000)// �Զ�����Ϣ����

class CClientController
{
public:
	static CClientController* getInstance();
	int InitController();
    int Invoke(CWnd* pMainWnd);//����������
    //LRESULT SendMessage(MSG msg);//������Ϣ
    //������� ClientSocket ��
    void UpdateAddress(int nIP, int nPort) {
        CClientSocket::getInstance()->UpdateAddress(nIP, nPort);

    }
    int DealCommand() {
        return CClientSocket::getInstance()->DealCommand();
    }
    void CloseSocket() {
        CClientSocket::getInstance()->CloseSocket();
    }
    //bool SendPacket(const CPacket& pack) {
    //    CClientSocket *pClient = CClientSocket::getInstance();
    //    if (pClient->InitSocket() == false)return false;
    //    pClient->Send(pack);
    //}
    bool SendCommandPacket(HWND hWnd,//���ݰ��յ�����ҪӦ��Ĵ���
        int nCmd,bool bAutoClose = true,BYTE* pData = NULL,
        size_t nLength = 0, WPARAM wParam = 0);

    int GetImage(CImage& image) {
        CClientSocket* pClient = CClientSocket::getInstance();
        return CTool::BytesToImage(image, pClient->GetPacket().strData);
    }

    int DownFile(CString strPath);

    //��ΪRemoteClientDlg�Ǳ߲���ʹ�� m_statusDlg �����Է�������
    void DownloadEnd();

    void StartWatchScreen();
protected:
    void threadWatchScreen();
    static void threadWatchScreenEntry(void* arg);

    //void threadDownloadFile();
    //static void threadDownloadEntry(void* arg);

    CClientController():
        m_statusDlg(&m_remoteDlg),m_watchDlg(&m_watchDlg)
    {
        m_hThread = INVALID_HANDLE_VALUE;
        m_nThreadID = -1;
        m_hThreadDownload = INVALID_HANDLE_VALUE;
        m_hThreadWatch = INVALID_HANDLE_VALUE;
        m_isClosed = true;
    }

    ~CClientController() {
        WaitForSingleObject(m_hThread, 100);
    }

    void threadFunc();
    static unsigned _stdcall threadEntry(void* arg);

    static void releaseInstance() {
        if (m_instance != NULL) {
            delete m_instance;
            m_instance = NULL;
        }
    }

    LRESULT OnShowStatus(UINT nMsg, WPARAM wParam, LPARAM lParam);
    LRESULT OnShowWatch(UINT nMsg, WPARAM wParam, LPARAM lParam);

private:
    typedef struct MsgInfo {
        MSG msg;//��Ϣ����
        LRESULT result;//ִ�н��
        HANDLE hEvent;//����ͬ��
        MsgInfo(MSG m) {
            result = 0;
            //Ϊʲômsg��ֵҪ�����ַ�ʽ��
            memcpy(&msg, &m, sizeof(MSG));
        }
        MsgInfo(const MsgInfo& m) {
            result = m.result;
            memcpy(&msg, &m.msg, sizeof(MSG));
        }
        MsgInfo& operator=(const MsgInfo& m) {
            if (this != &m) {
                result = m.result;
                memcpy(&msg, &m.msg, sizeof(MSG));
            }
            return *this;
        }
    }MSGINFO;

    typedef LRESULT(CClientController::* MSGFUNC)(UINT nMsg, WPARAM wParam, LPARAM lParam);
    //ά����Ϣ���UINT����Ӧ����Ϣ��������ӳ��
    static std::map<UINT, MSGFUNC> m_mapFunc;
    //std::map<UUID, MSGINFO*>m_mapMessage;//��Ϣ�洢��ȷ����Ϣ�ں�̨�̴߳���ǰ���ᶪʧ

    CWatchDialog m_watchDlg;
    CRemoteClientDlg m_remoteDlg;
    CStatusDlg m_statusDlg;
    static CClientController* m_instance;
    HANDLE m_hThread;//�߳̾����ָ���̨��Ϣ�����߳�
    unsigned m_nThreadID;//�߳� ID���������̨�̷߳�����Ϣ

    HANDLE m_hThreadDownload;//���ڲ��������߳�
    HANDLE m_hThreadWatch;
    bool m_isClosed;//�����Ƿ�ر�

    CString m_strRemote;//�����ļ�Զ��·��
    CString m_strLocal;//�����ļ�����·��

    //�ڳ����˳�ʱ�� CHelper �����������Զ��ͷ� CClientController
    class CHelper {
    public:
        CHelper() {
            //CClientController::getInstance();
        }
        ~CHelper() {
        //CHelper ���ܴ��� CClientController ʵ�������øú����ķǾ�̬�汾������releaseInstance��Ҫʱ��̬��
            CClientController::releaseInstance();
        }
    };

    static CHelper m_helper;//Ҫд����������
};

