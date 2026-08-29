#pragma once

inline constexpr int FRAME_BUFFER_WIDTH = 1920;
inline constexpr int FRAME_BUFFER_HEIGHT = 1080;

inline constexpr float CENTER_X = FRAME_BUFFER_WIDTH / 2.0f;
inline constexpr float CENTER_Y = FRAME_BUFFER_HEIGHT / 2.0f;
inline constexpr float TITLEBUTTON_X_OFFSET = FRAME_BUFFER_WIDTH / 8.0f;
inline constexpr float TITLEBUTTON_Y_OFFSET = FRAME_BUFFER_HEIGHT / 10.0f;

inline constexpr float LOBBYBUTTON_X_OFFSET = FRAME_BUFFER_WIDTH / 3.0f;
inline constexpr float LOBBYBUTTON_Y_OFFSET = FRAME_BUFFER_HEIGHT / 4.0f * 3.0f;
inline constexpr float LOBBYROOMLIST_X_OFFSET = FRAME_BUFFER_WIDTH / 23.0f;
inline constexpr float LOBBYROOMLIST_X_OFFSET2 = FRAME_BUFFER_WIDTH / 18.0f;
inline constexpr float LOBBYROOMLIST_Y_OFFSET = FRAME_BUFFER_HEIGHT / 23.0f;

inline constexpr float MAX_RESCUE_GUAGE = 960.0f / 4.0f;
inline constexpr float PROFILE_UI_OFFSET_X = FRAME_BUFFER_WIDTH * 0.01f;
inline constexpr float PROFILE_UI_OFFSET_Y = FRAME_BUFFER_HEIGHT * 0.1f;
inline constexpr float PROFILE_UI_WIDTH = FRAME_BUFFER_WIDTH * 0.1f;
inline constexpr float PROFILE_UI_HEIGHT = FRAME_BUFFER_HEIGHT * 0.1f;
inline constexpr float BIG_PROFILE_UI_OFFSET_Y = PROFILE_UI_OFFSET_Y * 7.0f;
inline constexpr float BIG_PROFILE_UI_WIDTH = FRAME_BUFFER_WIDTH * 0.2f;
inline constexpr float BIG_PROFILE_UI_HEIGHT = FRAME_BUFFER_HEIGHT * 0.2f;
inline constexpr float STATUS_UI_WIDTH = PROFILE_UI_WIDTH * 0.8f;
inline constexpr float STATUS_UI_HEIGHT = PROFILE_UI_HEIGHT * 0.8f;
inline constexpr float GAMEROOM_BUTTON_X_OFFSET = FRAME_BUFFER_WIDTH / 1.8f;
inline constexpr float GAMEROOM_BUTTON_Y_OFFSET = FRAME_BUFFER_HEIGHT / 1.2f;
inline constexpr float FontSize = 50.0f;
inline constexpr float IDPW_Y_OFFSET = FontSize / 2.0f;

inline constexpr int ANIMATION_TYPE_ONCE = 0;
inline constexpr int ANIMATION_TYPE_LOOP = 1;
inline constexpr int ANIMATION_TYPE_PINGPONG = 2;
inline constexpr float ANIMATION_CALLBACK_EPSILON = 0.0165f;

enum Layout
{
	PLAYER, MAP, BOUDS, BULLET, SWITCH, SIREN, DOOR, GENERATOR, SOUND, EFFECT
};
