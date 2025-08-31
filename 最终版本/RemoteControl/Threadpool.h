#pragma once
#include "pch.h"
#include <atomic>
#include <vector>
#include <mutex>

class ThreadFuncBase {};
typedef int(ThreadFuncBase::* FUNCTYPE)();

//ThreadWorker ���̳߳��еġ������װ��������ĳ�������ĳ�Ա�������һ����ִ�е����񣬲������̳߳��е���ִ��
class ThreadWorker {
public:
	ThreadWorker() : thiz(NULL), func(NULL) {}
	//����������Ӧ�ĺ������Ϳ���ִ�иú���
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
		if (IsValid())//�̶߳���ͺ�����������������������
			return (thiz->*func)();//���û���ĳ�Ա����
		return -1;
	}

	bool IsValid() const{
		return (thiz != NULL) && (func != NULL);
	}

private:
	ThreadFuncBase* thiz;//����ָ��
	//FUNCTYPE func;
	//��ͨ����ָ�벻�ܴ洢��Ա���������ThreadWorkerʹ���˳�Ա����ָ��(ThreadFuncBase::* FUNCTYPE)
	int (ThreadFuncBase::* func)();//�����Ա�����ĺ���ָ��
};


//������ֹͣ�������߳�
class Thread{
private:
	HANDLE m_hThread;                      // �߳̾��
	bool m_bStatus;                         // �߳�״̬��true�����У�false��ֹͣ��
	std::atomic<::ThreadWorker*> m_worker;   // �߳�����ԭ�Ӳ�����֤�̰߳�ȫ��//ע�����ð��
public:
	Thread() {
		m_hThread = NULL;
		m_bStatus = false;
	}

	~Thread(){
		Stop();
	}

	//�����߳�
	bool Start() {
		m_bStatus = true;
		m_hThread = (HANDLE)_beginthread(Thread::ThreadEntry, 0, this);
		if (!IsValid())//�߳��Ƿ񴴽��ɹ������Ϸ���Ҫ�˳�
			m_bStatus = false;
		return m_bStatus;
	}

	bool IsValid() {
		if (m_hThread == NULL || (m_hThread == INVALID_HANDLE_VALUE))return false;
		//���� WAIT_TIMEOUT����ʾ�߳��������У�˵���߳���Ч������˵���߳����˳���������
		return WaitForSingleObject(m_hThread, 0) == WAIT_TIMEOUT;
	}

	//ֹͣ��������ǰ����
	bool Stop() {
		if (m_bStatus == false)return true;
		m_bStatus = false;//���� m_bStatus = false �������߳�״̬Ϊ ����
		bool ret = WaitForSingleObject(m_hThread, 1000);
		// WAIT_TIMEOUT ˵���߳�״̬û�б仯����û�н�������Ҫ�ֶ�����
		if (ret == WAIT_TIMEOUT) {
			//ǿ�ƽ���
			TerminateThread(m_hThread, -1);
		}
		// ��ΪʲôҲû�д��룬�൱������߳�����
		UpdateWorker();
		return ret == WAIT_OBJECT_0;
	}

	// ���߳��������Ϊ����������񣬼��� m_worker �и�������
	void UpdateWorker(const ::ThreadWorker& worker = ::ThreadWorker()){
		//�����������������⣬����ʲôҲû�д���(����� Stop ����)������NULL�������
		if (worker.IsValid()==false) {
			m_worker.store(NULL);//store �Ḳ��m_worker��ԭ��������,�洢�µ�����ʹ�� std::atomic::store() �Ա�֤�̰߳�ȫ
			return;
		}
		//���������͵�ǰ������ͬ�����޸�
		if (m_worker.load() == &worker) return;

		//��� m_worker�Ѿ��������������ǵ�ǰ�����������˵�� m_worker ����һ����������Ҫ������
		if (m_worker.load() != NULL && (m_worker.load() != &worker)) {
			::ThreadWorker* pWorker = m_worker.load();
			TRACE("delete pWorker = %08X m_worker = %08X\r\n", pWorker, m_worker.load());
			m_worker.store(NULL);
			delete pWorker;
		}
		::ThreadWorker* pWorker2 = new ::ThreadWorker(worker);
		TRACE("new pWorker = %08X m_worker = %08X\r\n", pWorker2, m_worker.load());
		// ���������������Ĺؼ����� m_worker �и�������
		m_worker.store(new ::ThreadWorker(worker));
	}

	bool IsIdle() {//true ��ʾ���У�false ��ʾ������
		if (m_worker.load() == NULL)return true;
		return !m_worker.load()->IsValid();//IsValid()Ϊfalse˵����ǰ�߳�ʱ���е�
	}

private:
	//��鵱ǰ�̲߳�����ִ������
	void ThreadWorker() {
		//m_bStatus �� Stop()�����б���Ϊ false���Ӷ�ֹͣ�߳�
		while (m_bStatus) {
			if (m_worker.load() == NULL) {
				Sleep(1);//��� m_worker û���������� 1 ���룬���� CPU ��ռ��
				continue;
			}
			//worker�� ThreadWorker ���󣬿���ֱ��ͨ�� worker() ������غ���
			//�������˲���ִ�е������Ȼ���������continue��
			::ThreadWorker worker = *m_worker.load();  
			if(worker.IsValid())
			{
				// 0 ��ʾ�������أ�������� WAIT_TIMEOUT��˵���߳���Ȼ�����У�״̬û�б仯��������ִ������
				if (WaitForSingleObject(m_hThread, 0) == WAIT_TIMEOUT) {
					int ret = worker();//���ú���
					if (ret != 0) {
						CString str;
						str.Format(_T("thread found warning code %d\r\n"), ret);
						OutputDebugString(str);
					}
					//˵���߳���û�������̲߳��Ϸ������������� if(worker.IsValid()) ֮�У��ǲ��Ǹ�������ִ�е���
					//˵������ worker() ������غ���ʧ�ܣ��߳���æ��
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

//�̳߳أ�Ѱ�ҿ����̣߳�
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

	// ����Start�����̣߳�����д����� Stop
	bool Invoke() {
		bool ret = true;
		for (size_t i = 0; i < m_threads.size(); i++) {
			if (m_threads[i]->Start() == false){
				ret = false;
				break;
			}
		}
		//ȫ���߳������ɹ�������ɹ����������ʧ��
		if (ret == false) {
			//for (size_t i = 0; i < m_threads.size(); i++)
			//	m_threads[i]->Stop();
			Stop();
		}
		return ret;
	}
	
	//ֹͣ�������������߳�����
	void Stop() {
		for (size_t i = 0; i < m_threads.size(); i++)
			m_threads[i]->Stop();
	}

	//Ѱ�ҿ����̲߳��ҷ�������
	int DispatchWorker(const ThreadWorker& worker) {
		int index = -1;
		//����ΪʲôҪ����
		m_lock.lock();
		//Ϊ��ͬ�̷߳��䲻ͬ��worker��������Ҫ���������߳���Ѱ�ҿ����߳�
		for (size_t i = 0; i < m_threads.size(); i++)
		{
			if (m_threads[i]->IsIdle()) {
				//(ͨ�� UpdateWorker ���� m_worker �и�������)
				m_threads[i]->UpdateWorker(worker);
				index = i;
				break;
			}
		}
		m_lock.unlock();
		//��ʾ�� index ���̱߳������ worker�������-1��ʾ�����̶߳���æ
		return index;
	}

	//��鵱ǰ�߳��Ƿ����
	bool CheckThreadValid(size_t index) {
		if (index < m_threads.size()) {
			return m_threads[index]->IsValid();
		}
		return false;
	}
};