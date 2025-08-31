#pragma once
#include "ClientSocket.h"
#include "CWatchDialog.h"
#include "RemoteClientDlg.h"
#include "StatusDlg.h"
#include <map>
#include "resource.h"
#include "CTool.h"


#define WM_SHOW_STATUS (WM_USER+3) //展示状态
#define WM_SHOW_WATCH (WM_USER+4) //远程监控
#define WM_SEND_MESSAGE (WM_USER+0x1000)// 自定义消息处理

class CClientController
{
public:
	static CClientController* getInstance();
	int InitController();
    int Invoke(CWnd* pMainWnd);//启动主窗口
    //LRESULT SendMessage(MSG msg);//发送消息
    //网络相关 ClientSocket 类
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
    bool SendCommandPacket(HWND hWnd,//数据包收到后，需要应答的窗口
        int nCmd,bool bAutoClose = true,BYTE* pData = NULL,
        size_t nLength = 0, WPARAM wParam = 0);

    int GetImage(CImage& image) {
        CClientSocket* pClient = CClientSocket::getInstance();
        return CTool::BytesToImage(image, pClient->GetPacket().strData);
    }

    int DownFile(CString strPath);

    //因为RemoteClientDlg那边不能使用 m_statusDlg ，所以放在这里
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
        MSG msg;//消息内容
        LRESULT result;//执行结果
        HANDLE hEvent;//进程同步
        MsgInfo(MSG m) {
            result = 0;
            //为什么msg赋值要用这种方式？
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
    //维护消息编号UINT到对应的消息处理函数的映射
    static std::map<UINT, MSGFUNC> m_mapFunc;
    //std::map<UUID, MSGINFO*>m_mapMessage;//消息存储，确保消息在后台线程处理前不会丢失

    CWatchDialog m_watchDlg;
    CRemoteClientDlg m_remoteDlg;
    CStatusDlg m_statusDlg;
    static CClientController* m_instance;
    HANDLE m_hThread;//线程句柄，指向后台消息处理线程
    unsigned m_nThreadID;//线程 ID，用于向后台线程发送消息

    HANDLE m_hThreadDownload;//用于操作下载线程
    HANDLE m_hThreadWatch;
    bool m_isClosed;//监视是否关闭

    CString m_strRemote;//下载文件远程路径
    CString m_strLocal;//下载文件本地路径

    //在程序退出时由 CHelper 的析构函数自动释放 CClientController
    class CHelper {
    public:
        CHelper() {
            //CClientController::getInstance();
        }
        ~CHelper() {
        //CHelper 不能创建 CClientController 实例来调用该函数的非静态版本，所以releaseInstance需要时静态的
            CClientController::releaseInstance();
        }
    };

    static CHelper m_helper;//要写在声明下面
};

