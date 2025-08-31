#pragma once
#include "pch.h"
#include <atomic>
#include <vector>
#include <mutex>

class ThreadFuncBase {};
typedef int(ThreadFuncBase::* FUNCTYPE)();

//ThreadWorker 是线程池中的“任务封装器”，把某个类对象的成员函数变成一个可执行的任务，并用于线程池中调度执行
class ThreadWorker {
public:
	ThreadWorker() : thiz(NULL), func(NULL) {}
	//传入对象和响应的函数，就可以执行该函数
	ThreadWorker(void* obj, FUNCTYPE f) :thiz((ThreadFuncBase*)obj ), func(f) {}

	ThreadWorker(const ThreadWorker& worker) {
		thiz = worker.thiz;
		func = worker.func;
	}

	//ThreadWorker& operator=(const ThreadWorker& worker) {
	//	if (this != &worker) {
	//		thiz = worker.thiz;
	//		func = worker.func;
	//	}
	//	return *this;
	//}

	int operator()() {
		if (IsValid())//线程对象和函数都正常，才能运行任务
			return (thiz->*func)();//调用基类的成员函数
		return -1;
	}

	bool IsValid() const{
		return (thiz != NULL) && (func != NULL);
	}

private:
	ThreadFuncBase* thiz;//基类指针
	//FUNCTYPE func;
	//普通函数指针不能存储成员函数，因此ThreadWorker使用了成员函数指针(ThreadFuncBase::* FUNCTYPE)
	int (ThreadFuncBase::* func)();//基类成员函数的函数指针
};


//启动，停止，更新线程
class Thread{
private:
	HANDLE m_hThread;                      // 线程句柄
	bool m_bStatus;                         // 线程状态（true：运行，false：停止）
	std::atomic<::ThreadWorker*> m_worker;   // 线程任务（原子操作保证线程安全）//注意加上冒号
public:
	Thread() {
		m_hThread = NULL;
		m_bStatus = false;
	}

	~Thread(){
		Stop();
	}

	//开启线程
	bool Start() {
		m_bStatus = true;
		m_hThread = (HANDLE)_beginthread(Thread::ThreadEntry, 0, this);
		if (!IsValid())//线程是否创建成功，不合法就要退出
			m_bStatus = false;
		return m_bStatus;
	}

	bool IsValid() {
		if (m_hThread == NULL || (m_hThread == INVALID_HANDLE_VALUE))return false;
		//返回 WAIT_TIMEOUT，表示线程仍在运行，说明线程有效，否则，说明线程已退出，不可用
		return WaitForSingleObject(m_hThread, 0) == WAIT_TIMEOUT;
	}

	//停止并且清理当前任务
	bool Stop() {
		if (m_bStatus == false)return true;
		m_bStatus = false;//设置 m_bStatus = false ，重置线程状态为 空闲
		bool ret = WaitForSingleObject(m_hThread, 1000);
		// WAIT_TIMEOUT 说明线程状态没有变化，即没有结束，需要手动结束
		if (ret == WAIT_TIMEOUT) {
			//强制结束
			TerminateThread(m_hThread, -1);
		}
		// 因为什么也没有传入，相当于清空线程任务
		UpdateWorker();
		return ret == WAIT_OBJECT_0;
	}

	// 将线程任务更新为传入的新任务，即向 m_worker 中更新数据
	void UpdateWorker(const ::ThreadWorker& worker = ::ThreadWorker()){
		//如果传入的任务有问题，或者什么也没有传入(上面的 Stop 函数)，则传入NULL清空任务
		if (worker.IsValid()==false) {
			m_worker.store(NULL);//store 会覆盖m_worker中原来的任务,存储新的任务，使用 std::atomic::store() 以保证线程安全
			return;
		}
		//如果新任务和当前任务相同，则不修改
		if (m_worker.load() == &worker) return;

		//如果 m_worker已经有任务，且它不是当前传入的新任务，说明 m_worker 存在一个旧任务，需要先清理
		if (m_worker.load() != NULL && (m_worker.load() != &worker)) {
			::ThreadWorker* pWorker = m_worker.load();
			TRACE("delete pWorker = %08X m_worker = %08X\r\n", pWorker, m_worker.load());
			m_worker.store(NULL);
			delete pWorker;
		}
		::ThreadWorker* pWorker2 = new ::ThreadWorker(worker);
		TRACE("new pWorker = %08X m_worker = %08X\r\n", pWorker2, m_worker.load());
		// 这里才是这个函数的关键，向 m_worker 中更新数据
		m_worker.store(new ::ThreadWorker(worker));
	}

	bool IsIdle() {//true 表示空闲，false 表示有任务
		if (m_worker.load() == NULL)return true;
		return !m_worker.load()->IsValid();//IsValid()为false说明当前线程时空闲的
	}

private:
	//检查当前线程并尝试执行任务
	void ThreadWorker() {
		//m_bStatus 在 Stop()函数中被设为 false，从而停止线程
		while (m_bStatus) {
			if (m_worker.load() == NULL) {
				Sleep(1);//如果 m_worker 没有任务，休眠 1 毫秒，避免 CPU 高占用
				continue;
			}
			//worker是 ThreadWorker 对象，可以直接通过 worker() 调用相关函数
			//有任务了才能执行到这里，不然会在上面就continue了
			::ThreadWorker worker = *m_worker.load();  
			if(worker.IsValid())
			{
				// 0 表示立即返回，如果返回 WAIT_TIMEOUT，说明线程仍然在运行（状态没有变化），继续执行任务
				if (WaitForSingleObject(m_hThread, 0) == WAIT_TIMEOUT) {
					int ret = worker();//调用函数
					if (ret != 0) {
						CString str;
						str.Format(_T("thread found warning code %d\r\n"), ret);
						OutputDebugString(str);
					}
					//说明线程中没有任务（线程不合法），由于是在 if(worker.IsValid()) 之中，是不是根本不会执行到？
					//说明上面 worker() 调用相关函数失败（线程正忙）
					if (ret < 0){
						::ThreadWorker* pWorker = m_worker.load();
						m_worker.store(NULL);
						delete pWorker;
					}
				}
			}
			else {
				Sleep(1);
			}
		}
	}

	static void ThreadEntry(void* arg) {
		Thread* thiz = (Thread*)arg;
		if (thiz) {
			thiz->ThreadWorker();
		}
		_endthread();
	}
};

//线程池，寻找可用线程，
class Threadpool {
private:
	std::mutex m_lock;
	std::vector<Thread*>m_threads;

public:
	Threadpool(){}
	~Threadpool(){
		Stop();
		for (size_t i = 0; i < m_threads.size(); i++){
			delete m_threads[i];
			m_threads[i] = NULL;
		}

		m_threads.clear();
	}
	Threadpool(size_t size) {
		m_threads.resize(size);
		for (size_t i = 0; i < size; i++){
			m_threads[i] = new Thread();
		}
	}

	// 调用Start开启线程，如果有错会调用 Stop
	bool Invoke() {
		bool ret = true;
		for (size_t i = 0; i < m_threads.size(); i++) {
			if (m_threads[i]->Start() == false){
				ret = false;
				break;
			}
		}
		//全部线程启动成功才能算成功，否则就算失败
		if (ret == false) {
			//for (size_t i = 0; i < m_threads.size(); i++)
			//	m_threads[i]->Stop();
			Stop();
		}
		return ret;
	}
	
	//停止并且清理所有线程任务
	void Stop() {
		for (size_t i = 0; i < m_threads.size(); i++)
			m_threads[i]->Stop();
	}

	//寻找可用线程并且分配任务
	int DispatchWorker(const ThreadWorker& worker) {
		int index = -1;
		//这里为什么要上锁
		m_lock.lock();
		//为不同线程分配不同的worker，这里需要遍历所有线程来寻找空闲线程
		for (size_t i = 0; i < m_threads.size(); i++)
		{
			if (m_threads[i]->IsIdle()) {
				//(通过 UpdateWorker 来向 m_worker 中更新数据)
				m_threads[i]->UpdateWorker(worker);
				index = i;
				break;
			}
		}
		m_lock.unlock();
		//表示第 index 个线程被设成了 worker，如果是-1表示所有线程都在忙
		return index;
	}

	//检查当前线程是否空闲
	bool CheckThreadValid(size_t index) {
		if (index < m_threads.size()) {
			return m_threads[index]->IsValid();
		}
		return false;
	}
};