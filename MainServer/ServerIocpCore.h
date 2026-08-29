#pragma once
#include "IocpCore.h"
#include "RoomManager.h"

class ServerSession;

class ServerIocpCore : public IocpCore
{

public:
	ServerIocpCore();
	~ServerIocpCore();
	void RemoveSession(int32 sid);
	void BroadCastingAll(void* packet);

public:
	std::shared_mutex _lock;
	std::unordered_map<int32, ServerSession*> _clients;
	std::set<int32> _cList;
	RoomManager* _rmgr;
};


extern ServerIocpCore ServerIocpCore;
