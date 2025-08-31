#include "pch.h"
#include "Server.h"
#include "CTool.h"
#pragma warning(disable:4407)

template<ServerOperator op>
//���캯��
AcceptOverlapped<op> ::AcceptOverlapped()
{
	//ThreadWorker �� AcceptWorker() ������������ IOCP ����
	m_worker = ThreadWorker(this, (FUNCTYPE)&AcceptOverlapped<op>::AcceptWorker);
	m_operator = EAccept;//��ǲ�������Ϊ EAccept
	memset(&m_overlapped, 0, sizeof(m_overlapped));//ȷ�� OVERLAPPED �ṹ���ʼ��Ϊ 0
	m_buffer.resize(1024);//�洢 AcceptEx ���صĿͻ���������Ϣ
	m_server = NULL;
}

template<ServerOperator op>
//AcceptEx ����ִ��֮�󣬻�ͨ�� threadIocp �������õ�ǰ����������ͻ������ӣ���õģ��ͻ��� IP �� �˿�
int AcceptOverlapped<op>::AcceptWorker() {
	INT lLength = 0, rLength = 0;
	//GetAcceptExSockaddrs ����AcceptEx���ص�sockaddr ��ַ(���� m_client ֮��)����ȡ���ص�ַ(m_laddr)��Զ�̵�ַ(m_raddr)
	if (m_client->GetBufferSize()>0) {
        sockaddr* plocal = NULL, * promote = NULL;
		GetAcceptExSockaddrs(*m_client, 0,
			sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,
            (sockaddr**)&plocal, &lLength, //���ص�ַ
            (sockaddr**)&promote, &rLength//Զ�̵�ַ
		);
        memcpy(m_client->GetLocalAddr(), plocal, sizeof(sockaddr_in));
        memcpy(m_client->GetRemoteAddr(), promote, sizeof(sockaddr_in));

        m_server->BindNewSocket(*m_client);//�󶨿ͻ��˵� SOCKET �� IOCP

        //���ڽ��տͻ��˷��͵����񣬵������� ���ջ�����û�г�ʼ���������޷����룬������������þ�ֻ��֪ͨ threadiocp ����RecvWorker����
		int ret = WSARecv(
            (SOCKET)*m_client,//�ͻ��˵� SOCKET��ת���� m_client���� m_sock��
            m_client->RecvWSABuffer(),//���ջ���������Ž��յ����ݣ����ݴ��� m_wsabuffer 
            1,//��������������ʾ WSABUF �ṹ�ĸ���������ֻʹ��һ����
            *m_client,//�����ֽ��� ��ָ�루ת���� m_client��ָ�� m_received_size��
            &m_client->flags(), //������־��ͨ��Ϊ 0
            m_client->RecvOverlapped(), //OVERLAPPED �ṹָ�루ת���� m_client��ָ�� m_overlapped��
            NULL//�ص�������IOCP ģʽ�´� NULL��
        );
		if (ret == SOCKET_ERROR && (WSAGetLastError() != WSA_IO_PENDING)){
            TRACE("ret = %d error = %d\r\n", ret, WSAGetLastError());
		}

		if (!m_server->NewAccept())
		{
			return -2;//�д��� 
		}
	}
	return -1;//û����
}


template<ServerOperator op>//op���� ERecv
inline RecvOverlapped<op>::RecvOverlapped() {
    m_operator = op;
    m_worker = ThreadWorker(this, (FUNCTYPE)&RecvOverlapped<op>::RecvWorker);
    memset(&m_overlapped, 0, sizeof(m_overlapped));
    m_buffer.resize(1024 * 256);
}

template<ServerOperator op>//op���� ESend
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
	//WSA_FLAG_OVERLAPPED ģʽ,���첽 IO ģʽ��������IOCP 
	m_sock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	m_buffer.resize(1024);
	memset(&m_laddr, 0, sizeof(m_laddr));
	memset(&m_raddr, 0, sizeof(m_raddr));
}

//������� Client ����󶨵� m_overlapped��m_recv �� m_send��ȷ�����첽 I/O ����������ȷ����ͻ��������Ϣ
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
//    //    // SendWSABuffer() ������������
//    //    int ret = WSASend(m_sock, SendWSABuffer(), 1, &m_received_size, m_flags, &m_send->m_overlapped, NULL);
//    //    if (ret != 0 && (WSAGetLastError() != WSA_IO_PENDING))
//    //        CTool::ShowError();
//    //    return -1;//������
//    //}
//    return 0;
//}

//bool CServer::StartService()


int CServer::threadIocp()
{
    //����������
    DWORD tranferred = 0;//�������ݴ�����ֽ�������������δʹ�ã�
    ULONG_PTR CompletionKey = 0;//�����Ψһ��ʶ�����ﴫ�����IOCP_PARAM*�����������
    OVERLAPPED* lpOverlapped = NULL;//ͨ������ I/O ���������ﲻʹ�ã�ʼ��Ϊ NULL
//�ȴ� IOCP �¼���1. WSARecv()��WSASend()���  2.AcceptEx()������ɣ��¿ͻ������ӣ�3.PostQueuedCompletionStatus()��������
    if (GetQueuedCompletionStatus(m_hIOCP, &tranferred, &CompletionKey, &lpOverlapped, INFINITE)) {
        if (CompletionKey != 0) {
            // CONTAINING_RECORD() �� OVERLAPPED*  ͨ��m_overlappedת���� ServerOverlapped*���Ա���� �ṹ���б����������
            // ��ʱ �����Ӧ�Ľṹ�� ServerOverlapped �е���������Ѿ�׼�����ˣ����Կ�ʼ�������������
            ServerOverlapped* pOverlapped = CONTAINING_RECORD(lpOverlapped, ServerOverlapped, m_overlapped);
            TRACE("pOverlapped->m_operator %d \r\n", pOverlapped->m_operator);

            pOverlapped->m_server = this;

            switch (pOverlapped->m_operator) {
            case EAccept:
            {
                ACCEPTOVERLAPPED* pOver = (ACCEPTOVERLAPPED*)pOverlapped;
                //��һ���߳�������ǰ������m_worker �����ڴ���ִ�� I/O ����Ļص�����
                //AcceptWorker();��ʵ���ǵ����������
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
            return -1;//��Ӧ
        }
    }
    return 0;
}
