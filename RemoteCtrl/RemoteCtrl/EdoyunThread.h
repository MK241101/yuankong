#pragma once
#include<atomic>
#include<vector>


class ThreadFuncBase {};

typedef int (ThreadFuncBase::*FUNCTYPE)();


class ThreadWorker {
public:
    ThreadWorker():func(NULL),thiz(NULL){}
    ThreadWorker(ThreadFuncBase* obj, FUNCTYPE f):thiz(obj), func(f){}
    ThreadWorker(const ThreadWorker& worker) {
        thiz= worker.thiz;
        func = worker.func;
    
    }

    ThreadWorker& operator=(const ThreadWorker& worker) {
        if (this != &worker) {
            thiz = worker.thiz;
            func = worker.func;
        
        }
        return *this;
    }

    int operator()() {
        if (IsValid()) {
            return (thiz->*func)();
        }
        return -1;
    }

    bool IsValid() {
        return (thiz != NULL) && (func != NULL);
    
    }

private:

    ThreadFuncBase* thiz;
    FUNCTYPE func;


};



class EdoyunThread
{
public:
    EdoyunThread() {
        m_hThread = NULL;
    }

    ~EdoyunThread() {
        Stop();
    }


    // true 表示成功  false表示失败
    bool Start() {
        m_bStatus = true;
        m_hThread = (HANDLE)_beginthread(&EdoyunThread::ThreadEntry, 0, this);
        if (!IsValid()) {
            m_bStatus = false;
        }
        return m_bStatus;
    }

    bool IsValid() {
        if (m_hThread == NULL || (m_hThread == INVALID_HANDLE_VALUE)) return false;
        return WaitForSingleObject(m_hThread,0) == WAIT_TIMEOUT;
    }

    bool Stop() {
        if(m_bStatus==FALSE) return true;
        m_bStatus = false;
        return WaitForSingleObject(m_hThread, INFINITY)== WAIT_OBJECT_0;
    }

    void UpdateWorker(const::ThreadWorker& worker=::ThreadWorker()) {
        m_worker.store(worker);
    
    }
   

private:
    void ThreadWorker() {
        while (m_bStatus) {
            ::ThreadWorker worker = m_worker.load();
            if (worker.IsValid()) {
                int ret = worker();
                if (ret != 0) {
                    CString str;
                    str.Format(_T("thread found warning code %d\r\n"), ret);
                    OutputDebugString(str);
                }
                if (ret < 0) {
                    m_worker.store(::ThreadWorker());
                }

            }
            else { Sleep(1); }
         
        }
    }

    static void ThreadEntry(void* arg) {
        EdoyunThread* thiz = (EdoyunThread*)arg;
        if (thiz) {
            thiz->ThreadWorker();
        }
        _endthread();
    }


private:
    HANDLE m_hThread;
    bool m_bStatus;    //true 表示线程正在运行     false表示线程将要关闭
    std::atomic<::ThreadWorker> m_worker;

};

class EdoyunThreadPool {
public:
    EdoyunThreadPool() {}
    EdoyunThreadPool(size_t size) {
        m_threads.resize(size);
    }
    ~EdoyunThreadPool() {
        Stop();
        m_threads.clear();
    }

    bool Invoke() {
        bool ret = true;
        for (size_t i = 0; i < m_threads.size(); i++) {
            if (m_threads[i].Start() == false) {
                ret = false;
                break;
            }
        }
        if (ret == false) {
            for (size_t i = 0; i < m_threads.size(); i++) {
                m_threads[i].Stop();
            }
        }
        return ret;
    }

    void Stop() {
        for (size_t i = 0; i < m_threads.size(); i++) {
            m_threads[i].Stop();
        }
    
    }

    int DispatchWorker(const ThreadWorker& worker) {
    
    
    }


private:
    std::vector<EdoyunThread> m_threads;


};