#pragma once
#include <MSWSock.h>
#include "EdoyunThread.h"
#include "CEdoyunQueue.h"
#include <map>
#include "EdoyunTool.h"


//标记重叠IO事件类型
enum EdoyunOperator{
	ENone,		// 无操作
	EAccept,	// 客户端连接接入操作
	ERecv,		// 数据接收操作
	ESend,		// 数据发送操作
	EError		// 错误处理操作

};

class EdoyunServer;
class EdoyunClient;
typedef std::shared_ptr<EdoyunClient> PCLIENT;
 
class EdoyunOverlapped {         // 所有具体IO操作重叠对象的父类
public:
	OVERLAPPED m_overlapped;
	DWORD m_operator;            // 重叠IO的操作类型，对应EdoyunOperator枚举 
	std::vector<char> m_buffer;  // IO操作的缓冲区：接收/发送数据都存在这里
	ThreadWorker m_worker;       // 线程池任务对象：封装【处理函数+调用者】，用于线程池异步执行业务逻辑
	EdoyunServer* m_server;      // 指向所属的服务器对象

	EdoyunClient* m_client;      // 指向当前IO操作关联的客户端对象
	WSABUF m_wsabuffer;          // Windows网络编程缓冲区，用于WSARecv/WSASend等重叠IO函数
	virtual ~EdoyunOverlapped() {
		m_buffer.clear();
	}
};

template<EdoyunOperator>class AcceptOverlapped;
typedef AcceptOverlapped<EAccept> ACCEPTOVERLAPPED;   // 类型别名：简化模板类调用，固定模板参数为【EAccept】

template<EdoyunOperator>class RecvOverlapped;     // 模板声明
typedef RecvOverlapped<ERecv> RECVOVERLAPPED;     // 接收专用别名

template<EdoyunOperator>class SendOverlapped;
typedef SendOverlapped<ESend> SENDOVERLAPPED;


class EdoyunClient:public ThreadFuncBase {      // 客户端连接对象，每个客户端连接对应一个Socket、一套接收/发送/接入的重叠对象、缓冲区等
public:
	EdoyunClient();      // 作用：创建一个新的客户端对象，并初始化所有资源

	~EdoyunClient() { 
		m_buffer.clear();
		closesocket(m_sock); 
		m_recv.reset(); 
		m_send.reset();
		m_overlapped.reset();
		m_vecSend.Clear();

	}

	void SetOverlapped(PCLIENT& ptr);			 // 给当前客户端的所有重叠对象绑定自身的智能指针，让重叠对象能关联到客户端

	operator SOCKET() { return m_sock; }		 // 重载类型转换符：只要需要 SOCKET 的地方，直接传客户端对象，编译器自动转
	operator PVOID() { return &m_buffer[0]; }    // 重载类型转换符：直接将客户端对象转为缓冲区首地址，给AcceptEx用
	operator LPOVERLAPPED();                     // 重载类型转换符：直接将客户端对象转为重叠IO结构体指针，给重叠IO函数用
	operator LPDWORD() { return &m_received; }   // 重载类型转换符：直接将客户端对象转为DWORD指针，接收重叠IO的返回字节数

	LPWSABUF RecvWSABuffer();                    // 获取当前客户端的接收专用WSABUF缓冲区指针
	LPWSAOVERLAPPED RecvOverlapped();            // 获取当前客户端的接收专用WSABUF重叠结构指针
	LPWSABUF SendWSABuffer();                    // 获取当前客户端的发送专用WSABUF缓冲区指针
	LPWSAOVERLAPPED SendOverlapped();            // 获取当前客户端的发送专用WSABUF重叠结构指针

	DWORD& flags() { return m_flags; }						// 获取重叠IO的flags标记（WSARecv/WSASend用）
	sockaddr_in* GetLocalAddr() { return &m_laddr; }		// 获取客户端绑定的本地地址
	sockaddr_in* GetRemoteAddr() { return &m_raddr; }		// 获取客户端的远端地址（客户端IP+端口）
	size_t GetBufferSize()const { return m_buffer.size(); } // 获取缓冲区总大小

	int Recv();
	int Send(void* buffer, size_t nSize);      // 给外部调用的【发送接口】，只管把数据塞进队列，不直接发送！
	int SendData(std::vector<char>& data);     // 内部真正执行【异步发送】，调用 WSASend 投递到 IOCP！

private:

	SOCKET m_sock;                                      // 客户端专属Socket
	DWORD m_received;                                   // 接收重叠IO的返回字节数
	DWORD m_flags;										// 重叠IO的标记
	std::shared_ptr<ACCEPTOVERLAPPED> m_overlapped;     // 连接重叠对象
	std::shared_ptr<RECVOVERLAPPED> m_recv;             // 接收重叠对象
	std::shared_ptr<SENDOVERLAPPED> m_send;             // 发送重叠对象

	std::vector<char> m_buffer;                        // 数据缓冲区
	size_t m_used;                                     // 已经使用的缓冲区大小
	sockaddr_in m_laddr;                               // 客户端绑定的本地地址
	sockaddr_in m_raddr;                               // 客户端的远端地址
	bool m_isbusy;                                     // 标记客户端不忙
	EdoyunSendQueue<std::vector<char>>m_vecSend;       // 发送数据的队列

};



// Accept 连接事件对象    模板参数：指定当前事件的操作类型，实现「一个模板，多种操作」的复用
template<EdoyunOperator>  
class AcceptOverlapped :public EdoyunOverlapped, ThreadFuncBase {     

public:
	AcceptOverlapped();
	int AcceptWorker();          // Accept事件的核心业务处理函数，线程池异步执行

};


template<EdoyunOperator>
class RecvOverlapped :public EdoyunOverlapped, ThreadFuncBase {
public:
	RecvOverlapped();
	int RecvWorker() {
		int ret=m_client->Recv();
		return ret;
	}
};


template<EdoyunOperator>
class SendOverlapped :public EdoyunOverlapped, ThreadFuncBase {

public:
	SendOverlapped(); 
	int SendWorker() {
		if (m_client == nullptr) {
			return -1;
		}
		if (m_client->m_vecSend.Size() > 0) {
			// 调用客户端发送函数，继续发下一条
			return m_client->SendData();
		}

		return 0;
	}
};



template<EdoyunOperator>
class ErrorOverlapped :public EdoyunOverlapped, ThreadFuncBase {

public:
	ErrorOverlapped() :m_operator(EdoyunOperator::EError), m_worker(this, &ErrorOverlapped::ErrorWorker) {
		memset(&m_overlapped, 0, sizeof(m_overlapped));
		m_buffer.resize(1024);

	}
	int ErrorWorker() {

		return -1;

	}
};

typedef ErrorOverlapped<EError> ERROROVERLAPPED;




class EdoyunServer :public ThreadFuncBase{
public:
	/*  作用：服务器启动前的【初始化准备工作】
	    线程池创建10个工作线程，初始化完成端口、Socket、ip地址、端口号
	*/
	EdoyunServer(const std::string& ip="0.0.0.0",short port=9527) :m_pool(10) {
		m_hIOCP=INVALID_HANDLE_VALUE;
		m_sock = INVALID_SOCKET;                                        
		m_addr.sin_family = AF_INET;
		m_addr.sin_port = htons(port);
		m_addr.sin_addr.s_addr = inet_addr(ip.c_str());
	}

	// 关闭监听 Socket、释放所有客户端、清空客户端列表、关闭 IOCP 端口、停止线程池、清理网络库
	~EdoyunServer();

	bool StartService();  //启动服务器：初始化网络、绑定端口、监听、创建IOCP、开始接受连接

	bool NewAccept();
	
	void BindNewSocket(SOCKET s);
private:
	void CreateSocket();

	int threadIocp();  //IOCP核心工作线程函数,循环从IOCP中取出完成的重叠IO事件，分发到线程池处理
private:
	EdoyunThreadPool m_pool;                // 线程池对象：初始化10个工作线程，异步处理所有业务逻辑
	HANDLE m_hIOCP;                         // IOCP完成端口核心句柄
	SOCKET m_sock;                          // 服务器监听套接字
	sockaddr_in m_addr;                       // 服务器地址
	std::map<SOCKET, std::shared_ptr<EdoyunClient>> m_client; // 客户端管理容器：fd映射客户端对象，智能指针自动释放

};

