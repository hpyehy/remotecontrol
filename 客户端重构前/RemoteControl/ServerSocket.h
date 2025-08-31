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
    //���еľ�̬�������������ֱ��ͨ������ȥ����
    static CServerSocket* getInstance() {
        if (m_instance == NULL) {//��̬����û��thisָ�룬�����޷�ֱ�ӷ��ʳ�Ա����
            m_instance = new CServerSocket();
        }
        return m_instance;
    }

    bool InitSocket(short port) {
        //SOCKET serv_sock = socket(PF_INET, SOCK_STREAM, 0);
        if (serv_sock == -1)return false;
        //У��
        sockaddr_in serv_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;//�������������е�IP
        serv_addr.sin_port = htons(9527);

        if (bind(serv_sock, (sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
            return false;
        }
        if (listen(serv_sock, 1) == -1) {
            return false;
        }

        return true;
    }

    //����������ӣ��������������DealCommand ���ܿͻ��˵����ݴ��浽CPacket�У�
    //callback �� CCommand::RunCommand��arg �� &cmd��port��Run
    int Run(SOCKET_CALLBACK callback, void* arg, short port = 9527) {
        bool ret = InitSocket(port);
        if (ret == false)return -1;
        std::list <CPacket> lstPackets;
        //��ʼ��
        m_callback = callback;
        m_arg = arg;
        int count = 0;
        while (true) {
            if (AcceptClient() == false) {
                if (count >= 3)
                    return -2;
                count++;
                //continue;//���Լ��ӵ�
            }
            //AcceptClient() == -1 �������� m_client Ϊ false������DealCommand() Ҳ����false
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
    //���տͻ��˷��͵�����(�����������ݣ���ҪCPacket(const BYTE* pData, size_t& nSize)�������� )����ʼ�� m_packet ����
    int DealCommand() {
        if (m_client == -1)return -1;
        //char buffer[1024] = "";
        //��������
        char* buffer = new char[BUFFER_SIZE];
        if (buffer == NULL) {
            TRACE("�ڴ治�㣡\r\n");
            return -2;
        }
        memset(buffer, 0, BUFFER_SIZE);
        size_t index = 0; // ��¼ buffer ���Ѵ洢�����ݴ�С

        while (true) {

            // ��ȡ���ݣ������� buffer + index λ�ã�BUFFER_SIZE - index ��ʣ��������len�ǽ��յ������ݵĴ�С
            // len ��¼ÿ�ζ�ȡ�������ݵĴ�С��index��¼��ǰ buffer �е��ܴ�С
            // �����һ�η��͵����ݲ�������recv������ȡ����һ�������ݣ���ô�ڶ��ν���whileѭ����ʱ�򣬾�Ҫ��buffer + indexλ�ÿ�ʼ��
            size_t len = recv(m_client, buffer + index, BUFFER_SIZE - index, 0);
            if (len <= 0) {
                delete[]buffer;
                return -1;
            }
            //TRACE("DealCommand�е�recv���յ����ݳ��ȣ�%d\r\n", len);
            index += len; // ���� index��ָ�� buffer ��ǰ�洢�������ݴ�С

            size_t templen = index;// templen��ʾ ��ǰbuffer�е����ݵ��ܴ�С������ buffer �е������ݴ�С�������յ��ۼ���������
            //�������ݰ����ɶ��󣬣�templen����CPacket֮����޸ĺ󴫳�����֮��templen��ʾ CPacket �����������ݵĴ�С
            m_packet = CPacket((BYTE*)buffer, templen);
                
            //��� templen==0 ����˵��������������߲���������Ҫͨ��whileѭ�����¶�ȡ
            if (templen > 0) {
                //buffer��Ŀ���ַ�����������ʼλ�ã���buffer + templen��Դ��ַ��δ��������ݵ���ʼλ�ã���BUFFER_SIZE - templen����Ҫ�������ֽ���
                memmove(buffer, buffer + templen, BUFFER_SIZE - templen);
                index -= templen;//�Ѵ����СΪ templen �����ݣ����ⲿ�ּ�������ʾ��¼ʣ��δ�������ݵĴ�С
                delete[]buffer;
                return m_packet.sCmd;//���ؽ�����������
            }
        }
        delete[]buffer;
        return -1;
    }

    bool Send(const char* pData, int nSize) {//ֱ�ӷ��� pData
        if (m_client == -1)return false;
        return send(m_client, pData, nSize, 0) > 0;
    }

    bool Send(CPacket& pack) {//��װ���ݰ������ͣ�ͨ��Data()������
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
    //��̬��Ա�������������������ʼ������̬��Ա����ֻ�ܷ��ʾ�̬��Ա����
    static CServerSocket* m_instance;
    
    //CHepler��
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
    static CHelper m_helper;//Ҫд����������

    SOCKET_CALLBACK m_callback;
    void* m_arg;
    CPacket m_packet;
    SOCKET m_client;
    SOCKET serv_sock;

    CServerSocket() {
        m_client = INVALID_SOCKET;
        if (InitSockEnv() == FALSE) // ��ʼ���׽��ֻ���
        {
            MessageBox(NULL, _T("�޷���ʼ���׽��ֻ�����"), _T("��ʼ������"), MB_OK | MB_ICONERROR);
            exit(0); // ��ʼ��ʧ��ʱֱ���˳�����
        }
        serv_sock = socket(PF_INET, SOCK_STREAM, 0);//��ʼ�������׽���
    }

    ~CServerSocket() {
        closesocket(serv_sock);
        WSACleanup(); // �����׽��ֻ���
    }

    BOOL InitSockEnv() {
        WSADATA data;
        if (WSAStartup(MAKEWORD(1, 1), &data) != 0) {
            return FALSE; // �����ʼ��ʧ�ܷ��� FALSE
        }
        return TRUE; // ��ʼ���ɹ����� TRUE
    }

    //˽�л��������캯��
    CServerSocket& operator=(const CServerSocket& ss) {}
    CServerSocket(const CServerSocket& ss) {
        serv_sock = ss.serv_sock;
        m_client = ss.m_client;
    }
};

//extern CServerSocket server; // ����һ���ⲿ���� server