#pragma once

#include <cmath>
#include <cstdint>

enum class GeneratorPhase : std::uint8_t
{
	Idle,
	Interacting,
	Activated,
};

enum class GeneratorTransition : std::uint8_t
{
	None,
	Activated,
};

struct GeneratorState
{
	std::int32_t index = -1;
	GeneratorPhase phase = GeneratorPhase::Idle;
	float progress = 0.0f;
	float maxProgress = 100.0f;
	float progressPerSecond = 10.0f;
	bool advancesProgress = false;

	[[nodiscard]] bool IsAvailable() const noexcept
	{
		return phase == GeneratorPhase::Idle;
	}

	[[nodiscard]] bool IsAnimating() const noexcept
	{
		return phase != GeneratorPhase::Idle;
	}

	[[nodiscard]] bool IsActivated() const noexcept
	{
		return phase == GeneratorPhase::Activated;
	}

	[[nodiscard]] bool BeginInteraction(bool shouldAdvanceProgress) noexcept
	{
		if (phase == GeneratorPhase::Activated)
		{
			return false;
		}

		const bool changed = phase == GeneratorPhase::Idle;
		phase = GeneratorPhase::Interacting;
		advancesProgress = advancesProgress || shouldAdvanceProgress;
		return changed;
	}

	[[nodiscard]] bool EndInteraction() noexcept
	{
		if (phase != GeneratorPhase::Interacting)
		{
			return false;
		}

		phase = GeneratorPhase::Idle;
		advancesProgress = false;
		return true;
	}

	[[nodiscard]] bool Activate() noexcept
	{
		if (phase == GeneratorPhase::Activated)
		{
			return false;
		}

		progress = maxProgress;
		phase = GeneratorPhase::Activated;
		advancesProgress = false;
		return true;
	}

	[[nodiscard]] GeneratorTransition Tick(float deltaSeconds) noexcept
	{
		if (phase != GeneratorPhase::Interacting || !advancesProgress ||
			!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f ||
			!std::isfinite(progressPerSecond) || progressPerSecond <= 0.0f ||
			!std::isfinite(maxProgress) || maxProgress <= 0.0f) {
			return GeneratorTransition::None;
}

		progress += progressPerSecond * deltaSeconds;
		if (progress < maxProgress)
		{
			return GeneratorTransition::None;
		}

		(void)Activate();
		return GeneratorTransition::Activated;
	}

	void Reset() noexcept
	{
		phase = GeneratorPhase::Idle;
		progress = 0.0f;
		advancesProgress = false;
	}
};
