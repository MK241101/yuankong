#include "pch.h"
#include "Command.h"

CCommand::CCommand():threadid(0){
	struct {
		int nCmd;
		CMDFUNC func;
	}data[] = {
		{1,&CCommand::MakeDriverInfo},
		{2,&CCommand::MakeDirectoryInfo},
		{3,&CCommand::RunFile},
		{4,&CCommand::DownloadFile},
		{5,&CCommand::MouseEvent},
		{6,&CCommand::SendScreen},
		{7,&CCommand::LockMachine},
		{8,&CCommand::UnlockMachine},
		{9,&CCommand::DeleteLocalFile},
		{1981,&CCommand::TestConnect},
		{-1,NULL},
	};

	for (int i = 0; data[i].nCmd != -1; i++) {
		m_mapFunction.insert(std::make_pair(data[i].nCmd, data[i].func));
	
	}
}

CCommand::~CCommand(){}

//命令调度入口，负责根据传入的命令 ID 分发到对应的处理函数
int CCommand::ExcuteCommand(int nCmd, std::list<CPacket>& lstPacket, CPacket& inPacket)
{
	std::map<int, CMDFUNC>::iterator it = m_mapFunction.find(nCmd);
	if (it == m_mapFunction.end()) {
		return -1;
	}
	return (this->*it->second)(lstPacket, inPacket);
}
