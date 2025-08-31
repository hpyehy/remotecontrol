#include "pch.h"
#include "ClientSocket.h"

// CClientSocket server;
CClientSocket* CClientSocket::m_instance = NULL;//类外初始化静态成员变量
// 创建一个 CHelper 的对象，触发 CHelper 的构造函数，保证程序启动时创建单例实例
CClientSocket::CHelper CClientSocket::m_helper;
// m_instance 是对单例实例 m_instance 的一个指针引用,用于调用相关的函数
CClientSocket* pclient = CClientSocket::getInstance();

//函数的作用是根据 WSAGetLastError 返回的错误码，获取对应的错误信息，并返回一个string形式的错误描述
std::string GetErrInfo(int wsaErrCode)
{
	std::string ret;
	LPVOID lpMsgBuf = NULL;
	FormatMessage(
		//表示错误信息来自 Windows 系统 | 系统自动分配缓冲区存储错误信息
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		NULL,
		wsaErrCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),//指定使用默认的语言
		(LPTSTR)&lpMsgBuf, 0, NULL);
	ret = (char*)lpMsgBuf;//直接将 lpMsgBuf 转换为 std::string 类型
	LocalFree(lpMsgBuf);//释放 FormatMessage 分配的内存，避免内存泄露
	return ret;
}

void Dump(BYTE* pData, size_t nSize)
{//以十六进制格式打印数据
	std::string strOut;
	for (size_t i = 0; i < nSize; i++)
	{
		char buf[8] = "";
		if (i > 0 && (i % 16 == 0))strOut += "\n";// 每行显示16个子节
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
	char* pBuffer = (char*)strBuffer.c_str();//(char*)：将 const char 转化为 char
	int index = 0;
	InitSocket();
	while (m_sock !=INVALID_SOCKET){
		if (m_lstSend.size() > 0) {//消息队列中有数据，则处理数据
			TRACE("lstSend size : %d\r\n", m_lstSend.size());
			//取数据要上锁
			m_lock.lock();
			CPacket& head = m_lstSend.front();
			m_lock.unlock();
			//发送数据
			if (Send(head) == false) {
				TRACE("发送失败！\r\n");
				continue;
			}

			//使用 it 在 m_mapAck 中根据 head.hEvent 寻找和操作对应的事件，每个线程只能操作对应的事件
			std::map<HANDLE, std::list<CPacket>&>::iterator it;
			it = m_mapAck.find(head.hEvent);  //查找对应事件的响应数据

			if (it != m_mapAck.end())
			{
				std::map<HANDLE, bool>::iterator it0 = m_mapAutoClosed.find(head.hEvent);
				do {
					// 接收服务端传回来的事件相关数据
					int length = recv(m_sock, pBuffer + index, BUFFER_SIZE - index, 0);
					// length=0 index>0的时候没执行到这里？
					if (length > 0 || index > 0) {
						index += length;
						size_t size = (size_t)index;
						CPacket pack((BYTE*)pBuffer, size);

						if (size > 0) {
							// T0D0:通知对应的事件
							pack.hEvent = head.hEvent;
							it->second.push_back(pack);//将收到的数据存入 lstPacks 中

							memmove(pBuffer, pBuffer + size, index - size);
							index -= size;//减去处理掉的数据包的长度

							//当前事件处理完成,唤醒 SendPacket()，让它把收到的数据存入 lstPacks，供相应函数使用
							// 如果 m_bAutoClose 为 true，只接收一次数据就关闭连接
							if (it0->second)
								SetEvent(head.hEvent);
						}
					}
					else if (length <= 0 && index <= 0) {
						CloseSocket();//然后在析构的地方加了 m_sock = INVALID_SOCKET;
						SetEvent(head.hEvent);//需要等服务器关闭之后，才能通知事情处理完成
						if (it0 != m_mapAutoClosed.end()) {
							TRACE("SetEvent %d %d\r\n", head.sCmd, it0->second);
						}
						else {
							TRACE("异常情况退出！\r\n");
						}
						break;
					}
				} while (it0->second == false);// 即 m_bAutoClose 为错，不自动关闭，就可以一直接受了
			}
			m_lock.lock();
			m_lstSend.pop_front();
			m_mapAutoClosed.erase(head.hEvent);//这个也要清除
			m_lock.unlock();
			
			if (InitSocket() == false) {
				InitSocket();
			}
		}
		Sleep(1);
	}
	CloseSocket();
}

//创建线程以及给线程发送消息
bool CClientSocket::SendPacket(HWND hWnd, const CPacket& pack, bool isAutoClosed, WPARAM wParam)
{
	// 确定是否启用 CSM_AUTOCLOSE（是否在 recv() 后自动关闭 socket）
	UINT nMode = isAutoClosed ? CSM_AUTOCLOSE : 0;
	std::string strOut;
	pack.Data(strOut);

	PACKET_DATA* pData = new PACKET_DATA(strOut.c_str(), strOut.size(), nMode, wParam);
	//发送 WM_SEND_PACK 消息到 threadFunc2（对应 CClientSocket::SendPack）
	bool ret = PostThreadMessage(m_nThreadID, WM_SEND_PACK, //目标线程的 ID（threadFunc2 线程）
		(WPARAM)pData,//消息参数，传递要发送的数据包
		(LPARAM)hWnd);//目标窗口 hWnd 

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
	SetEvent(m_eventInvoke);//告诉CClientSocket()构造函数线程启动成功了
	MSG msg;
	// 等待 PostThreadMessage 发送的消息
	while (::GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		//find 找不到会怎么样？
		if (m_mapFunc.find(msg.message) != m_mapFunc.end())
		{
			//其实就是 sendpack
			(this->*m_mapFunc[msg.message])(msg.message, msg.wParam, msg.lParam);
		}
	}
}

void CClientSocket::SendPack(UINT nMsg, WPARAM wParam, LPARAM lParam)
{//定义一个消息的数据结构（数据和数据长度，模式）回调消息的的数据结构(HWND MESSAGE)
	PACKET_DATA data = *(PACKET_DATA*)wParam; // 获取数据
	delete (PACKET_DATA*)wParam;//
	HWND hWnd = (HWND)lParam; // 目标窗口

	//记录文件的信息
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
				int length = recv(m_sock, pBuffer + index, BUFFER_SIZE - index, 0); //存在问题  length为负数
				if (length > 0 || (index > 0)) {
					index += (size_t)length;
					size_t nLen = index;
					CPacket pack((BYTE*)pBuffer, nLen);
					if (nLen > 0) {
						// 发送 WM_SEND_PACK_ACK 通知 hWnd（UI 线程）
						TRACE("ack pack %d to hWnd %08X %d %d\r\n", pack.sCmd, hWnd, index, nLen);
						TRACE("文件头:   %04X\r\n", *(WORD*)pBuffer + nLen);

						::SendMessage(hWnd, WM_SEND_PACK_ACK, (WPARAM)new CPacket(pack), data.wParam);

						if (data.nMode & CSM_AUTOCLOSE) {
							CloseSocket();
							return;
						}
						index -= nLen;
						memmove(pBuffer, pBuffer + nLen, index);
					}
				}
				else { //TODO:对方关闭了套接字，或者网络设备异常
					TRACE("recv failed length %d index %d cmd %d\r\n", length, index, current.sCmd);
					CloseSocket();
					//接收最后的发送结束信息，没有这个，发送过程不会结束
					::SendMessage(hWnd, WM_SEND_PACK_ACK, (WPARAM)new CPacket(current.sCmd,NULL,0), 1);
				}
			}
		}
		else {
			CloseSocket();
			//网络终止处理
			::SendMessage(hWnd, WM_SEND_PACK_ACK, NULL, -1);
		}
	}
	else {
		::SendMessage(hWnd, WM_SEND_PACK_ACK, NULL, -2);
	}
		
}
