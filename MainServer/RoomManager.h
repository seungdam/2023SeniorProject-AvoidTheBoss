#pragma once
#include "CGameManager.h"
#include "RoomCommand.h"

#include <array>
#include <chrono>
#include <deque>
#include <mutex>


class QueueEvent;
class RoomManager;

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
	bool ProcessAttackEvent(const int32& frame, const int16& target) { return (_gameLogic._history.IsAttackAvailable(frame, target)); }
	void Update();
	void AddEvent(QueueEvent* packet, float after); // 이벤트 패킷이 들어오면 큐에다가 추가를 할 것이다.
	void AddEvent(QueueEvent* qe);
	void StartGame() { _timer.Reset(); }

	void SendRoomListPacket();
	void SendRoomInfoPacket();
	CGameManager& GameLogic() noexcept { return _gameLogic; }
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

	void InitGame();
	void UpdateReady(int32 idx, bool val);
	bool IsGameStartAvailable();
private:
	static constexpr auto ReconnectGracePeriod = std::chrono::seconds(30);
	int32 GetMemberIndex(int32 sid, bool connectedOnly) const;
	void ExpireReconnects();

	friend class RoomManager;

	CGameManager _gameLogic;
	mutable std::shared_mutex _listLock;
	std::array<RoomMember, PLAYERNUM> _roomMembers{};
	uint8 _status = (int8)ROOM_STATUS::EMPTY; // 방 상태
	int32 _roomNumber = 0; // 방번호
	Atomic<int32> _nMembers = 0;
	Timer _timer;
};

class RoomManager
{
public:
	static constexpr int32 RoomCapacity = 100;

	void EnqueueCommand(RoomCommand command);
	void ExitRoom(int32 sid, int32 rmNum);
	void DisconnectSession(int32 sid, int32 rmNum, uint64 resumeToken);
	void ResumeSession(int32 sid, uint64 resumeToken);
	bool EnterRoom(int32 sid, int32 rmNum);
	void CreateRoom(int32 sid);
	void UpdateRooms();
	bool IsValidRoom(int32 rmNum) const noexcept { return rmNum >= 0 && rmNum < RoomCapacity; }
	Room& GetRoom(int32 rmNum) { return _rooms[rmNum]; }
	void Init();

private:
	void DrainCommands();
	void ExecuteCommand(const RoomCommand& command);

	std::array<Room, RoomCapacity> _rooms{};
	std::mutex _commandLock;
	std::deque<RoomCommand> _commands;
};

