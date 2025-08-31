#include "pch.h"
#include "ClientSocket.h"

// CClientSocket server;
CClientSocket* CClientSocket::m_instance = NULL;//�����ʼ����̬��Ա����
// ����һ�� CHelper �Ķ��󣬴��� CHelper �Ĺ��캯������֤��������ʱ��������ʵ��
CClientSocket::CHelper CClientSocket::m_helper;
// m_instance �ǶԵ���ʵ�� m_instance ��һ��ָ������,���ڵ�����صĺ���
CClientSocket* pclient = CClientSocket::getInstance();

//�����������Ǹ��� WSAGetLastError ���صĴ����룬��ȡ��Ӧ�Ĵ�����Ϣ��������һ��string��ʽ�Ĵ�������
std::string GetErrInfo(int wsaErrCode)
{
	std::string ret;
	LPVOID lpMsgBuf = NULL;
	FormatMessage(
		//��ʾ������Ϣ���� Windows ϵͳ | ϵͳ�Զ����仺�����洢������Ϣ
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		NULL,
		wsaErrCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),//ָ��ʹ��Ĭ�ϵ�����
		(LPTSTR)&lpMsgBuf, 0, NULL);
	ret = (char*)lpMsgBuf;//ֱ�ӽ� lpMsgBuf ת��Ϊ std::string ����
	LocalFree(lpMsgBuf);//�ͷ� FormatMessage ������ڴ棬�����ڴ�й¶
	return ret;
}

void Dump(BYTE* pData, size_t nSize)
{//��ʮ�����Ƹ�ʽ��ӡ����
	std::string strOut;
	for (size_t i = 0; i < nSize; i++)
	{
		char buf[8] = "";
		if (i > 0 && (i % 16 == 0))strOut += "\n";// ÿ����ʾ16���ӽ�
		snprintf(buf, sizeof(buf), "%02X ", pData[i] & 0xFF);
		strOut += buf;
	}
	strOut += "\n";
	OutputDebugStringA(strOut.c_str());
}