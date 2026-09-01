#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ratio>

namespace ServerSimulation
{
	inline constexpr std::uint32_t TickRate = 60;
	inline constexpr float FixedDeltaSeconds = 1.0f / static_cast<float>(TickRate);
	inline constexpr std::uint32_t PositionBroadcastRate = 45;
	inline constexpr std::size_t MaxCatchUpSteps = 4;

	static_assert(PositionBroadcastRate <= TickRate);

	[[nodiscard]] inline bool AdvancePositionBroadcastPhase(std::uint32_t& phase) noexcept
	{
		phase += PositionBroadcastRate;
		if (phase < TickRate) return false;
		phase -= TickRate;
		return true;
	}

	class FixedStepScheduler
	{
	public:
		using Clock = std::chrono::steady_clock;
		using TimePoint = Clock::time_point;

		explicit FixedStepScheduler(const TimePoint epoch) noexcept : _epoch(epoch) {}

		[[nodiscard]] std::size_t ConsumeDueSteps(const TimePoint now) noexcept
		{
			std::size_t steps = 0;
			while (steps < MaxCatchUpSteps && now >= NextDeadline())
			{
				++_nextTick;
				++steps;
			}

			if (now >= NextDeadline())
			{
				_epoch = now;
				_nextTick = 1;
			}
			return steps;
		}

		[[nodiscard]] TimePoint NextDeadline() const noexcept
		{
			return _epoch + std::chrono::duration_cast<Clock::duration>(TickDuration{ _nextTick });
		}

	private:
		using TickDuration = std::chrono::duration<std::int64_t, std::ratio<1, TickRate>>;

		TimePoint _epoch;
		std::int64_t _nextTick = 1;
	};
}
