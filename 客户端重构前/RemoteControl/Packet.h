#pragma once
#include "pch.h"
#include "framework.h"

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

    //�������յ������ݰ�������DealCommand()���õ���
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
        if (nLength + i > nSize) {//��δ��ȫ���յ����ͷ��أ�����ʧ��
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
        *(WORD*)pData = sHead; pData += 2;//����� *(WORD*)pData = sHead �ǽ��� sHead ��ֵ���� pData ָ��ĵ�
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