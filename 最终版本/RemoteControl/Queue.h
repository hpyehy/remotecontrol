#pragma once
#include<string>
#include<atomic>
#include<list>
#include "pch.h"
#include "Threadpool.h"
using namespace std;

template<class T>
//�̰߳�ȫ�Ķ���(����I0CPʵ��)
/*
threadMain() �� CQueue �ĺ�̨�̴߳���������ͨ�� GetQueuedCompletionStatus()�� IOCP ������л�ȡ���񣬲�ִ����Ӧ�Ķ��в�����Ҫ������������
threadMain()�� get ����Ϣ���� PostQueuedCompletionStatus ���� post �е���Ϣ����ͨ�� push_back �� pop_front �ӱ�ĵط����͹�����
�յ���Ϣ��ͨ�� DealParam ���д������ǲ��� list<T> m_lstData; ������ߵ������ݣ�����������غ������� ��
*/
class CQueue{
protected:
	list<T> m_lstData;
	HANDLE m_hCompeletionPort;
	HANDLE m_hThread;
	std::atomic<bool> m_lock;
public:
	enum {
		QNone,
		QPush,
		QPop,
		QSize,
		QClear
	};

	typedef struct IocpParam {
		int nOperator; // �������� (Push/Pop/Empty)
		T Data; // �洢������
		_beginthread_proc_type cbFunc; // �ص�����
		HANDLE hEvent;//pop������Ҫ��
		IocpParam(int op, const T& data, HANDLE hEve = NULL) {
			nOperator = op;
			Data = data;
			hEvent = hEve;
		}
		IocpParam() {
			nOperator = QNone;
		}
	}PPARAM;//����Ͷ����Ϣ�Ľṹ��

	CQueue() {
		m_lock = false;
		// ���� IOCP �������
		m_hCompeletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 1);
		m_hThread = INVALID_HANDLE_VALUE;
		if (m_hCompeletionPort != NULL) {
			// �����������߳� threadMain���ڶ���������ʾ�̵߳�ջ��С��0��ʾʹ��Ĭ��ջ��С��this�Ǵ��ݸ��̵߳Ĳ���
			m_hThread = (HANDLE)_beginthread(&CQueue<T>::threadEntry, 0, this);
		}
	}

	virtual ~CQueue() {
		if (m_lock)return;
		m_lock = true; //�رն��У���ֹ�µ������ύ
		
		//�� IOCP ��������ύ NULL��֪ͨ�߳��˳�
		PostQueuedCompletionStatus(m_hCompeletionPort, 0, NULL, NULL);
		//�ȴ��������߳�ֱ����ȫ�˳�
		WaitForSingleObject(m_hThread, INFINITE);
		if (m_hCompeletionPort != NULL) {
			HANDLE hTemp = m_hCompeletionPort;
			//���ÿգ���رգ���Ϊ�˷�ֹ���ر�֮���ÿ�֮ǰ m_hCompeletionPort������ָ�룬���ܵ��·������ͷ���Դ��NULL��û����
			m_hCompeletionPort = NULL; //�߳̽��������ÿ� IOCP ���
			CloseHandle(hTemp); //�ر� IOCP ���
		}
	}

	bool push_back(const T& data) {
		//ִ�к��������񣺴������������Ϣ
		IocpParam* pParam = new IocpParam(QPush, data);//�����������
		//��ֹ��������Ȼ�ύ����
		if (m_lock == true){
			delete pParam;
			return false;
		}

		//������е����������������postʧ�ܣ�Ҫ���д���
		//PostQueuedCompletionStatus() ���첽�ģ�����ʹ�þֲ����������ͳɹ��˾Ͳ���������
		bool ret = PostQueuedCompletionStatus(m_hCompeletionPort,sizeof(PPARAM), (ULONG_PTR)pParam, NULL);
		//���postʧ�ܣ���Ҫ��ʱ�ͷŶ��󣬳ɹ�����շ��ᴦ��
		if (ret == false)delete pParam;
		//printf("push back done %d  %08p\r\n", ret, (void*)pParam);
		return ret;
	}

	//��������Ԫ�ز��һ�ȡ��ֵ
	//ԭʼ CQueue �汾 �� pop_front() �������ģ���Ҫ�ȴ� IOCP ������ɣ�
	//SendQueue ��д���� pop_front() ��ɷ������ģ�ֻͶ�� IOCP ���񣬶����ȴ����
	virtual bool pop_front(T& data) {
		HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		IocpParam Param(QPop, data, hEvent);

		if (m_lock == true) {
			if (hEvent)CloseHandle(hEvent);
			return false;
		}
		//ͨ���̺߳������޸� Param �����е�ֵ�Ӷ���ȡ��������
		bool ret = PostQueuedCompletionStatus(m_hCompeletionPort,sizeof(PPARAM), (ULONG_PTR)&Param, NULL);
		//���postʧ�ܣ���Ҫ��ʱ�ͷŶ��󣬳ɹ������̴߳���
		if (ret == false){
			CloseHandle(hEvent);
			return false;
		}
		//�����ȴ� ���ݿ��ã�ֱ�� IOCP �̴߳��� QPop ����
		ret = WaitForSingleObject(hEvent, INFINITE)== WAIT_OBJECT_0;// WAIT_OBJECT_0 ��ʾ�ȴ��Ķ����Ϊ���ź�״̬
		if (ret) {
			data = Param.Data;//��ȡ����Ԫ�ش����� param ������
		}
		return ret;
	}

	size_t Size() {
		HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		IocpParam Param(QSize, T(), hEvent);

		if (m_lock == true){
			if (hEvent)CloseHandle(hEvent);
			return -1;
		}
		//ͨ���̺߳������޸� Param �����е�ֵ�Ӷ���ȡ��������
		bool ret = PostQueuedCompletionStatus(m_hCompeletionPort,sizeof(PPARAM), (ULONG_PTR)&Param, NULL);
		//���postʧ�ܣ���Ҫ��ʱ�ͷŶ��󣬳ɹ������̴߳���
		if (ret == false){
			CloseHandle(hEvent);
			return -1;
		}
		ret = WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0;
		if (ret) {
			// nOperator �������DealParam�б����ó��� m_lstData.size()
			return Param.nOperator;
		}
		return -1;
	}

	bool Clear() {
		if (m_lock == true)return false;//��ֹ��������Ȼ�ύ����
		//T* pData = new T(data);
		IocpParam* pParam = new IocpParam(QClear, T());//�����������
		//������е����������������postʧ�ܣ�Ҫ���д���
		bool ret = PostQueuedCompletionStatus(m_hCompeletionPort,sizeof(PPARAM), (ULONG_PTR)pParam, NULL);
		//���postʧ�ܣ���Ҫ��ʱ�ͷŶ��󣬳ɹ������̴߳���
		if (ret == false)delete pParam;
		//printf("Clear done %d  %08p\r\n", ret, (void*)pParam);
		return ret;
	}


protected:
	static void threadEntry(void* arg) {
		CQueue<T>* thiz = (CQueue<T>*)arg;
		thiz->threadMain();
		_endthread();
	}
	
	//����� pParam ��ͨ�� threadMain �е� GetQueuedCompletionStatus �õ���
	virtual void DealParam(PPARAM* pParam) {
		if (pParam->nOperator == QPush) {
			m_lstData.push_back(pParam->Data);
			delete pParam;// QPush ����ִ�к󣬲�����Ҫ pParam ��delete��ֹ�ڴ�й©
			//printf("delete %08p\r\n", (void*)pParam);
		}
		else if (pParam->nOperator == QPop) {
			if (m_lstData.size() > 0) {
				pParam->Data = m_lstData.front();// ��������
				m_lstData.pop_front();
			}

			if (pParam->hEvent != NULL) {//�ص�������Ϊ��
				SetEvent(pParam->hEvent);//֪ͨ pop_front()�����������
			}
			//delete pParam;
		}
		else if (pParam->nOperator == QSize) {
			pParam->nOperator = m_lstData.size();
			if (pParam->hEvent != NULL) {//�ص�������Ϊ��
				SetEvent(pParam->hEvent);
			}
		}
		else if (pParam->nOperator == QClear) {
			m_lstData.clear();
			delete pParam;//ִ�к�����Ҫ pParam������delete����ֹ�ڴ�й©
			//printf("Clear done %08p\r\n", (void*)pParam);
		}
	}

	virtual void threadMain() {
		PPARAM* pParam = NULL;
		DWORD dwTransferred = 0;//�������ݴ�����ֽ�������������δʹ�ã�
		ULONG_PTR CompletionKey = 0;//����������������
		OVERLAPPED* pOverlapped = NULL;
		//��������ʱ��GetQueuedCompletionStatus �᷵�� TRUE ����������Ϣ��䵽������ , CompletionKey�Ƿ��͵���Ҫ��Ϣ
		while (GetQueuedCompletionStatus(m_hCompeletionPort, &dwTransferred,
			&CompletionKey, &pOverlapped, INFINITE)) {

			if ((dwTransferred == 0) || (CompletionKey == NULL)) {
				//printf("thread is prepare to exit!\r\n");
				break;//ֱ������ѭ��
			}
			// pParam �ɸ������������������ݵ� IOCP �������
			pParam = (PPARAM*)CompletionKey;// ȡ����������
			DealParam(pParam);
		}
		
		//while (GetQueuedCompletionStatus(m_hCompeletionPort, &dwTransferred,
		//	&CompletionKey, &pOverlapped, 0)) {
		//	if ((dwTransferred == 0) || (CompletionKey == NULL)) {
		//		//printf("thread is prepare to exit!\r\n");
		//		continue;//continue���ǻᵼ����Զ�޷��˳���
		//		//break;
		//	}
		//	pParam = (PPARAM*)CompletionKey;// ȡ����������
		//	DealParam(pParam);
		//}

		HANDLE hTemp = m_hCompeletionPort;//������еľ��
		m_hCompeletionPort = NULL;//�洢�������̵߳ľ��
		CloseHandle(hTemp);
	}


};



//class ThreadFuncBase {};
//typedef int(ThreadFuncBase::* FUNCTYPE)();


/*
// CQueue<T> �ṩ�������ݴ洢��ThreadFuncBase �ṩ�̳߳��������
template<class T>
class SendQueue 
	:public CQueue<T>,public ThreadFuncBase {
public:
	typedef int(ThreadFuncBase::* ECALLBACK)(T& data);//ECALLBACK ��һ������ָ��

	SendQueue(ThreadFuncBase* obj , ECALLBACK callback):
		CQueue<T>(), m_base(obj), m_callback(callback)//m_base ��¼���� SendQueue �Ķ���callback ʵ���� Client::SendData
		{
			m_thread.Start();//������̨�̣߳����ڴ��� SendQueue ����
			// �� threadTick() �̺߳������� IOCP �߳�ѭ�������������
			m_thread.UpdateWorker(::ThreadWorker(this, (FUNCTYPE)&SendQueue<T>::threadTick));
		}

	virtual ~SendQueue() {
		m_base = NULL;
		m_callback = NULL;
		m_thread.Stop();//ֹͣ��������ǰ����
	}
protected:
	int threadTick() {
		//  != WAIT_TIMEOUT ˵���߳���ǰ������
		if (WaitForSingleObject(CQueue<T>::m_hThread, 0) != WAIT_TIMEOUT)
			return 0;
		if (CQueue<T>::m_lstData.size()> 0) {
			pop_front();//���� pop �������������ʵ���ϲ�pop�����Ƿ�����Ϣ����������д���ĺ�����
		}
		Sleep(1);
		return 0;
	}

	virtual bool pop_front(T& data) {//����ʹ����
		return false;
	}
	//��Ȼ���� pop_front ������ֻ���з�������Ĺ���
	bool pop_front()
	{
		//HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		typename CQueue<T>::IocpParam* Param = new typename CQueue<T>::IocpParam(CQueue<T>::QPop, T());
		if (CQueue<T>::m_lock == true) {
			//if (hEvent)CloseHandle(hEvent);
			delete Param;
			return false;
		}
		//ͨ���̺߳������޸� Param �����е�ֵ�Ӷ���ȡ��������  // ���� DealParam() ���� QPop ����ȡ����������
		bool ret = PostQueuedCompletionStatus(CQueue<T>::m_hCompeletionPort, sizeof(*Param), (ULONG_PTR)&Param, NULL);
		//���postʧ�ܣ���Ҫ��ʱ�ͷŶ��󣬳ɹ������̴߳���
		if (ret == false) {
			//CloseHandle(hEvent);
			delete Param;
			return false;
		}
		//ret = WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0;
		//if (ret) {
		//	data = Param.Data;//��ȡ����Ԫ�ش����� param ������
		//}
		return ret;
	}


	//����� pParam ��CQueue�ĳ�Ա�������͹�������Ҫ��������ݣ���ͨ�� threadMain �е� GetQueuedCompletionStatus �õ���
	//typename��Ϊ�˷�ֹ�������壺�� T �����������T��ͬһ����������һ���µ�����
	virtual void DealParam(typename CQueue<T>::PPARAM* pParam) {
		if (pParam->nOperator == CQueue<T>::QPush) {
			CQueue<T>::m_lstData.push_back(pParam->Data);
			delete pParam;// QPush ����ִ�к󣬲�����Ҫ pParam ��delete��ֹ�ڴ�й©
			//printf("delete %08p\r\n", (void*)pParam);
		}
		else if (pParam->nOperator == CQueue<T>::QPop) {
			if (CQueue<T>::m_lstData.size() > 0) {
				pParam->Data = CQueue<T>::m_lstData.front();// ��������

				// ֻ�з��ͳɹ�ʱ�ŴӶ�����ɾ����callback ʵ���� Client::SendData
				if((m_base->*m_callback)(pParam->Data)==0)
					CQueue<T>::m_lstData.pop_front();
			}
			//if (pParam->hEvent) {//�ص�������Ϊ��
			//	SetEvent(pParam->hEvent);//֪ͨ pop_front()�����������
			//}
			delete pParam;
		}
		else if (pParam->nOperator == CQueue<T>::QSize) {
			pParam->nOperator = CQueue<T>::m_lstData.size();
			if (pParam->hEvent != NULL) {//�ص�������Ϊ��
				SetEvent(pParam->hEvent);
			}
		}
		else if (pParam->nOperator == CQueue<T>::QClear) {
			CQueue<T>::m_lstData.clear();
			delete pParam;//ִ�к�����Ҫ pParam������delete����ֹ�ڴ�й©
			//printf("Clear done %08p\r\n", (void*)pParam);
		}
	}

private:
	ThreadFuncBase* m_base;
	ECALLBACK m_callback;//��һ������ָ������
	Thread m_thread;
};

//SendQueue<std::vector<char>> �� SendQueue<T> ģ�����һ��ʵ��
//typedef int (ThreadFuncBase::* SENDCALLBACK)(std::vector<char>& data);
typedef SendQueue<std::vector<char>>::ECALLBACK SENDCALLBACK;


*/