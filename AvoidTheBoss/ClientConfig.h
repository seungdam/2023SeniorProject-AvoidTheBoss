#pragma once

namespace atb::client::config
{
inline constexpr int DefaultWindowWidth = 1920;
inline constexpr int DefaultWindowHeight = 1080;
}

namespace atb::client::ui
{
inline constexpr float CenterX = config::DefaultWindowWidth / 2.0f;
inline constexpr float CenterY = config::DefaultWindowHeight / 2.0f;
inline constexpr float TitleButtonXOffset = config::DefaultWindowWidth / 8.0f;
inline constexpr float TitleButtonYOffset = config::DefaultWindowHeight / 10.0f;

inline constexpr float LobbyButtonXOffset = config::DefaultWindowWidth / 3.0f;
inline constexpr float LobbyButtonYOffset = config::DefaultWindowHeight / 4.0f * 3.0f;
inline constexpr float LobbyRoomListXOffset = config::DefaultWindowWidth / 23.0f;
inline constexpr float LobbyRoomListRightOffset = config::DefaultWindowWidth / 18.0f;
inline constexpr float LobbyRoomListYOffset = config::DefaultWindowHeight / 23.0f;

inline constexpr float MaxRescueGaugeWidth = 960.0f / 4.0f;
inline constexpr float ProfileOffsetX = config::DefaultWindowWidth * 0.01f;
inline constexpr float ProfileOffsetY = config::DefaultWindowHeight * 0.1f;
inline constexpr float ProfileWidth = config::DefaultWindowWidth * 0.1f;
inline constexpr float ProfileHeight = config::DefaultWindowHeight * 0.1f;
inline constexpr float LargeProfileOffsetY = ProfileOffsetY * 7.0f;
inline constexpr float LargeProfileWidth = config::DefaultWindowWidth * 0.2f;
inline constexpr float LargeProfileHeight = config::DefaultWindowHeight * 0.2f;
inline constexpr float StatusWidth = ProfileWidth * 0.8f;
inline constexpr float StatusHeight = ProfileHeight * 0.8f;
inline constexpr float GameRoomButtonXOffset = config::DefaultWindowWidth / 1.8f;
inline constexpr float GameRoomButtonYOffset = config::DefaultWindowHeight / 1.2f;
inline constexpr float FontSize = 50.0f;
}

enum Layout
{
	PLAYER, MAP, BOUDS, BULLET, SWITCH, SIREN, DOOR, GENERATOR, SOUND, EFFECT
};
