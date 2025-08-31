#pragma once

#include "pch.h"
#include "framework.h"
#include <string>
#include <vector>

#pragma pack(push)
#pragma pack(1)
class CPacket
{
public:
    CPacket() :sHead(0), nLength(0), sCmd(0), sSum(0) {}
    //���ݴ������Ϣ����һ�����ݰ����󣬷�װ���͸��ͻ��˵����ݣ�MackeDriverInfo()ʹ�õ���
    CPacket(WORD nCmd, const BYTE* pData, size_t nSize) {
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
    CPacket(const BYTE* pData, size_t& nSize) {
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
        //��δ��ȫ���յ����ͷ��أ�����ʧ�ܣ������ nLength + i > 4096 , ˵���ͻ���δ��������ݹ���
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
            nSize = i;// �����ɹ������ؽ���������ݰ���С
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
    const char* Data() {
        strOut.resize(nLength + 6);//6��ǰ��İ�ͷ��ʶ�����ݳ��ȣ�nLength�Ǻ���������ʶ���������ݣ�У���
        BYTE* pData = (BYTE*)strOut.c_str();//pData ָ�� strOut ��������������������ݰ�,ת��Ϊ BYTE* ,ȷ�����ֽڼ���д������
        *(WORD*)pData = sHead; pData += 2;//����� *(WORD*)pData = sHead �ǽ� sHead ��ֵ���� pData ָ��ĵ�
        *(DWORD*)(pData) = nLength; pData += 4;
        *(WORD*)pData = sCmd; pData += 2;
        memcpy(pData, strData.c_str(), strData.size());//emcpy() ���� strData ���ݲ���
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
    std::string strOut;//������������
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

    bool InitSocket(int nIP,int nPort) {
        if (m_sock != INVALID_SOCKET)CloseSocket();
        m_sock = socket(PF_INET, SOCK_STREAM, 0);//����ÿ�γ�ʼ���������ť���Ϳ������´����׽���
        TRACE("m_sock = %d\r\n", m_sock);
        //У��
        sockaddr_in serv_addr, cli_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        //TRACE("addr %08X nIP %08X\r\n", inet_addr("127.0.0.1"), nIP);
        //serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        serv_addr.sin_addr.s_addr = htonl(nIP);
        serv_addr.sin_port = htons(nPort);

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

#define BUFFER_SIZE 4096000
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
        //size_t index = 0;
        while (true) {
            // ��ȡ���ݣ������� buffer + index λ�ã�BUFFER_SIZE - index ��ʣ��������len�ǽ��յ������ݵĴ�С
            // len ��¼ÿ�ζ�ȡ�������ݵĴ�С��index��¼��ǰ buffer �е��ܴ�С
            // �����һ�η��͵����ݲ�������recv������ȡ����һ�������ݣ���ô�ڶ��ν���whileѭ����ʱ�򣬾�Ҫ��buffer + indexλ�ÿ�ʼ��
            size_t len = recv(m_sock, buffer + index, BUFFER_SIZE - index, 0);
            if ((len <= 0&& index <= 0 ) ) {//
                return -1;
            }
            //TRACE("�ͻ���DealCommand()�����յ������ݳ��ȣ� %d\r\n", len);
            index += len; // ���� index��ָ�� buffer ��ǰ�洢�������ݴ�С

            size_t templen = index;// �� len ��¼ buffer �е������ݴ�С�������յ��ۼ���������
            //�������ݰ���ʹ�� CPacket �������ݰ���len ����ǰ���ݴ�С��len����CPacket֮����޸ĺ󴫳���
            m_packet = CPacket((BYTE*)buffer, templen);
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

    bool Send(const char* pData, int nSize) {//ֱ�ӷ��� pData
        if (m_sock == -1)return false;
        return send(m_sock, pData, nSize, 0) > 0;
    }
    bool Send(CPacket& pack) {//��װ���ݰ������͸���������ͨ��Data()������
        //TRACE("m_sock = %d \r\n", m_sock);
        if (m_sock == -1)return false;
        //Dump((BYTE*)pack.Data(), pack.Size());
        return send(m_sock, pack.Data(), pack.Size(), 0) > 0;
    }

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
private:
    //��̬��Ա�������������������ʼ������̬��Ա����ֻ�ܷ��ʾ�̬��Ա����
    static CClientSocket* m_instance;

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

    std::vector<char> m_buffer;
    CPacket m_packet;
    //SOCKET m_client;
    SOCKET m_sock;
    CClientSocket() {
        if (InitSockEnv() == FALSE) // ��ʼ���׽��ֻ���
        {
            MessageBox(NULL, _T("�޷���ʼ���׽��ֻ�����"), _T("��ʼ������"), MB_OK | MB_ICONERROR);
            exit(0); // ��ʼ��ʧ��ʱֱ���˳�����
        }
        m_buffer.resize(BUFFER_SIZE);
        memset(m_buffer.data(), 0, BUFFER_SIZE);
    }

    ~CClientSocket() {
        closesocket(m_sock);
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
        m_sock = ss.m_sock;
    }
};

//extern CClientSocket server; // ����һ���ⲿ���� server
