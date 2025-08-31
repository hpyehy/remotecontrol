// RemoteControl.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
#include "pch.h"
#include "framework.h"
#include "RemoteControl.h"
#include "ServerSocket.h"
#include "Command.h"
#include <conio.h>
#include "Queue.h"
#include <mswSock.h>
#include "Server.h"

#ifdef _DEBUG
#define new DEBUG_NEW 
#endif
//#pragma comment( linker, "/subsystem:windows /entry:WinMainCRTStartup" )
//#pragma comment( linker, "/subsystem:windows /entry:mainCRTStartup" )
//#pragma comment( linker, "/subsystem:console /entry:mainCRTStartup" )
//#pragma comment( linker, "/subsystem:console /entry:WinMainCRTStartup" )

//#define INVOKE_PATH _T(C:\\Windows\\System32\\RemoteControl.exe)
#define INVOKE_PATH _T("C:\\Users\\186542\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\RemoteControl.exe")

#define IOCP_LIST_EMPTY 0
#define IOCP_LIST_PUSH 1
#define IOCP_LIST_POP 2

enum{
    IocpListEmpty,
    IocpListPush,
    IocpListPop
};


// 唯一的应用程序对象  
// 分支001
CWinApp theApp;
using namespace std;

//选择是否将程序写入电脑开始菜单根目录或者系统目录和注册表
bool ChooseAutoInvoke(const CString& strPath) {
    TCHAR wcsSystem[MAX_PATH] = _T("");
    if (PathFileExists(strPath)) {
        return true;
    }
    //"HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Run";
    CString strInfo = _T("该程序只允许用于合法的用途！\n");
    strInfo += _T("继续运行该程序，将使得这台机器处于被监控状态！\n");
    strInfo += _T("如果你不希望这样，请按“取消”按钮，退出程序。\n");
    strInfo += _T("按下“是”按钮，该程序将被复制到你的机器上，并随系统启动而自动运行！\n");
    strInfo += _T("按下“否”按钮，程序只运行一次，不会在系统内留下任何东西！\n");
    //MB_ICONWARNING 黄色三角形 ，MB_TOPMOST 指定消息框始终保持在最前端，不会被遮挡
    int ret = MessageBox(NULL, strInfo, _T("警告"), MB_YESNOCANCEL | MB_ICONWARNING | MB_TOPMOST);
    if (ret == IDYES) {
        //WriteRegisterTable();
        if (CTool::WriteStartupDir(strPath) == false) {
            MessageBox(NULL, _T("复制文件失败，是否权限不足？\r\n"), _T("错误"), MB_ICONERROR | MB_TOPMOST);
            return false;
        }
    }
    else if (ret == IDCANCEL) {
        return false;
    }
    return true;
}

//用于储存相关的数据信息
typedef struct IocpPrarm {
    int nOperator; // 操作类型 (Push/Pop/Empty)
    std::string strData; // 存储的数据
    _beginthread_proc_type cbFunc; // 回调函数
    IocpPrarm(int op, const char* sData, _beginthread_proc_type cb = NULL){
        nOperator = op;
        strData = sData;
        cbFunc = cb;
    }
    IocpPrarm() {
        nOperator = -1;
    }

}IOCP_PARAM;


void threadmain(HANDLE hIOCP) {
    //hIOCP：IOCP 句柄，代表任务队列的管理器，用来接收和分发任务
    std::list<std::string>lstString;
    DWORD dwTransferred = 0;//返回数据传输的字节数（本代码中未使用）
    ULONG_PTR CompletionKey = 0;//任务的唯一标识，这里传入的是IOCP_PARAM*（任务参数）
    OVERLAPPED* pOverlapped = NULL;//通常用于 I/O 操作，这里不使用，始终为 NULL

    //当有任务时，GetQueuedCompletionStatus 会返回 TRUE 并将任务信息填充到参数中 , CompletionKey是发送的主要消息
    while (GetQueuedCompletionStatus(hIOCP, &dwTransferred, &CompletionKey, &pOverlapped, INFINITE)) {
        if ((dwTransferred == 0) || (CompletionKey == NULL)) {
            printf("thread is prepare to exit!\r\n");
            break;
        }
        //处理消息内容
        IOCP_PARAM* pParam = (IOCP_PARAM*)CompletionKey;// 取出任务数据

        if (pParam->nOperator == IocpListPush) {
            lstString.push_back(pParam->strData);
        }
        else if (pParam->nOperator == IocpListPop) {
            std::string* pStr = NULL;
            if (lstString.size() > 0) {
                pStr = new std::string(lstString.front());// 复制数据
                lstString.pop_front();
            }
            if (pParam->cbFunc) {//回调函数不为空
                pParam->cbFunc(pStr);
            }
        }
        else if (pParam->nOperator == IocpListEmpty) {
            lstString.clear();
        }
        delete pParam;
    }
}

void threadQueueEntry(HANDLE hIOCP) {
    threadmain(hIOCP);
    _endthread();
}

void func(void* arg){

    std::string* pstr = (std::string*)arg;
    if (pstr != NULL) {
        printf("pop from list:%s\r\n", pstr->c_str());
        delete pstr;
    }
    else {
        printf("pop is empty,no data!\r\n");
    }

}

void test() {
    CQueue<std::string>lstStrings;

    ULONGLONG tick1 = GetTickCount64();
    ULONGLONG tick2 = GetTickCount64();
    ULONGLONG total = GetTickCount64();
    while (GetTickCount64()-total <= 1000) {//分离请求和实现
        //发送 pop 请求消息 
        if (GetTickCount64() - tick1 > 55) {
            lstStrings.push_back("hello world");
            tick1 = GetTickCount64();
        }
        //发送 push 请求消息 
        if (GetTickCount64() - tick2 > 77) {
            std::string str;
            lstStrings.pop_front(str);
            tick2 = GetTickCount64();
            printf("pop from queue:%s \r\n", str.c_str());
        }
        Sleep(1);
    }
    printf("exit done!size %d\r\n", lstStrings.Size());
    lstStrings.Clear();
    //::exit(0);
}

void iocp();

int main()
{
    if (CTool::Init() == false)
        return 1;
    iocp();
    /*
    if (CTool::IsAdmin())
    {
        if (CTool::Init() == false)return 1;
        //OutputDebugString(L"current is run as administrator!\r\n");
        //MessageBox(NULL, _T("管理员"), _T("用户状态"), 0);
        if (ChooseAutoInvoke(INVOKE_PATH) == false) {
            ::exit(0);
        }

        CCommand cmd;
        //通过Run连接网络，通过RunCommand 处理命令
        int ret = CServerSocket::getInstance()->Run(&CCommand::RunCommand, &cmd);
        switch (ret)
        {
        case -1:
            MessageBox(NULL, _T("网络初始化异常，未能成功初始化，请检查网络状态！"), _T("网络初始化失败"), MB_OK | MB_ICONERROR);
            break;
        case -2:
            MessageBox(NULL, _T("多次无法正常接入用户，结束程序！"), _T("接入用户失败！"), MB_OK | MB_ICONERROR);
            break;
        default:
            break;
        }
    }
    else//是普通用户
    {
        //OutputDebugString(L"current is run as normal user!\r\n");
        //获取管理员权限，使用管理员权限创建进程
        if (CTool::RunAsAdmin() == false) {
            CTool::ShowError();
            return 1;
        }
    }
    */

    return 0;
}

class COverlapped {
public:
    OVERLAPPED m_overlapped;//用于存储异步 I/O 操作状态
    DWORD m_operator;// 标识操作类型
    char m_buffer[4096];//用于数据存储的缓冲区
    COverlapped()
    {
        m_operator = 0;
        memset(&m_overlapped, 0, sizeof(m_overlapped));
        memset(m_buffer, 0, sizeof(m_buffer));
    }
};


void iocp()
{
    CServer server;
    server.StartService();
    getchar();
}


//void iocp() {
//    //SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);//TCP
//    //WSA_FLAG_OVERLAPPED 是重叠结构，使socket变成非阻塞
//    SOCKET sock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
//    if (sock == INVALID_SOCKET){
//        CTool::ShowError();
//        return;
//    }
//    HANDLE hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, sock, 4);//4表示2核
//    SOCKET client = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
//    CreateIoCompletionPort((HANDLE)sock, hIOCP, 0, 0);//关联 sock 套接字到 IOCP
//
//    sockaddr_in addr;
//    addr.sin_family = PF_INET;
//    addr.sin_addr.s_addr = inet_addr("0.0.0.0");
//    addr.sin_port = htons(9527);
//    bind(sock, (sockaddr*)&addr, sizeof(addr));
//    listen(sock, 5);
//
//    COverlapped overlapped;
//    overlapped.m_operator = 1;
//    memset(&overlapped, 0, sizeof(OVERLAPPED));
//    DWORD received = 0;
//
//    // 异步连接接受，使用 overlapped 结构体存储状态
//    if (AcceptEx(sock, client, overlapped.m_buffer, 0, sizeof(sockaddr_in) + 16,
//        sizeof(sockaddr_in) + 16, &received, &overlapped.m_overlapped ) == FALSE) {
//        CTool::ShowError();
//    }
//
//    overlapped.m_operator = 2;
//    //WSASend();
//    overlapped.m_operator = 3;
//    //WSARecv();
//
//    //开启线程
//    while (true) {
//        LPOVERLAPPED pOverlapped = NULL;
//        DWORD transferred = 0;
//        //DWORD key = 0;
//        ULONG_PTR key = 0;//64位系统用 
//        if (GetQueuedCompletionStatus(hIOCP, &transferred, &key, &pOverlapped, INFINITY)) {
//            COverlapped* pO = CONTAINING_RECORD(pOverlapped, COverlapped, m_overlapped);
//            switch (pO->m_operator) {
//            case 1:
//                break;
//            }
//
//        }
//
//    }
//
//}
