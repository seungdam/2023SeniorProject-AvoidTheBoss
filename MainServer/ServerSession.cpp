#include "pch.h"
#include "SocketUtil.h"
#include "ServerSession.h"
#include "SPlayer.h"
#include "ServerIocpCore.h"
#include "JobQueue.h"
#include "RoomCommand.h"
using namespace std;
// =========== 서버 세션 ============

void LoginProcess(ServerSession* s, std::wstring sqlexec)
{

}


void RegisterProcess(ServerSession* s, std::wstring sqlexec)
{

}

ServerSession::ServerSession()
{
	_sock = SocketUtil::CreateSocket();
}

ServerSession::~ServerSession()
{
	Disconnect();
}

void ServerSession::OnIocpError(IocpEvent* iocpEvent, const int32 errCode)
{
	BaseSession::OnIocpError(iocpEvent, errCode);
	ServerIocpCore.RequestRemoveSession(GetSid());
}

void ServerSession::Processing(IocpEvent* iocpEvent, int32 numOfBytes)
{
	if (EventType::Recv == iocpEvent->_comp)
	{
		auto* rev = static_cast<RecvEvent*>(iocpEvent);
		auto remain_data = numOfBytes + _prev_remain;
		auto* p = rev->_rbuf;
		while (remain_data > 0)
		{
			uint8 packet_size = p[0];
			if (packet_size <= remain_data)
			{
				ProcessPacket(p);
				p = p + packet_size;
				remain_data = remain_data - packet_size;
			}
			else break;
		}
		_prev_remain = remain_data;
		if (remain_data > 0)
		{
			memcpy(rev->_rbuf, p, remain_data);
		}
		DoRecv();
	}
}

bool ServerSession::DoSend(void* packet)
{
	DWORD sendLen(0);
	DWORD flag(0);
	SendEvent* sev = new SendEvent(reinterpret_cast<char*>(packet));
	sev->_sid = _sid;
	std::shared_lock socketLock(_lock);
	if (_sock == INVALID_SOCKET) { delete sev; return false; }
	BeginIO();
	if (WSASend(_sock, &sev->_sWsaBuf, 1, &sendLen, flag, static_cast<LPWSAOVERLAPPED>(sev), NULL) == SOCKET_ERROR)
	{
		int32 errcode = WSAGetLastError();
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			CompleteIO();
			delete sev;
			cout << "Send Error " << errcode << "\n";
			return false;
		}

	}
	return true;
}

bool ServerSession::DoRecv()
{
	_rev.Init();
	_rev._sid = _sid;
	_rev._cid = _cid;
	_rev._rWsaBuf.buf = _rev._rbuf + _prev_remain;
	_rev._rWsaBuf.len = BUFSIZE - _prev_remain;
	DWORD recvBytes(0);
	DWORD flag(0);
	std::shared_lock socketLock(_lock);
	if (_sock == INVALID_SOCKET) return false;
	BeginIO();

	if (WSARecv(_sock, &_rev._rWsaBuf, 1, &recvBytes, &flag, static_cast<LPWSAOVERLAPPED>(&_rev), NULL) == SOCKET_ERROR)
	{
		int32 errcode = WSAGetLastError();
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			CompleteIO();
			cout << "Recv Error " << errcode << "\n";
			return false;
		}
	}
	return true;
}

void ServerSession::DoSendLoginPacket(bool isSuccess)
{

	if (isSuccess)
	{
		S2C_LOGIN_OK loginOkPacket{};
		loginOkPacket.size = sizeof(S2C_LOGIN_OK);
		loginOkPacket.type = (uint8)S_TITLE_PACKET_TYPE::LOGIN_OK;
		loginOkPacket.cid = _cid;
		loginOkPacket.sid = _sid;
		DoSend(&loginOkPacket);
	}
	else
	{
		S2C_LOGIN_FAIL loginFailPacket{};
		loginFailPacket.size = sizeof(S2C_LOGIN_FAIL);
		loginFailPacket.type = (uint8)S_TITLE_PACKET_TYPE::LOGIN_FAIL;
		DoSend(&loginFailPacket);
	}
}

void ServerSession::ProcessPacket(char* packet)
{
	const auto getCurrentRoom = [this]() -> Room*
	{
		const int32 roomNum = _myRm.load();
		if (!ServerIocpCore._rmgr->IsValidRoom(roomNum)) return nullptr;
		Room& room = ServerIocpCore._rmgr->GetRoom(roomNum);
		return room.GetSidIndexBySid(_sid) >= 0 ? &room : nullptr;
	};

	switch ((uint8)packet[1])
	{
		case (uint8)C_TITLE_PACKET_TYPE::ACQ_LOGIN:
		{
			auto* lp = reinterpret_cast<C2S_LOGIN*>(packet);
			std::wstring sqlExec(L"EXEC search_user_db ");
			sqlExec.append(lp->name);
			sqlExec.append(L", ");
			sqlExec.append(lp->pw);
			//LoginProcess(this, sqlExec);
			DoSendLoginPacket(true);
		}
		break;
		case (uint8)C_TITLE_PACKET_TYPE::ACQ_REG:
		{
			auto* lp  = reinterpret_cast<C2S_LOGIN*>(packet);
			std::wstring sqlExec(L"EXEC update_user_status ");
			sqlExec.append(lp->name);
			sqlExec.append(L", ");
			sqlExec.append(lp->pw);
			//RegisterProcess(this, sqlExec);
		}
		break;

		// ======== 방 시스템 패킷
		case (uint8)C_ROOM_PACKET_TYPE::ACQ_ENTER_RM:
		{
			auto* rep = reinterpret_cast<C2S_ROOM_ENTER*>(packet);
			ServerIocpCore._rmgr->EnqueueCommand(
				{ RoomCommandType::Enter, _sid, rep->rmNum });
		}
		break;
		case (uint8)C_ROOM_PACKET_TYPE::ACQ_MK_RM:
		{
			ServerIocpCore._rmgr->EnqueueCommand({ RoomCommandType::Create, _sid });
		}
		break;
		case (uint8)C_ROOM_PACKET_TYPE::ACQ_READY:
		{
			ServerIocpCore._rmgr->EnqueueCommand({ RoomCommandType::SetReady, _sid, -1, true });
		}
		break;
		case (uint8)C_ROOM_PACKET_TYPE::ACQ_READY_CANCEL:
		{
			ServerIocpCore._rmgr->EnqueueCommand({ RoomCommandType::SetReady, _sid, -1, false });
		}
		break;
		case (uint8)C_ROOM_PACKET_TYPE::ACQ_EXIT_ROOM:
			ServerIocpCore._rmgr->EnqueueCommand({ RoomCommandType::Exit, _sid, _myRm.load() });
		break;

		case (uint8)C_GAME_PACKET_TYPE::CKEY:
		{
			Room* room = getCurrentRoom();
			if (!room) break;
			// 키 패킷 처리
			C2S_KEY* movePacket = reinterpret_cast<C2S_KEY*>(packet);
			moveEvent* mv = new moveEvent(_sid, movePacket->key, XMFLOAT3{ movePacket->x,0,movePacket->z });
			QueueEvent* me = static_cast<QueueEvent*>(mv);

			// 서버키 패킷 전송
			S2C_KEY packet;
			packet.size = sizeof(S2C_KEY);
			packet.type = (uint8)S_GAME_PACKET_TYPE::SKEY;
			packet.sid = _sid;
			packet.key = movePacket->key;
			packet.x = movePacket->x;
			packet.z = movePacket->z;

			room->AddEvent(me, 0.f);
			room->BroadCastingExcept(&packet, _sid);
		}
		break;
		case (uint8)C_GAME_PACKET_TYPE::CROT:
		{
			Room* room = getCurrentRoom();
			if (!room) break;
			C2S_ROTATE* rotatePacket = reinterpret_cast<C2S_ROTATE*>(packet);
			S2C_ROTATE packet;
			packet.size = sizeof(S2C_ROTATE);
			packet.type = (uint8)S_GAME_PACKET_TYPE::SROT;
			packet.sid = _sid;
			packet.angle = rotatePacket->angle;
			room->BroadCastingExcept(&packet, _sid);

		}
		break;
		case (uint8)C_GAME_PACKET_TYPE::CCHAT:
		{
			Room* room = getCurrentRoom();
			if (!room) break;

			_CHAT* cp = reinterpret_cast<_CHAT*>(packet);
			_CHAT  np;
			memcpy(&np, cp, sizeof(_CHAT));
			np.type = (uint8)S_GAME_PACKET_TYPE::SCHAT;

			room->BroadCasting(&np);
		}
		break;

		case (uint8)C_GAME_PACKET_TYPE::CATTACK:
		{
			Room* room = getCurrentRoom();
			if (!room) break;
			C2S_ATTACK* ap = reinterpret_cast<C2S_ATTACK*>(packet);
			if (ap->tidx < 0 || ap->tidx >= PLAYERNUM)
			{
				ServerIocpCore.RequestRemoveSession(_sid);
				break;
			}

			AttackEvent* ape = new AttackEvent();
			ape->_sid = _sid;
			ape->_tidx = ap->tidx;
			ape->_wf = ap->wf;
			room->AddEvent(ape, 0);
		}
			break;
		case (uint8)SC_GAME_PACKET_TYPE::GAMEEVENT:
		{
			Room* room = getCurrentRoom();
			if (!room) break;
			SC_EVENTPACKET* ep = reinterpret_cast<SC_EVENTPACKET*>(packet);
			InteractionEvent* swev = new InteractionEvent();
			swev->eventId = ep->eventId;
			std::cout << "EVENT ID : " << (int32)swev->eventId << "\n";
 			swev->_sid = _sid;
			room->AddEvent(swev, 0);
		}
		break;

	}
}

