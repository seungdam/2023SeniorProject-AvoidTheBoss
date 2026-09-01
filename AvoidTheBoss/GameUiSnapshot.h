#pragma once

#include <array>
#include <cstddef>
#include <optional>

inline constexpr std::size_t kGameUiPlayerCount = 4;
inline constexpr std::size_t kLobbyUiRoomCount = 5;

struct LobbyRoomUiSnapshot
{
	int roomNumber = 0;
	int memberCount = 0;
};

struct LobbyUiSnapshot
{
	std::array<std::optional<LobbyRoomUiSnapshot>, kLobbyUiRoomCount> rooms;
};

struct RoomMemberUiSnapshot
{
	bool occupied = false;
	bool ready = false;
};

struct RoomUiSnapshot
{
	std::array<RoomMemberUiSnapshot, kGameUiPlayerCount> members;
};

struct ResultUiSnapshot
{
	bool bossWon = false;
};

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
