#pragma once
#include "pch.h"
#include<atomic>
#include<vector>
#include<mutex>
#include<Windows.h>


//ThreadFuncBase 这个空基类就是专门为了定义通用成员函数指针类型 FUNCTYPE，
//让不同类的成员函数能被统一封装、统一管理，方便线程池调度。
class ThreadFuncBase {};

// 让所有要在线程中执行的类都继承这个空基类，再定义出一个通用、统一的成员函数指针类型FUNCTYPE，
// 从而实现任意类的成员函数都能被封装、被线程池统一调度
typedef int (ThreadFuncBase::*FUNCTYPE)();


/*
ThreadWorker类的核心作用，是封装 C++对象指针 + 成员函数指针，解决非静态成员函数不能直接作为线程入口函数的问题。
它把“要执行的对象”和“要调用的成员函数”打包成一个可调用对象，让线程池可以统一、通用地调度任意类的任意成员函数，
不需要为每个业务单独写静态线程入口函数，从而实现线程复用、框架统一、代码解耦。
*/
class ThreadWorker {
public:
    ThreadWorker(): thiz(NULL), func(NULL){};

    //带参构造：绑定回调对象和成员函数指针
    //把「已存在的业务对象地址」和「该对象的成员函数指针」绑定在一起，为后续调用提供「合法的this指针」
    //void* obj 是「外部已创建对象的地址」，FUNCTYPE f 是该对象的成员函数指针
    //成员thiz：将外部对象地址强转为基类指针，作为后续调用的this指针
    //构造函数完成的是「一一对应绑定」：某个已存在对象的地址 和 该对象所属类的成员函数指针，二者绑定后，
    // 就形成了一个「可独立执行的回调单元」—— 后续只要通过operator()调用，就能把thiz作为this指针，
    // 传递给func指向的成员函数，完成合法调用。
    ThreadWorker(void* obj, FUNCTYPE f):thiz((ThreadFuncBase*)obj), func(f){}  
    
    // 拷贝构造：支持工作器对象的拷贝（线程池分发、容器存储需要）
    ThreadWorker(const ThreadWorker& worker) {
        thiz = worker.thiz;
        func = worker.func;
    }

    ThreadWorker& operator=(const ThreadWorker& worker) {
        if (this != &worker) {
            thiz = worker.thiz;
            func = worker.func;
        }
        return *this;
    }

    // 重载()运算符：让ThreadWorker成为「可调用对象」
    // 线程执行核心：调用绑定的成员函数，返回函数执行结果
    int operator()() {
        if (IsValid()) {
            return (thiz->*func)();   // 调用成员函数：对象指针->*成员函数指针
        }
        return -1;
    }

    bool IsValid() const{ return (thiz != NULL) && (func != NULL); }

private:
    ThreadFuncBase* thiz;   // 回调函数所属的对象指针
    FUNCTYPE func;          // 要执行的成员函数指针
};


// 可动态更新回调的工作线程类：核心底层线程工具
// 特性：支持运行中更新回调任务、线程安全的任务管理、优雅启停
class EdoyunThread
{
public:
    EdoyunThread() {
        m_hThread = NULL;
        m_bStatus = false;
    }

    ~EdoyunThread() {
        Stop();
    }

    // 启动线程：创建并运行工作线程，返回启动结果   true 表示成功  false表示失败
    bool Start() {
        m_bStatus = true;
        m_hThread = (HANDLE)_beginthreadex(NULL, 0, ThreadEntry, this, 0, NULL);
        if (!IsValid()) {
            m_bStatus = false;
        }
        return m_bStatus;
    }

    // 检查线程是否有效：句柄合法且线程正在运行
    bool IsValid() {
        if (m_hThread == NULL || (m_hThread == INVALID_HANDLE_VALUE)) return false;
        return WaitForSingleObject(m_hThread,0) == WAIT_TIMEOUT;
    }

    // 停止线程：优雅停止（等待1秒），超时则强制终止
    // 返回是否优雅停止成功
    bool Stop() {
        if(m_bStatus==FALSE) return true;
        m_bStatus = false;

        DWORD ret= WaitForSingleObject(m_hThread, 1000);  //等待线程自己退出，最多等 1 秒
        if (ret == WAIT_TIMEOUT) {
            TerminateThread(m_hThread, -1);        // 如果1s都没退出 强制终止线程
        }

        UpdateWorker();
        return ret==WAIT_OBJECT_0;
    }

    // 安全地【替换 / 清空】线程正在执行的任务（ThreadWorker），并且保证不内存泄漏、不崩溃、线程安全。
    void UpdateWorker(const::ThreadWorker& worker=::ThreadWorker()) {
        
        // 第一步：释放旧的工作器（如果存在且与新工作器不同）
        if (m_worker.load() != NULL&&(m_worker.load() != &worker)) {
            ::ThreadWorker* pWorker = m_worker.load();  // 原子加载旧工作器指针

            m_worker.store(NULL); // 原子置空，防止线程主循环访问
            delete pWorker;       // 释放旧工作器内存
        }
        
        if (m_worker.load() == &worker) return;  // 避免重复赋值：如果新工作器就是当前工作器，直接返回
        
        if (!worker.IsValid()) {      // 第二步：如果新工作器无效，直接置空即可
            m_worker.store(NULL);
            return;
        }

        // 第三步：创建新工作器的堆对象，原子更新到m_worker
        ::ThreadWorker* pWorker = new ::ThreadWorker(worker);
        TRACE("new pWorker=%08X  m_worker=%08X\r\n", pWorker, m_worker.load());
        m_worker.store(pWorker);   // 原子存储新工作器指针，线程主循环会立即感知
    }
    
    bool IsIdle() {     // 判断线程是否空闲：无有效工作器（无任务可执行）
        if (m_worker.load() == NULL)return true;    // 工作器指针为空，直接空闲
        return!m_worker.load()->IsValid();       // 工作器存在但无效，也视为空闲
    }

private:
    void ThreadWorker() {       // 线程主循环：线程的实际执行函数，处理工作器调用、循环运行
        while (m_bStatus) {
            if (m_worker.load() == NULL) {
                Sleep(1);       // 无工作器时，短暂休眠（1ms），减少CPU占用
                continue;
            }
            //通过原子 load 安全读取子线程共享的任务对象指针，解引用后拷贝到局部变量，
            // 避免主线程并发修改导致的线程安全问题，保证任务能够完整、稳定地执行。
            ::ThreadWorker worker =*m_worker.load();// 从原子指针取出工作器对象，拷贝到局部变量

            if (worker.IsValid()) {
                if (WaitForSingleObject(m_hThread, 0) == WAIT_TIMEOUT){   // 再次检查线程是否正在运行（防止执行中线程被停止）
                    int ret = worker();   //真正执行业务逻辑

                    if (ret != 0) {      // 回调返回非0：输出警告日志（自定义业务码）
                        CString str;
                        str.Format(_T("thread found warning code %d\r\n"), ret);
                        OutputDebugString(str);
                    }

                    if (ret < 0) {      // 回调返回负数：表示任务执行失败/需要停止，清空并释放工作器
                        ::ThreadWorker* pWorker = m_worker.load();
                        m_worker.store(NULL);
                        delete pWorker;
                    }
                }
            }
            else { Sleep(1); }   // 工作器无效，短暂休眠 
        }
    }

    static unsigned __stdcall ThreadEntry(void* arg) {
        EdoyunThread* thiz = (EdoyunThread*)arg;
        if (thiz) {
            thiz->ThreadWorker();
        }
        _endthreadex(0);
        return 0;
    }


private:
    HANDLE m_hThread;
    bool m_bStatus;    //true 表示线程正在运行     false表示线程将要关闭
    
    // 原子指针：存储工作器指针，保证多线程下「加载/存储」的原子性（线程安全，无锁）
    // 核心：主线程更新工作器、子线程执行工作器，无锁同步
    std::atomic<::ThreadWorker*> m_worker;

};


// 线程池类：管理多个EdoyunThread工作线程，实现任务的空闲分发
// 特性：固定线程数、基于空闲状态分发任务、线程安全的任务调度
class EdoyunThreadPool {
public:
    EdoyunThreadPool() {} 
    EdoyunThreadPool(size_t size) {       // 带参构造：创建指定数量的工作线程（初始为停止状态）
        m_threads.resize(size);
        for (size_t i = 0; i < size; i++) {
            m_threads[i] = new EdoyunThread();    // 创建工作线程对象，存入数组
        }
    }
    ~EdoyunThreadPool() {
        Stop();          // 先停止所有线程
        for (size_t i = 0; i < m_threads.size(); i++) {    // 遍历释放所有工作线程对象
            delete m_threads[i];
            m_threads[i] = NULL;
        }
        m_threads.clear();    // 清空动态数组
    }

    // 启动线程池：启动所有工作线程，返回启动结果
    // 有一个线程启动失败，则停止所有已启动线程，返回false
    bool Invoke() {
        bool ret = true;
        for (size_t i = 0; i < m_threads.size(); i++) {
            if (m_threads[i]->Start() == false) {
                ret = false;
                break;
            }
        }
        if (ret == false) {
            for (size_t i = 0; i < m_threads.size(); i++) {
                m_threads[i]->Stop();
            }
        }
        return ret;
    }

    // 停止线程池：停止所有工作线程
    void Stop() {
        for (size_t i = 0; i < m_threads.size(); i++) {
            m_threads[i]->Stop();
        }
    
    }

    // 分发工作器到线程池：寻找第一个空闲线程，绑定工作器并返回线程索引
    // 返回：-1-所有线程繁忙，分发失败；>=0-成功，返回绑定的线程索引
    // 线程安全：通过互斥锁保护临界区（遍历/修改线程状态）
    int DispatchWorker(const ThreadWorker& worker) {
        int index = -1;
        m_lock.lock();
        for (size_t i = 0; i < m_threads.size(); i++) {
            if (m_threads[i]->IsIdle()) {
                m_threads[i]->UpdateWorker(worker);
                index = i;
                break;
            }
        }
        m_lock.unlock();
        return index;
    }

    // 检查指定索引的线程是否有效（正在运行）
    // index：线程索引，需小于线程池大小
    bool CheckThreadValid(size_t index) {
        if (index < m_threads.size()) {
            return m_threads[index]->IsValid();
        }
        return false;
    }

private:
    std::mutex m_lock;                      // 互斥锁：保护线程池的任务分发、线程状态修改等临界区
    std::vector<EdoyunThread*> m_threads;   // 线程池底层存储：动态数组存工作线程指针
};


