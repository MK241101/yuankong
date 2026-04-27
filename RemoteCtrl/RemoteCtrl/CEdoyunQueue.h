#pragma once
#include<atomic>
#include"pch.h"
#include<list>
#include"EdoyunThread.h"

template<class T>
class CEdoyunQueue        //线程安全队列（利用IOCP）
{
public:

    enum {
        EQNone,          // 无操作
        EQPush,          // 入队操作：向队列尾部添加元素
        EQPop,           // 出队操作：从队列头部取出元素
        EQSize,          // 获取队列长度
        EQClear          // 清空队列
    };

    // IOCP投递的「任务参数结构体」：所有队列操作的【指令+数据】都通过该结构体传递
    // 该结构体是 主线程 和 IOCP工作线程 之间的通信载体
    typedef struct IocpParam {
        int nOperator;                    // 核心：要执行的操作类型（对应上面的枚举）
        T Data;                           // 操作携带的字符串数据
        HANDLE hEvent;                    // 事件句柄：用于【同步阻塞】，出队/获取长度时等待执行结果

        IocpParam(int op, const T& data, HANDLE hEve=NULL) {
            nOperator = op;
            Data = data;
            hEvent = hEve;
        }
        IocpParam() {
            nOperator = EQNone;
        }
    }PPARAM;

public: 
    CEdoyunQueue() {              //初始化IOCP核心资源+启动工作线程
        m_lock = false;           // 原子布尔锁：标记队列是否已销毁，原子操作保证线程安全

        //最后一个参数表示最大并发线程数：最多1个线程处理完成事件
        m_hCompeletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 1);  
        m_hThread = INVALID_HANDLE_VALUE;      // 初始化工作线程句柄
        if (m_hCompeletionPort != NULL) {      //如果IOCP创建成功，启动工作线程
            m_hThread = (HANDLE)_beginthread(&CEdoyunQueue<T>::threadEntry, 0, this);
        }
    }

    /*
    当一个类被设计为基类（允许被其他类继承）时，若通过基类指针 / 引用销毁派生类对象，虚析构能保证派生类的
    析构函数会被优先调用，再调用基类析构函数，从而完整释放所有资源；如果析构函数不是虚函数，只会调用基类析构，
    派生类的析构不会执行，导致资源泄漏。
    */

    virtual ~CEdoyunQueue() {        
        if (m_lock) return;       // 如果已经加锁，直接返回，防止重复销毁
        m_lock = true;            // 加锁：标记队列开始销毁，禁止再投递新任务
        PostQueuedCompletionStatus(m_hCompeletionPort, 0, NULL, NULL);        // 投递【退出指令】到IOCP
        WaitForSingleObject(m_hThread, INFINITE);           // 等待工作线程执行完毕，保证优雅退出

        //if (m_hCompeletionPort != NULL) {        // 释放IOCP句柄资源
        //    HANDLE hTemp = m_hCompeletionPort;
        //    m_hCompeletionPort = NULL;
        //    CloseHandle(hTemp);
        //}
    }

    bool PushBack(const T& data) {         // 向队列尾部添加元素，非阻塞
        IocpParam* pParam = new IocpParam(EQPush, data);        //封装【入队指令+要入队的数据】

        if (m_lock) {                     // 如果队列已销毁，释放内存并返回失败
            delete pParam;
            return false;
        }
        bool ret=PostQueuedCompletionStatus(m_hCompeletionPort, sizeof(PPARAM), (ULONG_PTR)pParam, NULL);   //把任务投递到IOCP完成端口
        if (ret == false)delete pParam;   // 把任务投递到IOCP完成端口
        return ret;
    }

    virtual bool PopFront(T& data) {      // 从队列头部取出元素，阻塞式
        HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);  //创建【手动重置事件】：用于阻塞等待出队操作执行完成
        IocpParam Param(EQPop, data, hEvent);                  //封装【出队指令 + 事件句柄】
        
        if (m_lock) {                     // 如果队列已销毁，释放事件句柄并返回失败
            if (hEvent) CloseHandle(hEvent);
            return false;
        }
        bool ret = PostQueuedCompletionStatus(m_hCompeletionPort, sizeof(PPARAM), (ULONG_PTR)&Param, NULL);  //投递出队任务到IOCP
        /*
        (ULONG_PTR)&Param：主线程栈对象的内存地址，强转后作为「完成键」，投递到 IOCP 完成端口
        把主线程栈对象的地址，交给 IOCP 线程的核心操作，也是跨线程访问的起点
        */

        if (ret == false) {              // 投递失败，释放事件句柄
            CloseHandle(hEvent);
            return false;
        }
        ret=WaitForSingleObject(hEvent, INFINITE)== WAIT_OBJECT_0;     //核心：阻塞等待！直到IOCP工作线程执行完出队操作，并触发该事件
        if (ret) {
            data = Param.Data;          //出队成功，把工作线程取出的值赋值给外部参数
        }
        //CloseHandle(hEvent);
        return ret;
    }

    size_t Size() {              //获取队列长度接口，阻塞式
        HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);     //创建事件句柄，用于阻塞等待获取长度结果
        IocpParam Param(EQSize, T(), hEvent);         //封装【获取长度指令+事件句柄】
        if (m_lock) {
            if (hEvent)CloseHandle(hEvent);
            return -1;
        }
        bool ret = PostQueuedCompletionStatus(m_hCompeletionPort, sizeof(PPARAM), (ULONG_PTR)&Param, NULL);  //投递获取长度任务到IOCP
        if (ret == false) {
            CloseHandle(hEvent);
            return -1;
        }
        ret = WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0;      //阻塞等待工作线程执行完成
        if (ret) {
            return Param.nOperator;
        }
        //CloseHandle(hEvent);

        return -1;
    
    }

    bool Clear() {              //外提供的【清空队列接口】：线程安全，非阻塞
        if (m_lock)return false;
        IocpParam* pParam = new IocpParam(EQClear, T());   // 新建任务参数，封装【清空队列指令】
        bool ret = PostQueuedCompletionStatus(m_hCompeletionPort, sizeof(PPARAM), (ULONG_PTR)pParam, NULL);  // 投递清空任务到IOCP
        if (ret == false)delete pParam;
        return ret;
    
    }
protected:
    static void threadEntry(void* arg) {
        CEdoyunQueue<T>* thiz = (CEdoyunQueue<T>*)arg;
        thiz->threadMain();
        _endthread();
    }

/*
* DealParam()
IOCP 线程安全队列的「核心业务处理中枢」，IOCP 工作线程从完成端口拿到任务参数（PPARAM）后，会调用这个函数，
解析参数中的「操作指令」（入队 / 出队 / 查长度 / 清空），并执行对应的队列核心操作（对 m_lstData 的读写）。
*/
    virtual void DealParam(PPARAM* pParam) {   
        switch (pParam->nOperator)
        {
        case EQPush:
            m_lstData.push_back(pParam->Data);
            delete pParam;
            break;

        case EQPop:
            if (m_lstData.size() > 0) {             // IOCP 线程，通过内存地址，直接修改了主线程栈对象的成员变量 
                pParam->Data = m_lstData.front();   //【IOCP线程 访问+修改 主线程的栈对象】   跨线程访问栈对象
                m_lstData.pop_front();
            }
            if (pParam->hEvent != NULL) { SetEvent(pParam->hEvent); }  // 触发事件，通知主线程出队完成   跨线程访问栈对象
            break;

        case EQSize:
            pParam->nOperator = m_lstData.size();
            if (pParam->hEvent != NULL) SetEvent(pParam->hEvent);
            break;

        case EQClear:
            m_lstData.clear();
            delete pParam;
            break;

        default:
            OutputDebugStringA("unknown operator!\r\n");
            break;
        }
    }

    virtual void threadMain() {
        DWORD dwTransferred = 0;        // 接收传输字节数，无实际意义，仅为API参数
        PPARAM* pParam = NULL;          // 指向任务参数结构体的指针
        ULONG_PTR CompletionKey = 0;    // 完成键：核心！接收投递过来的任务参数指针
        OVERLAPPED* pOverlapped = NULL; // 重叠结构，这里没用(IOCP用于文件/socket时才用)

        // 阻塞等待从IOCP中获取任务，INFINITE: 永久阻塞，直到有任务投递过来
        while (GetQueuedCompletionStatus(m_hCompeletionPort, &dwTransferred, &CompletionKey, &pOverlapped, INFINITE)) {     
            if ((dwTransferred == 0) || (CompletionKey == NULL)) {
                printf("线程准备退出！\r\n");
                break;
            }
            pParam = (PPARAM*)CompletionKey;      // 把完成键强转回任务参数指针，拿到本次要执行的任务
            DealParam(pParam);                    // 执行具体的队列操作
             
        }
        // 非阻塞遍历IOCP剩余的任务，保证所有任务都执行完毕,  0：非阻塞，有任务就执行，没任务就退出
        while (GetQueuedCompletionStatus(m_hCompeletionPort, &dwTransferred, &CompletionKey, &pOverlapped, 0)) {
            if ((dwTransferred == 0) || (CompletionKey == NULL)) {
                printf("线程准备退出！\r\n");
                continue;
            }
            pParam = (PPARAM*)CompletionKey;      // 把完成键强转回任务参数指针，拿到本次要执行的任务
            DealParam(pParam);                    // 执行具体的队列操作
        }
        HANDLE hTemp = m_hCompeletionPort;
        m_hCompeletionPort = NULL;
        CloseHandle(m_hCompeletionPort);
    }

protected:
    std::list<T> m_lstData;            // 底层存储容器：std::list，增删效率高，是整个队列的 “数据载体”
    HANDLE m_hCompeletionPort;         // IOCP核心句柄：完成端口句柄
    HANDLE m_hThread;                  // IOCP工作线程句柄
    std::atomic<bool> m_lock;          // 原子布尔锁：线程安全的销毁标记，无锁开销
};

/*
继承关系：
1. 继承CEdoyunQueue<T>：复用其IOCP线程安全队列的核心能力（入队/出队/存储）
2. 继承ThreadFuncBase：为了兼容回调函数的调用（回调函数是ThreadFuncBase的成员函数）
*/

template<class T>
class EdoyunSendQueue :public CEdoyunQueue<T>,public ThreadFuncBase {   // 自动发送队列
public:
    // 定义回调函数类型：ThreadFuncBase类的成员函数指针，入参是T&，返回值int
    // 用途：队列元素出队后，通过该回调函数处理元素（比如发送网络数据）
    typedef int (ThreadFuncBase::* EDYCALLBACK)(T& data);

    EdoyunSendQueue(ThreadFuncBase* obj, EDYCALLBACK callback):
        CEdoyunQueue<T>(),         // 调用基类CEdoyunQueue的构造函数，初始化IOCP队列
        m_base(obj),               // 保存回调对象
        m_callback(callback) {     // 保存回调函数指针
            m_thread.Start();      // 启动线程
            // 绑定线程的工作函数为当前类的threadTick（线程循环检测队列并消费）
            m_thread.UpdateWorker(::ThreadWorker(this,(FUNCTYPE)& EdoyunSendQueue<T>::threadTick));
    }
    
    virtual ~EdoyunSendQueue() {
        m_base = NULL;
        m_callback = NULL;
        m_thread.Stop();
    }

protected:
    // 重写基类的PopFront(T&)：禁用原有的“阻塞式出队并返回数据”功能
    // 原因：本队列是自动消费，不需要外部主动获取数据，因此直接返回false
    virtual bool PopFront(T& data) { return false; };
    
    // 自定义无参版PopFront：仅触发出队操作（不返回数据），非阻塞
    // 作用：通知IOCP线程执行出队逻辑（真正的出队+回调处理在DealParam中）
    bool PopFront(){                                          
        typename CEdoyunQueue<T>::IocpParam* Param=new typename CEdoyunQueue<T>::IocpParam(CEdoyunQueue<T>::EQPop, T());   //封装【出队指令 + 事件句柄】

        if (CEdoyunQueue<T>::m_lock) {                     // 如果队列已销毁，释放事件句柄并返回失败
            delete Param;
            return false;
        }
        bool ret = PostQueuedCompletionStatus(CEdoyunQueue<T>::m_hCompeletionPort, sizeof(*Param), (ULONG_PTR)&Param, NULL);  //投递出队任务到IOCP
        /*
        (ULONG_PTR)&Param：主线程栈对象的内存地址，强转后作为「完成键」，投递到 IOCP 完成端口
        把主线程栈对象的地址，交给 IOCP 线程的核心操作，也是跨线程访问的起点
        */
        if (ret == false) {              // 投递失败，释放事件句柄
            delete Param;
            return false;
        }
        
        //CloseHandle(hEvent);
        return ret;
    }

    // 消费线程的核心循环函数（由EdoyunThread周期性调用）
    // 作用：检测队列是否有元素，有则触发出队+回调处理
    int threadTick() {
        if ((WaitForSingleObject(CEdoyunQueue<T>::m_hThread, 0) != WAIT_TIMEOUT))
            return 0;
        if (CEdoyunQueue<T>::m_lstData.size() > 0) {
            PopFront();
        }
        //Sleep(1);
        return 0;
    }

    // 重写基类的DealParam：核心修改出队逻辑，增加回调处理
    // 作用：IOCP线程执行任务时，调用此函数；出队后先调用回调处理元素，仅回调成功才删除元素
    virtual void DealParam(typename CEdoyunQueue<T>::PPARAM* pParam) {
        switch (pParam->nOperator)
        {
        case CEdoyunQueue<T>::EQPush:
            CEdoyunQueue<T>::m_lstData.push_back(pParam->Data);
            delete pParam;
            break;

        case CEdoyunQueue<T>::EQPop:
            if (CEdoyunQueue<T>::m_lstData.size() > 0) {             // IOCP 线程，通过内存地址，直接修改了主线程栈对象的成员变量 
                pParam->Data = CEdoyunQueue<T>::m_lstData.front();   //  1. 取数据
                if((m_base->*m_callback)(pParam->Data)==0)           //  2. 调用回调函数！！！
                    CEdoyunQueue<T>::m_lstData.pop_front();
            }
            delete pParam;
            break;

        case CEdoyunQueue<T>::EQSize:
            pParam->nOperator = CEdoyunQueue<T>::m_lstData.size();
            if (pParam->hEvent != NULL) SetEvent(pParam->hEvent);
            break;

        case CEdoyunQueue<T>::EQClear:
            CEdoyunQueue<T>::m_lstData.clear();
            delete pParam;
            break;

        default:
            OutputDebugStringA("unknown operator!\r\n");
            break;
        }
    }

private:
    ThreadFuncBase* m_base;       // 回调函数所属的对象指针
    EDYCALLBACK m_callback;       // 处理队列元素的回调函数指针
    EdoyunThread m_thread;        // 自动消费线程（检测队列并触发出队）
};

//存储字符向量的 EdoyunSendQueue 队列中，定义的回调函数类型
typedef EdoyunSendQueue<std::vector<char>>::EDYCALLBACK SENDCALLBACK;