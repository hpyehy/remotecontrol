#pragma once
#include<string>
#include<atomic>
#include<list>
#include "pch.h"
#include "Threadpool.h"
using namespace std;

template<class T>
//线程安全的队列(利用I0CP实现)
/*
threadMain() 是 CQueue 的后台线程处理函数，它通过 GetQueuedCompletionStatus()从 IOCP 任务队列获取任务，并执行相应的队列操作，要先理解这个才行
threadMain()中 get 的消息来自 PostQueuedCompletionStatus ，而 post 中的信息就是通过 push_back 和 pop_front 从别的地方发送过来的
收到消息后，通过 DealParam 进行处理（就是操作 list<T> m_lstData; 加入或者弹出数据，并不调用相关函数处理 ）
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
		int nOperator; // 操作类型 (Push/Pop/Empty)
		T Data; // 存储的数据
		_beginthread_proc_type cbFunc; // 回调函数
		HANDLE hEvent;//pop操作需要的
		IocpParam(int op, const T& data, HANDLE hEve = NULL) {
			nOperator = op;
			Data = data;
			hEvent = hEve;
		}
		IocpParam() {
			nOperator = QNone;
		}
	}PPARAM;//用于投递信息的结构体

	CQueue() {
		m_lock = false;
		// 创建 IOCP 任务队列
		m_hCompeletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 1);
		m_hThread = INVALID_HANDLE_VALUE;
		if (m_hCompeletionPort != NULL) {
			// 启动任务处理线程 threadMain，第二个参数表示线程的栈大小，0表示使用默认栈大小，this是传递给线程的参数
			m_hThread = (HANDLE)_beginthread(&CQueue<T>::threadEntry, 0, this);
		}
	}

	virtual ~CQueue() {
		if (m_lock)return;
		m_lock = true; //关闭队列，防止新的任务提交
		
		//向 IOCP 任务队列提交 NULL，通知线程退出
		PostQueuedCompletionStatus(m_hCompeletionPort, 0, NULL, NULL);
		//等待任务处理线程直到完全退出
		WaitForSingleObject(m_hThread, INFINITE);
		if (m_hCompeletionPort != NULL) {
			HANDLE hTemp = m_hCompeletionPort;
			//先置空，后关闭，是为了防止：关闭之后置空之前 m_hCompeletionPort是悬空指针，可能导致访问已释放资源，NULL就没问题
			m_hCompeletionPort = NULL; //线程结束后，再置空 IOCP 句柄
			CloseHandle(hTemp); //关闭 IOCP 句柄
		}
	}

	bool push_back(const T& data) {
		//执行函数的任务：创建命令，发送消息
		IocpParam* pParam = new IocpParam(QPush, data);//分配任务对象
		//防止析构后仍然提交任务
		if (m_lock == true){
			delete pParam;
			return false;
		}

		//如果运行到这里队列析构，则post失败，要进行处理
		//PostQueuedCompletionStatus() 是异步的，不能使用局部变量，发送成功了就不用析构了
		bool ret = PostQueuedCompletionStatus(m_hCompeletionPort,sizeof(PPARAM), (ULONG_PTR)pParam, NULL);
		//如果post失败，就要及时释放对象，成功则接收方会处理
		if (ret == false)delete pParam;
		//printf("push back done %d  %08p\r\n", ret, (void*)pParam);
		return ret;
	}

	//弹出队首元素并且获取其值
	//原始 CQueue 版本 的 pop_front() 是阻塞的（需要等待 IOCP 任务完成）
	//SendQueue 重写后，让 pop_front() 变成非阻塞的，只投递 IOCP 任务，而不等待结果
	virtual bool pop_front(T& data) {
		HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		IocpParam Param(QPop, data, hEvent);

		if (m_lock == true) {
			if (hEvent)CloseHandle(hEvent);
			return false;
		}
		//通过线程函数来修改 Param 对象中的值从而获取所需数据
		bool ret = PostQueuedCompletionStatus(m_hCompeletionPort,sizeof(PPARAM), (ULONG_PTR)&Param, NULL);
		//如果post失败，就要及时释放对象，成功则有线程处理
		if (ret == false){
			CloseHandle(hEvent);
			return false;
		}
		//阻塞等待 数据可用，直到 IOCP 线程处理 QPop 任务
		ret = WaitForSingleObject(hEvent, INFINITE)== WAIT_OBJECT_0;// WAIT_OBJECT_0 表示等待的对象变为有信号状态
		if (ret) {
			data = Param.Data;//获取队首元素储存在 param 对象中
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
		//通过线程函数来修改 Param 对象中的值从而获取所需数据
		bool ret = PostQueuedCompletionStatus(m_hCompeletionPort,sizeof(PPARAM), (ULONG_PTR)&Param, NULL);
		//如果post失败，就要及时释放对象，成功则有线程处理
		if (ret == false){
			CloseHandle(hEvent);
			return -1;
		}
		ret = WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0;
		if (ret) {
			// nOperator 在下面的DealParam中被设置成了 m_lstData.size()
			return Param.nOperator;
		}
		return -1;
	}

	bool Clear() {
		if (m_lock == true)return false;//防止析构后仍然提交任务
		//T* pData = new T(data);
		IocpParam* pParam = new IocpParam(QClear, T());//分配任务对象
		//如果运行到这里队列析构，则post失败，要进行处理
		bool ret = PostQueuedCompletionStatus(m_hCompeletionPort,sizeof(PPARAM), (ULONG_PTR)pParam, NULL);
		//如果post失败，就要及时释放对象，成功则有线程处理
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
	
	//这里的 pParam 是通过 threadMain 中的 GetQueuedCompletionStatus 得到的
	virtual void DealParam(PPARAM* pParam) {
		if (pParam->nOperator == QPush) {
			m_lstData.push_back(pParam->Data);
			delete pParam;// QPush 任务执行后，不再需要 pParam ，delete防止内存泄漏
			//printf("delete %08p\r\n", (void*)pParam);
		}
		else if (pParam->nOperator == QPop) {
			if (m_lstData.size() > 0) {
				pParam->Data = m_lstData.front();// 复制数据
				m_lstData.pop_front();
			}

			if (pParam->hEvent != NULL) {//回调函数不为空
				SetEvent(pParam->hEvent);//通知 pop_front()函数任务完成
			}
			//delete pParam;
		}
		else if (pParam->nOperator == QSize) {
			pParam->nOperator = m_lstData.size();
			if (pParam->hEvent != NULL) {//回调函数不为空
				SetEvent(pParam->hEvent);
			}
		}
		else if (pParam->nOperator == QClear) {
			m_lstData.clear();
			delete pParam;//执行后不再需要 pParam，必须delete，防止内存泄漏
			//printf("Clear done %08p\r\n", (void*)pParam);
		}
	}

	virtual void threadMain() {
		PPARAM* pParam = NULL;
		DWORD dwTransferred = 0;//返回数据传输的字节数（本代码中未使用）
		ULONG_PTR CompletionKey = 0;//传过来的数据内容
		OVERLAPPED* pOverlapped = NULL;
		//当有任务时，GetQueuedCompletionStatus 会返回 TRUE 并将任务信息填充到参数中 , CompletionKey是发送的主要消息
		while (GetQueuedCompletionStatus(m_hCompeletionPort, &dwTransferred,
			&CompletionKey, &pOverlapped, INFINITE)) {

			if ((dwTransferred == 0) || (CompletionKey == NULL)) {
				//printf("thread is prepare to exit!\r\n");
				break;//直接跳出循环
			}
			// pParam 由各个函数创建，并传递到 IOCP 任务队列
			pParam = (PPARAM*)CompletionKey;// 取出任务数据
			DealParam(pParam);
		}
		
		//while (GetQueuedCompletionStatus(m_hCompeletionPort, &dwTransferred,
		//	&CompletionKey, &pOverlapped, 0)) {
		//	if ((dwTransferred == 0) || (CompletionKey == NULL)) {
		//		//printf("thread is prepare to exit!\r\n");
		//		continue;//continue不是会导致永远无法退出吗
		//		//break;
		//	}
		//	pParam = (PPARAM*)CompletionKey;// 取出任务数据
		//	DealParam(pParam);
		//}

		HANDLE hTemp = m_hCompeletionPort;//任务队列的句柄
		m_hCompeletionPort = NULL;//存储任务处理线程的句柄
		CloseHandle(hTemp);
	}


};



//class ThreadFuncBase {};
//typedef int(ThreadFuncBase::* FUNCTYPE)();


/*
// CQueue<T> 提供队列数据存储，ThreadFuncBase 提供线程池任务管理
template<class T>
class SendQueue 
	:public CQueue<T>,public ThreadFuncBase {
public:
	typedef int(ThreadFuncBase::* ECALLBACK)(T& data);//ECALLBACK 是一个函数指针

	SendQueue(ThreadFuncBase* obj , ECALLBACK callback):
		CQueue<T>(), m_base(obj), m_callback(callback)//m_base 记录调用 SendQueue 的对象，callback 实际是 Client::SendData
		{
			m_thread.Start();//启动后台线程，用于处理 SendQueue 任务
			// 绑定 threadTick() 线程函数，让 IOCP 线程循环处理队列任务
			m_thread.UpdateWorker(::ThreadWorker(this, (FUNCTYPE)&SendQueue<T>::threadTick));
		}

	virtual ~SendQueue() {
		m_base = NULL;
		m_callback = NULL;
		m_thread.Stop();//停止并且清理当前任务
	}
protected:
	int threadTick() {
		//  != WAIT_TIMEOUT 说明线程提前结束了
		if (WaitForSingleObject(CQueue<T>::m_hThread, 0) != WAIT_TIMEOUT)
			return 0;
		if (CQueue<T>::m_lstData.size()> 0) {
			pop_front();//调用 pop 函数（这个函数实际上不pop，而是发送消息，是上面重写过的函数）
		}
		Sleep(1);
		return 0;
	}

	virtual bool pop_front(T& data) {//不再使用了
		return false;
	}
	//虽然还叫 pop_front ，但是只进行发送任务的功能
	bool pop_front()
	{
		//HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		typename CQueue<T>::IocpParam* Param = new typename CQueue<T>::IocpParam(CQueue<T>::QPop, T());
		if (CQueue<T>::m_lock == true) {
			//if (hEvent)CloseHandle(hEvent);
			delete Param;
			return false;
		}
		//通过线程函数来修改 Param 对象中的值从而获取所需数据  // 最终 DealParam() 处理 QPop 任务，取出队列数据
		bool ret = PostQueuedCompletionStatus(CQueue<T>::m_hCompeletionPort, sizeof(*Param), (ULONG_PTR)&Param, NULL);
		//如果post失败，就要及时释放对象，成功则有线程处理
		if (ret == false) {
			//CloseHandle(hEvent);
			delete Param;
			return false;
		}
		//ret = WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0;
		//if (ret) {
		//	data = Param.Data;//获取队首元素储存在 param 对象中
		//}
		return ret;
	}


	//这里的 pParam 是CQueue的成员函数发送过来的需要处理的数据，是通过 threadMain 中的 GetQueuedCompletionStatus 得到的
	//typename是为了防止产生歧义：将 T 看作和上面的T是同一个，而不是一个新的类型
	virtual void DealParam(typename CQueue<T>::PPARAM* pParam) {
		if (pParam->nOperator == CQueue<T>::QPush) {
			CQueue<T>::m_lstData.push_back(pParam->Data);
			delete pParam;// QPush 任务执行后，不再需要 pParam ，delete防止内存泄漏
			//printf("delete %08p\r\n", (void*)pParam);
		}
		else if (pParam->nOperator == CQueue<T>::QPop) {
			if (CQueue<T>::m_lstData.size() > 0) {
				pParam->Data = CQueue<T>::m_lstData.front();// 复制数据

				// 只有发送成功时才从队列中删除，callback 实际是 Client::SendData
				if((m_base->*m_callback)(pParam->Data)==0)
					CQueue<T>::m_lstData.pop_front();
			}
			//if (pParam->hEvent) {//回调函数不为空
			//	SetEvent(pParam->hEvent);//通知 pop_front()函数任务完成
			//}
			delete pParam;
		}
		else if (pParam->nOperator == CQueue<T>::QSize) {
			pParam->nOperator = CQueue<T>::m_lstData.size();
			if (pParam->hEvent != NULL) {//回调函数不为空
				SetEvent(pParam->hEvent);
			}
		}
		else if (pParam->nOperator == CQueue<T>::QClear) {
			CQueue<T>::m_lstData.clear();
			delete pParam;//执行后不再需要 pParam，必须delete，防止内存泄漏
			//printf("Clear done %08p\r\n", (void*)pParam);
		}
	}

private:
	ThreadFuncBase* m_base;
	ECALLBACK m_callback;//是一个函数指针类型
	Thread m_thread;
};

//SendQueue<std::vector<char>> 是 SendQueue<T> 模板类的一个实例
//typedef int (ThreadFuncBase::* SENDCALLBACK)(std::vector<char>& data);
typedef SendQueue<std::vector<char>>::ECALLBACK SENDCALLBACK;


*/