#pragma once

#include "RoomCommand.h"

#include <atomic>
#include <memory>
#include <memory_resource>
#include <optional>
#include <shared_mutex>
#include <unordered_map>

class ServerSession;

class ServerSessionManager final
{
public:
	ServerSessionManager(LobbyCommandQueue& lobbyCommands, GameCommandQueue& gameCommands,
		std::pmr::memory_resource* poolUpstream = std::pmr::get_default_resource());

	std::shared_ptr<ServerSession> AcquireSession();
	std::optional<int32> ActivateSession(const std::shared_ptr<ServerSession>& session);
	std::shared_ptr<ServerSession> FindSession(int32 sid) const;
	void RequestRemoveSession(int32 sid);
	void BeginShutdown();
	void DrainRetiredSessions();
	bool IsDrained() const;
	void BroadcastAll(void* packet) const;

private:
	// ponytail: this process-lifetime pool retains peak session allocation;
	// add a capped resource only if connection-churn measurements justify it.
	std::pmr::synchronized_pool_resource _sessionPool;
	LobbyCommandQueue& _lobbyCommands;
	GameCommandQueue& _gameCommands;
	std::atomic<int32> _nextSessionId = 0;
	mutable std::shared_mutex _lock;
	bool _accepting = true;
	std::unordered_map<int32, std::shared_ptr<ServerSession>> _activeSessions;
	std::unordered_map<int32, std::shared_ptr<ServerSession>> _retiredSessions;
};
