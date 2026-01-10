#pragma once


template<class T>
class CEdoyunQueue        //线程安全队列（利用IOCP）
{
public:
    CEdoyunQueue();
    ~CEdoyunQueue();
    bool PushBack(const T& data);
    bool PopFront(T& data);
    size_t Size();
    void Clear();
private:
    static void threadEntry(void* arg);
    void threadMain();

private:
    std::list<T> m_lstData;
    HANDLE m_hCompeletionPort;
    HANDLE m_hThread;



public:
    // IOCP投递的「任务参数结构体」，所有要执行的任务数据都存在这里
    typedef struct IocpParam {
        int nOperator;                    // 核心：要执行的操作类型（对应上面的枚举）
        T strData;                        // 操作携带的字符串数据
        HANDLE hEvent;
        IocpParam(int op, const char* sData, _beginthread_proc_type cb = NULL) {
            nOperator = op;
            strData = sData;
        }
        IocpParam() {
            nOperator = -1;
        }
    }PPARAM;

    enum {
        EQPush,
        EQPop,
        EQSize,
        EQClear,
    };



};

