#include "pch.h"
#include "ServerIocpCore.h"
#include "ServerSession.h"
#include "CollisionDetector.h"

#include <vector>

class ServerIocpCore ServerIocpCore;

ServerIocpCore::ServerIocpCore()
{
	_rmgr = new RoomManager();
	_rmgr->Init();
	BoxTree = new OcTree(XMFLOAT3(0, 0, 0), 60);
	BoxTree->BuildTree();
	BoxTree->ReadBoundingBoxInfoFromFile("bounding_boxes2.txt");
}

ServerIocpCore::~ServerIocpCore()
{
	delete _rmgr;
}

bool ServerIocpCore::Processing(const uint32_t limit_time)
{
	const bool processed = IocpCore::Processing(limit_time);
	DrainRetiredSessions();
	return processed;
}

bool ServerIocpCore::AddSession(const int32 sid, std::shared_ptr<ServerSession> session)
{
	if (!session) return false;

	std::unique_lock lock(_lock);
	if (_retiredSessions.contains(sid)) return false;
	if (!_clients.emplace(sid, std::move(session)).second) return false;
	_cList.insert(sid);
	return true;
}

std::shared_ptr<ServerSession> ServerIocpCore::FindSession(const int32 sid) const
{
	std::shared_lock lock(_lock);
	const auto it = _clients.find(sid);
	return it == _clients.end() ? nullptr : it->second;
}

void ServerIocpCore::RequestRemoveSession(const int32 sid)
{
	const auto session = FindSession(sid);
	if (!session) return;

	session->RetireGameBinding();
	session->Disconnect();
	{
		std::unique_lock lock(_lock);
		const auto it = _clients.find(sid);
		if (it == _clients.end() || it->second != session) return;
		if (!_retiredSessions.emplace(sid, session).second) return;
		_cList.erase(sid);
		_clients.erase(it);
	}

	LobbyCommand command{ LobbyCommandType::Disconnected, sid, -1, false, session->GetResumeToken() };
	const bool queued = _rmgr->EnqueueLobbyCommand(std::move(command));
	ASSERT_CRASH(queued); // Disconnected is the queue's must-admit lifecycle command.
	std::cout << "[" << sid << "] Disconnected" << std::endl;
	DrainRetiredSessions();
}

void ServerIocpCore::DrainRetiredSessions()
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

void ServerIocpCore::BroadCastingAll(void* packet)
{
	std::vector<std::shared_ptr<ServerSession>> sessions;
	{
		std::shared_lock lock(_lock);
		for (const auto& [sid, session] : _clients) sessions.push_back(session);
	}
	for (const auto& session : sessions)
	{
		session->DoSend(packet);
	}
}
