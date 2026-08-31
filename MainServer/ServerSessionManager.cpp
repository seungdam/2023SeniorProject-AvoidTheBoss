#include "pch.h"
#include "ServerSessionManager.h"
#include "ServerSession.h"

#include <vector>

ServerSessionManager::ServerSessionManager(LobbyCommandQueue& lobbyCommands,
	GameCommandQueue& gameCommands, std::pmr::memory_resource* poolUpstream)
	: _sessionPool(poolUpstream), _lobbyCommands(lobbyCommands), _gameCommands(gameCommands)
{
}

std::shared_ptr<ServerSession> ServerSessionManager::AcquireSession()
{
	ServerSessionRoutes routes{
		[this](const int32 sid) { RequestRemoveSession(sid); },
		[this](LobbyCommand command)
		{
			return _lobbyCommands.TryEnqueue(std::move(command));
		},
		[this](GameCommand command)
		{
			return _gameCommands.TryEnqueue(std::move(command));
		}
	};
	std::pmr::polymorphic_allocator<ServerSession> allocator{ &_sessionPool };
	return std::allocate_shared<ServerSession>(allocator, std::move(routes));
}

std::optional<int32> ServerSessionManager::ActivateSession(
	const std::shared_ptr<ServerSession>& session)
{
	if (!session) return std::nullopt;

	const int32 sid = _nextSessionId.fetch_add(1, std::memory_order_relaxed);
	session->SetIdentity(sid, sid);

	{
		std::unique_lock lock(_lock);
		if (!_accepting || _retiredSessions.contains(sid)) return std::nullopt;

		const auto [active, inserted] = _activeSessions.emplace(sid, session);
		if (!inserted) return std::nullopt;

		if (_lobbyCommands.TryEnqueue({ LobbyCommandType::SessionConnected, sid })) return sid;
		_activeSessions.erase(active);
	}

	// Admission failed before the first receive was posted; reject the socket
	// without publishing a Disconnected command for a session the room domain never observed.
	session->Disconnect();
	return std::nullopt;
}

std::shared_ptr<ServerSession> ServerSessionManager::FindSession(const int32 sid) const
{
	std::shared_lock lock(_lock);
	const auto it = _activeSessions.find(sid);
	return it == _activeSessions.end() ? nullptr : it->second;
}

void ServerSessionManager::RequestRemoveSession(const int32 sid)
{
	const auto session = FindSession(sid);
	if (!session) return;

	session->RetireGameBinding();
	session->Disconnect();
	{
		std::unique_lock lock(_lock);
		const auto it = _activeSessions.find(sid);
		if (it == _activeSessions.end() || it->second != session) return;
		if (!_retiredSessions.emplace(sid, session).second) return;
		_activeSessions.erase(it);
	}

	LobbyCommand command{ LobbyCommandType::Disconnected, sid, -1, false, session->GetResumeToken() };
	const bool queued = _lobbyCommands.TryEnqueue(std::move(command));
	ASSERT_CRASH(queued); // Disconnected is the queue's must-admit lifecycle command.
	std::cout << "[" << sid << "] Disconnected" << std::endl;
	DrainRetiredSessions();
}

void ServerSessionManager::BeginShutdown()
{
	std::vector<int32> activeSessionIds;
	{
		std::unique_lock lock(_lock);
		if (!_accepting) return;
		_accepting = false;
		activeSessionIds.reserve(_activeSessions.size());
		for (const auto& [sid, session] : _activeSessions) activeSessionIds.push_back(sid);
	}

	for (const int32 sid : activeSessionIds) RequestRemoveSession(sid);
}

void ServerSessionManager::DrainRetiredSessions()
{
	std::vector<std::shared_ptr<ServerSession>> released;
	{
		std::unique_lock lock(_lock);
		for (auto it = _retiredSessions.begin(); it != _retiredSessions.end();)
		{
			if (it->second->PendingIO() != 0)
			{
				++it;
				continue;
			}
			released.push_back(std::move(it->second));
			it = _retiredSessions.erase(it);
		}
	}
}

bool ServerSessionManager::IsDrained() const
{
	std::shared_lock lock(_lock);
	return _activeSessions.empty() && _retiredSessions.empty();
}

void ServerSessionManager::BroadcastAll(void* packet) const
{
	std::vector<std::shared_ptr<ServerSession>> sessions;
	{
		std::shared_lock lock(_lock);
		sessions.reserve(_activeSessions.size());
		for (const auto& [sid, session] : _activeSessions) sessions.push_back(session);
	}
	for (const auto& session : sessions) session->DoSend(packet);
}
