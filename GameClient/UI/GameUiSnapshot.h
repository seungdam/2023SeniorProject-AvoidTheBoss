#pragma once

#include <array>
#include <cstddef>
#include <optional>

namespace atb::client::ui
{
inline constexpr std::size_t GamePlayerCount = 4;
inline constexpr std::size_t LobbyRoomSlotCount = 5;
}

struct LobbyRoomUiSnapshot
{
	int roomNumber = 0;
	int memberCount = 0;
};

struct LobbyUiSnapshot
{
	std::array<std::optional<LobbyRoomUiSnapshot>, atb::client::ui::LobbyRoomSlotCount> rooms;
};

struct RoomMemberUiSnapshot
{
	bool occupied = false;
	bool ready = false;
};

struct RoomUiSnapshot
{
	std::array<RoomMemberUiSnapshot, atb::client::ui::GamePlayerCount> members;
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
	std::array<std::optional<PlayerUiSnapshot>, atb::client::ui::GamePlayerCount> players;
	std::optional<EmployeeUiSnapshot> localEmployee;
};
