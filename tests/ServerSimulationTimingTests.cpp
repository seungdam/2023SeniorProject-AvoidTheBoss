#include "pch.h"
#include "CollisionDetector.h"
#include "FixedStepScheduler.h"
#include "MatchState.h"
#include "RoomManager.h"
#include "ServerSessionManager.h"

#include <cmath>
#include <ratio>

static_assert(ServerSimulation::TickRate == 60);
static_assert(ServerSimulation::FixedDeltaSeconds == 1.0f / 60.0f);
static_assert(ServerSimulation::PositionBroadcastRate == 45);
static_assert(ServerSimulation::MaxCatchUpSteps == 4);

namespace
{
	using Scheduler = ServerSimulation::FixedStepScheduler;
	using Clock = Scheduler::Clock;
	using TickDuration = std::chrono::duration<std::int64_t,
		std::ratio<1, ServerSimulation::TickRate>>;

	bool Check(const bool condition, const char* message)
	{
		if (condition) return true;
		std::cerr << message << '\n';
		return false;
	}

	Clock::time_point AtTick(const Clock::time_point epoch, const std::int64_t tick)
	{
		return epoch + std::chrono::duration_cast<Clock::duration>(TickDuration{ tick });
	}

	std::size_t CountStepsAtWakeRate(const int wakeRate, const int seconds)
	{
		const Clock::time_point epoch{};
		Scheduler scheduler(epoch);
		std::size_t steps = 0;
		const int wakeCount = wakeRate * seconds;
		for (int wake = 1; wake <= wakeCount; ++wake)
		{
			const auto elapsed = std::chrono::duration<double>(
				static_cast<double>(wake) / static_cast<double>(wakeRate));
			steps += scheduler.ConsumeDueSteps(
				epoch + std::chrono::duration_cast<Clock::duration>(elapsed));
		}
		return steps;
	}
}

int main()
{
	const Clock::time_point epoch{};
	Scheduler scheduler(epoch);
	const Clock::time_point firstDeadline = scheduler.NextDeadline();
	if (!Check(scheduler.ConsumeDueSteps(firstDeadline - Clock::duration{ 1 }) == 0,
		"the scheduler produced a step before the first deadline") ||
		!Check(scheduler.ConsumeDueSteps(firstDeadline) == 1,
			"the scheduler did not produce one step at the first deadline")) return 1;

	for (const int wakeRate : { 30, 60, 144, 240 })
	{
		if (!Check(CountStepsAtWakeRate(wakeRate, 1) == ServerSimulation::TickRate,
			"one second did not produce exactly 60 simulation steps")) return 1;
	}
	if (!Check(CountStepsAtWakeRate(240, 10) == ServerSimulation::TickRate * 10,
		"ten seconds accumulated simulation drift")) return 1;

	Scheduler irregular(epoch);
	if (!Check(irregular.ConsumeDueSteps(AtTick(epoch, 4)) == 4,
		"an irregular wake did not catch up every due step within the limit")) return 1;

	Scheduler stalled(epoch);
	const Clock::time_point stalledAt = epoch + std::chrono::milliseconds(500);
	if (!Check(stalled.ConsumeDueSteps(stalledAt) == ServerSimulation::MaxCatchUpSteps,
		"a long stall exceeded or missed the catch-up limit") ||
		!Check(stalled.NextDeadline() > stalledAt,
			"a long stall left the next deadline in the past") ||
		!Check(stalled.ConsumeDueSteps(stalledAt) == 0,
			"discarded backlog leaked into the next scheduler poll") ||
		!Check(stalled.ConsumeDueSteps(stalled.NextDeadline()) == 1,
			"the scheduler did not resume one step after backlog resynchronization")) return 1;

	std::uint32_t positionPhase = 0;
	std::size_t broadcasts = 0;
	for (std::uint32_t tick = 0; tick < ServerSimulation::TickRate; ++tick)
		broadcasts += ServerSimulation::AdvancePositionBroadcastPhase(positionPhase) ? 1 : 0;
	if (!Check(broadcasts == ServerSimulation::PositionBroadcastRate && positionPhase == 0,
		"the position cadence did not produce exactly 45 broadcasts per 60 ticks")) return 1;

	positionPhase = 0;
	broadcasts = 0;
	for (std::uint32_t tick = 0; tick < ServerSimulation::TickRate; ++tick)
		broadcasts += ServerSimulation::AdvancePositionBroadcastPhase(positionPhase) ? 1 : 0;
	if (!Check(broadcasts == ServerSimulation::PositionBroadcastRate && positionPhase == 0,
		"the reset position cadence retained state from the previous match")) return 1;

	WorldStatus emptyWorld;
	bool emptyPositions = true;
	for (const POS& position : emptyWorld._pPos)
		emptyPositions = emptyPositions && position.x == 0.0f && position.z == 0.0f;
	if (!Check(emptyPositions && emptyWorld._myWorldFrame == 0 && emptyWorld._bossDir.x == 0.0f &&
		emptyWorld._bossDir.y == 0.0f && emptyWorld._bossDir.z == 1.0f,
		"the rewind frame-zero snapshot was not initialized canonically")) return 1;

	LobbyCommandQueue lobbyCommands(1);
	GameCommandQueue gameCommands(1);
	ServerSessionManager sessions(lobbyCommands, gameCommands);
	OcTree collisionTree(XMFLOAT3(0.f, 0.f, 0.f), 60.f);
	collisionTree.BuildTree();
	SPlayer sixtyHzPlayer;
	SPlayer thirtyHzPlayer;
	const XMFLOAT3 initialPosition{ 1.0f, 0.25f, 2.0f };
	const XMFLOAT3 velocity{ 6.0f, 0.0f, -3.0f };
	sixtyHzPlayer.SetPosition(initialPosition);
	sixtyHzPlayer.SetVelocity(velocity);
	thirtyHzPlayer.SetPosition(initialPosition);
	thirtyHzPlayer.SetVelocity(velocity);
	sixtyHzPlayer.Update(1.0f / 60.0f, collisionTree);
	sixtyHzPlayer.Update(1.0f / 60.0f, collisionTree);
	thirtyHzPlayer.Update(1.0f / 30.0f, collisionTree);
	const XMFLOAT3 sixtyHzPosition = sixtyHzPlayer.GetPosition();
	const XMFLOAT3 thirtyHzPosition = thirtyHzPlayer.GetPosition();
	const XMFLOAT3 expectedPosition{ 1.2f, 0.25f, 1.9f };
	constexpr float PositionTolerance = 1.0e-5f;
	const auto IsNear = [=](const float lhs, const float rhs)
	{
		return std::fabs(lhs - rhs) <= PositionTolerance;
	};
	if (!Check(IsNear(sixtyHzPosition.x, thirtyHzPosition.x) &&
		IsNear(sixtyHzPosition.z, thirtyHzPosition.z) &&
		IsNear(sixtyHzPosition.x, expectedPosition.x) &&
		IsNear(sixtyHzPosition.y, expectedPosition.y) &&
		IsNear(sixtyHzPosition.z, expectedPosition.z) &&
		IsNear(thirtyHzPosition.x, expectedPosition.x) &&
		IsNear(thirtyHzPosition.y, expectedPosition.y) &&
		IsNear(thirtyHzPosition.z, expectedPosition.z),
		"fixed-delta player integration changed with equivalent step durations")) return 1;

	Room room(sessions, collisionTree, 0);
	MatchState match(collisionTree);
	const std::array<int16, PLAYERNUM> playerSids{ 1, 2, 3, 4 };

	const auto CheckMatchCadence = [&]()
	{
		(void)match.Start(playerSids);
		std::size_t matchBroadcasts = 0;
		for (std::uint32_t tick = 1; tick <= ServerSimulation::TickRate; ++tick)
		{
			const MatchTickResult result = match.Tick(room, ServerSimulation::FixedDeltaSeconds);
			if (result.frame != static_cast<int32>(tick)) return false;
			matchBroadcasts += result.broadcastPositions ? 1 : 0;
		}
		return matchBroadcasts == ServerSimulation::PositionBroadcastRate;
	};

	if (!Check(CheckMatchCadence(),
		"MatchState did not produce 60 history frames and 45 broadcasts")) return 1;
	match.Reset();
	if (!Check(CheckMatchCadence(),
		"MatchState retained history or broadcast phase across reset/start")) return 1;

	match.Reset();
	(void)match.Start(playerSids);
	const MatchTickResult partialTick = match.Tick(room, ServerSimulation::FixedDeltaSeconds);
	if (!Check(partialTick.frame == 1 && !partialTick.broadcastPositions,
		"MatchState did not enter the expected partial broadcast phase")) return 1;

	match.Reset();
	(void)match.Start(playerSids);
	constexpr std::array<bool, 8> ExpectedBroadcastPattern{
		false, true, true, true, false, true, true, true
	};
	for (std::size_t tick = 0; tick < ExpectedBroadcastPattern.size(); ++tick)
	{
		const MatchTickResult result = match.Tick(room, ServerSimulation::FixedDeltaSeconds);
		if (!Check(result.frame == static_cast<int32>(tick + 1) &&
			result.broadcastPositions == ExpectedBroadcastPattern[tick],
			"MatchState did not reset to the exact 45/60 broadcast phase pattern")) return 1;
	}

	return 0;
}
