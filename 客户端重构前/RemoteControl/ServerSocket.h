#pragma once

#include "pch.h"
#include "framework.h"
#include "list"
#include "Packet.h"

//void Dump(BYTE* pData, size_t nSize);
//typedef void(*SOCKET_CALLBACK)(void* arg,int status,std::list<CPacket>&lstPacket);
typedef void(*SOCKET_CALLBACK)(void*, int, std::list<CPacket>&, CPacket inPacket);


class CServerSocket
{
public:
    //类中的静态函数在类外可以直接通过类名去访问
    static CServerSocket* getInstance() {
        if (m_instance == NULL) {//静态函数没有this指针，所以无法直接访问成员变量
            m_instance = new CServerSocket();
        }
        return m_instance;
    }

    bool InitSocket(short port) {
        //SOCKET serv_sock = socket(PF_INET, SOCK_STREAM, 0);
        if (serv_sock == -1)return false;
        //校验
        sockaddr_in serv_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;//监听服务器所有的IP
        serv_addr.sin_port = htons(9527);

        if (bind(serv_sock, (sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
            return false;
        }
        if (listen(serv_sock, 1) == -1) {
            return false;
        }

        return true;
    }

    //检查网络连接，调用命令处理函数，DealCommand 接受客户端的数据储存到CPacket中，
    //callback 是 CCommand::RunCommand，arg 是 &cmd，port是Run
    int Run(SOCKET_CALLBACK callback, void* arg, short port = 9527) {
        bool ret = InitSocket(port);
        if (ret == false)return -1;
        std::list <CPacket> lstPackets;
        //初始化
        m_callback = callback;
        m_arg = arg;
        int count = 0;
        while (true) {
            if (AcceptClient() == false) {
                if (count >= 3)
                    return -2;
                count++;
                //continue;//我自己加的
            }
            //AcceptClient() == -1 成立，则 m_client 为 false，下面DealCommand() 也返回false
            int ret = DealCommand();
            if (ret > 0) {
                //CCommand::RunCommand(&cmd, ret)
                m_callback(m_arg, ret, lstPackets , m_packet);
                while (lstPackets.size() > 0) {
                    Send(lstPackets.front());
                    lstPackets.pop_front();
                }

            }
            CloseClient();
        }
        return 0;
    }
protected:
    bool AcceptClient() {
        sockaddr_in cli_addr;
        char buffer[1024];
        int cli_sz = sizeof(cli_addr);
        m_client = accept(serv_sock, (sockaddr*)&cli_addr, &cli_sz);
        //TRACE("m_client= %d \r\n",m_client);
        if (m_client == -1)return false;
        return true;
    }

#define BUFFER_SIZE 4096
    //接收客户端发送的数据(是完整的数据，需要CPacket(const BYTE* pData, size_t& nSize)函数处理 )，初始化 m_packet 对象
    int DealCommand() {
        if (m_client == -1)return -1;
        //char buffer[1024] = "";
        //处理命令
        char* buffer = new char[BUFFER_SIZE];
        if (buffer == NULL) {
            TRACE("内存不足！\r\n");
            return -2;
        }
        memset(buffer, 0, BUFFER_SIZE);
        size_t index = 0; // 记录 buffer 中已存储的数据大小

        while (true) {

            // 读取数据，并存入 buffer + index 位置，BUFFER_SIZE - index 是剩余容量，len是接收到的数据的大小
            // len 记录每次读取到的数据的大小，index记录当前 buffer 中的总大小
            // 如果第一次发送的数据不完整，recv函数读取到了一部分数据，那么第二次进入while循环的时候，就要从buffer + index位置开始存
            size_t len = recv(m_client, buffer + index, BUFFER_SIZE - index, 0);
            if (len <= 0) {
                delete[]buffer;
                return -1;
            }
            //TRACE("DealCommand中的recv，收到数据长度：%d\r\n", len);
            index += len; // 更新 index，指向 buffer 当前存储的总数据大小

            size_t templen = index;// templen表示 当前buffer中的数据的总大小，就是 buffer 中的总数据大小（即接收的累计数据量）
            //解析数据包构成对象，（templen传入CPacket之后会修改后传出），之后templen表示 CPacket 解析过的数据的大小
            m_packet = CPacket((BYTE*)buffer, templen);
                
            //如果 templen==0 ，就说明数据有问题或者不完整，需要通过while循环重新读取
            if (templen > 0) {
                //buffer：目标地址（拷贝后的起始位置），buffer + templen：源地址（未处理的数据的起始位置），BUFFER_SIZE - templen：需要拷贝的字节数
                memmove(buffer, buffer + templen, BUFFER_SIZE - templen);
                index -= templen;//已处理大小为 templen 的数据，将这部分减掉，表示记录剩余未解析数据的大小
                delete[]buffer;
                return m_packet.sCmd;//返回解析出的命令
            }
        }
        delete[]buffer;
        return -1;
    }

    bool Send(const char* pData, int nSize) {//直接发送 pData
        if (m_client == -1)return false;
        return send(m_client, pData, nSize, 0) > 0;
    }

    bool Send(CPacket& pack) {//封装数据包并发送（通过Data()函数）
        if (m_client == -1)return false;
        //Dump((BYTE*)pack.Data(), pack.Size());
        return send(m_client, pack.Data(), pack.Size(), 0) > 0;
    }

    void CloseClient() {
        if(m_client!=INVALID_SOCKET)
        {
            closesocket(m_client);
            m_client = INVALID_SOCKET;
        }
    }

private:
    //静态成员变量类内声明，类外初始化，静态成员函数只能访问静态成员变量
    static CServerSocket* m_instance;
    
    //CHepler类
    class CHelper {
    public:
        CHelper() {
            CServerSocket::getInstance();
        }
        ~CHelper() {
            CServerSocket::releaseInstance();
        }
    };
    static void releaseInstance() {
        if (m_instance != NULL) {
            CServerSocket* tmp = m_instance;
            m_instance = NULL;
            delete tmp;
        }
    }
    static CHelper m_helper;//要写在声明下面

    SOCKET_CALLBACK m_callback;
    void* m_arg;
    CPacket m_packet;
    SOCKET m_client;
    SOCKET serv_sock;

    CServerSocket() {
        m_client = INVALID_SOCKET;
        if (InitSockEnv() == FALSE) // 初始化套接字环境
        {
            MessageBox(NULL, _T("无法初始化套接字环境！"), _T("初始化错误！"), MB_OK | MB_ICONERROR);
            exit(0); // 初始化失败时直接退出程序
        }
        serv_sock = socket(PF_INET, SOCK_STREAM, 0);//初始化网络套接字
    }

    ~CServerSocket() {
        closesocket(serv_sock);
        WSACleanup(); // 清理套接字环境
    }

    BOOL InitSockEnv() {
        WSADATA data;
        if (WSAStartup(MAKEWORD(1, 1), &data) != 0) {
            return FALSE; // 如果初始化失败返回 FALSE
        }
        return TRUE; // 初始化成功返回 TRUE
    }

    //私有化拷贝构造函数
    CServerSocket& operator=(const CServerSocket& ss) {}
    CServerSocket(const CServerSocket& ss) {
        serv_sock = ss.serv_sock;
        m_client = ss.m_client;
    }
};

//extern CServerSocket server; // 声明一个外部变量 server