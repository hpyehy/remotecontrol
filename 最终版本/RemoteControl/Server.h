#pragma once
#include <MSWSock.h>
#include "Threadpool.h"
#include <map>
#include "Queue.h"
#include "CTool.h"
#include "Command.h"
static CCommand m_cmd;


enum ServerOperator{
    ENone,
    EAccept,
    ERecv,
    ESend,
    EError
};

class CServer;
class Client;
typedef std::shared_ptr<Client> PCLIENT;


//�ص��ṹ,������ �����첽I/O�����Ĺؼ��ṹ����Accept,Recv,Send �ȹ��ܵĻ���
class ServerOverlapped {
public:
    OVERLAPPED m_overlapped;  // �ؼ��� OVERLAPPED �ṹ���� I/O ���������� I/O ��ɺ� IOCP �ᴥ���ص�
    DWORD m_operator;  // ��ǰ I/O �������� (EAccept, ERecv, ESend, EError)
    std::vector<char> m_buffer;  // ���ݴ洢�Ļ�����
    ThreadWorker m_worker;  // �̳߳�����ִ�� I/O ����Ļص�����
    CServer* m_server;  // ����������ָ�룬���ڷ��ʷ���������
    Client* m_client;
    //PCLIENT m_client;  // �󶨵Ŀͻ���
    WSABUF m_wsabuffer;  // ���� WSARecv() / WSASend() �� Windows ������

    virtual ~ServerOverlapped() {
        m_buffer.clear();
    }
};

//template<ServerOperator>class AcceptOverlapped;
//typedef AcceptOverlapped<EAccept> ACCEPTOVERLAPPED;
//template<ServerOperator>class RecvOverlapped;
//typedef RecvOverlapped<ERecv> RECVOVERLAPPED;
//template<ServerOperator>class SendOverlapped;
//typedef SendOverlapped<ESend> SENDOVERLAPPED;

//ServerOperator ��ö�����ͣ��������ģ����ServerOperator��ģ�֮��ͨ�����廯ʵ�ֹ���
//��ʵ��ʹ��ģ����Ļ�����ֻ��Ҫ�� ServerOverlapped �е���Щ���Էֱ�д��ÿ�����о�����
template<ServerOperator>//ServerOperator������ ENone,EAccept,ERecv,ESend,EError
class AcceptOverlapped :public ServerOverlapped, public ThreadFuncBase
{
public:
    AcceptOverlapped();
    int AcceptWorker();
};
typedef AcceptOverlapped<EAccept> ACCEPTOVERLAPPED;//�� ServerOperator �� EAccept �����


/*
int WSASend(
    SOCKET s,                    // �׽���
    LPWSABUF lpBuffers,          // ���ͻ�����
    DWORD dwBufferCount,         // ���ͻ���������
    LPDWORD lpNumberOfBytesSent, // ���͵��ֽ���
    DWORD dwFlags,               // ���ͱ�־
    LPWSAOVERLAPPED lpOverlapped,// OVERLAPPED �ṹ�壨�����첽���ͣ�
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine // ��ѡ����ɻص�
);
*/

template<ServerOperator>
class RecvOverlapped :public ServerOverlapped, ThreadFuncBase {
public:
    RecvOverlapped();
    int RecvWorker() {
        int index = 0;

        int len = m_client->Recv();//��ȻAcceptWorker�е�WSARecv���Խ������ݣ�����ʵ���Ͽͻ��˵�������������յ�
        index += len;
        size_t templen = index;
        // �ͻ�����Ҫ���Ƿ���������Ϣ������ռ�úܶ�ռ�
        CPacket pack((BYTE*)m_client->m_buffer.data(), templen);
        m_cmd.ExcuteCommand(pack.sCmd, m_client->sendPackets, pack);

        if (index == templen) {
            // m_client->SendWSABuffer() ����Ҫ���͵�����
            // m_client->SendOverlapped() ���� ThreadIocp ��ʵ�ֵ��� SendWorker ����
            WSASend((SOCKET)*m_client, m_client->SendWSABuffer(), 1, *m_client, m_client->flags(), 
                m_client->SendOverlapped(), NULL);
            TRACE("����: %d\r\n", pack.sCmd);
        }
        // WSASend(m_client->m_sock, m_client->m_send->m_wsabuffer, 1, &m_client->m_buffer, 0, m_client->m_send->m_overlapped);
        return -1;
    }
};
typedef RecvOverlapped<ERecv> RECVOVERLAPPED;


template<ServerOperator>
class SendOverlapped :public ServerOverlapped, ThreadFuncBase {
public:
    SendOverlapped();
    int SendWorker()
    { //WSA_IO_PENGING
        //TODO:

        /*
         1.Send���ܲ����������
        */
        while (m_client->sendPackets.size() > 0)
        {
            //TRACE("send size: %d", m_client->sendPackets.size());
            CPacket pack = m_client->sendPackets.front();
            m_client->sendPackets.pop_front();
            int ret = send(m_client->m_sock, pack.Data(), pack.Size(), 0);
            TRACE("send ret: %d\r\n", ret);
        }
        closesocket(m_client->m_sock);
        return -1;
    }
};
typedef SendOverlapped<ESend> SENDOVERLAPPED;


template<ServerOperator>
class ErrorOverlapped :public ServerOverlapped, ThreadFuncBase {
public:
    ErrorOverlapped() :m_operator(EError), m_worker(this, &ErrorOverlapped::ErrorWorker) {
        memset(&m_overlapped, 0, sizeof(m_overlapped));
        m_buffer.resize(1024);
    }
    int ErrorWorker() {
        return -1;
    }
};
typedef ErrorOverlapped<EError> ERROROVERLAPPED;


class Client: public ThreadFuncBase {
public:
    Client();
    ~Client() {
        closesocket(m_sock);
        m_recv.reset();
        m_send.reset();
        m_overlapped.reset();
        m_buffer.clear();
        //m_vecSend.Clear();
    }

    void SetOverlapped(PCLIENT& ptr);
    //ʹ Client ��������ֱ�ӵ��� SOCKET ʹ��
    //Client client; SOCKET sock = client;  // ��ʽת������Ч�� client.m_sock
    operator SOCKET() {return m_sock;}
    operator PVOID() { return &m_buffer[0]; }
    operator LPOVERLAPPED (){ return &m_overlapped->m_overlapped; }
    operator LPDWORD() {return &m_received_size;}

    // ��ȡ��������
    LPWSABUF RecvWSABuffer();
    LPWSABUF SendWSABuffer();
    LPWSAOVERLAPPED RecvOverlapped() { return &m_recv->m_overlapped; }
    LPWSAOVERLAPPED SendOverlapped() { return &m_send->m_overlapped; }

    DWORD& flags() { return m_flags; }
    //��ȡ m_client �еĵ�ַ
    sockaddr_in* GetLocalAddr() { return &m_laddr; }
    sockaddr_in* GetRemoteAddr(){ return &m_raddr; }

    size_t GetBufferSize()const {return m_buffer.size();}

    int Recv() {
        //�ӿͻ��˽������ݣ����ݴ洢�� m_buffer ��
        int ret = recv(m_sock, m_buffer.data() + m_used, m_buffer.size() - m_used, 0);
        if (ret <= 0)return -1;
        m_used += (size_t)ret;
        CTool::Dump((BYTE*)m_buffer.data(), ret);

        return ret;//��ʾ���ܳɹ�
    }

    //����������ݴ��� SendQueue��m_vecSend����Ϊ�첽����������׼��
    int Send(void* buffer, size_t nSize) {
        //Ϊʲô����Ҫ�� vector<char> 
        std::vector<char> data(nSize);
        memcpy(data.data(), buffer, nSize);//

        //if (m_vecSend.push_back(data))return 0;
        //return -1;
        return 0;
    }

    //int SendData(std::vector<char>& data);

private:
    DWORD m_received_size;               // �������ݵ��ֽ���
    DWORD m_flags;//�洢 WSARecv() �� WSASend() ������ ��־λ��flags����ָ����ĳЩ������ѡ�����Ϊ
    std::shared_ptr<ACCEPTOVERLAPPED> m_overlapped;// AcceptEx �������ص��ṹ

    //ACCEPTOVERLAPPED m_overlapped;  
    size_t m_used;                  // �Ѿ�ʹ�õĻ�������С
    sockaddr_in m_laddr;            // ���ص�ַ
    sockaddr_in m_raddr;            // Զ�̵�ַ
    bool m_isbusy;                  // ��ʶ�ͻ����Ƿ����ڴ�������
    //SendQueue<std::vector<char>>m_vecSend;//�������ݶ��� // ע��������CQueue ��ΪSendQueue

public:
    std::shared_ptr<RECVOVERLAPPED> m_recv;
    std::shared_ptr<SENDOVERLAPPED> m_send;
    SOCKET m_sock;                  // �ͻ��˵��׽���
    std::vector<char> m_buffer;     // �洢����/�������ݵĻ�����
    std::list<CPacket> recvPackets;
    std::list<CPacket> sendPackets;
};




/*
��������SOCKET , ��IOCP , ʹ�� AcceptEx �첽�����¿ͻ���
����ͻ������� , ʹ��IOCP �� �̳߳ش��� I/O
*/
class CServer :public ThreadFuncBase{

private:
    Threadpool m_pool;  // �̳߳�
    HANDLE m_hIOCP;  // IOCP ���
    SOCKET m_sock;  // �����������׽���
    sockaddr_in m_addr;  // ��������ַ
    std::map<SOCKET, std::shared_ptr<Client>> m_client;  // �ͻ��˹�������

public:
    CServer(const std::string& ip = "0.0.0.0", short port = 9527) :m_pool(10) {
        m_hIOCP = INVALID_HANDLE_VALUE;
        //WSA_FLAG_OVERLAPPED ģʽ,���첽 IO ģʽ��������IOCP 
        m_sock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

        m_addr.sin_family= AF_INET;
        m_addr.sin_port =htons(port);
        m_addr.sin_addr.s_addr = inet_addr(ip.c_str());

    }

    ~CServer() {
        closesocket(m_sock);
        std::map<SOCKET, PCLIENT>::iterator it = m_client.begin();
        for (; it != m_client.end(); it++) {
            it->second.reset();
        }
        m_client.clear();
        CloseHandle(m_hIOCP);//
        m_pool.Stop();
        //WSACleanup();
    }


    //���ǽ���� IOCP �� �̳߳� ��TCP���Ӵ������̣�SOCKET��bind��listen������ IOCP�������̳߳� (m_pool)��AcceptEx ���տͻ������Ӳ�����������Ϣ
    bool StartService() {
        CreateSocket();
        if (bind(m_sock, (sockaddr*)&m_addr, sizeof(m_addr)) == -1) {
            closesocket(m_sock);
            m_sock = INVALID_SOCKET;
            return false;
        }

        //���� (listen)���������� accept() ����ͻ���֮ǰ�������ݴ���� 3 ��δ��������ӣ����� 3 ��ʱ���µ����ӿ��ܻᱻ�ܾ�
        //���ͻ���ʹ�� connect() ���ӷ�����ʱ�������� m_sock ���⵽�µ��������󣬲����� AcceptEx() �� accept() ��������
        if (listen(m_sock, 3) == -1) {
            closesocket(m_sock);
            m_sock = INVALID_SOCKET;
            return false;
        }

        //���� IOCP��INVALID_HANDLE_VALUE: �����µ� IOCP��NULL: ������� ��֮��󶨣�
        m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 4);//���� IOCP �˿�
        if (m_hIOCP == NULL) {
            closesocket(m_sock);
            m_sock = INVALID_SOCKET;
            m_hIOCP = INVALID_HANDLE_VALUE;
            return false;
        }

        //�� SOCKET �� IOCP���� m_sock ���� IOCP ������ʹ��֧���첽 I/O��(ULONG_PTR)this ���� ����������ָ�룬���ڻص�ʱʶ�� CServer ʵ��
        CreateIoCompletionPort((HANDLE)m_sock, m_hIOCP, (ULONG_PTR)this, 0);
        // ���������̣߳��ȴ������������˲���ִ��
        m_pool.Invoke();
        // ����IO���� : �󶨶�������񣬷����̣߳�Ȼ��������ܵȵ�����
        m_pool.DispatchWorker(ThreadWorker(this, (FUNCTYPE)&CServer::threadIocp));
        if (!NewAccept())return false;
        return true;
    }

    //���ǽ� ServerOverlapped �󶨵� Client
    bool NewAccept() {
        PCLIENT pClient(new Client());  // �����ͻ��˶���
        pClient->SetOverlapped(pClient);  // �� Overlapped �ṹ
        // *pClient ���� SOCKET m_sock �ͻ��˵��׽��֣�����SOCKET����pClient���Թ������ͻ��˶���
        m_client.insert(std::pair<SOCKET, PCLIENT>(*pClient, pClient));//m_client�Ƿ������Ķ���

        // AcceptEx �� Windows �ṩ�ĸ�Ч�첽�������ӵ� API���� accept() ����Ч�������첽���ս����¿ͻ������ӣ�Ȼ���µ� Client ������� m_client ������
        // ���� *pClient ��Ϊ SOCKET��PVOID��LPOVERLAPPED ������ɫ�������� Client ��� operator ����
        // 
        if (!AcceptEx(
                m_sock,               // ���������� SOCKET�����ڽ���������
                *pClient,             // �ͻ��� SOCKET��ת���� pClient���� m_sock��
                *pClient,             // PVOID() return &m_buffer[0];�洢�ͻ��˵�ַ��Ϣ�Ļ�������ת���� pClient��
                0,                    // Ԥ��ȡ���ݳ��ȣ�ͨ��Ϊ 0
                sizeof(sockaddr_in) + 16, // ���ص�ַ�洢��������С
                sizeof(sockaddr_in) + 16, // Զ�̵�ַ��С
                *pClient,             // LPDWORD() , return &m_received_size; �洢�����ֽ�����ָ��
                *pClient              // OVERLAPPED �ṹָ�� LPOVERLAPPED��ת���� pClient���� m_overlapped��
            ) ) {
            TRACE("%d\r\n", WSAGetLastError());
            //ʧ�ܴ���,���� ����� WSA_IO_PENDING �������
            if (WSAGetLastError() != WSA_IO_PENDING){
                closesocket(m_sock);
                m_sock = INVALID_SOCKET;
                m_hIOCP = INVALID_HANDLE_VALUE;
                return false;
            }
        }
        return true;
    }

    int AcceptClient(){}

    void BindNewSocket(SOCKET s) {
        CreateIoCompletionPort((HANDLE)s, m_hIOCP, (ULONG_PTR)this, 0);
    }

private:
    void CreateSocket() {
        WSADATA WSAData;
        WSAStartup(MAKEWORD(2, 2), & WSAData);
        m_sock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
        int opt = 1;
        //���� SO_REUSEADDR������˿ڸ���
        setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
        //PCLIENT& GetFreeClient();
    }

    int threadIocp();
};
