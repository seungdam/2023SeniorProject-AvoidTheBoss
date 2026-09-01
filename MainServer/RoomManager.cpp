#include "pch.h"
#include "RoomManager.h"
#include "ServerSessionManager.h"
#include "ServerSession.h"
#include "JobQueue.h"


using namespace std;
//========== ROOM =============

//    cList Lock 호출 타이밍
//    1. 쓰는 경우 : 유저가 방에 들어오거나 나갈 때, cList 값을 갱신할 때
//    2. 읽는 경우 : 방에 있는 유저에게 브로드 캐스팅 진행하는 경우 cList를 탐색할 때

//=============================
Room::Room(ServerSessionManager& sessions, OcTree& collisionTree, const int32 roomNumber)
	: _sessions(sessions), _matchState(collisionTree), _roomNumber(roomNumber)
{
}

Room::~Room()
{
}

void Room::UserOut(int32 sid)
{
	int idx = -1;
	bool removed = false;

	{
		// cList Lock 쓰기 호출
		std::unique_lock<std::shared_mutex> wll(_listLock);
		for (int i = 0; i < PLAYERNUM; ++i)
		{
			if (sid == _roomMembers[i].sid)
			{
				const uint64 nextGeneration = _roomMembers[i].generation + 1;
				_roomMembers[i] = {};
				_roomMembers[i].generation = nextGeneration;
				if (_nMembers) _nMembers.fetch_sub(1);
				idx = i;
				removed = true;
				break;
			}
		}
	}
	if (!removed) return;
	if (const auto session = _sessions.FindSession(sid)) session->ClearRoomBinding();

	if (_status == (uint8)ROOM_STATUS::INGAME)
	{
		const MatchPlayerDeparture departure = _matchState.OnPlayerLeft(sid);
		idx = departure.playerIndex;
		if (departure.effect != PlayerDepartureEffect::None)
		{
			if (departure.effect == PlayerDepartureEffect::EndMatch)
			{
				SC_EVENTPACKET packet;
				packet.type = (uint8)SC_GAME_PACKET_TYPE::GAMEEVENT;
				packet.size = sizeof(SC_EVENTPACKET);
				packet.eventId = (uint8)EVENT_TYPE::EMP_WIN;
				BroadCastingExcept(&packet, sid);


				std::array<int16, PLAYERNUM> members{};
				{
					// cList Lock 쓰기 호출
					std::unique_lock<std::shared_mutex> wll(_listLock);

				for (int i = 0; i < PLAYERNUM; ++i)
				{
					members[i] = _roomMembers[i].sid;
					const uint64 nextGeneration = _roomMembers[i].generation + 1;
					_roomMembers[i] = {};
					_roomMembers[i].generation = nextGeneration;
				}
				}
				_nMembers.store(0);
				_status = (uint8)ROOM_STATUS::EMPTY;
				for (const int16 memberSid : members)
				{
					if (const auto member = _sessions.FindSession(memberSid))
						member->ClearRoomBinding();
				}
				std::cout << "STRANGE GAME END\n";
				return;
			}
			else
			{
				SC_EVENTPACKET packet;
				packet.size = sizeof(SC_EVENTPACKET);
				packet.type = (uint8)SC_GAME_PACKET_TYPE::GAMEEVENT;
				packet.eventId = (uint8)EVENT_TYPE::HIDE_PLAYER_ONE + idx;
				BroadCastingExcept(&packet, sid);
				std::cout << idx << "HIDE\n";
			}
		}
	}
	else
	{
		if (_status == (uint8)ROOM_STATUS::FULL)
		{
			_status = (uint8)ROOM_STATUS::NOT_FULL;
			std::cout << "LEFT USER SID LIST [";
			for (auto i : _roomMembers)  std::cout << (int32)i.sid << "|";
			std::cout << " ]\n";
		}

		// 방이 폭파되는 경우.. 게임을 리셋한다.
		if (IsDestroyRoom())
		{
			_status = (uint8)ROOM_STATUS::EMPTY;
			{
				std::unique_lock<std::shared_mutex> wll(_listLock);
				for (auto& i : _roomMembers)
				{
					const uint64 nextGeneration = i.generation + 1;
					i = {};
					i.generation = nextGeneration;
				}
			}
			std::cout << "Destroy Room\n";
		}
		SendRoomListPacket();
	}


	SendRoomInfoPacket();

}

int32 Room::GetMemberIndex(const int32 sid, const bool connectedOnly) const
{
	std::shared_lock lock(_listLock);
	for (int32 i = 0; i < PLAYERNUM; ++i)
	{
		if (_roomMembers[i].sid != sid) continue;
		if (!connectedOnly || _roomMembers[i].connection == ConnectionState::Connected) return i;
	}
	return -1;
}

bool Room::IsCurrentMember(const int32 sid, const uint64 generation) const
{
	std::shared_lock lock(_listLock);
	for (const RoomMember& member : _roomMembers)
	{
		if (member.sid == sid && member.connection == ConnectionState::Connected && member.generation == generation)
			return true;
	}
	return false;
}

bool Room::IsCurrentLease(const int32 sid, const MatchLease& lease) const
{
	return _status == static_cast<uint8>(ROOM_STATUS::INGAME) &&
		_matchState.IsCurrentGeneration(lease.matchGeneration) &&
		IsCurrentMember(sid, lease.memberGeneration);
}

void Room::OnTransportDisconnected(const int32 sid, const uint64 resumeToken)
{
	const int32 idx = GetMemberIndex(sid, false);
	if (idx < 0) return;
	if (_status != static_cast<uint8>(ROOM_STATUS::INGAME) || idx == 0 || resumeToken == 0)
	{
		UserOut(sid);
		return;
	}

	{
		std::unique_lock lock(_listLock);
		RoomMember& member = _roomMembers[idx];
		if (member.sid == sid && member.connection == ConnectionState::Reconnecting) return;
		if (member.sid != sid || member.connection != ConnectionState::Connected || member.resumeToken != resumeToken)
			return;
		member.connection = ConnectionState::Reconnecting;
		member.reconnectDeadline = std::chrono::steady_clock::now() + ReconnectGracePeriod;
		++member.generation;
	}

	_matchState.PausePlayer(sid);
}

bool Room::ResumeUser(const int32 newSid, const uint64 resumeToken)
{
	const auto session = _sessions.FindSession(newSid);
	if (!session || !IsSessionUnbound(session->GetRoomNumber()) ||
		_status != static_cast<uint8>(ROOM_STATUS::INGAME)) return false;

	int32 idx = -1;
	int16 oldSid = -1;
	bool replaceConnectedSession = false;
	{
		std::unique_lock lock(_listLock);
		for (int32 i = 1; i < PLAYERNUM; ++i)
		{
			RoomMember& member = _roomMembers[i];
			if (member.resumeToken != resumeToken) continue;
			if (member.connection == ConnectionState::Reconnecting &&
				std::chrono::steady_clock::now() >= member.reconnectDeadline)
				return false;

			const uint64 nextGeneration = member.generation + 1;
			if (!session->BindRoom(_roomNumber, nextGeneration, _matchState.Generation())) return false;
			idx = i;
			oldSid = member.sid;
			replaceConnectedSession = member.connection == ConnectionState::Connected;
			member.sid = static_cast<int16>(newSid);
			member.connection = ConnectionState::Connected;
			member.reconnectDeadline = {};
			member.generation = nextGeneration;
			break;
		}
	}

	if (idx < 0) return false;
	_matchState.RebindPlayer(newSid, idx);
	session->SetResumeToken(resumeToken);

	if (replaceConnectedSession && oldSid != newSid)
	{
		if (const auto oldSession = _sessions.FindSession(oldSid))
		{
			oldSession->ClearRoomBinding();
			_sessions.RequestRemoveSession(oldSid);
		}
	}

	S2C_RESUME packet{};
	packet.size = sizeof(packet);
	packet.type = static_cast<uint8>(S_TITLE_PACKET_TYPE::RESUME_OK);
	packet.oldSid = oldSid;
	packet.newSid = static_cast<int16>(newSid);
	packet.playerIndex = static_cast<uint8>(idx);
	BroadCasting(&packet);
	return true;
}

void Room::ExpireReconnects()
{
	std::array<int32, PLAYERNUM> expired{};
	int32 count = 0;
	const auto now = std::chrono::steady_clock::now();
	{
		std::shared_lock lock(_listLock);
		for (const RoomMember& member : _roomMembers)
		{
			if (member.connection == ConnectionState::Reconnecting && now >= member.reconnectDeadline)
				expired[count++] = member.sid;
		}
	}
	for (int32 i = 0; i < count; ++i) UserOut(expired[i]);
}

bool Room::UserIn(int32 sid)
{
	const auto session = _sessions.FindSession(sid);
	if (!session || session->GetResumeToken() == 0) return false;

	S2C_ROOM_ENTER packet;
	packet.size = sizeof(S2C_ROOM_ENTER);
	packet.type = (uint8)S_ROOM_PACKET_TYPE::REP_ENTER_OK;


	// 비어있는 방 (만들어지지 않은 방) or 현재 인원수가 4명이면 접속 실패
	if (_status == (int8)ROOM_STATUS::FULL || _status == (int8)ROOM_STATUS::EMPTY || _status == (int8)ROOM_STATUS::INGAME)
	{
		// enter fail
		std::cout << "Enter Fail\n";
		packet.rmNum = -1;
		packet.type = (uint8)S_ROOM_PACKET_TYPE::REP_ENTER_FAIL;
		session->DoSend(&packet);
		return false;
	}
	else if (_status == (int8)ROOM_STATUS::NOT_FULL) // 아니면 접속 성공
	{
		packet.rmNum = _roomNumber;
		bool inserted = false;
		{
			//cList Lock 쓰기 호출
			std::unique_lock<std::shared_mutex> wll(_listLock);
			// 빈 배열 자리에다가 정보 채우기
			for (int k = 0; k < PLAYERNUM; ++k)
			{
				if (-1 == _roomMembers[k].sid)
				{
					RoomMember& member = _roomMembers[k];
					const uint64 nextGeneration = member.generation + 1;
					if (!session->BindRoom(_roomNumber, nextGeneration)) break;
					member.sid = static_cast<int16>(sid);
					member.bReady = false;
					member.resumeToken = session->GetResumeToken();
					member.connection = ConnectionState::Connected;
					member.reconnectDeadline = {};
					member.generation = nextGeneration;
					_nMembers.fetch_add(1);
					inserted = true;
					break;
				}
			}
			std::cout << "LEFT USER SID LIST [";
			for (auto i : _roomMembers)  std::cout << (int32)i.sid << "|";
			std::cout << " ]\n";
		}
		if (!inserted)
		{
			packet.rmNum = -1;
			packet.type = static_cast<uint8>(S_ROOM_PACKET_TYPE::REP_ENTER_FAIL);
			session->DoSend(&packet);
			return false;
		}
		session->DoSend(&packet);
		SendRoomListPacket();
		if (_nMembers == PLAYERNUM)
		{
			_status = (int8)ROOM_STATUS::FULL;
		}

	}

	SendRoomInfoPacket();
	// 갱신할 방 리스트 정보와 방 정보를 보낸다.

	std::cout << "RM [" << _roomNumber << "][" << _nMembers.load() << "/4]" << std::endl;

	// 갱신하는걸 보내줄지 말지 미정
	return true;
}

void Room::BroadCasting(void* packet) // 방에 속하는 클라이언트에게만 전달하기
{
	std::array<int16, PLAYERNUM> memberSids{};
	int32 count = 0;
	{
		std::shared_lock<std::shared_mutex> rll(_listLock);
		for (const RoomMember& member : _roomMembers)
		{
			if (member.sid != -1 && member.connection == ConnectionState::Connected)
				memberSids[count++] = member.sid;
		}
	}

	for (int32 i = 0; i < count; ++i)
	{
		const auto session = _sessions.FindSession(memberSids[i]);
		if (session) session->DoSend(packet);
	}
}

void Room::BroadCastingExcept(void* packet, int32 sid) // 방에 속하는 클라이언트에게만 전달하기
{
	std::array<int16, PLAYERNUM> memberSids{};
	int32 count = 0;
	{
		std::shared_lock<std::shared_mutex> rll(_listLock);
		for (const RoomMember& member : _roomMembers)
		{
			if (member.sid != -1 && member.sid != sid && member.connection == ConnectionState::Connected)
				memberSids[count++] = member.sid;
		}
	}

	for (int32 i = 0; i < count; ++i)
	{
		const auto session = _sessions.FindSession(memberSids[i]);
		if (session) session->DoSend(packet);
	}
}

// 방에 있는 유저에 대한 게임 로직 업데이트 진행
void Room::Update(const float fixedDeltaSeconds)
{
	if (_status != (int8)ROOM_STATUS::INGAME) return;
	ExpireReconnects();
	if (_status != (int8)ROOM_STATUS::INGAME) return;
	const MatchTickResult tick = _matchState.Tick(*this, fixedDeltaSeconds);

	if (tick.frame >= 0)
	{
		S2C_FRAMEPACKET packet;
		packet.size = sizeof(S2C_FRAMEPACKET);
		packet.type = (uint8)S_GAME_PACKET_TYPE::FRAME;
		packet.wf = tick.frame;
		BroadCasting(&packet);
	}

	if (tick.broadcastPositions) // 1/45초마다 정확한 위치값을 브로드캐스팅 한다.
	{

		for (int i = 0; i < PLAYERNUM; ++i)
		{

			SPlayer& ps = _matchState.GetPlayerByIdx(i);

			S2C_POS packet;
			packet.sid = ps.m_sid;
			packet.size = sizeof(S2C_POS);
			packet.type = (uint8)S_GAME_PACKET_TYPE::SPOS;
			packet.x = ps.GetPosition().x;
			packet.z = ps.GetPosition().z;
			BroadCasting(&packet);

		}
	}


	// 게임이 끝났다면 게임이 끝났다는 패킷 전송
	if (GAMESTATE::EMP_WIN == tick.state || GAMESTATE::BOSS_WIN == tick.state)
	{
		_status = (uint8)ROOM_STATUS::FULL;

		SC_EVENTPACKET packet;
		packet.size = sizeof(SC_EVENTPACKET);
		packet.type = (uint8)SC_GAME_PACKET_TYPE::GAMEEVENT;
		if (GAMESTATE::BOSS_WIN == tick.state) packet.eventId = (uint8)EVENT_TYPE::BOSS_WIN;
		else  packet.eventId = (uint8)EVENT_TYPE::EMP_WIN;

		BroadCasting(&packet);
		std::cout << "Normal Game END\n";

		std::array<int32, PLAYERNUM> memberSids{};
		{
			std::shared_lock lock(_listLock);
			for (int i = 0; i < PLAYERNUM; ++i) memberSids[i] = _roomMembers[i].sid;
		}
		for (const int32 memberSid : memberSids)
		{
			if (memberSid != -1) UserOut(memberSid);
		}

		_matchState.Reset();
	}
}

void Room::AddEvent(QueueEvent* event, const MatchLease& lease, const float after)
{
	if (!event) return;
	if (!IsCurrentLease(event->_sid, lease))
	{
		delete event;
		return;
	}
	event->_lease = lease;
	_matchState.AddEventAfterTime(after, event);
}

void Room::ProcessGamePacket(const GameCommand& command)
{
	const uint8 packetSize = command.packetSize;
	if (packetSize < 2 || packetSize > command.packet.size() ||
		!IsCurrentLease(command.sid, command.lease)) return;
	const char* packet = command.packet.data();
	const int32 sid = command.sid;

	switch (static_cast<uint8>(packet[1]))
	{
	case static_cast<uint8>(C_GAME_PACKET_TYPE::CKEY):
	{
		if (packetSize != sizeof(C2S_KEY)) return;
		const auto* movePacket = reinterpret_cast<const C2S_KEY*>(packet);
		AddEvent(new moveEvent(sid, movePacket->key, XMFLOAT3{ movePacket->x, 0, movePacket->z }),
			command.lease);
		return;
	}
	case static_cast<uint8>(C_GAME_PACKET_TYPE::CROT):
	{
		if (packetSize != sizeof(C2S_ROTATE)) return;
		const auto* rotatePacket = reinterpret_cast<const C2S_ROTATE*>(packet);
		S2C_ROTATE response{};
		response.size = sizeof(response);
		response.type = static_cast<uint8>(S_GAME_PACKET_TYPE::SROT);
		response.sid = static_cast<int16>(sid);
		response.angle = rotatePacket->angle;
		BroadCastingExcept(&response, sid);
		return;
	}
	case static_cast<uint8>(C_GAME_PACKET_TYPE::CCHAT):
	{
		if (packetSize != sizeof(_CHAT)) return;
		_CHAT response{};
		memcpy(&response, packet, sizeof(response));
		response.type = static_cast<uint8>(S_GAME_PACKET_TYPE::SCHAT);
		BroadCasting(&response);
		return;
	}
	case static_cast<uint8>(C_GAME_PACKET_TYPE::CATTACK):
	{
		if (packetSize != sizeof(C2S_ATTACK)) return;
		const auto* attackPacket = reinterpret_cast<const C2S_ATTACK*>(packet);
		if (attackPacket->tidx < 0 || attackPacket->tidx >= PLAYERNUM)
		{
			_sessions.RequestRemoveSession(sid);
			return;
		}
		auto* event = new AttackEvent();
		event->_sid = sid;
		event->_tidx = attackPacket->tidx;
		event->_wf = attackPacket->wf;
		AddEvent(event, command.lease);
		return;
	}
	case static_cast<uint8>(SC_GAME_PACKET_TYPE::GAMEEVENT):
	{
		if (packetSize != sizeof(SC_EVENTPACKET)) return;
		const auto* eventPacket = reinterpret_cast<const SC_EVENTPACKET*>(packet);
		auto* event = new InteractionEvent();
		event->_sid = sid;
		event->eventId = eventPacket->eventId;
		AddEvent(event, command.lease);
		return;
	}
	default:
		return;
	}
}

void Room::SendRoomListPacket()
{

	S2C_ROOM_LIST rmpacket; // 로비에서 리스트를 갱신하기 위한 패킷
	rmpacket.size = sizeof(S2C_ROOM_LIST);
	rmpacket.type = (int8)S_ROOM_PACKET_TYPE::UPDATE_LIST;
	rmpacket.member = _nMembers.load();
	rmpacket.rmNum = _roomNumber;

	_sessions.BroadcastAll(&rmpacket);

}
void Room::SendRoomInfoPacket()
{
	S2C_ROOM_INFO rmifpacket; // 게임 방에 현재 들어와 있는 멤버들의 리스트를 보내주기 위한 패킷, 방 유저에게만 전송
	rmifpacket.size = sizeof(S2C_ROOM_INFO);
	rmifpacket.type = (uint8)S_ROOM_PACKET_TYPE::ROOM_INFO;
	{
		shared_lock<std::shared_mutex> rl(_listLock);
		for (int i = 0; i < PLAYERNUM; ++i)
		{
			rmifpacket.sids[i] = _roomMembers[i].sid;
			rmifpacket.rd[i] = _roomMembers[i].bReady;
		}

	}
	BroadCasting(&rmifpacket);

}
void Room::InitGame()
{
	if (IsGameStartAvailable())
	{
		S2C_GAMESTART packet{};
		packet.size = sizeof(S2C_GAMESTART);
		packet.type = (uint8)S_ROOM_PACKET_TYPE::GAME_START;
		std::array<int16, PLAYERNUM> playerSids{};
		{
			std::shared_lock lock(_listLock);
			for (int i = 0; i < PLAYERNUM; ++i)
			{
				packet.sids[i] = _roomMembers[i].sid;
				playerSids[i] = _roomMembers[i].sid;
			}
		}
		{
			std::unique_lock<std::shared_mutex> rl(_listLock);
			for (auto& i : _roomMembers) i.bReady = false;
		}
		const uint64 matchGeneration = _matchState.Start(playerSids);
		_status = (uint8)ROOM_STATUS::INGAME;
		for (const int16 playerSid : playerSids)
		{
			if (const auto session = _sessions.FindSession(playerSid))
				(void)session->BindMatch(matchGeneration);
		}
		BroadCasting(&packet);
		std::cout << "TOTAL USER SID LIST[";
		for (const int16 playerSid : playerSids) std::cout << playerSid << " | ";
		std::cout << "]\n";
	}
}
bool Room::TryUpdateReady(const int32 sid, const bool val)
{
	if (_status != static_cast<uint8>(ROOM_STATUS::NOT_FULL) &&
		_status != static_cast<uint8>(ROOM_STATUS::FULL)) return false;

	std::unique_lock<std::shared_mutex> lock(_listLock);
	for (RoomMember& member : _roomMembers)
	{
		if (member.sid != sid || member.connection != ConnectionState::Connected) continue;
		member.bReady = val;
		return true;
	}
	return false;
}

bool Room::IsGameStartAvailable()
{
	 int cnt = 0;

	 {
		 shared_lock<std::shared_mutex> rl(_listLock);
		 for (int i = 0; i < 4; ++i) if (_roomMembers[i].bReady) ++cnt;
		 return (PLAYERNUM == cnt);
	 }
}

// ======= RoomManager ========

// ============================
RoomManager::RoomManager(ServerSessionManager& sessions, LobbyCommandQueue& lobbyCommands,
	GameCommandQueue& gameCommands, OcTree& collisionTree)
	: _sessions(sessions), _lobbyCommands(lobbyCommands), _gameCommands(gameCommands)
{
	for (int32 roomNumber = 0; roomNumber < RoomCapacity; ++roomNumber)
		_rooms.emplace_back(sessions, collisionTree, roomNumber);
}

void RoomManager::DrainLobbyCommands()
{
	auto commands = _lobbyCommands.Drain(MaxLobbyCommandsPerUpdate);
	for (const LobbyCommand& command : commands)
		ExecuteLobbyCommand(command);
}

void RoomManager::DrainGameCommands()
{
	auto commands = _gameCommands.Drain(MaxGameCommandsPerUpdate);
	for (const GameCommand& command : commands)
		ExecuteGameCommand(command);
}

void RoomManager::ExecuteLobbyCommand(const LobbyCommand& command)
{
	switch (command.type)
	{
	case LobbyCommandType::SessionConnected:
		SendInitialRoomList(command.sid);
		return;
	case LobbyCommandType::Create:
		CreateRoom(command.sid);
		return;
	case LobbyCommandType::Enter:
		EnterRoom(command.sid, command.roomNum);
		return;
	case LobbyCommandType::Exit:
		if (IsValidRoom(command.roomNum))
			ExitRoom(command.sid, command.roomNum);
		return;
	case LobbyCommandType::Disconnected:
		DisconnectSession(command.sid, command.resumeToken);
		return;
	case LobbyCommandType::Resume:
		ResumeSession(command.sid, command.resumeToken);
		return;
	case LobbyCommandType::SetReady:
	{
		const auto session = _sessions.FindSession(command.sid);
		if (!session) return;
		const int32 roomNum = session->GetRoomNumber();
		if (!IsValidRoom(roomNum)) return;

		Room& room = GetRoom(roomNum);
		if (!room.TryUpdateReady(command.sid, command.isReady)) return;

		S2C_ROOM_READY packet{};
		packet.size = sizeof(S2C_ROOM_READY);
		packet.type = command.isReady
			? static_cast<uint8>(S_ROOM_PACKET_TYPE::REP_READY)
			: static_cast<uint8>(S_ROOM_PACKET_TYPE::REP_READY_CANCEL);
		packet.sid = command.sid;

		room.BroadCastingExcept(&packet, command.sid);
		if (command.isReady) room.InitGame();
		return;
	}
	}
}

void RoomManager::SendInitialRoomList(const int32 sid)
{
	const auto session = _sessions.FindSession(sid);
	if (!session) return;

	S2C_ROOM_LIST packet{};
	packet.size = sizeof(packet);
	packet.type = static_cast<uint8>(S_ROOM_PACKET_TYPE::UPDATE_LIST);
	for (int32 i = 0; i < 5; ++i)
	{
		const int32 roomNumber = session->_curPage * 5 + i;
		if (!IsValidRoom(roomNumber)) return;
		packet.member = GetRoom(roomNumber).GetMemberCount();
		packet.rmNum = roomNumber;
		if (!session->DoSend(&packet))
		{
			_sessions.RequestRemoveSession(sid);
			return;
		}
	}
}

void RoomManager::ExecuteGameCommand(const GameCommand& command)
{
	if (!IsValidRoom(command.roomNum)) return;
	const auto session = _sessions.FindSession(command.sid);
	if (!session) return;
	const ServerSession::GameBinding binding = session->GetGameBinding();
	if (binding.roomNumber != command.roomNum ||
		!command.lease.Matches(binding.lease.memberGeneration, binding.lease.matchGeneration)) return;
	GetRoom(command.roomNum).ProcessGamePacket(command);
}

bool RoomManager::EnterRoom(int32 sid, int32 rmNum)
{
	if (!IsValidRoom(rmNum))
	{
		_sessions.RequestRemoveSession(sid);
		return false;
	}
	const auto session = _sessions.FindSession(sid);
	if (!session || !IsSessionUnbound(session->GetRoomNumber())) return false;
	return _rooms[rmNum].UserIn(sid);
}
void RoomManager::CreateRoom(int32 sid)
{
	const auto session = _sessions.FindSession(sid);
	if (!session) return;
	S2C_ROOM_EVENT packet;
	packet.size = sizeof(S2C_ROOM_EVENT);

	if (!IsSessionUnbound(session->GetRoomNumber()))
	{
		packet.type = (uint8)S_ROOM_PACKET_TYPE::MK_RM_FAIL;
		session->DoSend(&packet);
		return;
	}
	for (int i = 0; i < RoomCapacity; ++i)
	{
		if (_rooms[i]._status == (int8)ROOM_STATUS::EMPTY)
		{
			_rooms[i]._status = (int8)ROOM_STATUS::NOT_FULL;
			if (EnterRoom(sid, i))
			{
				packet.type = (uint8)S_ROOM_PACKET_TYPE::MK_RM_OK;
				session->DoSend(&packet);
				return;
			}
			_rooms[i]._status = (int8)ROOM_STATUS::EMPTY;
		}
	}

	packet.type = (uint8)S_ROOM_PACKET_TYPE::MK_RM_FAIL;
	session->DoSend(&packet);
}
void RoomManager::UpdateRooms(const float fixedDeltaSeconds)
{
	DrainLobbyCommands();
	DrainGameCommands();

	for (int i = 0; i < RoomCapacity; ++i)
	{
		if (_rooms[i]._status != (int8)ROOM_STATUS::INGAME) continue;
		_rooms[i].Update(fixedDeltaSeconds);
	}
}
void RoomManager::ExitRoom(int32 sid, int32 rmNum)
{
	if (!IsValidRoom(rmNum)) return;
	_rooms[rmNum].UserOut(sid);
}

void RoomManager::DisconnectSession(const int32 sid, const uint64 resumeToken)
{
	// ponytail: 100 rooms x 4 slots avoids an enqueue-time room snapshot race;
	// add an owner-maintained SID index only if this fixed capacity grows.
	for (Room& room : _rooms) room.OnTransportDisconnected(sid, resumeToken);
}

void RoomManager::ResumeSession(const int32 sid, const uint64 resumeToken)
{
	bool resumed = false;
	if (resumeToken != 0)
	{
		for (Room& room : _rooms)
		{
			if (room.ResumeUser(sid, resumeToken))
			{
				resumed = true;
				break;
			}
		}
	}

	if (resumed) return;
	if (const auto session = _sessions.FindSession(sid))
	{
		S2C_RESUME_FAIL packet{};
		packet.size = sizeof(packet);
		packet.type = static_cast<uint8>(S_TITLE_PACKET_TYPE::RESUME_FAIL);
		session->DoSend(&packet);
	}
}
