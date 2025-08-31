#include "pch.h"
#include "ServerSocket.h"

// CServerSocket server;
CServerSocket* CServerSocket::m_instance = NULL;//类外初始化静态成员变量
// 创建一个 CHelper 的对象，触发 CHelper 的构造函数，保证程序启动时创建单例实例
CServerSocket::CHelper CServerSocket::m_helper;
// m_instance 是对单例实例 m_instance 的一个指针引用,用于调用相关的函数
CServerSocket* pserver = CServerSocket::getInstance();