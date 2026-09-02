#include "../Platform/pch.h"
// 네트워크 관련 헤더
#include "SocketUtil.h"
#include "ClientPacketDispatcher.h"
#include "ClientSession.h"
#include "../Diagnostics/ClientTestMode.h"
// 이벤트 처리관련 헤더
#include "IocpEvent.h"

namespace
{
	constexpr int32 PacketHeaderSize = 2;
	// ponytail: fixed client bound; add packet coalescing only when queue telemetry proves it is needed.
	constexpr std::size_t MaxPendingPackets = 4096;

	std::size_t GetExpectedServerPacketSize(const uint8 type)
	{
		switch (type)
		{
		case static_cast<uint8>(S_TITLE_PACKET_TYPE::REG_FAIL):
		case static_cast<uint8>(S_TITLE_PACKET_TYPE::REG_OK):
			return sizeof(S2C_REG);
		case static_cast<uint8>(S_TITLE_PACKET_TYPE::LOGIN_OK):
			return sizeof(S2C_LOGIN_OK);
		case static_cast<uint8>(S_TITLE_PACKET_TYPE::LOGIN_FAIL):
			return sizeof(S2C_LOGIN_FAIL);
		case static_cast<uint8>(S_TITLE_PACKET_TYPE::RESUME_OK):
			return sizeof(S2C_RESUME);
		case static_cast<uint8>(S_TITLE_PACKET_TYPE::RESUME_FAIL):
			return sizeof(S2C_RESUME_FAIL);

		case static_cast<uint8>(S_ROOM_PACKET_TYPE::MK_RM_OK):
		case static_cast<uint8>(S_ROOM_PACKET_TYPE::MK_RM_FAIL):
		case static_cast<uint8>(S_ROOM_PACKET_TYPE::REP_EXIT_RM):
			return sizeof(S2C_ROOM_EVENT);
		case static_cast<uint8>(S_ROOM_PACKET_TYPE::REP_ENTER_FAIL):
		case static_cast<uint8>(S_ROOM_PACKET_TYPE::REP_ENTER_OK):
			return sizeof(S2C_ROOM_ENTER);
		case static_cast<uint8>(S_ROOM_PACKET_TYPE::UPDATE_LIST):
			return sizeof(S2C_ROOM_LIST);
		case static_cast<uint8>(S_ROOM_PACKET_TYPE::ROOM_INFO):
			return sizeof(S2C_ROOM_INFO);
		case static_cast<uint8>(S_ROOM_PACKET_TYPE::REP_READY):
		case static_cast<uint8>(S_ROOM_PACKET_TYPE::REP_READY_CANCEL):
			return sizeof(S2C_ROOM_READY);
		case static_cast<uint8>(S_ROOM_PACKET_TYPE::GAME_START):
			return sizeof(S2C_GAMESTART);

		case static_cast<uint8>(S_GAME_PACKET_TYPE::SCHAT):
			return sizeof(_CHAT);
		case static_cast<uint8>(S_GAME_PACKET_TYPE::SKEY):
			return sizeof(S2C_KEY);
		case static_cast<uint8>(S_GAME_PACKET_TYPE::SROT):
			return sizeof(S2C_ROTATE);
		case static_cast<uint8>(S_GAME_PACKET_TYPE::SPOS):
			return sizeof(S2C_POS);
		case static_cast<uint8>(S_GAME_PACKET_TYPE::ANIM):
			return sizeof(S2C_ANIMPACKET);
		case static_cast<uint8>(S_GAME_PACKET_TYPE::FRAME):
			return sizeof(S2C_FRAMEPACKET);
		case static_cast<uint8>(SC_GAME_PACKET_TYPE::GAMEEVENT):
			return sizeof(SC_EVENTPACKET);
		default:
			return 0;
		}
	}
}




ClientSession::ClientSession()
{
}

ClientSession::~ClientSession()
{
	Disconnect();
}

void ClientSession::SetIdentity(const int32 cid, const int32 sid)
{
	std::unique_lock identityLock(_lock);
	_cid = cid;
	_sid = sid;
}

void ClientSession::SetSid(const int32 sid)
{
	std::unique_lock identityLock(_lock);
	_sid = sid;
}

int32 ClientSession::GetSid()
{
	std::shared_lock identityLock(_lock);
	return _sid;
}

std::pair<int32, int32> ClientSession::GetIdentity()
{
	std::shared_lock identityLock(_lock);
	return { _cid, _sid };
}

void ClientSession::ResetForReconnect(const SOCKET socket)
{
	{
		std::unique_lock identityLock(_lock);
		_resumeSid.store(_sid, std::memory_order_release);
		_sock = socket;
		_sid = -1;
		_prevRemainBytes = 0;
		_stopping.store(false, std::memory_order_release);
	}
	std::lock_guard packetLock(_packetMutex);
	_pendingPackets.clear();
}

bool ClientSession::QueuePacket(const char* packet, const std::size_t packetSize)
{
	std::lock_guard packetLock(_packetMutex);
	if (_pendingPackets.size() >= MaxPendingPackets)
	{
		return false;
	}
	_pendingPackets.emplace_back(packet, packet + packetSize);
	return true;
}

void ClientSession::DispatchPackets(atb::ClientPacketDispatcher& dispatcher)
{
	std::deque<std::vector<char>> packets;
	{
		std::lock_guard packetLock(_packetMutex);
		packets.swap(_pendingPackets);
	}

	for (auto &packet : packets)
	{
		dispatcher.Apply(*this, packet.data());
	}
}

void ClientSession::Stop()
{
	RequestStop();
	Disconnect();
}

void ClientSession::Processing(IocpEvent* iocpEvent, int32 numOfBytes)
{
	//TODO
	switch (iocpEvent->_comp)
	{
	case EventType::Connect:
	{
		ConnectEvent* connectEvent = static_cast<ConnectEvent*>(iocpEvent);
		delete connectEvent;
		SetSid(0);
		if (!IsStopping())
		{
			if (DoRecv())
			{
				if (HasResumeToken())
				{
					C2S_RESUME resume{};
					resume.size = sizeof(resume);
					resume.type = static_cast<uint8>(C_TITLE_PACKET_TYPE::ACQ_RESUME);
					resume.resumeToken = GetResumeToken();
					if (!DoSend(&resume))
					{
						g_clientTestMode.OnProtocolFailure("failed to send resume request");
					}
				}
				else
				{
					g_clientTestMode.OnConnected(*this); // Connect하고 Do recv 수행
				}
			}
			else
			{
				g_clientTestMode.OnProtocolFailure("failed to start the first receive");
			}
		}
	}
	break;
	case EventType::Recv:
	{
		if (IsStopping())
		{
			break;
		}
		RecvEvent* rev = static_cast<RecvEvent*>(iocpEvent);
		if (numOfBytes < 0 || _prevRemainBytes < 0 || _prevRemainBytes > BUFSIZE || numOfBytes > BUFSIZE - _prevRemainBytes)
		{
			std::cerr << "Invalid receive buffer state\n";
			_prevRemainBytes = 0;
			RequestStop();
			return;
		}

		auto remainBytes = numOfBytes + _prevRemainBytes;
		char* p = rev->_rbuf;
		while (remainBytes >= PacketHeaderSize)
		{
			const auto cbPacketSize = static_cast<uint8>(p[0]);
			const auto packetType = static_cast<uint8>(p[1]);

			if (cbPacketSize < PacketHeaderSize)
			{
				std::cerr << "Rejected packet: invalid size " << static_cast<int32>(cbPacketSize) << "\n";
				_prevRemainBytes = 0;
				RequestStop();
				return;
			}

			if (cbPacketSize > remainBytes)
			{
				break;
			}

			const std::size_t expected_size = GetExpectedServerPacketSize(packetType);
			if (expected_size == 0)
			{
				std::cerr << "Ignored unknown packet type " << static_cast<int32>(packetType) << "\n";
			}
			else if (cbPacketSize != expected_size)
			{
				std::cerr << "Rejected packet type " << static_cast<int32>(packetType)
					<< ": expected " << expected_size
					<< " bytes, received " << static_cast<int32>(cbPacketSize) << "\n";
				_prevRemainBytes = 0;
				RequestStop();
				return;
			}
			else
			{
				if (packetType == static_cast<uint8>(S_TITLE_PACKET_TYPE::LOGIN_OK))
				{
					const auto* login = reinterpret_cast<const S2C_LOGIN_OK*>(p);
					SetIdentity(login->cid, login->sid);
					_resumeToken.store(login->resumeToken, std::memory_order_release);
					_resumeSid.store(-1, std::memory_order_release);
				}

				if (!QueuePacket(p, cbPacketSize))
				{
					::OutputDebugStringA("[Network] Pending packet queue overflow\n");
					std::cerr << "Pending packet queue overflow\n";
					_prevRemainBytes = 0;
					RequestStop();
					return;
				}
			}

			p += cbPacketSize;
			remainBytes -= cbPacketSize;
		}
		_prevRemainBytes = remainBytes;
		if (remainBytes > 0)
		{
			memmove(rev->_rbuf, p, remainBytes);
		}
		if (!IsStopping())
		{
			DoRecv();
		}
	}
	break;
	case EventType::Send:
	{
		if (iocpEvent == nullptr) ASSERT_CRASH("double Del");
		delete static_cast<SendEvent*>(iocpEvent);
	}
	break;
	}
}

bool ClientSession::DoSend(void* packet)
{
	if (IsStopping())
	{
		return false;
	}
	DWORD sendLen(0);
	DWORD flag(0);
	SendEvent* sev = new SendEvent(reinterpret_cast<char*>(packet));
	sev->_sid = GetSid();
	std::shared_lock socketLock(_lock);
	if (_sock == INVALID_SOCKET)
	{
		delete sev;
		return false;
	}
	BeginIO();
	if (WSASend(_sock, &sev->_sWsaBuf, 1, &sendLen, flag, static_cast<LPWSAOVERLAPPED>(sev), NULL) == SOCKET_ERROR)
	{
		int32 errcode = WSAGetLastError();
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			std::cout << errcode << std::endl;
			CompleteIO();
			delete sev;
			return false;
		}
	}
	return true;
}



bool ClientSession::DoRecv()
{
	if (IsStopping())
	{
		return false;
	}
	_rev.Init();
	const auto [cid, sid] = GetIdentity();
	_rev._sid = sid;
	_rev._cid = cid;
	DWORD recvBytes(0);
	DWORD flag(0);
	_rev._rWsaBuf.buf = _rev._rbuf + _prevRemainBytes;
	_rev._rWsaBuf.len = BUFSIZE - _prevRemainBytes;
	std::shared_lock socketLock(_lock);
	if (_sock == INVALID_SOCKET)
	{
		return false;
	}
	BeginIO();
	if (WSARecv(_sock, &_rev._rWsaBuf, 1, &recvBytes, &flag, static_cast<LPWSAOVERLAPPED>(&_rev), NULL) == SOCKET_ERROR)
	{
		int32 errcode = WSAGetLastError();
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			std::cout << errcode << std::endl;
			CompleteIO();
			return false;
		}
	}
	return true;
}
