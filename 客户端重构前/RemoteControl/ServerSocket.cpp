#include "pch.h"
#include "ServerSocket.h"

// CServerSocket server;
CServerSocket* CServerSocket::m_instance = NULL;//�����ʼ����̬��Ա����
// ����һ�� CHelper �Ķ��󣬴��� CHelper �Ĺ��캯������֤��������ʱ��������ʵ��
CServerSocket::CHelper CServerSocket::m_helper;
// m_instance �ǶԵ���ʵ�� m_instance ��һ��ָ������,���ڵ�����صĺ���
CServerSocket* pserver = CServerSocket::getInstance();