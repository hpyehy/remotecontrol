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