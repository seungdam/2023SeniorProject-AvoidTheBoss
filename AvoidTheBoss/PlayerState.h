#pragma once

#include <algorithm>
#include <cstdint>

enum class CLIENT_TYPE : std::uint8_t
{
	OWNER,
	OTHER_PLAYER,
	NONE
};

enum class PLAYER_TYPE : std::uint8_t
{
	NONE = 0,
	BOSS = 1,
	EMPLOYEE = 2
};

struct PlayerState
{
	static constexpr std::int32_t MaxHealth = 3;

	PLAYER_TYPE playerType = PLAYER_TYPE::NONE;
	CLIENT_TYPE clientType = CLIENT_TYPE::OTHER_PLAYER;
	std::int16_t sessionId = -1;
	std::int32_t playerIndex = -1;
	std::int32_t health = MaxHealth;
	std::int32_t behavior = 0;
	bool hidden = false;

	void SetHealth(const std::int32_t value) noexcept
	{
		health = std::clamp(value, std::int32_t{0}, MaxHealth);
	}

	[[nodiscard]] bool ApplyDamage() noexcept
	{
		if (health <= 0) return false;
		--health;
		return true;
	}

	void RestoreHealth() noexcept { health = MaxHealth; }

	void ResetTransient() noexcept
	{
		health = MaxHealth;
		behavior = 0;
		hidden = false;
	}
};
