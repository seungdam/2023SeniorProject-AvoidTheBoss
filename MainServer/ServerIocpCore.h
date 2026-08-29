#pragma once
#include "IocpCore.h"
#include "RoomManager.h"

#include <memory>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>

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
	void DrainRemovedSessions();

	mutable std::shared_mutex _lock;
	std::unordered_map<int32, std::shared_ptr<ServerSession>> _clients;
	std::set<int32> _cList;
	std::unordered_set<int32> _removeRequests;
};


extern ServerIocpCore ServerIocpCore;
