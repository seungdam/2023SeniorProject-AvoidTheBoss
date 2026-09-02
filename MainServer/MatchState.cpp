#include "pch.h"
#include "MatchState.h"
#include "FixedStepScheduler.h"
#include "JobQueue.h"

#include <cstdint>
#include <stdexcept>

namespace
{
	constexpr std::uint32_t PositionBroadcastRate = 45;
	static_assert(PositionBroadcastRate <= atb::FixedStepScheduler::TickRate);

	[[nodiscard]] bool AdvancePositionBroadcastPhase(std::uint32_t& phase) noexcept
	{
		phase += PositionBroadcastRate;
		if (phase < atb::FixedStepScheduler::TickRate) return false;
		phase -= atb::FixedStepScheduler::TickRate;
		return true;
	}
}

MatchState::MatchState(OcTree& collisionTree)
	: _collisionTree(collisionTree)
{
	_jobQueue = std::make_unique<ClientEventScheduler>();
}

MatchState::~MatchState()
{
	ClearQueuedEvents();
}

uint64 MatchState::Start(const std::array<int16, PLAYERNUM>& playerSids)
{
	for (int32 i = 0; i < PLAYERNUM; ++i)
	{
		_players[i].m_idx = i;
		_players[i].m_sid = playerSids[i];
	}

	_players[0].SetPosition(XMFLOAT3(0, 0, -18));
	_players[1].SetPosition(XMFLOAT3(10, 0, -18));
	_players[2].SetPosition(XMFLOAT3(15, 0, -18));
	_players[3].SetPosition(XMFLOAT3(20, 0, -18));

	_generators[0]._pos = XMFLOAT3(-23.12724f, 1.146619f, 1.814123f);
	_generators[1]._pos = XMFLOAT3(23.08867f, 1.083242f, 3.155997f);
	_generators[2]._pos = XMFLOAT3(0.6774719f, 1.083242f, -23.05909f);
	for (SGenerator& generator : _generators) generator.ResetState();

	_exitReady = false;
	if (++_generation == 0) ++_generation;
	_state = GAMESTATE::IN_GAME;
	_history.Clear();
	ClearQueuedEvents();
	_positionBroadcastPhase = 0;
	return _generation;
}

MatchTickResult MatchState::Tick(Room& room, const float fixedDeltaSeconds)
{
	Update(room, fixedDeltaSeconds);
	LateUpdate();

	MatchTickResult result{};
	result.state = _state;
	_history.AddHistory(_players.data());
	result.frame = _history.GetCurFrame();
	result.broadcastPositions = AdvancePositionBroadcastPhase(_positionBroadcastPhase);
	return result;
}

MatchPlayerDeparture MatchState::OnPlayerLeft(const int32 sid)
{
	SPlayer& player = GetPlayerBySid(sid);
	player.SetVelocity(XMFLOAT3(0, 0, 0));
	player.m_hide = true;
	player.SetBehavior(PLAYER_BEHAVIOR::CRAWL);

	MatchPlayerDeparture result{ player.m_idx, PlayerDepartureEffect::None };
	if (_state != GAMESTATE::IN_GAME) return result;
	if (player.m_idx == 0)
	{
		Reset();
		result.effect = PlayerDepartureEffect::EndMatch;
	}
	else
	{
		result.effect = PlayerDepartureEffect::HidePlayer;
	}
	return result;
}

void MatchState::PausePlayer(const int32 sid)
{
	GetPlayerBySid(sid).SetVelocity(XMFLOAT3(0, 0, 0));
}

void MatchState::RebindPlayer(const int32 sid, const int32 idx)
{
	_players[idx].m_idx = idx;
	_players[idx].m_sid = sid;
}

SPlayer& MatchState::GetPlayerBySid(const int32 sid)
{
	for (SPlayer& player : _players)
		if (sid == player.m_sid) return player;
	throw std::out_of_range("Player SID not found");
}

bool MatchState::IsCurrentGeneration(const uint64 generation) const noexcept
{
	return _state == GAMESTATE::IN_GAME && generation != 0 && generation == _generation;
}

void MatchState::Update(Room& room, const float elapsedTime)
{
	if (_state != GAMESTATE::IN_GAME) return;
	_jobQueue->DoTasks(room, *this);
	for (SPlayer& player : _players)
		if (!player.m_hide) player.Update(elapsedTime);
}

void MatchState::LateUpdate()
{
	if (_state != GAMESTATE::IN_GAME) return;
	int32 activeGeneratorCount = 0;
	for (SPlayer& player : _players)
		if (!player.m_hide) player.LateUpdate(_collisionTree);
	for (const SGenerator& generator : _generators)
		if (generator._IsActive) ++activeGeneratorCount;
	if (activeGeneratorCount >= GENCNT) _exitReady = true;
	_state = CheckGameState();
}

void MatchState::AddEventAfterTime(const float time, QueueEvent* event)
{
	_jobQueue->PushTask(event, time);
}

void MatchState::AddEvent(QueueEvent* event)
{
	_jobQueue->PushTask(event);
}

void MatchState::ClearQueuedEvents()
{
	_jobQueue->Clear();
}

GAMESTATE MatchState::CheckGameState()
{
	int32 crawlCount = 0;
	int32 escapeCount = 0;
	for (SPlayer& player : _players)
	{
		if (static_cast<int32>(PLAYER_BEHAVIOR::CRAWL) == player.GetBehavior()) ++crawlCount;
		if (player.GetEscaped()) ++escapeCount;
	}
	if (PLAYERNUM - 1 == crawlCount) _state = GAMESTATE::BOSS_WIN;
	else if (crawlCount + escapeCount == PLAYERNUM - 1 && _exitReady) _state = GAMESTATE::EMP_WIN;
	return _state;
}

void MatchState::Reset()
{
	ClearQueuedEvents();
	_history.Clear();
	for (SPlayer& player : _players) player.ResetState();
	for (SGenerator& generator : _generators) generator.ResetState();
	_players[0].SetPosition(XMFLOAT3(0, 0.25f, -18));
	_players[1].SetPosition(XMFLOAT3(10, 0.25f, -18));
	_players[2].SetPosition(XMFLOAT3(15, 0.25f, -18));
	_players[3].SetPosition(XMFLOAT3(20, 0.25f, -18));
	_exitReady = false;
	_positionBroadcastPhase = 0;
	_state = GAMESTATE::NONE;
}
