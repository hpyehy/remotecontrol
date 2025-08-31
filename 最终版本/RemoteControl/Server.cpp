#include "pch.h"
#include "Server.h"
#include "CTool.h"
#pragma warning(disable:4407)

template<ServerOperator op>
//构造函数
AcceptOverlapped<op> ::AcceptOverlapped()
{
	//ThreadWorker 绑定 AcceptWorker() 处理函数，用于 IOCP 调用
	m_worker = ThreadWorker(this, (FUNCTYPE)&AcceptOverlapped<op>::AcceptWorker);
	m_operator = EAccept;//标记操作类型为 EAccept
	memset(&m_overlapped, 0, sizeof(m_overlapped));//确保 OVERLAPPED 结构体初始化为 0
	m_buffer.resize(1024);//存储 AcceptEx 返回的客户端连接信息
	m_server = NULL;
}

template<ServerOperator op>
//AcceptEx 函数执行之后，会通过 threadIocp 函数调用当前函数来处理客户端连接，获得的：客户端 IP 和 端口
int AcceptOverlapped<op>::AcceptWorker() {
	INT lLength = 0, rLength = 0;
	//GetAcceptExSockaddrs 解析AcceptEx传回的sockaddr 地址(存在 m_client 之中)，获取本地地址(m_laddr)和远程地址(m_raddr)
	if (m_client->GetBufferSize()>0) {
        sockaddr* plocal = NULL, * promote = NULL;
		GetAcceptExSockaddrs(*m_client, 0,
			sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,
            (sockaddr**)&plocal, &lLength, //本地地址
            (sockaddr**)&promote, &rLength//远程地址
		);
        memcpy(m_client->GetLocalAddr(), plocal, sizeof(sockaddr_in));
        memcpy(m_client->GetRemoteAddr(), promote, sizeof(sockaddr_in));

        m_server->BindNewSocket(*m_client);//绑定客户端的 SOCKET 到 IOCP

        //用于接收客户端发送的任务，但是由于 接收缓冲区没有初始化，数据无法存入，所以这里的作用就只是通知 threadiocp 调用RecvWorker函数
		int ret = WSARecv(
            (SOCKET)*m_client,//客户端的 SOCKET（转换自 m_client，即 m_sock）
            m_client->RecvWSABuffer(),//接收缓冲区，存放接收的数据，数据存入 m_wsabuffer 
            1,//缓冲区数量，表示 WSABUF 结构的个数（这里只使用一个）
            *m_client,//接收字节数 的指针（转换自 m_client，指向 m_received_size）
            &m_client->flags(), //操作标志，通常为 0
            m_client->RecvOverlapped(), //OVERLAPPED 结构指针（转换自 m_client，指向 m_overlapped）
            NULL//回调函数（IOCP 模式下传 NULL）
        );
		if (ret == SOCKET_ERROR && (WSAGetLastError() != WSA_IO_PENDING)){
            TRACE("ret = %d error = %d\r\n", ret, WSAGetLastError());
		}

		if (!m_server->NewAccept())
		{
			return -2;//有错误 
		}
	}
	return -1;//没错误
}


template<ServerOperator op>//op就是 ERecv
inline RecvOverlapped<op>::RecvOverlapped() {
    m_operator = op;
    m_worker = ThreadWorker(this, (FUNCTYPE)&RecvOverlapped<op>::RecvWorker);
    memset(&m_overlapped, 0, sizeof(m_overlapped));
    m_buffer.resize(1024 * 256);
}

template<ServerOperator op>//op就是 ESend
inline SendOverlapped<op>::SendOverlapped() {
    m_operator = op;
    m_worker = ThreadWorker(this, (FUNCTYPE)&SendOverlapped<op>::SendWorker);
    memset(&m_overlapped, 0, sizeof(m_overlapped));
    m_buffer.resize(1024 * 256);
}


Client::Client():
    m_isbusy(false), m_flags(0),
    m_overlapped(new ACCEPTOVERLAPPED()),
    m_recv(new RECVOVERLAPPED()),//typedef RecvOverlapped<ERecv> RECVOVERLAPPED;
    m_send(new SENDOVERLAPPED()) //typedef SendOverlapped<ESend> SENDOVERLAPPED;
    //,m_vecSend(this, (SENDCALLBACK)& Client::SendData)
{
	//WSA_FLAG_OVERLAPPED 模式,是异步 IO 模式，适用于IOCP 
	m_sock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	m_buffer.resize(1024);
	memset(&m_laddr, 0, sizeof(m_laddr));
	memset(&m_raddr, 0, sizeof(m_raddr));
}

//将传入的 Client 对象绑定到 m_overlapped、m_recv 和 m_send，确保在异步 I/O 操作中能正确处理客户端相关信息
void Client::SetOverlapped(PCLIENT& ptr) {
	m_overlapped->m_client = ptr.get();
    m_recv->m_client = ptr.get();
    m_send->m_client = ptr.get();
}

LPWSABUF Client::RecvWSABuffer()
{
    return &m_recv->m_wsabuffer;
}

LPWSABUF Client::SendWSABuffer()
{
    return &m_send->m_wsabuffer;
}

//int Client::SendData(std::vector<char>& data)
//{
//    //if (m_vecSend.Size() > 0) {
//    //    // SendWSABuffer() 储存数据内容
//    //    int ret = WSASend(m_sock, SendWSABuffer(), 1, &m_received_size, m_flags, &m_send->m_overlapped, NULL);
//    //    if (ret != 0 && (WSAGetLastError() != WSA_IO_PENDING))
//    //        CTool::ShowError();
//    //    return -1;//有问题
//    //}
//    return 0;
//}

//bool CServer::StartService()


int CServer::threadIocp()
{
    //后三个参数
    DWORD tranferred = 0;//返回数据传输的字节数（本代码中未使用）
    ULONG_PTR CompletionKey = 0;//任务的唯一标识，这里传入的是IOCP_PARAM*（任务参数）
    OVERLAPPED* lpOverlapped = NULL;//通常用于 I/O 操作，这里不使用，始终为 NULL
//等待 IOCP 事件：1. WSARecv()或WSASend()完成  2.AcceptEx()任务完成（新客户端连接）3.PostQueuedCompletionStatus()发送任务
    if (GetQueuedCompletionStatus(m_hIOCP, &tranferred, &CompletionKey, &lpOverlapped, INFINITE)) {
        if (CompletionKey != 0) {
            // CONTAINING_RECORD() 把 OVERLAPPED*  通过m_overlapped转换回 ServerOverlapped*，以便访问 结构体中别的数据内容
            // 此时 任务对应的结构体 ServerOverlapped 中的相关数据已经准备好了，可以开始处理相关任务了
            ServerOverlapped* pOverlapped = CONTAINING_RECORD(lpOverlapped, ServerOverlapped, m_overlapped);
            TRACE("pOverlapped->m_operator %d \r\n", pOverlapped->m_operator);

            pOverlapped->m_server = this;

            switch (pOverlapped->m_operator) {
            case EAccept:
            {
                ACCEPTOVERLAPPED* pOver = (ACCEPTOVERLAPPED*)pOverlapped;
                //找一个线程来处理当前的任务，m_worker 就用于储存执行 I/O 处理的回调函数
                //AcceptWorker();其实就是调用这个函数
                m_pool.DispatchWorker(pOver->m_worker);
            }
            break;
            case ERecv:
            {
                RECVOVERLAPPED* pOver = (RECVOVERLAPPED*)pOverlapped;
                m_pool.DispatchWorker(pOver->m_worker);
            }
            break;
            case ESend:
            {
                SENDOVERLAPPED* pOver = (SENDOVERLAPPED*)pOverlapped;
                m_pool.DispatchWorker(pOver->m_worker);
            }
            break;
            case EError:
            {
                ERROROVERLAPPED* pOver = (ERROROVERLAPPED*)pOverlapped;
                m_pool.DispatchWorker(pOver->m_worker);
            }
            break;

            }
        }
        else {
            return -1;//对应
        }
    }
    return 0;
}
