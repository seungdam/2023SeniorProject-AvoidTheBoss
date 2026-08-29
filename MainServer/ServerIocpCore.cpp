#include "pch.h"
#include "ServerIocpCore.h"
#include "ServerSession.h"
#include "CollisionDetector.h"

ServerIocpCore ServerIocpCore;

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


void ServerIocpCore::RemoveSession(int32 sid)
{
	std::cout << "[" << _clients[sid]->GetSid() << "] Disconnected" << std::endl;
	if(sid >= 0 && _clients[sid]->_myRm != -1) _rmgr->ExitRoom(sid, _clients[sid]->_myRm);

	{
		std::unique_lock<std::shared_mutex> lock(_lock);
		_cList.erase(_clients[sid]->GetSid());
		_clients.erase(sid);
		std::cout << "Dead Lock Checking\n";
	}
}

void ServerIocpCore::BroadCastingAll(void* packet)
{
	for (auto& i : _clients)
	{
		i.second->DoSend(packet);
	}
}
