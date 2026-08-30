#include "pch.h"
#include "SocketUtil.h"
#include "ServerSession.h"
#include "SPlayer.h"
#include "ServerIocpCore.h"
#include "JobQueue.h"
#include "RoomCommand.h"
using namespace std;
// =========== 서버 세션 ============

namespace
{
	constexpr auto PacketHeaderSize = sizeof(uint8) * 2;

	uint8 ExpectedClientPacketSize(const uint8 type)
	{
		switch (type)
		{
		case static_cast<uint8>(C_TITLE_PACKET_TYPE::ACQ_REG):
		case static_cast<uint8>(C_TITLE_PACKET_TYPE::ACQ_LOGIN):
			return static_cast<uint8>(sizeof(C2S_LOGIN));
		case static_cast<uint8>(C_TITLE_PACKET_TYPE::ACQ_LOGOUT):
			return static_cast<uint8>(sizeof(C2S_LOGOUT));
		case static_cast<uint8>(C_TITLE_PACKET_TYPE::ACQ_RESUME):
			return static_cast<uint8>(sizeof(C2S_RESUME));
		case static_cast<uint8>(C_ROOM_PACKET_TYPE::ACQ_MK_RM):
		case static_cast<uint8>(C_ROOM_PACKET_TYPE::ACQ_EXIT_ROOM):
		case static_cast<uint8>(C_ROOM_PACKET_TYPE::ACQ_READY):
		case static_cast<uint8>(C_ROOM_PACKET_TYPE::ACQ_READY_CANCEL):
			return static_cast<uint8>(sizeof(C2S_ROOM_EVENT));
		case static_cast<uint8>(C_ROOM_PACKET_TYPE::ACQ_ENTER_RM):
			return static_cast<uint8>(sizeof(C2S_ROOM_ENTER));
		case static_cast<uint8>(C_GAME_PACKET_TYPE::CKEY):
			return static_cast<uint8>(sizeof(C2S_KEY));
		case static_cast<uint8>(C_GAME_PACKET_TYPE::CROT):
			return static_cast<uint8>(sizeof(C2S_ROTATE));
		case static_cast<uint8>(C_GAME_PACKET_TYPE::CCHAT):
			return static_cast<uint8>(sizeof(_CHAT));
		case static_cast<uint8>(C_GAME_PACKET_TYPE::CATTACK):
			return static_cast<uint8>(sizeof(C2S_ATTACK));
		case static_cast<uint8>(SC_GAME_PACKET_TYPE::GAMEEVENT):
			return static_cast<uint8>(sizeof(SC_EVENTPACKET));
		default:
			return 0;
		}
	}

	uint64 GenerateResumeToken()
	{
		uint64 token = 0;
		while (token == 0)
		{
			if (BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(&token), sizeof(token),
				BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
				return 0;
		}
		return token;
	}
}

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
		auto cbRemainBytes = numOfBytes + _cbPrevRemainPacket;
		if (cbRemainBytes < 0 || cbRemainBytes > BUFSIZE)
		{
			ServerIocpCore.RequestRemoveSession(GetSid());
			return;
		}
		auto* p = rev->_rbuf;
		while (cbRemainBytes >= PacketHeaderSize)
		{
			const auto packetSize = static_cast<uint8>(p[0]);
			const auto expectedSize = ExpectedClientPacketSize(static_cast<uint8>(p[1]));
			if (packetSize < PacketHeaderSize || expectedSize == 0 || packetSize != expectedSize)
			{
				ServerIocpCore.RequestRemoveSession(GetSid());
				return;
			}
			if (packetSize > cbRemainBytes) break;

			ProcessPacket(p);
			p += packetSize;
			cbRemainBytes -= packetSize;
		}
		if (cbRemainBytes == BUFSIZE)
		{
			ServerIocpCore.RequestRemoveSession(GetSid());
			return;
		}
		_cbPrevRemainPacket = cbRemainBytes;
		if (cbRemainBytes > 0)
		{
			memmove(rev->_rbuf, p, cbRemainBytes);
		}
		if (!DoRecv()) ServerIocpCore.RequestRemoveSession(GetSid());
	}
}

bool ServerSession::DoSend(void* packet)
{
	DWORD sendLen(0);
	DWORD flag(0);
	auto* sev = new SendEvent(reinterpret_cast<char*>(packet));
	sev->_sid = _sid;
	std::shared_lock socketLock(_lock);
	if (_sock == INVALID_SOCKET) { delete sev; return false; }
	BeginIO();
	if (WSASend(_sock, &sev->_sWsaBuf, 1, &sendLen, flag, static_cast<LPWSAOVERLAPPED>(sev), NULL) == SOCKET_ERROR)
	{
		auto errCode = WSAGetLastError();
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			CompleteIO();
			delete sev;
			cout << "Send Error " << errCode << "\n";
			return false;
		}

	}
	return true;
}

bool ServerSession::DoRecv()
{
	if (_cbPrevRemainPacket < 0 || _cbPrevRemainPacket >= BUFSIZE) return false;
	_rev.Init();
	_rev._sid = _sid;
	_rev._cid = _cid;
	_rev._rWsaBuf.buf = _rev._rbuf + _cbPrevRemainPacket;
	_rev._rWsaBuf.len = BUFSIZE - _cbPrevRemainPacket;
	DWORD recvBytes(0);
	DWORD flag(0);
	std::shared_lock socketLock(_lock);
	if (_sock == INVALID_SOCKET) return false;
	BeginIO();

	if (WSARecv(_sock, &_rev._rWsaBuf, 1, &recvBytes, &flag, static_cast<LPWSAOVERLAPPED>(&_rev), NULL) == SOCKET_ERROR)
	{
		auto errCode = WSAGetLastError();
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			CompleteIO();
			cout << "Recv Error " << errCode << "\n";
			return false;
		}
	}
	return true;
}

void ServerSession::DoSendLoginPacket(bool isSuccess)
{

	if (isSuccess)
	{
		uint64 token = GetResumeToken();
		if (token == 0) token = GenerateResumeToken();
		if (token == 0)
		{
			DoSendLoginPacket(false);
			return;
		}
		SetResumeToken(token);
		S2C_LOGIN_OK loginOkPacket{};
		loginOkPacket.size = sizeof(S2C_LOGIN_OK);
		loginOkPacket.type = (uint8)S_TITLE_PACKET_TYPE::LOGIN_OK;
		loginOkPacket.cid = _cid;
		loginOkPacket.sid = _sid;
		loginOkPacket.resumeToken = token;
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
	const auto EnqueueControl = [this](RoomCommand command)
	{
		if (!ServerIocpCore._rmgr->EnqueueCommand(std::move(command)))
			ServerIocpCore.RequestRemoveSession(_sid);
	};

	const auto EnqueueGamePacket = [this, packet]()
	{
		const auto roomNum = _myRoomNumber.load();
		if (!ServerIocpCore._rmgr->IsValidRoom(roomNum)) return;

		RoomCommand command{};
		command.type = RoomCommandType::GamePacket;
		command.sid = _sid;
		command.roomNum = roomNum;
		command.packetSize = static_cast<uint8>(packet[0]);
		if (command.packetSize > command.packet.size())
		{
			ServerIocpCore.RequestRemoveSession(_sid);
			return;
		}
		memcpy(command.packet.data(), packet, command.packetSize);
		ServerIocpCore._rmgr->EnqueueCommand(std::move(command));
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
	case (uint8)C_TITLE_PACKET_TYPE::ACQ_LOGOUT:
		ServerIocpCore.RequestRemoveSession(GetSid());
		break;
	case (uint8)C_TITLE_PACKET_TYPE::ACQ_RESUME:
	{
		const auto* resume = reinterpret_cast<C2S_RESUME*>(packet);
		if (resume->resumeToken == 0)
		{
			ServerIocpCore.RequestRemoveSession(GetSid());
			break;
		}
		EnqueueControl({ RoomCommandType::Resume, _sid, -1, false, resume->resumeToken });
	}
	break;

		// ======== 방 시스템 패킷
		case (uint8)C_ROOM_PACKET_TYPE::ACQ_ENTER_RM:
		{
			auto* rep = reinterpret_cast<C2S_ROOM_ENTER*>(packet);
			EnqueueControl(
				{ RoomCommandType::Enter, _sid, rep->rmNum });
		}
		break;
		case (uint8)C_ROOM_PACKET_TYPE::ACQ_MK_RM:
		{
			EnqueueControl({ RoomCommandType::Create, _sid });
		}
		break;
		case (uint8)C_ROOM_PACKET_TYPE::ACQ_READY:
		{
			EnqueueControl({ RoomCommandType::SetReady, _sid, -1, true });
		}
		break;
		case (uint8)C_ROOM_PACKET_TYPE::ACQ_READY_CANCEL:
		{
			EnqueueControl({ RoomCommandType::SetReady, _sid, -1, false });
		}
		break;
		case (uint8)C_ROOM_PACKET_TYPE::ACQ_EXIT_ROOM:
			EnqueueControl({ RoomCommandType::Exit, _sid, _myRoomNumber.load() });
		break;

		case (uint8)C_GAME_PACKET_TYPE::CKEY:
		case (uint8)C_GAME_PACKET_TYPE::CROT:
		case (uint8)C_GAME_PACKET_TYPE::CCHAT:
		case (uint8)C_GAME_PACKET_TYPE::CATTACK:
		case (uint8)SC_GAME_PACKET_TYPE::GAMEEVENT:
			EnqueueGamePacket();
			break;

	}
}

