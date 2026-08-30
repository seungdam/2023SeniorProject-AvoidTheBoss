#pragma once
#include "MatchState.h"
#include "RoomCommand.h"

#include <array>
#include <chrono>


class QueueEvent;
class RoomManager;
class InteractionEvent;
class moveEvent;
class AttackEvent;

// 방은 호스트가 요청하는 순간 생성한다.
class Room
{
	enum class ConnectionState : uint8
	{
		Connected,
		Reconnecting,
	};

	struct RoomMember
	{
		int16 sid = -1;
		bool bReady = false;
		// ponytail: process-memory token only; replace with account-backed, persisted credentials when the identity DB exists.
		uint64 resumeToken = 0;
		uint64 generation = 0;
		ConnectionState connection = ConnectionState::Connected;
		std::chrono::steady_clock::time_point reconnectDeadline{};
	};
public:
	Room();
	~Room();
	bool IsDestroyRoom() { return (_nMembers.load() == 0); } // false 반환 시 방 파괴 --> 호스트가 방을 나갔을 경우 파괴하도록함.
	void UserOut(int32 sid);
	bool UserIn(int32 sid);
	void OnTransportDisconnected(int32 sid, uint64 resumeToken);
	bool ResumeUser(int32 newSid, uint64 resumeToken);
	void BroadCasting(void* packet);
	void BroadCastingExcept(void* packet, int32 sid);
	void Update();

	void SendRoomListPacket();
	void SendRoomInfoPacket();
	int32 GetMemberCount() const noexcept { return _nMembers.load(); }

	int32 GetSidIndexBySid(int32 sid)
	{
		std::shared_lock<std::shared_mutex> rll(_listLock);
		for (int32 i = 0; i < PLAYERNUM; ++i)
		{
			if (_roomMembers[i].sid == sid && _roomMembers[i].connection == ConnectionState::Connected) return i;
		}
		return -1;
	};
	bool IsCurrentMember(int32 sid, uint64 generation) const;
private:
	static constexpr auto ReconnectGracePeriod = std::chrono::seconds(30);
	int32 GetMemberIndex(int32 sid, bool connectedOnly) const;
	void InitGame();
	bool TryUpdateReady(int32 sid, bool val);
	bool IsGameStartAvailable();
	void ExpireReconnects();
	bool IsCurrentLease(int32 sid, const MatchLease& lease) const;
	void ProcessGamePacket(const GameCommand& command);
	void AddEvent(QueueEvent* event, const MatchLease& lease, float after = 0.f);

	friend class RoomManager;
	friend class QueueEvent;
	friend class InteractionEvent;
	friend class moveEvent;
	friend class AttackEvent;

	MatchState _matchState;
	mutable std::shared_mutex _listLock;
	std::array<RoomMember, PLAYERNUM> _roomMembers{};
	uint8 _status = (int8)ROOM_STATUS::EMPTY; // 방 상태
	int32 _roomNumber = 0; // 방번호
	Atomic<int32> _nMembers = 0;
};

class RoomManager
{
public:
	static constexpr int32 RoomCapacity = 100;

	[[nodiscard]] bool EnqueueLobbyCommand(LobbyCommand command);
	[[nodiscard]] bool EnqueueGameCommand(GameCommand command);
	void ExitRoom(int32 sid, int32 rmNum);
	void DisconnectSession(int32 sid, uint64 resumeToken);
	void ResumeSession(int32 sid, uint64 resumeToken);
	bool EnterRoom(int32 sid, int32 rmNum);
	void CreateRoom(int32 sid);
	void UpdateRooms();
	bool IsValidRoom(int32 rmNum) const noexcept { return rmNum >= 0 && rmNum < RoomCapacity; }
	Room& GetRoom(int32 rmNum) { return _rooms[rmNum]; }
	void Init();

private:
	// ponytail: one bounded queue per domain fits the current 100 rooms × 4 members;
	// add per-room/per-match queues only when profiling proves either dispatcher is saturated.
	static constexpr std::size_t MaxPendingLobbyCommands = 4096;
	static constexpr std::size_t MaxPendingGameCommands = 4096;
	static constexpr std::size_t MaxLobbyCommandsPerUpdate = 512;
	static constexpr std::size_t MaxGameCommandsPerUpdate = 512;

	void DrainLobbyCommands();
	void DrainGameCommands();
	void ExecuteLobbyCommand(const LobbyCommand& command);
	void ExecuteGameCommand(const GameCommand& command);

	std::array<Room, RoomCapacity> _rooms{};
	LobbyCommandQueue _lobbyCommands{ MaxPendingLobbyCommands };
	GameCommandQueue _gameCommands{ MaxPendingGameCommands };
};

