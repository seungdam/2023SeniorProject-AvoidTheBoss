#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ratio>

namespace atb
{
	class FixedStepScheduler final
	{
	public:
		using Clock = std::chrono::steady_clock;
		using TimePoint = Clock::time_point;

		static constexpr std::uint32_t TickRate = 60;
		static constexpr float FixedDeltaSeconds = 1.0f / static_cast<float>(TickRate);
		static constexpr std::size_t MaxCatchUpSteps = 4;

		FixedStepScheduler() noexcept : FixedStepScheduler(Clock::now()) {}
		explicit FixedStepScheduler(const TimePoint epoch) noexcept : _epoch(epoch) {}

		void Reset(const TimePoint epoch = Clock::now()) noexcept
		{
			_epoch = epoch;
			_nextTick = 1;
		}

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
				Reset(now);
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
