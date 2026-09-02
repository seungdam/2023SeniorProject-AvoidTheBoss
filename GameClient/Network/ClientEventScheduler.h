#pragma once

#include "ClientPacketEvent.h"

#include <chrono>
#include <cstdint>
#include <queue>
#include <vector>

class CGameScene;

struct ScheduledClientEvent
{
	int64 _executeAt = 0;
	std::uint64_t _sequence = 0;
	ClientEvent _event;
};

struct JobComparator
{
	[[nodiscard]] static constexpr bool RunsAfter(
		const int64 lhsTime, const std::uint64_t lhsSequence,
		const int64 rhsTime, const std::uint64_t rhsSequence) noexcept
	{
		return lhsTime != rhsTime ? lhsTime > rhsTime : lhsSequence > rhsSequence;
	}

	bool operator()(const ScheduledClientEvent& lhs, const ScheduledClientEvent& rhs) const noexcept
	{
		return RunsAfter(lhs._executeAt, lhs._sequence, rhs._executeAt, rhs._sequence);
	}
};

static_assert(JobComparator::RunsAfter(10, 2, 10, 1));
static_assert(!JobComparator::RunsAfter(10, 1, 10, 2));

using JobPriorityQueue = std::priority_queue<
	ScheduledClientEvent,
	std::vector<ScheduledClientEvent>,
	JobComparator>;

class ClientEventScheduler
{
public:
	using Clock = std::chrono::steady_clock;

	explicit ClientEventScheduler(CGameScene* ownerScene = nullptr);
	void Reset() noexcept;
	void PushTask(ClientEvent event, float afterMilliseconds);
	void DoTasks();
	void Clear() noexcept;

	[[nodiscard]] int64 GetCurrentTick() const noexcept
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(
			Clock::now() - _beginTickPoint).count();
	}

private:
	JobPriorityQueue _taskQueue;
	Clock::time_point _beginTickPoint;
	std::uint64_t _nextSequence = 0;
	CGameScene* _ownerScene = nullptr;
};
