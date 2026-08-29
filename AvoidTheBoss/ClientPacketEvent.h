#pragma once

#include "clientIocpCore.h"

#include <variant>

struct moveEvent
{
	int32 _playerIndex = -1;
	uint8 _key = 0;
	XMFLOAT3 _direction{ 0.0f, 0.0f, 0.0f };

	void Task();
};

struct posEvent
{
	int32 _playerIndex = -1;
	XMFLOAT3 _position{ 0.0f, 0.0f, 0.0f };

	void Task();
};

struct rotateEvent
{
	int32 _playerIndex = -1;
	float _angle = 0.0f;

	void Task();
};

struct animationEvent
{
	int32 _playerIndex = -1;
	uint8 _track = 0;

	void Task();
};

struct InteractionEvent
{
	uint8 _eventId = static_cast<uint8>(-1);

	void Task();
};

struct FrameEvent
{
	int32 _worldFrame = -1;

	void Task();
};

struct DelayEvent
{
	C2S_ATTACK _packet{};

	void Task();
};

using ClientEvent = std::variant<
	moveEvent,
	posEvent,
	rotateEvent,
	animationEvent,
	InteractionEvent,
	FrameEvent,
	DelayEvent>;
