#pragma once
#include<atomic>
#include"pch.h"

template<class T>
class CEdoyunQueue        //线程安全队列（利用IOCP）
{
public:

    enum {
        EQNone,
        EQPush,
        EQPop,
        EQSize,
        EQClear,
    };

    // IOCP投递的「任务参数结构体」，所有要执行的任务数据都存在这里
    typedef struct IocpParam {
        int nOperator;                    // 核心：要执行的操作类型（对应上面的枚举）
        T Data;                        // 操作携带的字符串数据
        HANDLE hEvent;
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
    CEdoyunQueue() {
        m_lock = false;
        m_hCompeletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 1);
        m_hThread = INVALID_HANDLE_VALUE;
        if (m_hCompeletionPort != NULL) {
            m_hThread = (HANDLE)_beginthread(&CEdoyunQueue<T>::threadEntry, 0, m_hCompeletionPort);
        }
    }

    ~CEdoyunQueue() {
        if (m_lock)return;
        m_lock = true;
        HANDLE hTemp = m_hCompeletionPort;
        PostQueuedCompletionStatus(m_hCompeletionPort, 0, NULL, NULL);
        WaitForSingleObject(m_hThread, INFINITE);
        m_hCompeletionPort = NULL;
        CloseHandle(hTemp);

    }

    bool PushBack(const T& data) {
        IocpParam* pParam = new IocpParam(EQPush, data);

        if (m_lock) {
            delete pParam;
            return false;
        }
        bool ret=PostQueuedCompletionStatus(m_hCompeletionPort, sizeof(PPARAM), (ULONG_PTR)pParam, NULL);
        if (ret == false)delete pParam;
        return ret;
    }

    bool PopFront(T& data) {
        HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        IocpParam Param(EQPop, data, hEvent);
        if (m_lock) {
            if (hEvent)CloseHandle(hEvent);
            return false;
        }
        bool ret = PostQueuedCompletionStatus(m_hCompeletionPort, sizeof(PPARAM), (ULONG_PTR)&Param, NULL);
        if (ret == false) {
            CloseHandle(hEvent);
            return false;
        }
        ret=WaitForSingleObject(hEvent, INFINITE)== WAIT_OBJECT_0;
        if (ret) {
            data = Param.Data;
        }
        return ret;
    }

    size_t Size() {
        HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        IocpParam Param(EQSize, T(), hEvent);
        if (m_lock) {
            if (hEvent)CloseHandle(hEvent);
            return -1;
        }
        bool ret = PostQueuedCompletionStatus(m_hCompeletionPort, sizeof(PPARAM), (ULONG_PTR)&Param, NULL);
        if (ret == false) {
            CloseHandle(hEvent);
            return -1;
        }
        ret = WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0;
        if (ret) {
            return Param.nOperator;
        }
        return -1;
    
    }

    bool Clear() {
        if (m_lock)return false;
        IocpParam* pParam = new IocpParam(EQClear, T());
        bool ret = PostQueuedCompletionStatus(m_hCompeletionPort, sizeof(PPARAM), (ULONG_PTR)pParam, NULL);
        if (ret == false)delete pParam;
        return ret;
    
    }
private:
    static void threadEntry(void* arg) {
    
        CEdoyunQueue<T>* thiz = (CEdoyunQueue<T>*)arg;
        thiz->threadMain();
        _endthread();
    }

    void DealParam(PPARAM* pParam) {
        switch (pParam->nOperator)
        {
        case EQPush:
            m_lstData.push_back(pParam->Data);
            delete pParam;
            break;

        case EQPop:
            if (m_lstData.size() > 0) {
                pParam->Data = m_lstData.front();
                m_lstData.pop_front();
            }
            if (pParam->hEvent != NULL) SetEvent(pParam->hEvent);
            break;

        case EQSize:
            pParam->nOperator = m_lstData.size();
            if (pParam->hEvent != NULL)
                SetEvent(pParam->hEvent);
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

    void threadMain() {
        DWORD dwTransferred = 0;
        PPARAM* pParam = NULL; 
        ULONG_PTR CompletionKey = 0;
        OVERLAPPED* pOverlapped = NULL;

        while (GetQueuedCompletionStatus(m_hCompeletionPort, &dwTransferred, &CompletionKey, &pOverlapped, INFINITE)) {     // 死循环,在这里阻塞,从 IOCP 队列取任务
            if ((dwTransferred == 0) || (CompletionKey == NULL)) {
                printf("线程准备退出！\r\n");
                break;
            }
            pParam = (PPARAM*)CompletionKey;     // 把完成键强转回我们的任务结构体指针，拿到本次要执行的任务
            DealParam(pParam);
            
        }
        while (GetQueuedCompletionStatus(m_hCompeletionPort, &dwTransferred, &CompletionKey, &pOverlapped, 0)) {
            if ((dwTransferred == 0) || (CompletionKey == NULL)) {
                printf("线程准备退出！\r\n");
                continue;
            }
            pParam = (PPARAM*)CompletionKey;     // 把完成键强转回我们的任务结构体指针，拿到本次要执行的任务
            DealParam(pParam);
        }
        CloseHandle(m_hCompeletionPort);
    }

private:
    std::list<T> m_lstData;
    HANDLE m_hCompeletionPort;
    HANDLE m_hThread;
    std::atomic<bool> m_lock;





};

