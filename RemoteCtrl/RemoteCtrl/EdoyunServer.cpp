#include "pch.h"
#include "EdoyunServer.h"

template<EdoyunOperator op>
AcceptOverlapped<op>::AcceptOverlapped() {

	// 1. 绑定线程池执行函数：AcceptWorker
	m_worker=ThreadWorker(this, (FUNCTYPE)& AcceptOverlapped<op>::AcceptWorker);

	m_operator = EAccept;      // 2. 标记操作类型 = 客户端连接
	memset(&m_overlapped, 0, sizeof(m_overlapped));      // 3. 清空重叠结构体
	m_buffer.resize(1024);    // 4. 创建1024字节缓冲区

	m_server = NULL;    // 5. 服务器指针置空
}

template<EdoyunOperator op>
int AcceptOverlapped<op>::AcceptWorker() {             // Accept事件的业务处理函数：线程池异步执行
	INT lLength = 0, rLength = 0;
	if (m_client->GetBufferSize() > 0) {
		sockaddr* plocal=NULL,*promote=NULL;

		// 1、获取客户端 IP + 端口
		GetAcceptExSockaddrs(*m_client, 0, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,
			(sockaddr**)&plocal, &lLength,     //本地地址
			(sockaddr**)&promote, &rLength);   //远程地址
		
		// 2、把地址保存到客户端对象
		memcpy(m_client->GetLocalAddr(), plocal, sizeof(sockaddr_in));
		memcpy(m_client->GetRemoteAddr(), promote, sizeof(sockaddr_in));
		
		// 3、把新客户端 Socket 绑定到 IOCP
		m_server->BindNewSocket(*m_client);

		// 4、投递异步接收（关键！）
		int ret=WSARecv((SOCKET)*m_client,m_client->RecvWSABuffer(), 1, *m_client, &m_client->flags(),m_client->RecvOverlapped(), NULL);
		if (ret == SOCKET_ERROR&&(WSAGetLastError() != WSA_IO_PENDING)) {
			TRACE("ret=%d error=%d\n", ret, WSAGetLastError());

		}
		// 5、继续监听下一个客户端连接
		if (!m_server->NewAccept()) {
			return -2;
		}
	}
	return -1;
}


template<EdoyunOperator op>
inline SendOverlapped<op>::SendOverlapped() {
	m_operator = op;
	m_worker = ThreadWorker(this, (FUNCTYPE)&SendOverlapped<op>::SendWorker);
	memset(&m_overlapped, 0, sizeof(m_overlapped));
	m_buffer.resize(1024 * 256);

}



template<EdoyunOperator op>
inline RecvOverlapped<op>::RecvOverlapped() {     // 专门处理【客户端发送数据过来】的异步任务类
	m_operator = op;   // 1. 设置操作类型 = 接收
	m_worker = ThreadWorker(this, &RecvOverlapped<op>::RecvWorker); // 2. 绑定线程池要执行的函数：RecvWorker
	memset(&m_overlapped, 0, sizeof(m_overlapped));    // 3. 清空重叠IO结构体（必须清零）
	m_buffer.resize(1024 * 256);    // 4. 创建一个 256KB 的大缓冲区，存放客户端发来的数据

}


/*
EdoyunClient 构造函数一共做了 6 件大事：
创建一个支持 IOCP 的客户端 Socket
创建 3 个重叠 IO 对象（连接、接收、发送）
创建线程安全的发送队列，并绑定发送函数
分配接收数据缓冲区（1024 字节）
初始化各种状态为默认值（不忙、标记 0）
清空地址结构
*/
EdoyunClient::EdoyunClient():
	m_isbusy(false),        // 1. 标记客户端不忙
	m_flags(0),             // 2. 重叠IO标记清零
	m_overlapped(new ACCEPTOVERLAPPED()),  // 3. 创建【连接】重叠对象
	m_recv(new RECVOVERLAPPED()),          // 4. 创建【接收】重叠对象
	m_send(new SENDOVERLAPPED()),          // 5. 创建【发送】重叠对象
	m_vecSend(this, (SENDCALLBACK)&EdoyunClient::SendData)  // 6. 初始化发送队列
{
	m_sock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);   // 1. 创建支持重叠IO的Socket
	m_buffer.resize(1024);                     // 2. 创建1024字节的接收缓冲区
	memset(&m_laddr, 0, sizeof(m_laddr));      // 3. 清空本地地址
	memset(&m_raddr, 0, sizeof(m_raddr));      // 4. 清空远端客户端地址
}

void EdoyunClient::SetOverlapped(PCLIENT& ptr) {
	m_overlapped->m_client = ptr.get();
	m_recv->m_client = ptr.get();
	m_send->m_client = ptr.get();
}

EdoyunClient::operator LPOVERLAPPED() { return &m_overlapped->m_overlapped; }

LPWSABUF EdoyunClient::RecvWSABuffer(){ return &m_recv->m_wsabuffer; }

LPWSABUF EdoyunClient::SendWSABuffer(){ return &m_send->m_wsabuffer; }

LPWSAOVERLAPPED EdoyunClient::RecvOverlapped() { return &m_recv->m_overlapped; }

LPWSAOVERLAPPED EdoyunClient::SendOverlapped() { return &m_send->m_overlapped; }

/*
这是传统同步阻塞 recv，调用它 → 线程卡住等待数据，数据来了 → 才返回
高并发下卡死服务器  它不适合 IOCP 高并发模型！
*/
int EdoyunClient::Recv()   //同步数据接收函数（实际业务中会被异步逻辑替代）
{              
	int ret = recv(m_sock, m_buffer.data() + m_used, m_buffer.size() - m_used, 0);
	if (ret <= 0)return -1;
	m_used += (size_t)ret;

	//CEdoyunTool::Dump((BYTE*)m_buffer.data(), ret);

	return 0;
}

// 给外部调用的【发送接口】，只管把数据塞进队列，不直接发送！
int EdoyunClient::Send(void* buffer, size_t nSize)
{
	std::vector<char> data(nSize);   // 1. 把外部传来的裸数据，包装成安全的vector
	memcpy(data.data(), buffer, nSize);
	
	if (m_vecSend.PushBack(data)) {   // 2. 把数据塞进【发送队列 m_vecSend】
		return 0;
	}

	return -1;
}

//内部真正执行【异步发送】，调用 WSASend 投递到 IOCP！
int EdoyunClient::SendData(std::vector<char>& data)
{
	if (m_vecSend.Size() > 0) {
		// 如果队列里还有数据，就发送给重叠IO
		int ret=WSASend(m_sock, SendWSABuffer(), 1, &m_received, m_flags, &m_send->m_overlapped, NULL);
		if (ret != 0 && (WSAGetLastError() != WSA_IO_PENDING)) {
			CEdoyunTool::ShowError();
			return -1;
		}
	}
	return 0;
}

// 关闭监听 Socket、释放所有客户端、清空客户端列表、关闭 IOCP 端口、停止线程池、清理网络库
EdoyunServer::~EdoyunServer()
{
	closesocket(m_sock);
	std::map<SOCKET, PCLIENT>::iterator it = m_client.begin();
	for (; it != m_client.end(); it++) {
		it->second.reset();
	
	}
	m_client.clear();
	CloseHandle(m_hIOCP);
	m_pool.Stop();
	WSACleanup();
}

// 启动服务器：初始化网络、绑定端口、监听、创建IOCP、开始接受连接
bool EdoyunServer::StartService() {
	
	// 1. 创建Socket，初始化Winsock，设置端口复用
	CreateSocket();    

	// 2. 绑定Socket到 IP:端口
	if (bind(m_sock, (sockaddr*)&m_addr, sizeof(m_addr)) == -1) {   
		closesocket(m_sock);
		m_sock = INVALID_SOCKET;
		return false;
	}

	// 3. 开始监听客户端连接
	if (listen(m_sock, 3) == -1) {    
		closesocket(m_sock);
		m_sock = INVALID_SOCKET;
		return false;
	}

	// 4. 创建IOCP完成端口（核心！Windows异步网络引擎）
	m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 4);  
	if (m_hIOCP == NULL) {
		closesocket(m_sock);
		m_sock = INVALID_SOCKET;
		m_hIOCP = INVALID_HANDLE_VALUE;
		return false;
	}

	// 5. 将【监听Socket】绑定到IOCP，以后AcceptEx的连接完成事件会发给这个IOCP
	CreateIoCompletionPort((HANDLE)m_sock, m_hIOCP, (ULONG_PTR)this, 0);   

	// 6. 启动线程池
	m_pool.Invoke();   

	// 7. 在线程池中运行【IOCP主线程】，循环处理事件
	m_pool.DispatchWorker(ThreadWorker(this, (FUNCTYPE)&EdoyunServer::threadIocp));  

	// 8. 投递第一个AcceptEx，开始等待客户端连接
	if (!NewAccept())return false;
	return true;
}

//投递一个异步Accept请求，等待客户端连接 ，每接受一个客户端，必须重新调用一次
bool EdoyunServer::NewAccept()
{
	// 1. 创建一个新的客户端对象（预分配，给即将连入的客户端用）
	PCLIENT pClient(new EdoyunClient());

	// 2. 让重叠对象持有这个客户端的指针（关键关联）
	pClient->SetOverlapped(pClient);

	// 3. 把客户端加入服务器的客户端管理map
	/*
	   首先pClient是EdoyunClient的一个智能指针，*pClient是对智能指针进行解引用，得到EdoyunClient对象，
	   由于operator SOCKET() { return m_sock; }，对象会自动转换成m_sock
	*/
	m_client.insert(std::pair<SOCKET, PCLIENT>(*pClient, pClient));

	// 4. 投递AcceptEx，异步等待客户端连接
	if (!AcceptEx(
		m_sock,         // 监听Socket
		*pClient,       // 新客户端的Socket   1 → 转成 SOCKET
		*pClient,       // 数据缓冲区首地址   2 → 转成 PVOID（缓冲区地址）
		0,              // 接受连接时，不预接收数据
		sizeof(sockaddr_in) + 16,	// 本地地址长度
		sizeof(sockaddr_in) + 16,	// 远程地址长度
		*pClient,				// 接收字节数的指针     3 → 转成 LPDWORD（字节数指针）
		*pClient))				// 叠结构体指针         4 → 转成 LPOVERLAPPED（重叠指针）
	{
		if (WSAGetLastError() != WSA_IO_PENDING) {
			closesocket(m_sock);
			m_sock = INVALID_SOCKET;
			m_hIOCP = INVALID_HANDLE_VALUE;
			return false;

		}
		
	}
	return true;
}

void EdoyunServer::BindNewSocket(SOCKET s)
{
	CreateIoCompletionPort((HANDLE)s, m_hIOCP, (ULONG_PTR)this, 0);   //将新客户端的Socket绑定到IOCP
}

void EdoyunServer::CreateSocket()
{
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);
	m_sock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);  //创建【重叠IO模式】的监听套接字
	int opt = 1; 

	//设置端口复用：让服务器关闭后可以立刻重启，不会提示 “端口被占用”
	setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));  
}

int EdoyunServer::threadIocp(){
	DWORD transferred = 0;                    // 本次IO实际传输的字节数：recv/send的字节数
	ULONG_PTR CompletionKey = 0;              // 完成键：绑定套接字时传入的参数，此处是EdoyunServer*
	OVERLAPPED* lpOverlapped = NULL;          // 系统返回的重叠IO指针

	// 1、阻塞等待IOCP中的完成事件
	if (GetQueuedCompletionStatus(m_hIOCP, &transferred, &CompletionKey, &lpOverlapped, INFINITE)) {  
		
		if (CompletionKey != 0) {  // 有效事件
			EdoyunOverlapped* pOverlapped = CONTAINING_RECORD(
				lpOverlapped, 
				EdoyunOverlapped, 
				m_overlapped);    //2、通过【结构体的成员指针】反推出【结构体对象的首地址】

			pOverlapped->m_server = this;  // 让重叠对象知道属于哪个服务器

			// 3、根据IO操作类型，分发到对应的业务处理逻辑
			switch (pOverlapped->m_operator) {           
			case EAccept:  //客户端连接完成
			{
				ACCEPTOVERLAPPED* pOver = (ACCEPTOVERLAPPED*)pOverlapped;
				m_pool.DispatchWorker(pOver->m_worker);  // 扔给线程池处理
			}break;

			case ERecv:   // 数据接收完成
			{
				RECVOVERLAPPED* pOver = (RECVOVERLAPPED*)pOverlapped;
				m_pool.DispatchWorker(pOver->m_worker);  // 扔给线程池处理
			}break;
			 
			case ESend:   // 数据发送完成
			{
				SENDOVERLAPPED* pOver = (SENDOVERLAPPED*)pOverlapped;
				m_pool.DispatchWorker(pOver->m_worker);   // 扔给线程池处理
			}break;

			case EError:   // 网络错误
			{
				ERROROVERLAPPED* pOver = (ERROROVERLAPPED*)pOverlapped;
				m_pool.DispatchWorker(pOver->m_worker);   // 扔给线程池处理
			}break; 

			}
		}
		else {
			return -1;
		}
	}

	return 0;


}
