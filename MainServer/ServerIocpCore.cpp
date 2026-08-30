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
	DrainRemovedSessions();
	return processed;
}

bool ServerIocpCore::AddSession(const int32 sid, std::shared_ptr<ServerSession> session)
{
	if (!session) return false;

	std::unique_lock lock(_lock);
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

	session->Disconnect();
	std::unique_lock lock(_lock);
	_removeRequests.insert(sid);
}

void ServerIocpCore::DrainRemovedSessions()
{
	std::vector<std::pair<int32, std::shared_ptr<ServerSession>>> ready;
	{
		std::unique_lock lock(_lock);
		for (auto it = _removeRequests.begin(); it != _removeRequests.end();)
		{
			const auto sessionIt = _clients.find(*it);
			if (sessionIt == _clients.end())
			{
				it = _removeRequests.erase(it);
				continue;
			}
			if (sessionIt->second->PendingIO() != 0)
			{
				++it;
				continue;
			}
			ready.emplace_back(*it, sessionIt->second);
			it = _removeRequests.erase(it);
		}
	}

	for (const auto& [sid, session] : ready)
	{
		const auto roomNum = session->_myRoomNumber.load();
		if (_rmgr->IsValidRoom(roomNum))
		{
			_rmgr->EnqueueCommand({ RoomCommandType::Disconnected, sid, roomNum, false,
				session->GetResumeToken() });
		}
		std::unique_lock lock(_lock);
		const auto it = _clients.find(sid);
		if (it == _clients.end() || it->second != session || session->PendingIO() != 0) continue;
		std::cout << "[" << sid << "] Disconnected" << std::endl;
		_cList.erase(sid);
		_clients.erase(it);
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
