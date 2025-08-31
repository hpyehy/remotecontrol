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


//重叠结构,是用于 处理异步I/O操作的关键结构，是Accept,Recv,Send 等功能的基类
class ServerOverlapped {
public:
    OVERLAPPED m_overlapped;  // 关键的 OVERLAPPED 结构，让 I/O 非阻塞，当 I/O 完成后 IOCP 会触发回调
    DWORD m_operator;  // 当前 I/O 操作类型 (EAccept, ERecv, ESend, EError)
    std::vector<char> m_buffer;  // 数据存储的缓冲区
    ThreadWorker m_worker;  // 线程池任务，执行 I/O 处理的回调函数
    CServer* m_server;  // 服务器对象指针，用于访问服务器功能
    Client* m_client;
    //PCLIENT m_client;  // 绑定的客户端
    WSABUF m_wsabuffer;  // 用于 WSARecv() / WSASend() 的 Windows 缓冲区

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

//ServerOperator 是枚举类型，这里代表模板是ServerOperator类的，之后通过具体化实现功能
//其实不使用模板类的话，就只需要把 ServerOverlapped 中的这些属性分别写在每个类中就行了
template<ServerOperator>//ServerOperator可以是 ENone,EAccept,ERecv,ESend,EError
class AcceptOverlapped :public ServerOverlapped, public ThreadFuncBase
{
public:
    AcceptOverlapped();
    int AcceptWorker();
};
typedef AcceptOverlapped<EAccept> ACCEPTOVERLAPPED;//即 ServerOperator 是 EAccept 的情况


/*
int WSASend(
    SOCKET s,                    // 套接字
    LPWSABUF lpBuffers,          // 发送缓冲区
    DWORD dwBufferCount,         // 发送缓冲区个数
    LPDWORD lpNumberOfBytesSent, // 发送的字节数
    DWORD dwFlags,               // 发送标志
    LPWSAOVERLAPPED lpOverlapped,// OVERLAPPED 结构体（用于异步发送）
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine // 可选，完成回调
);
*/

template<ServerOperator>
class RecvOverlapped :public ServerOverlapped, ThreadFuncBase {
public:
    RecvOverlapped();
    int RecvWorker() {
        int index = 0;

        int len = m_client->Recv();//虽然AcceptWorker中的WSARecv尝试接收数据，但是实际上客户端的数据是这里接收的
        index += len;
        size_t templen = index;
        // 客户端主要就是发送命令信息，不会占用很多空间
        CPacket pack((BYTE*)m_client->m_buffer.data(), templen);
        m_cmd.ExcuteCommand(pack.sCmd, m_client->sendPackets, pack);

        if (index == templen) {
            // m_client->SendWSABuffer() 储存要发送的数据
            // m_client->SendOverlapped() 会在 ThreadIocp 中实现调用 SendWorker 函数
            WSASend((SOCKET)*m_client, m_client->SendWSABuffer(), 1, *m_client, m_client->flags(), 
                m_client->SendOverlapped(), NULL);
            TRACE("命令: %d\r\n", pack.sCmd);
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
         1.Send可能不会立即完成
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
    //使 Client 类对象可以直接当作 SOCKET 使用
    //Client client; SOCKET sock = client;  // 隐式转换，等效于 client.m_sock
    operator SOCKET() {return m_sock;}
    operator PVOID() { return &m_buffer[0]; }
    operator LPOVERLAPPED (){ return &m_overlapped->m_overlapped; }
    operator LPDWORD() {return &m_received_size;}

    // 获取属性内容
    LPWSABUF RecvWSABuffer();
    LPWSABUF SendWSABuffer();
    LPWSAOVERLAPPED RecvOverlapped() { return &m_recv->m_overlapped; }
    LPWSAOVERLAPPED SendOverlapped() { return &m_send->m_overlapped; }

    DWORD& flags() { return m_flags; }
    //获取 m_client 中的地址
    sockaddr_in* GetLocalAddr() { return &m_laddr; }
    sockaddr_in* GetRemoteAddr(){ return &m_raddr; }

    size_t GetBufferSize()const {return m_buffer.size();}

    int Recv() {
        //从客户端接收数据，数据存储在 m_buffer 中
        int ret = recv(m_sock, m_buffer.data() + m_used, m_buffer.size() - m_used, 0);
        if (ret <= 0)return -1;
        m_used += (size_t)ret;
        CTool::Dump((BYTE*)m_buffer.data(), ret);

        return ret;//表示接受成功
    }

    //将传入的数据存入 SendQueue（m_vecSend），为异步发送任务做准备
    int Send(void* buffer, size_t nSize) {
        //为什么这里要用 vector<char> 
        std::vector<char> data(nSize);
        memcpy(data.data(), buffer, nSize);//

        //if (m_vecSend.push_back(data))return 0;
        //return -1;
        return 0;
    }

    //int SendData(std::vector<char>& data);

private:
    DWORD m_received_size;               // 接收数据的字节数
    DWORD m_flags;//存储 WSARecv() 或 WSASend() 函数的 标志位（flags），指定了某些操作的选项或行为
    std::shared_ptr<ACCEPTOVERLAPPED> m_overlapped;// AcceptEx 操作的重叠结构

    //ACCEPTOVERLAPPED m_overlapped;  
    size_t m_used;                  // 已经使用的缓冲区大小
    sockaddr_in m_laddr;            // 本地地址
    sockaddr_in m_raddr;            // 远程地址
    bool m_isbusy;                  // 标识客户端是否正在处理数据
    //SendQueue<std::vector<char>>m_vecSend;//发送数据队列 // 注意这里由CQueue 改为SendQueue

public:
    std::shared_ptr<RECVOVERLAPPED> m_recv;
    std::shared_ptr<SENDOVERLAPPED> m_send;
    SOCKET m_sock;                  // 客户端的套接字
    std::vector<char> m_buffer;     // 存储接收/发送数据的缓冲区
    std::list<CPacket> recvPackets;
    std::list<CPacket> sendPackets;
};




/*
创建监听SOCKET , 绑定IOCP , 使用 AcceptEx 异步接受新客户端
管理客户端连接 , 使用IOCP 和 线程池处理 I/O
*/
class CServer :public ThreadFuncBase{

private:
    Threadpool m_pool;  // 线程池
    HANDLE m_hIOCP;  // IOCP 句柄
    SOCKET m_sock;  // 服务器监听套接字
    sockaddr_in m_addr;  // 服务器地址
    std::map<SOCKET, std::shared_ptr<Client>> m_client;  // 客户端管理容器

public:
    CServer(const std::string& ip = "0.0.0.0", short port = 9527) :m_pool(10) {
        m_hIOCP = INVALID_HANDLE_VALUE;
        //WSA_FLAG_OVERLAPPED 模式,是异步 IO 模式，适用于IOCP 
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


    //就是结合了 IOCP 和 线程池 的TCP连接创建过程：SOCKET，bind，listen，创建 IOCP，启动线程池 (m_pool)，AcceptEx 接收客户端连接并储存连接信息
    bool StartService() {
        CreateSocket();
        if (bind(m_sock, (sockaddr*)&m_addr, sizeof(m_addr)) == -1) {
            closesocket(m_sock);
            m_sock = INVALID_SOCKET;
            return false;
        }

        //监听 (listen)，服务器在 accept() 处理客户端之前，可以暂存最多 3 个未处理的连接，超过 3 个时，新的连接可能会被拒绝
        //当客户端使用 connect() 连接服务器时，服务器 m_sock 会检测到新的连接请求，并触发 AcceptEx() 或 accept() 处理连接
        if (listen(m_sock, 3) == -1) {
            closesocket(m_sock);
            m_sock = INVALID_SOCKET;
            return false;
        }

        //创建 IOCP，INVALID_HANDLE_VALUE: 创建新的 IOCP，NULL: 句柄不绑定 （之后绑定）
        m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 4);//创建 IOCP 端口
        if (m_hIOCP == NULL) {
            closesocket(m_sock);
            m_sock = INVALID_SOCKET;
            m_hIOCP = INVALID_HANDLE_VALUE;
            return false;
        }

        //绑定 SOCKET 到 IOCP：让 m_sock 加入 IOCP 监听，使其支持异步 I/O，(ULONG_PTR)this 传递 服务器对象指针，用于回调时识别 CServer 实例
        CreateIoCompletionPort((HANDLE)m_sock, m_hIOCP, (ULONG_PTR)this, 0);
        // 启动所有线程，等待任务，有任务了才能执行
        m_pool.Invoke();
        // 处理IO任务 : 绑定对象和任务，分配线程，然后上面才能等到任务
        m_pool.DispatchWorker(ThreadWorker(this, (FUNCTYPE)&CServer::threadIocp));
        if (!NewAccept())return false;
        return true;
    }

    //就是将 ServerOverlapped 绑定到 Client
    bool NewAccept() {
        PCLIENT pClient(new Client());  // 创建客户端对象
        pClient->SetOverlapped(pClient);  // 绑定 Overlapped 结构
        // *pClient 就是 SOCKET m_sock 客户端的套接字（重载SOCKET），pClient可以管理多个客户端对象
        m_client.insert(std::pair<SOCKET, PCLIENT>(*pClient, pClient));//m_client是服务器的对象

        // AcceptEx 是 Windows 提供的高效异步接受连接的 API，比 accept() 更高效，负责异步接收接受新客户端连接，然后将新的 Client 对象存入 m_client 容器中
        // 这里 *pClient 作为 SOCKET，PVOID，LPOVERLAPPED 三个角色，利用了 Client 里的 operator 重载
        // 
        if (!AcceptEx(
                m_sock,               // 服务器监听 SOCKET，用于接受新连接
                *pClient,             // 客户端 SOCKET（转换自 pClient，即 m_sock）
                *pClient,             // PVOID() return &m_buffer[0];存储客户端地址信息的缓冲区（转换自 pClient）
                0,                    // 预读取数据长度，通常为 0
                sizeof(sockaddr_in) + 16, // 本地地址存储缓冲区大小
                sizeof(sockaddr_in) + 16, // 远程地址大小
                *pClient,             // LPDWORD() , return &m_received_size; 存储接收字节数的指针
                *pClient              // OVERLAPPED 结构指针 LPOVERLAPPED（转换自 pClient，即 m_overlapped）
            ) ) {
            TRACE("%d\r\n", WSAGetLastError());
            //失败处理,但是 如果是 WSA_IO_PENDING 则不算错误
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
        //开启 SO_REUSEADDR，允许端口复用
        setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
        //PCLIENT& GetFreeClient();
    }

    int threadIocp();
};
