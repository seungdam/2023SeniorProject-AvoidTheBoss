#pragma once
#include "IocpCore.h"
#include "RoomManager.h"

#include <memory>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <unordered_map>

class ServerSession;

class ServerIocpCore : public IocpCore
{
public:
	ServerIocpCore();
	~ServerIocpCore();
	bool Processing(uint32_t limit_time = INFINITE) override;
	bool AddSession(int32 sid, std::shared_ptr<ServerSession> session);
	std::shared_ptr<ServerSession> FindSession(int32 sid) const;
	void RequestRemoveSession(int32 sid);
	void BroadCastingAll(void* packet);

public:
	RoomManager* _rmgr;

private:
	void DrainRetiredSessions();

	mutable std::shared_mutex _lock;
	std::unordered_map<int32, std::shared_ptr<ServerSession>> _clients;
	std::unordered_map<int32, std::shared_ptr<ServerSession>> _retiredSessions;
	std::set<int32> _cList;
};


extern ServerIocpCore ServerIocpCore;
