#include "pch.h"
// 네트워크 관련 헤더
#include "SocketUtil.h"
#include "ClientSession.h"
// 프레임 워크 헤더
#include "GameFramework.h"
#include "ClientTestMode.h"
#include "SceneManager.h"
#include "UIManager.h"
// 이벤트 처리관련 헤더
#include "IocpEvent.h"
#include "ClientPacketEvent.h"
#include "CJobQueue.h"
// 객체 관련 헤더
#include "CBullet.h"
#include "CBoss.h"
#include "CEmployee.h"
// 씬관련 헤더
#include "OtherScenes.h"
#include "GameScene.h"

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

bool ClientSession::QueuePacket(const char* packet, const std::size_t packetSize)
{
	std::lock_guard packetLock(_packetMutex);
	if (_pendingPackets.size() >= MaxPendingPackets) return false;
	_pendingPackets.emplace_back(packet, packet + packetSize);
	return true;
}

void ClientSession::DispatchPackets()
{
	std::deque<std::vector<char>> packets;
	{
		std::lock_guard packetLock(_packetMutex);
		packets.swap(_pendingPackets);
	}

	for (auto& packet : packets) ApplyPacket(packet.data());
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
			if (DoRecv()) g_clientTestMode.OnConnected(*this); // Connect하고 Do recv 수행
			else g_clientTestMode.OnProtocolFailure("failed to start the first receive");
		}
	}
	break;
	case EventType::Recv:
	{
		if (IsStopping()) break;
		RecvEvent* rev = static_cast<RecvEvent*>(iocpEvent);
		if (numOfBytes < 0 || _prev_remain < 0 || _prev_remain > BUFSIZE || numOfBytes > BUFSIZE - _prev_remain)
		{
			std::cerr << "Invalid receive buffer state\n";
			_prev_remain = 0;
			RequestStop();
			return;
		}

		int remain_data = numOfBytes + _prev_remain;
		char* p = rev->_rbuf;
		while (remain_data >= PacketHeaderSize)
		{
			const uint8 packet_size = static_cast<uint8>(p[0]);
			const uint8 packet_type = static_cast<uint8>(p[1]);

			if (packet_size < PacketHeaderSize)
			{
				std::cerr << "Rejected packet: invalid size " << static_cast<int32>(packet_size) << "\n";
				_prev_remain = 0;
				RequestStop();
				return;
			}

			if (packet_size > remain_data) break;

			const std::size_t expected_size = GetExpectedServerPacketSize(packet_type);
			if (expected_size == 0)
			{
				std::cerr << "Ignored unknown packet type " << static_cast<int32>(packet_type) << "\n";
			}
			else if (packet_size != expected_size)
			{
				std::cerr << "Rejected packet type " << static_cast<int32>(packet_type)
					<< ": expected " << expected_size
					<< " bytes, received " << static_cast<int32>(packet_size) << "\n";
				_prev_remain = 0;
				RequestStop();
				return;
			}
			else
			{
				if (packet_type == static_cast<uint8>(S_TITLE_PACKET_TYPE::LOGIN_OK))
				{
					const auto* login = reinterpret_cast<const S2C_LOGIN_OK*>(p);
					SetIdentity(login->cid, login->sid);
				}

				if (!QueuePacket(p, packet_size))
				{
					::OutputDebugStringA("[Network] Pending packet queue overflow\n");
					std::cerr << "Pending packet queue overflow\n";
					_prev_remain = 0;
					RequestStop();
					return;
				}
			}

			p += packet_size;
			remain_data -= packet_size;
		}
		_prev_remain = remain_data;
		if (remain_data > 0)
		{
			memmove(rev->_rbuf, p, remain_data);
		}
		if (!IsStopping()) DoRecv();
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
	if (IsStopping() || _sock == INVALID_SOCKET) return false;
	DWORD sendLen(0);
	DWORD flag(0);
	SendEvent* sev = new SendEvent(reinterpret_cast<char*>(packet));
	sev->_sid = GetSid();
	std::shared_lock socketLock(_lock);
	if (_sock == INVALID_SOCKET) { delete sev; return false; }
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
	if (IsStopping() || _sock == INVALID_SOCKET) return false;
	_rev.Init();
	const auto [cid, sid] = GetIdentity();
	_rev._sid = sid;
	_rev._cid = cid;
	DWORD recvBytes(0);
	DWORD flag(0);
	_rev._rWsaBuf.buf = _rev._rbuf + _prev_remain;
	_rev._rWsaBuf.len = BUFSIZE - _prev_remain;
	std::shared_lock socketLock(_lock);
	if (_sock == INVALID_SOCKET) return false;
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

void ClientSession::ApplyPacket(char* packet)
{

	CTitleScene* ts =
		static_cast<CTitleScene*>(mainGame.m_SceneManager->GetSceneByIdx((int32)CGameFramework::SCENESTATE::TITLE));
	CGameScene* gs =
		static_cast<CGameScene*>(mainGame.m_SceneManager->GetSceneByIdx((int32)CGameFramework::SCENESTATE::INGAME));
	CLobbyScene* ls = static_cast<CLobbyScene*>(mainGame.m_SceneManager->GetSceneByIdx((int32)CGameFramework::SCENESTATE::LOBBY));
	CRoomScene* rs = static_cast<CRoomScene*>(mainGame.m_SceneManager->GetSceneByIdx((int32)CGameFramework::SCENESTATE::ROOM));
	CResultScene* rrs = static_cast<CResultScene*>(mainGame.m_SceneManager->GetSceneByIdx((int32)CGameFramework::SCENESTATE::RESULT));
	if (!gs) return; // something error detected;
	if (!ls) return; // something error detected;
	if (!rs) return;
	if (!rrs) return;

	const auto packetType = static_cast<uint8>(packet[1]);
	const bool requiresInGameScene =
		packetType == static_cast<uint8>(S_GAME_PACKET_TYPE::SKEY) ||
		packetType == static_cast<uint8>(S_GAME_PACKET_TYPE::SROT) ||
		packetType == static_cast<uint8>(S_GAME_PACKET_TYPE::SPOS) ||
		packetType == static_cast<uint8>(SC_GAME_PACKET_TYPE::GAMEEVENT) ||
		packetType == static_cast<uint8>(S_GAME_PACKET_TYPE::ANIM) ||
		packetType == static_cast<uint8>(S_GAME_PACKET_TYPE::FRAME);
	if (requiresInGameScene &&
		mainGame.m_curScene != static_cast<int32>(CGameFramework::SCENESTATE::INGAME))
		return;

	switch (packetType)
	{

	// ================ 로그인 관련 처리 ================
#pragma region Title
	case (uint8)S_TITLE_PACKET_TYPE::LOGIN_OK:
	{
		S2C_LOGIN_OK* lo = (S2C_LOGIN_OK*)packet;
		CScene::m_sid = lo->sid;
		CScene::m_cid = lo->cid;
		ts->loginLock.lock();
		ts->m_login = true;
		ts->loginLock.unlock();
		mainGame.m_UIRenderer->m_LoginResult[0].m_hide = false;
		mainGame.m_UIRenderer->m_LoginResult[1].m_hide = true;
		mainGame.m_UIRenderer->m_LoginResult[2].m_hide = true;
		g_clientTestMode.OnLoginOk(*this);
	}
	break;
	case (uint8)S_TITLE_PACKET_TYPE::LOGIN_FAIL:
	{
		mainGame.m_UIRenderer->m_LoginResult[0].m_hide = true;
		mainGame.m_UIRenderer->m_LoginResult[1].m_hide = false;
		mainGame.m_UIRenderer->m_LoginResult[2].m_hide = true;
		g_clientTestMode.OnProtocolFailure("login failed");
	}
	break;
	case (uint8)S_TITLE_PACKET_TYPE::REG_OK:
	{
		mainGame.m_UIRenderer->m_LoginResult[0].m_hide = true;
		mainGame.m_UIRenderer->m_LoginResult[1].m_hide = true;
		mainGame.m_UIRenderer->m_LoginResult[2].m_hide = false;
	}
	break;

#pragma endregion
	// ================ 로비씬 패킷      ===============
#pragma region  Lobby
	// ============= 방 관련 패킷 ============
	case (uint8)S_ROOM_PACKET_TYPE::REP_ENTER_OK:
	{
		S2C_ROOM_ENTER* rep = (S2C_ROOM_ENTER*)packet;
		rs->m_rmnum = rep->rmNum;
		mainGame.ChangeScene(CGameFramework::SCENESTATE::ROOM);
		g_clientTestMode.OnRoomEntered(*this, rep->rmNum);
	}
	break;
	case (uint8)S_ROOM_PACKET_TYPE::REP_ENTER_FAIL:
	{
		g_clientTestMode.OnProtocolFailure("room enter failed");
	}
	break;
	case (uint8)S_ROOM_PACKET_TYPE::MK_RM_FAIL:
	{

	}
	break;
	case (uint8)S_ROOM_PACKET_TYPE::MK_RM_OK:
	{

		mainGame.ChangeScene(CGameFramework::SCENESTATE::ROOM);
	}
		break;
	case (uint8)S_ROOM_PACKET_TYPE::UPDATE_LIST:
	{

		S2C_ROOM_LIST* rp = (S2C_ROOM_LIST*)packet;;
		ls->UpdateRoomStatus(rp->rmNum, rp->member);
		std::cout << "RM" << (int32)rp->rmNum << " MEMBER:" << (int32)rp->member << "\n";
	}
		break;

#pragma endregion
#pragma region Room
	case (uint8)S_ROOM_PACKET_TYPE::REP_READY:
	{
		S2C_ROOM_READY* rp = (S2C_ROOM_READY*)packet;
		rs->m_memLock.lock();
		rs->UpdateReady(rp->sid, true);
		rs->m_memLock.unlock();

		std::cout << rp->sid << "Ready\n";
	}
		break;
	case (uint8)S_ROOM_PACKET_TYPE::REP_READY_CANCEL:
	{
		S2C_ROOM_READY* rp = (S2C_ROOM_READY*)packet;
		rs->m_memLock.lock();
		rs->UpdateReady(rp->sid, false);
		rs->m_memLock.unlock();
		std::cout << rp->sid << "Cancel Ready\n";
	}
		break;
	case (uint8)S_ROOM_PACKET_TYPE::ROOM_INFO:
	{

		S2C_ROOM_INFO* rp = (S2C_ROOM_INFO*)packet;
		rs->m_memLock.lock();
		for (int i = 0; i < PLAYERNUM; ++i) rs->m_members[i].m_sid = rp->sids[i];
		for (int i = 0; i < PLAYERNUM; ++i) rs->m_members[i].isReady = rp->rd[i];

		rs->m_memLock.unlock();
	}
	break;
	case (uint8)S_ROOM_PACKET_TYPE::GAME_START:
	{
		// ================= 플레이어 초기 위치 초기화 ==================
		auto* gameStart = reinterpret_cast<S2C_GAMESTART*>(packet);
		if (!g_clientTestMode.ValidateGameStart(gameStart->sids, GetSid())) break;
		if (!gs->InitGame(gameStart, GetSid()))
		{
			g_clientTestMode.OnProtocolFailure("invalid or duplicate GAME_START");
			break;
		}


		// ================= 카메라 셋팅 ================================
		mainGame.m_UIRenderer->InitGameSceneUI(gs->CreateUiSnapshot());

		mainGame.ChangeScene(CGameFramework::SCENESTATE::INGAME);
		gs->InitScene();
		g_clientTestMode.OnGameStarted();
		rs->m_memLock.lock();

		for (int i = 0; i < PLAYERNUM; ++i)
		{
			rs->m_members[i].m_sid = -1;
			rs->m_members[i].isReady = false;
		}
		rs->m_memLock.unlock();
	}
	break;
#pragma endregion
	// ============== 인게임 관련 패킷 =============
#pragma region InGame
	case (uint8)S_GAME_PACKET_TYPE::SKEY:
	{
		S2C_KEY* movePacket = reinterpret_cast<S2C_KEY*>(packet);
		CPlayer* player = gs ->GetScenePlayerBySid(movePacket->sid);
		if (player == nullptr) break;

		gs->AddEvent(moveEvent{
			player->GetPlayerIndex(),
			movePacket->key,
			XMFLOAT3(movePacket->x, 0.0f, movePacket->z) },
			mainGame.m_activeDelay ? 320.0f : 0.0f);
	}
	break;
	case (uint8)S_GAME_PACKET_TYPE::SROT:
	{
		S2C_ROTATE* rotatePacket = reinterpret_cast<S2C_ROTATE*>(packet);
		CPlayer* player = gs->GetScenePlayerBySid(rotatePacket->sid);
		if (!player) break;
		gs->AddEvent(rotateEvent{ player->GetPlayerIndex(), static_cast<float>(rotatePacket->angle) },
			mainGame.m_activeDelay ? 320.0f : 0.0f);
	}
	break;
	case (uint8)S_GAME_PACKET_TYPE::SPOS: // 미리 계산한 좌표값을 보내준다.
	{
		S2C_POS* posPacket = reinterpret_cast<S2C_POS*>(packet);
		CPlayer* player = gs->GetScenePlayerBySid(posPacket->sid);
		if (player == nullptr) break;

		XMFLOAT3 newPos = XMFLOAT3(posPacket->x, player->GetPosition().y, posPacket->z);
		gs->AddEvent(posEvent{ player->GetPlayerIndex(), newPos },
			mainGame.m_activeDelay ? 320.0f : 0.0f);
	}
	break;
	case (uint8)SC_GAME_PACKET_TYPE::GAMEEVENT:
	{
		SC_EVENTPACKET* ev = (SC_EVENTPACKET*)packet;
		if (ev->eventId == (uint8)EVENT_TYPE::BOSS_WIN || ev->eventId == (uint8)EVENT_TYPE::EMP_WIN)
		{
			if (!gs->ResetGame())
			{
				g_clientTestMode.OnProtocolFailure("duplicate or out-of-order game result");
				break;
			}

			std::cout << "Go to Result\n";

			if (ev->eventId == (uint8)EVENT_TYPE::BOSS_WIN) rrs->m_case = 1;
			if (ev->eventId == (uint8)EVENT_TYPE::EMP_WIN) rrs->m_case = 0;

			rrs->m_timer.Reset();
			mainGame.ChangeScene(CGameFramework::SCENESTATE::RESULT);
		}
		else
		{
			gs->AddEvent(InteractionEvent{ ev->eventId },
				mainGame.m_activeDelay ? 320.0f : 0.0f);
		}
	}
	break;
	// ================= 플레이어 스위치 애니메이션 관련 패킷 ==================
	case (uint8)S_GAME_PACKET_TYPE::ANIM:
	{
		S2C_ANIMPACKET* sw = (S2C_ANIMPACKET*)packet;
		if (!gs->GetScenePlayerByIdx(sw->idx)) break;
		gs->AddEvent(animationEvent{ sw->idx, sw->track },
			mainGame.m_activeDelay ? 320.0f : 0.0f);
	}
	break;
	case (uint8)S_GAME_PACKET_TYPE::FRAME:
	{
		S2C_FRAMEPACKET* fp = (S2C_FRAMEPACKET*)packet;
		gs->AddEvent(FrameEvent{ fp->wf }, mainGame.m_activeDelay ? 320.0f : 0.0f);

	}
	break;
#pragma endregion
	}

}
