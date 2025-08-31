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


void CClientSocket::threadFunc()
{
	std::string strBuffer;
	strBuffer.resize(BUFFER_SIZE);
	char* pBuffer = (char*)strBuffer.c_str();//(char*)���� const char ת��Ϊ char
	int index = 0;
	InitSocket();
	while (m_sock !=INVALID_SOCKET){
		if (m_lstSend.size() > 0) {//��Ϣ�����������ݣ���������
			TRACE("lstSend size : %d\r\n", m_lstSend.size());
			//ȡ����Ҫ����
			m_lock.lock();
			CPacket& head = m_lstSend.front();
			m_lock.unlock();
			//��������
			if (Send(head) == false) {
				TRACE("����ʧ�ܣ�\r\n");
				continue;
			}

			//ʹ�� it �� m_mapAck �и��� head.hEvent Ѱ�ҺͲ�����Ӧ���¼���ÿ���߳�ֻ�ܲ�����Ӧ���¼�
			std::map<HANDLE, std::list<CPacket>&>::iterator it;
			it = m_mapAck.find(head.hEvent);  //���Ҷ�Ӧ�¼�����Ӧ����

			if (it != m_mapAck.end())
			{
				std::map<HANDLE, bool>::iterator it0 = m_mapAutoClosed.find(head.hEvent);
				do {
					// ���շ���˴��������¼��������
					int length = recv(m_sock, pBuffer + index, BUFFER_SIZE - index, 0);
					// length=0 index>0��ʱ��ûִ�е����
					if (length > 0 || index > 0) {
						index += length;
						size_t size = (size_t)index;
						CPacket pack((BYTE*)pBuffer, size);

						if (size > 0) {
							// T0D0:֪ͨ��Ӧ���¼�
							pack.hEvent = head.hEvent;
							it->second.push_back(pack);//���յ������ݴ��� lstPacks ��

							memmove(pBuffer, pBuffer + size, index - size);
							index -= size;//��ȥ����������ݰ��ĳ���

							//��ǰ�¼��������,���� SendPacket()���������յ������ݴ��� lstPacks������Ӧ����ʹ��
							// ��� m_bAutoClose Ϊ true��ֻ����һ�����ݾ͹ر�����
							if (it0->second)
								SetEvent(head.hEvent);
						}
					}
					else if (length <= 0 && index <= 0) {
						CloseSocket();//Ȼ���������ĵط����� m_sock = INVALID_SOCKET;
						SetEvent(head.hEvent);//��Ҫ�ȷ������ر�֮�󣬲���֪ͨ���鴦�����
						if (it0 != m_mapAutoClosed.end()) {
							TRACE("SetEvent %d %d\r\n", head.sCmd, it0->second);
						}
						else {
							TRACE("�쳣����˳���\r\n");
						}
						break;
					}
				} while (it0->second == false);// �� m_bAutoClose Ϊ�����Զ��رգ��Ϳ���һֱ������
			}
			m_lock.lock();
			m_lstSend.pop_front();
			m_mapAutoClosed.erase(head.hEvent);//���ҲҪ���
			m_lock.unlock();
			
			if (InitSocket() == false) {
				InitSocket();
			}
		}
		Sleep(1);
	}
	CloseSocket();
}

//�����߳��Լ����̷߳�����Ϣ
bool CClientSocket::SendPacket(HWND hWnd, const CPacket& pack, bool isAutoClosed, WPARAM wParam)
{
	// ȷ���Ƿ����� CSM_AUTOCLOSE���Ƿ��� recv() ���Զ��ر� socket��
	UINT nMode = isAutoClosed ? CSM_AUTOCLOSE : 0;
	std::string strOut;
	pack.Data(strOut);

	PACKET_DATA* pData = new PACKET_DATA(strOut.c_str(), strOut.size(), nMode, wParam);
	//���� WM_SEND_PACK ��Ϣ�� threadFunc2����Ӧ CClientSocket::SendPack��
	bool ret = PostThreadMessage(m_nThreadID, WM_SEND_PACK, //Ŀ���̵߳� ID��threadFunc2 �̣߳�
		(WPARAM)pData,//��Ϣ����������Ҫ���͵����ݰ�
		(LPARAM)hWnd);//Ŀ�괰�� hWnd 

	if (ret == false) delete pData;
	return ret;
}


unsigned CClientSocket::threadEntry(void* arg)
{
	CClientSocket* thiz = (CClientSocket*)arg;
	thiz->threadFunc2();
	_endthreadex(0);
	return 0;
}


void CClientSocket::threadFunc2()
{
	SetEvent(m_eventInvoke);//����CClientSocket()���캯���߳������ɹ���
	MSG msg;
	// �ȴ� PostThreadMessage ���͵���Ϣ
	while (::GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		//find �Ҳ�������ô����
		if (m_mapFunc.find(msg.message) != m_mapFunc.end())
		{
			//��ʵ���� sendpack
			(this->*m_mapFunc[msg.message])(msg.message, msg.wParam, msg.lParam);
		}
	}
}

void CClientSocket::SendPack(UINT nMsg, WPARAM wParam, LPARAM lParam)
{//����һ����Ϣ�����ݽṹ�����ݺ����ݳ��ȣ�ģʽ���ص���Ϣ�ĵ����ݽṹ(HWND MESSAGE)
	PACKET_DATA data = *(PACKET_DATA*)wParam; // ��ȡ����
	delete (PACKET_DATA*)wParam;//
	HWND hWnd = (HWND)lParam; // Ŀ�괰��

	//��¼�ļ�����Ϣ
	size_t nTemp = data.strData.size();
	CPacket current((BYTE*)data.strData.c_str(), nTemp);

	if (InitSocket() == true)
	{
		int ret = send(m_sock, (char*)data.strData.c_str(), (int)data.strData.size(), 0);
		if (ret > 0) {
			size_t index = 0;
			std::string strBuffer;
			strBuffer.resize(BUFFER_SIZE);
			char* pBuffer = (char*)strBuffer.c_str();

			while (m_sock != INVALID_SOCKET) {
				int length = recv(m_sock, pBuffer + index, BUFFER_SIZE - index, 0); //��������  lengthΪ����
				if (length > 0 || (index > 0)) {
					index += (size_t)length;
					size_t nLen = index;
					CPacket pack((BYTE*)pBuffer, nLen);
					if (nLen > 0) {
						// ���� WM_SEND_PACK_ACK ֪ͨ hWnd��UI �̣߳�
						TRACE("ack pack %d to hWnd %08X %d %d\r\n", pack.sCmd, hWnd, index, nLen);
						TRACE("�ļ�ͷ:   %04X\r\n", *(WORD*)pBuffer + nLen);

						::SendMessage(hWnd, WM_SEND_PACK_ACK, (WPARAM)new CPacket(pack), data.wParam);

						if (data.nMode & CSM_AUTOCLOSE) {
							CloseSocket();
							return;
						}
						index -= nLen;
						memmove(pBuffer, pBuffer + nLen, index);
					}
				}
				else { //TODO:�Է��ر����׽��֣����������豸�쳣
					TRACE("recv failed length %d index %d cmd %d\r\n", length, index, current.sCmd);
					CloseSocket();
					//�������ķ��ͽ�����Ϣ��û����������͹��̲������
					::SendMessage(hWnd, WM_SEND_PACK_ACK, (WPARAM)new CPacket(current.sCmd,NULL,0), 1);
				}
			}
		}
		else {
			CloseSocket();
			//������ֹ����
			::SendMessage(hWnd, WM_SEND_PACK_ACK, NULL, -1);
		}
	}
	else {
		::SendMessage(hWnd, WM_SEND_PACK_ACK, NULL, -2);
	}
		
}
