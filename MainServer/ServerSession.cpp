#include "pch.h"
#include "SocketUtil.h"
#include "ServerSession.h"
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

ServerSession::ServerSession(ServerSessionRoutes routes)
	: _routes(std::move(routes))
{
	ASSERT_CRASH(_routes.requestRemove && _routes.enqueueLobby && _routes.enqueueGame);
	_sock = SocketUtil::CreateSocket();
}

ServerSession::~ServerSession()
{
	Disconnect();
}

void ServerSession::OnIocpCompletion(IocpEvent* iocpEvent, const uint32_t bytes)
{
	[[maybe_unused]] const auto keepAlive = shared_from_this();
	BaseSession::OnIocpCompletion(iocpEvent, bytes);
}

void ServerSession::OnIocpError(IocpEvent* iocpEvent, const int32 errCode)
{
	[[maybe_unused]] const auto keepAlive = shared_from_this();
	BaseSession::OnIocpError(iocpEvent, errCode);
	RequestRemoval();
}

void ServerSession::RequestRemoval()
{
	_routes.requestRemove(GetSid());
}

void ServerSession::Processing(IocpEvent* iocpEvent, int32 numOfBytes)
{
	if (EventType::Recv == iocpEvent->_comp)
	{
		auto* rev = static_cast<RecvEvent*>(iocpEvent);
		auto cbRemainBytes = numOfBytes + _cbPrevRemainPacket;
		if (cbRemainBytes < 0 || cbRemainBytes > BUFSIZE)
		{
			RequestRemoval();
			return;
		}
		auto* p = rev->_rbuf;
		while (cbRemainBytes >= PacketHeaderSize)
		{
			const auto packetSize = static_cast<uint8>(p[0]);
			const auto expectedSize = ExpectedClientPacketSize(static_cast<uint8>(p[1]));
			if (packetSize < PacketHeaderSize || expectedSize == 0 || packetSize != expectedSize)
			{
				RequestRemoval();
				return;
			}
			if (packetSize > cbRemainBytes) break;

			if (!ProcessPacket(p)) return;
			p += packetSize;
			cbRemainBytes -= packetSize;
		}
		if (cbRemainBytes == BUFSIZE)
		{
			RequestRemoval();
			return;
		}
		_cbPrevRemainPacket = cbRemainBytes;
		if (cbRemainBytes > 0)
		{
			memmove(rev->_rbuf, p, cbRemainBytes);
		}
		if (!DoRecv()) RequestRemoval();
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

bool ServerSession::ProcessPacket(char* packet)
{
	const auto EnqueueLobby = [this](LobbyCommand command) -> bool
	{
		if (_routes.enqueueLobby(std::move(command))) return true;
		RequestRemoval();
		return false;
	};

	const auto EnqueueGamePacket = [this, packet]() -> bool
	{
		const GameBinding binding = GetGameBinding();
		if (IsSessionUnbound(binding.roomNumber)) return true;

		GameCommand command{};
		command.sid = _sid;
		command.roomNum = binding.roomNumber;
		command.lease = binding.lease;
		command.packetSize = static_cast<uint8>(packet[0]);
		if (command.packetSize > command.packet.size())
		{
			RequestRemoval();
			return false;
		}
		memcpy(command.packet.data(), packet, command.packetSize);
		if (_routes.enqueueGame(std::move(command))) return true;
		RequestRemoval();
		return false;
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
		return true;
	case (uint8)C_TITLE_PACKET_TYPE::ACQ_REG:
		{
			auto* lp  = reinterpret_cast<C2S_LOGIN*>(packet);
			std::wstring sqlExec(L"EXEC update_user_status ");
			sqlExec.append(lp->name);
			sqlExec.append(L", ");
			sqlExec.append(lp->pw);
			//RegisterProcess(this, sqlExec);
		}
		return true;
	case (uint8)C_TITLE_PACKET_TYPE::ACQ_LOGOUT:
		RequestRemoval();
		return false;
	case (uint8)C_TITLE_PACKET_TYPE::ACQ_RESUME:
	{
		const auto* resume = reinterpret_cast<C2S_RESUME*>(packet);
		if (resume->resumeToken == 0)
		{
			RequestRemoval();
			return false;
		}
		return EnqueueLobby({ LobbyCommandType::Resume, _sid, -1, false, resume->resumeToken });
	}

		// ======== 방 시스템 패킷
		case (uint8)C_ROOM_PACKET_TYPE::ACQ_ENTER_RM:
		{
			auto* rep = reinterpret_cast<C2S_ROOM_ENTER*>(packet);
			return EnqueueLobby(
				{ LobbyCommandType::Enter, _sid, rep->rmNum });
		}
		case (uint8)C_ROOM_PACKET_TYPE::ACQ_MK_RM:
		{
			return EnqueueLobby({ LobbyCommandType::Create, _sid });
		}
		case (uint8)C_ROOM_PACKET_TYPE::ACQ_READY:
		{
			return EnqueueLobby({ LobbyCommandType::SetReady, _sid, -1, true });
		}
		case (uint8)C_ROOM_PACKET_TYPE::ACQ_READY_CANCEL:
		{
			return EnqueueLobby({ LobbyCommandType::SetReady, _sid, -1, false });
		}
		case (uint8)C_ROOM_PACKET_TYPE::ACQ_EXIT_ROOM:
			if (!EnqueueLobby({ LobbyCommandType::Exit, _sid, GetRoomNumber() })) return false;
			ClearRoomBinding();
			return true;

		case (uint8)C_GAME_PACKET_TYPE::CKEY:
		case (uint8)C_GAME_PACKET_TYPE::CROT:
		case (uint8)C_GAME_PACKET_TYPE::CCHAT:
		case (uint8)C_GAME_PACKET_TYPE::CATTACK:
		case (uint8)SC_GAME_PACKET_TYPE::GAMEEVENT:
			return EnqueueGamePacket();

	}
	return true;
}

