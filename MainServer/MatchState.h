#pragma once

#include <array>
#include <memory>

#include "SGenerator.h"
#include "SPlayer.h"
#include "WorldRewinder.h"

class ClientEventScheduler;
class OcTree;
class Room;
class QueueEvent;
class InteractionEvent;
class moveEvent;
class AttackEvent;

enum class GAMESTATE
{
	NONE,
	IN_GAME,
	BOSS_WIN,
	EMP_WIN,
};

enum class PlayerDepartureEffect
{
	None,
	HidePlayer,
	EndMatch,
};

struct MatchTickResult
{
	GAMESTATE state = GAMESTATE::NONE;
	int32 frame = -1;
	bool broadcastPositions = false;
};

struct MatchPlayerDeparture
{
	int32 playerIndex = -1;
	PlayerDepartureEffect effect = PlayerDepartureEffect::None;
};

class MatchState
{
public:
	explicit MatchState(OcTree& collisionTree);
	~MatchState();

	uint64 Start(const std::array<int16, PLAYERNUM>& playerSids);
	MatchTickResult Tick(Room& room, float fixedDeltaSeconds);
	MatchPlayerDeparture OnPlayerLeft(int32 sid);
	void PausePlayer(int32 sid);
	void RebindPlayer(int32 sid, int32 idx);
	void Reset();

private:
	friend class Room;
	friend class InteractionEvent;
	friend class moveEvent;
	friend class AttackEvent;
	friend class QueueEvent;

	uint64 Generation() const noexcept { return _generation; }
	bool IsCurrentGeneration(uint64 generation) const noexcept;
	bool IsAttackAvailable(int32 frame, int16 target) { return _history.IsAttackAvailable(frame, target); }
	SPlayer& GetPlayerBySid(int32 sid);
	SPlayer& GetPlayerByIdx(int32 idx) { return _players[idx]; }
	SGenerator& GetGeneratorByIdx(int32 idx) { return _generators[idx]; }
	void AddEventAfterTime(float time, QueueEvent* event);
	void AddEvent(QueueEvent* event);
	void Update(Room& room, float elapsedTime);
	void LateUpdate(float elapsedTime);
	void ClearQueuedEvents();
	GAMESTATE CheckGameState();

	bool _exitReady = false;
	OcTree& _collisionTree;
	std::array<SPlayer, PLAYERNUM> _players{};
	std::array<SGenerator, GENCNT> _generators{};
	GAMESTATE _state = GAMESTATE::NONE;
	uint64 _generation = 0;
	Rewinder<30> _history;
	std::unique_ptr<ClientEventScheduler> _jobQueue;
	uint32 _positionBroadcastPhase = 0;
};
