#pragma once

#include "pch.h"
#include "framework.h"
#include <string>
#include <vector>
#include <list>
#include <map>
#include <mutex>

#define WM_SEND_PACK (WM_USER+1) //CClientSocket::SendPack�����Ͱ�����
#define WM_SEND_PACK_ACK (WM_USER+2) //���Ͱ�����Ӧ��

#pragma pack(push)
#pragma pack(1)
class CPacket
{
public:
    CPacket() :sHead(0), nLength(0), sCmd(0), sSum(0) {}
    //���ݴ������Ϣ����һ�����ݰ����󣬷�װ���͸��ͻ��˵����ݣ�MackeDriverInfo()ʹ�õ���
    CPacket(WORD nCmd, const BYTE* pData, size_t nSize){
        sHead = 0xFEFF;
        nLength = nSize + 4;
        sCmd = nCmd;
        if (nSize > 0) {
            strData.resize(nSize);
            memcpy((void*)strData.c_str(), pData, nSize);
        }
        else {
            strData.clear();
        }
        sSum = 0;
        for (size_t j = 0; j < strData.size(); j++)
        {
            sSum += BYTE(strData[j]) & 0xFF;
        }
    }
    //���ź͵ȺŵĿ������캯��
    CPacket(const CPacket& pack) {
        sHead = pack.sHead;
        nLength = pack.nLength;
        sCmd = pack.sCmd;
        strData = pack.strData;
        sSum = pack.sSum;
    }
    CPacket& operator=(const CPacket& pack) {
        if (this != &pack) {
            sHead = pack.sHead;
            nLength = pack.nLength;
            sCmd = pack.sCmd;
            strData = pack.strData;
            sSum = pack.sSum;
        }
        return *this;
    }

    //�������յ������ݰ�������DealCommand()���õ��ˣ�pData �� buffer��nSize �� buffer �����ݵĴ�С
    CPacket(const BYTE* pData, size_t& nSize){
        size_t i = 0;
        for (; i < nSize; i++) {
            if (*(WORD*)(pData + i) == 0xFEFF) {//Ѱ�� 0xFEFF �Ĳ���
                sHead = *(WORD*)(pData + i);
                i += 2;//i += 2 ʹ i ָ�� 0xFEFF ֮������ݣ�35.00��
                break;
            }
        }
        //�����ݿ��ܲ�ȫ�����߰�ͷδ��ȫ�����յ�����Ϊ i + 4 + 2 + 2 == nSize ��ʱ���൱��ֻ����������
        //���ݰ�ͷ��sHead�������ݳ��ȣ�nLength�������sCmd����У��ͣ�sSum������û���κε���������
        if (i + 4 + 2 + 2 > nSize) {
            nSize = 0;
            return;
        }
        //��ͷ2�����ݳ���4�������ʶ2��
        nLength = *(DWORD*)(pData + i); i += 4;
        // nLength �� �����ʶ + �������� + У��� �����ֽ���
        // ��δ��ȫ���յ����ͷ��أ�����ʧ�ܣ������ nLength + i > 4096 , ˵���ͻ���δ��������ݹ���
        // nLength ����˵��
        if (nLength + i > nSize) {
            nSize = 0;
            return;
        }
        sCmd = *(WORD*)(pData + i); i += 2;//i ָ�� �����ʶ ����

        // nLength ���� sCmd �� sSum�����ʵ�ʵ����ݲ��ִ�СΪ nLength - 4
        if (nLength > 4) {
            strData.resize(nLength - 2 - 2);//�������� strData �Ĵ�С
            //memcpy((void*)strData.c_str(), pData + i, nLength - 4);//�������Ǻܺ�
            memcpy(&strData[0], pData + i, nLength - 4); // ֱ���޸��ַ�������
            TRACE("%s\r\n", strData.c_str() + 12);
            i += nLength - 4;//iָ�����ݲ���֮�󣬼�У��� sCmd
        }
        sSum = *(WORD*)(pData + i); i += 2;//i ָ�� У�鲿�ֵ�ĩβ
        //������У���
        WORD sum = 0;
        for (size_t j = 0; j < strData.size(); j++)
        {
            sum += BYTE(strData[j]) & 0xFF;//��֪��ΪʲôҪ����������Ҳ��Ҫ̫�����
        }
        if (sum == sSum) {
            nSize = i;// �����ɹ��������Ѵ�������ݵĴ�С
            return;
        }
        TRACE("У��ʧ�ܣ��������ݰ�ʧ��\r\n\n");
        nSize = 0;//���򷵻�0����ʾ����������
    }

    ~CPacket() {}

    int Size() {//�����ݵĴ�С
        return nLength + 6;
    }
    //�� CPacket ��Ķ���ת��Ϊ char* ���͵Ķ��������������ڷ��ͣ������Send����ʹ��
    //�����const��ʾ�����޸���ĳ�Ա����
    const char* Data(std::string & strOut) const{
        strOut.resize(nLength + 6);//6��ǰ��İ�ͷ��ʶ�����ݳ��ȣ�nLength�Ǻ���������ʶ���������ݣ�У���
        BYTE* pData = (BYTE*)strOut.c_str();//pData ָ�� strOut ��������������������ݰ�,ת��Ϊ BYTE* ,ȷ�����ֽڼ���д������
        *(WORD*)pData = sHead; pData += 2;//����� *(WORD*)pData = sHead �ǽ� sHead ��ֵ���� pData ָ��ĵ�
        *(DWORD*)(pData) = nLength; pData += 4;
        *(WORD*)pData = sCmd; pData += 2;
        memcpy(pData, strData.c_str(), strData.size());//memcpy() ���� strData ���ݲ���
        pData += strData.size();
        *(WORD*)pData = sSum;
        return strOut.c_str();
    }

public:
    WORD sHead;//�̶�λ 0xFEFF
    DWORD nLength;//�����ȣ��ӿ������ʼ������У�������
    WORD sCmd;//��������
    std::string strData;//������
    WORD sSum;//��У��
    HANDLE hEvent;
    //std::string strOut;//������������
};
#pragma pack(pop)

void Dump(BYTE* pData, size_t nSize);
typedef struct MouseEvent {
    MouseEvent() {
        nAction = 0;   // 0=������1=˫����2=���£�3=�ſ�
        nButton = -1;  // 0=�����1=�Ҽ���2=�м���4=�ް���
        ptXY.x = 0;    // ��� X ����
        ptXY.y = 0;    // ��� Y ����
    }
    WORD nAction;  // �¼�����
    WORD nButton;  // ��������
    POINT ptXY;    // ���λ��
} MOUSEEV, * PMOUSEEV;

//�洢Ŀ¼���ļ�����Ϣ
typedef struct file_info {
    file_info() {
        IsInvalid = FALSE;
        IsDirectory = -1;
        HasNext = TRUE;
        memset(szFileName, 0, sizeof(szFileName));
    }
    BOOL IsInvalid;//�Ƿ���Ч
    BOOL IsDirectory;//�Ƿ�ΪĿ¼ 0 �� 1 ��
    BOOL HasNext;//�Ƿ��к��� 0 û�� 1 ��(�ļ�һ��һ�����ͣ���Ҫ���ж��Ƿ����)
    char szFileName[256];//�ļ���
}FILEINFO, * PFILEINFO;

enum {
    CSM_AUTOCLOSE = 1, //Client Socket Mode  �Զ��ر�ģʽ
};

typedef struct PacketData {
    std::string strData;
    UINT nMode;
    WPARAM wParam;
    PacketData(const char* pData, size_t nLen, UINT mode, WPARAM nParam = 0) {
        strData.resize(nLen);
        memcpy((char*)strData.c_str(), pData, nLen);
        nMode = mode;
        wParam = nParam;
    }
    PacketData(const PacketData& data) {
        strData = data.strData;
        nMode = data.nMode;
        wParam = data.wParam;
    }
    PacketData& operator=(const PacketData& data) {
        if (this != &data) {
            strData = data.strData;
            nMode = data.nMode;
            wParam = data.wParam;
        }
        return *this;
    }
}PACKET_DATA;


std::string GetErrInfo(int wsaErrCode);
class CClientSocket
{
public:
    //���еľ�̬�������������ֱ��ͨ������ȥ����
    static CClientSocket* getInstance() {
        if (m_instance == NULL) {//��̬����û��thisָ�룬�����޷�ֱ�ӷ��ʳ�Ա����
            m_instance = new CClientSocket();
        }
        return m_instance;
    }

    bool InitSocket() {
        if (m_sock != INVALID_SOCKET)CloseSocket();
        m_sock = socket(PF_INET, SOCK_STREAM, 0);//����ÿ�γ�ʼ���������ť���Ϳ������´����׽���
        //TRACE("m_sock = %d\r\n", m_sock);
        //У��
        sockaddr_in serv_addr, cli_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        //TRACE("addr %08X nIP %08X\r\n", inet_addr("127.0.0.1"), nIP);
        //serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        serv_addr.sin_addr.s_addr = htonl(m_nIP);
        serv_addr.sin_port = htons(m_nPort);

        if (serv_addr.sin_addr.s_addr == INADDR_NONE) {
            AfxMessageBox("ָ����IP��ַ�����ڣ�");
            return false;
        }
        int ret = connect(m_sock,(sockaddr*)&serv_addr,sizeof(serv_addr));
        if (ret == -1) {
            AfxMessageBox("����ʧ��!");
            TRACE("����ʧ�ܣ�%d %s\r\n", WSAGetLastError(), GetErrInfo(WSAGetLastError()).c_str());
            return false;
        }

        return true;
    }

#define BUFFER_SIZE 8192000
    //���շ��������͵�����(�����������ݣ���ҪCPacket(const BYTE* pData, size_t& nSize)�������� )����ʼ�� m_packet ����
    int DealCommand() {
        if (m_sock == -1)return -1;
        //char buffer[1024] = "";

        //��������
        char* buffer = m_buffer.data();
        if (buffer == NULL) {
            TRACE("�ڴ治�㣡\r\n");
            return -2;
        }
        //memset(buffer, 0, BUFFER_SIZE);//�����������㣬�ڹ��캯����
        static size_t index = 0; // ��¼ buffer ���Ѵ洢�����ݴ�С
        while (true) {
            // ��ȡ���ݣ������� buffer + index λ�ã�BUFFER_SIZE - index ��ʣ��������len�ǽ��յ������ݵĴ�С
            // len ��¼ÿ�ζ�ȡ�������ݵĴ�С��index��¼��ǰ buffer �е��ܴ�С
            // �����һ�η��͵����ݲ�������recv������ȡ����һ�������ݣ���ô�ڶ��ν���whileѭ����ʱ�򣬾�Ҫ��buffer + indexλ�ÿ�ʼ��
            size_t len = recv(m_sock, buffer + index, BUFFER_SIZE - index, 0);
            if (((int)len <= 0&& (int)index <= 0 ) ) {//
                return -1;
            }
            TRACE("recv len =  %d (0x%08X)   index = %d   (0x%08X)\r\n", len, len, index, index);
            //TRACE("�ͻ���DealCommand()�����յ������ݳ��ȣ� %d\r\n", len);
            index += len; // ���� index��ָ�� buffer ��ǰ�洢�������ݴ�С
            size_t templen = index;// �� templen ��¼ buffer �е������ݴ�С�������յ��ۼ���������
            TRACE("recv len =  %d (0x%08X)   index = %d   (0x%08X)\r\n", len, len, index, index);

            //�������ݰ���ʹ�� CPacket �������ݰ���templen ����ǰ���ݴ�С��templen����CPacket֮����޸ĺ󴫳���
            m_packet = CPacket((BYTE*)buffer, templen);
            TRACE("command %d r\n", m_packet.sCmd);
            //TRACE("CPacket���������ݳ��ȣ� %d\r\n", templen);

            //��� templen==0 ����˵��������������߲���������Ҫͨ��whileѭ�����¶�ȡ
            if (templen > 0) {
                //buffer��Ŀ���ַ�����������ʼλ�ã���buffer + templen��Դ��ַ��δ��������ݵ���ʼλ�ã���BUFFER_SIZE - templen����Ҫ�������ֽ���
                memmove(buffer, buffer + templen, BUFFER_SIZE - templen);
                index -= templen;//�Ѵ����СΪ templen �����ݣ����ⲿ�ּ�������ʾ��¼ʣ��δ�������ݵĴ�С��index�պ�ָ��ղ��ƶ�����֮���λ�ã��´ξʹ�index��ʼд��
                //delete[]buffer;
                return m_packet.sCmd;//���ؽ�����������
            }
        }
        //delete[]buffer;
        return -1;
    }

    bool CClientSocket::SendPacket(HWND hWnd, const CPacket& pack, bool isAutoClosed, WPARAM wParam = 0);

    /*
    bool CClientSocket::SendPacket(const CPacket& pack, std::list<CPacket>& lstPacks, bool isAutoClosed = true)
    {
        // m_hThread ����洢 threadFunc() ���е��߳̾�����������ܱ�֤threadFunc()ֻ��һ��ʵ��
        if (m_sock == INVALID_SOCKET && m_hThread == INVALID_HANDLE_VALUE) {
            //if (InitSocket() == false)return false;
            m_hThread = (HANDLE)_beginthread(&CClientSocket::threadEntry, 0, this);
            TRACE("start thread\r\n");
        }

        
        //ӳ�� HANDLE �� list<CPacket>��ȷ�� lstPacks ֻ�����Լ��� recv() ����
        auto pr = m_mapAck.insert(std::pair<HANDLE, std::list<CPacket>&>(pack.hEvent, lstPacks));
        m_mapAutoClosed.insert(std::pair<HANDLE, bool>(pack.hEvent, isAutoClosed));
        
        m_lock.lock();
        //�����ݰ����뷢�Ͷ��� (�ȴ�֮����)����threadFunc()����send()
        m_lstSend.push_back(pack);//������Զ��push��ͬ��������Ϊ���߳��Ǳ���������
        //������ǰ�̣߳�ֱ��CClientSocket::threadFunc()�� recv()�������ݲ� SetEvent(hEvent) ����
        m_lock.unlock();


        TRACE("cmd %d event %08X thread %d\r\n", pack.sCmd, pack.hEvent, GetCurrentThreadId());
        WaitForSingleObject(pack.hEvent, INFINITE);//�������֮����ܽ�������Ĳ���
        TRACE("cmd %d event %08X thread %d\r\n", pack.sCmd, pack.hEvent, GetCurrentThreadId());

        // ���� pack.hEvent����δ�����¼��� Ѱ�Ҷ�Ӧ���¼��������¼��� m_mapAck ���ó�
        std::map<HANDLE, std::list<CPacket>&>::iterator it;
        it = m_mapAck.find(pack.hEvent);
        if (it != m_mapAck.end()) {
            m_lock.lock();
            m_mapAck.erase(it);
            m_lock.unlock();
            return true;
        }
        return false;
    }
    */

    bool GetFilePath(std::string& strPath) {
        if (((m_packet.sCmd >= 2) && (m_packet.sCmd <= 4)) ||
            (m_packet.sCmd == 9))
        {
            strPath = m_packet.strData;// �����ݰ�����ȡ���ݲ��֣�����Ҫ��·��
            return true;
        }
        return false;
    }

    bool GetMouseEvent(MOUSEEV& mouse) {
        if (m_packet.sCmd == 5) {
            memcpy(&mouse, m_packet.strData.c_str(), sizeof(MOUSEEV));
            return true;
        }
        return false;
    }
    CPacket& GetPacket() {
        return m_packet;
    }
    void CloseSocket() {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
    }

    void UpdateAddress(int nIP, int nPort) {
        while ((m_nIP != nIP) || (m_nPort != nPort)) {
            m_nIP = nIP;
            m_nPort = nPort;
        }
    }

private:
    HANDLE m_eventInvoke;//�����¼�
    UINT m_nThreadID;
    typedef void(CClientSocket::* MSGFUNC)(UINT nMsg, WPARAM wParam, LPARAM lParam);
    std::map<UINT, MSGFUNC> m_mapFunc;
    std::mutex m_lock;
    HANDLE m_hThread;//�����̨�߳�
    bool m_bAutoClose;//���� recv() �Ƿ��Զ��ر�
    std::list<CPacket> m_lstSend;//�洢�����͵����ݰ�
    //�洢 recv() ���ص����ݣ�ȷ�����ݲ�����������ĳ�ȡ����
    std::map<HANDLE, std::list<CPacket>&>m_mapAck;
    std::map<HANDLE, bool>m_mapAutoClosed;//֪ͨ recv()�Ƿ������������
    std::vector<char> m_buffer;
    CPacket m_packet;
    //SOCKET m_client;
    SOCKET m_sock;
    int m_nIP;
    int m_nPort;
    //��̬��Ա�������������������ʼ������̬��Ա����ֻ�ܷ��ʾ�̬��Ա����
    static CClientSocket* m_instance;

    static unsigned __stdcall threadEntry(void* arg);
    void threadFunc();
    void threadFunc2();

    CClientSocket() :
        m_nIP(INADDR_ANY), m_nPort(0),m_sock(INVALID_SOCKET),
        m_bAutoClose(true), m_hThread(INVALID_HANDLE_VALUE), m_lstSend(NULL) {
        if (InitSockEnv() == FALSE) // ��ʼ���׽��ֻ���
        {
            MessageBox(NULL, _T("�޷���ʼ���׽��ֻ�����"), _T("��ʼ������"), MB_OK | MB_ICONERROR);
            exit(0); // ��ʼ��ʧ��ʱֱ���˳�����
        }

        m_eventInvoke = CreateEvent(NULL, TRUE, FALSE, NULL);
        m_hThread = (HANDLE)_beginthreadex(NULL, 0, &CClientSocket::threadEntry, this, 0, &m_nThreadID);
        if (WaitForSingleObject(m_eventInvoke, 100) == WAIT_TIMEOUT)
            TRACE("������Ϣ�����߳�����ʧ���ˣ�\r\n");
        CloseHandle(m_eventInvoke);

        m_buffer.resize(BUFFER_SIZE);
        memset(m_buffer.data(), 0, BUFFER_SIZE);

        struct {
            UINT message;
            MSGFUNC func;
        }funcs[] = {
            {WM_SEND_PACK,&CClientSocket::SendPack},
            {0,NULL}
            //{WM_SEND_PACK, },
        };

        for (int i = 0; funcs[i].message != 0; i++) {
            //m_mapFunc.insert(std::pair<UINT, MSGFUNC>(funcs[i].message,funcs[i].func));
            if (m_mapFunc.insert(std::pair<UINT, MSGFUNC>(funcs[i].message, funcs[i].func)).second == false)
            {
                TRACE("����ʧ�ܣ���Ϣֵ��%d ����ֵ��%O8X ��ţ�%d\r\n", funcs[i].message, funcs[i].func, i);
            }
        }
    }

    bool Send(const char* pData, int nSize) {//ֱ�ӷ��� pData
        if (m_sock == -1)return false;
        return send(m_sock, pData, nSize, 0) > 0;
    }

    bool Send(const CPacket& pack) {//��װ���ݰ������͸���������ͨ��Data()������
        //TRACE("m_sock = %d \r\n", m_sock);
        if (m_sock == -1)return false;
        //Dump((BYTE*)pack.Data(), pack.Size());
        std::string strOut;
        pack.Data(strOut);//�����ݷ�װ���� strOut ��

        return send(m_sock, strOut.c_str(), strOut.size(), 0) > 0;
    }

    ~CClientSocket() {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
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
    CClientSocket& operator=(const CClientSocket& ss) {}
    CClientSocket(const CClientSocket& ss) {
        m_hThread = INVALID_HANDLE_VALUE;
        m_bAutoClose = ss.m_bAutoClose;
        m_nIP = ss.m_nIP;
        m_nPort = ss.m_nPort;
        m_sock = ss.m_sock;
    }

    void SendPack(UINT nMsg, WPARAM WParam, LPARAM lParam);


    //CHepler��
    class CHelper {
    public:
        CHelper() {
            CClientSocket::getInstance();
        }
        ~CHelper() {
            CClientSocket::releaseInstance();
        }
    };
    static void releaseInstance() {
        if (m_instance != NULL) {
            CClientSocket* tmp = m_instance;
            m_instance = NULL;
            delete tmp;
        }
    }
    static CHelper m_helper;//Ҫд����������
};
