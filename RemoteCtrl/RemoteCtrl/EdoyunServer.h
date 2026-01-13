#pragma once
#include <MSWSock.h>
#include "EdoyunThread.h"
#include "CEdoyunQueue.h"
#include <map>




enum EdoyunOperator{
	ENone,
	EAccept,
	ERecv,
	ESend,
	EError

};


class EdoyunServer;
class EdoyunClient;
typedef std::shared_ptr<EdoyunClient> PCLIENT;

class EdoyunOverlapped {
public:
	OVERLAPPED m_overlapped;
	DWORD m_operator;            // 重叠IO的操作类型，对应EdoyunOperator枚举 
	std::vector<char> m_buffer;   
	ThreadWorker m_worker;       // 线程池任务对象：封装【处理函数+调用者】，用于线程池异步执行业务逻辑
	EdoyunServer* m_server;
};

template<EdoyunOperator>class AcceptOverlapped;
typedef AcceptOverlapped<EAccept> ACCEPTOVERLAPPED;   // 类型别名：简化模板类调用，固定模板参数为【EAccept】

class EdoyunClient {
public:
	EdoyunClient();

	~EdoyunClient() { closesocket(m_sock); }

	void SetOverlapped(PCLIENT& ptr);


	operator SOCKET() { return m_sock; }

	operator PVOID() { return &m_buffer[0]; }

	operator LPOVERLAPPED();

	operator LPDWORD() { return &m_received; }

	sockaddr_in* GetLoaclAddr() { return &m_laddr; }
	sockaddr_in* GetRemoteAddr() { return &m_raddr; }

private:
	SOCKET m_sock;
	DWORD m_received;
	std::shared_ptr<ACCEPTOVERLAPPED> m_overlapped;
	std::vector<char> m_buffer;
	sockaddr_in m_laddr;
	sockaddr_in m_raddr;
	bool m_isbusy;
};



// Accept 连接事件对象    模板参数：指定当前事件的操作类型，实现「一个模板，多种操作」的复用
template<EdoyunOperator>  
class AcceptOverlapped :public EdoyunOverlapped, ThreadFuncBase {     

public:
	AcceptOverlapped();
	int AcceptWorker();
	PCLIENT m_client;

};




template<EdoyunOperator>
class RecvOverlapped :public EdoyunOverlapped, ThreadFuncBase {

public:
	RecvOverlapped() :m_operator(EdoyunOperator::ERecv), m_worker(this, &RecvOverlapped::RecvWorker) {
		memset(&m_overlapped, 0, sizeof(m_overlapped));
		m_buffer.resize(1024*256);

	}
	int RecvWorker() {


	}
};
typedef RecvOverlapped<ERecv> RECVOVERLAPPED;



template<EdoyunOperator>
class SendOverlapped :public EdoyunOverlapped, ThreadFuncBase {

public:
	SendOverlapped() :m_operator(EdoyunOperator::ESend), m_worker(this, &SendOverlapped::SendWorker) {
		memset(&m_overlapped, 0, sizeof(m_overlapped));
		m_buffer.resize(1024 * 256);

	}
	int SendWorker() {


	}
};
typedef SendOverlapped<ESend> SENDOVERLAPPED;



template<EdoyunOperator>
class ErrorOverlapped :public EdoyunOverlapped, ThreadFuncBase {

public:
	ErrorOverlapped() :m_operator(EdoyunOperator::EError), m_worker(this, &ErrorOverlapped::ErrorWorker) {
		memset(&m_overlapped, 0, sizeof(m_overlapped));
		m_buffer.resize(1024);

	}
	int ErrorWorker() {


	}
};

typedef ErrorOverlapped<EError> ERROROVERLAPPED;






class EdoyunServer :public ThreadFuncBase{
public:
	EdoyunServer(const std::string& ip="0.0.0.0",short port=9527) :m_pool(10) {
		m_hIOCP=INVALID_HANDLE_VALUE;
		m_sock = INVALID_SOCKET;                                        
		m_addr.sin_family = AF_INET;
		m_addr.sin_port = htons(port);
		m_addr.sin_addr.s_addr = inet_addr(ip.c_str());
	}

	~EdoyunServer() {}

	bool StartService() {
		
		CreateSocket();

		if (bind(m_sock, (sockaddr*)&m_addr, sizeof(m_addr)) == -1) {
			closesocket(m_sock);
			m_sock = INVALID_SOCKET;
			return false;
		}

		if (listen(m_sock, 3) == -1) {
			closesocket(m_sock);
			m_sock = INVALID_SOCKET;
			return false;
		}

		m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 4);
		if (m_hIOCP == NULL) {
			closesocket(m_sock);
			m_sock = INVALID_SOCKET;
			m_hIOCP = INVALID_HANDLE_VALUE;
			return false;
		}

		CreateIoCompletionPort((HANDLE)m_sock, m_hIOCP, (ULONG_PTR)this, 0);   //将【监听套接字】绑定到完成端口,该套接字的所有重叠IO事件，都会投递到这个IOCP中
		m_pool.Invoke();
		m_pool.DispatchWorker(ThreadWorker(this, (FUNCTYPE)&EdoyunServer::threadIocp));  //线程池分发【IOCP核心工作线程】：常驻后台循环获取IO事件
		if (!NewAccept())return false;
		return true;
	}

	bool NewAccept() {
		PCLIENT pClient(new EdoyunClient());
		pClient->SetOverlapped(pClient);
		m_client.insert(std::pair<SOCKET, PCLIENT>(*pClient, pClient));

		if (!AcceptEx(m_sock, *pClient, *pClient, 0, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, *pClient, *pClient)) {

			closesocket(m_sock);
			m_sock = INVALID_SOCKET;
			m_hIOCP = INVALID_HANDLE_VALUE;
			return false;
		}
		return true;
	}
	

private:
	void CreateSocket() {
		m_sock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);  //创建【重叠IO模式】的监听套接字
		int opt = 1;                                                                //设置端口复用：解决服务器重启后端口被占用的问题
		setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
	}

	


	//IOCP核心工作线程函数,循环从IOCP中取出完成的重叠IO事件，分发到线程池处理
	int threadIocp() {
		DWORD transferred = 0;                    // 本次IO实际传输的字节数：recv/send的字节数
		ULONG_PTR CompletionKey = 0;              // 完成键：绑定套接字时传入的参数，此处是EdoyunServer*
		OVERLAPPED* lpOverlapped = NULL;

		if (GetQueuedCompletionStatus(m_hIOCP, &transferred, &CompletionKey, &lpOverlapped, INFINITE)) {  //阻塞等待IOCP中的完成事件
			if (transferred > 0 && (CompletionKey != 0)) {
				EdoyunOverlapped* pOverlapped = CONTAINING_RECORD(lpOverlapped, EdoyunOverlapped, m_overlapped);  //通过【结构体的成员指针】反推出【结构体对象的首地址】
				switch (pOverlapped->m_operator) {           // 根据IO操作类型，分发到对应的业务处理逻辑
					case EAccept:
					{
						ACCEPTOVERLAPPED* pOver=(ACCEPTOVERLAPPED*) pOverlapped;
						m_pool.DispatchWorker(pOver->m_worker);
					}break;

					case ERecv:
					{
						RECVOVERLAPPED* pOver = (RECVOVERLAPPED*)pOverlapped;
						m_pool.DispatchWorker(pOver->m_worker);
					}break;

					case ESend:
					{
						SENDOVERLAPPED* pOver = (SENDOVERLAPPED*)pOverlapped;
						m_pool.DispatchWorker(pOver->m_worker);
					}break;

                    case EError:
					{
						ERROROVERLAPPED* pOver = (ERROROVERLAPPED*)pOverlapped;
						m_pool.DispatchWorker(pOver->m_worker);
					}break;

				}
			}
			else {
				return -1;
			}
		}

		return 0;


	}
private:
	EdoyunThreadPool m_pool;                // 线程池对象：初始化10个工作线程，异步处理所有业务逻辑
	HANDLE m_hIOCP;                         // IOCP完成端口核心句柄
	SOCKET m_sock;                          // 服务器监听套接字
	sockaddr_in m_addr;                       // 服务器地址
	std::map<SOCKET, std::shared_ptr<EdoyunClient>> m_client; // 客户端管理容器：fd映射客户端对象，智能指针自动释放

};


