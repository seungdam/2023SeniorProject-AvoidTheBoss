#pragma once

#include <array>
#include <cstddef>
#include <optional>

inline constexpr std::size_t kGameUiPlayerCount = 4;

struct PlayerUiSnapshot
{
	int health = 0;
};

struct EmployeeUiSnapshot
{
	bool invincible = false;
	float uiCooldown = 0.0f;
	bool inGeneratorArea = false;
	bool generatorInteractionActive = false;
	std::optional<float> generatorGauge;
	bool rescueTargetAvailable = false;
	bool rescueInteractionActive = false;
	bool beingRescued = false;
	std::optional<float> rescueGauge;
};

struct GameUiSnapshot
{
	std::optional<std::size_t> localPlayerIndex;
	std::array<std::optional<PlayerUiSnapshot>, kGameUiPlayerCount> players;
	std::optional<EmployeeUiSnapshot> localEmployee;
};
