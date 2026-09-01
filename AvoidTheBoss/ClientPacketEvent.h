#pragma once

#include "../Shared/Protocol.h"

#include <DirectXMath.h>
#include <variant>

class CGameScene;

struct moveEvent
{
	int32 _playerIndex = -1;
	uint8 _key = 0;
	DirectX::XMFLOAT3 _direction{ 0.0f, 0.0f, 0.0f };

	void Task(CGameScene* scene);
};

struct posEvent
{
	int32 _playerIndex = -1;
	DirectX::XMFLOAT3 _position{ 0.0f, 0.0f, 0.0f };

	void Task(CGameScene* scene);
};

struct rotateEvent
{
	int32 _playerIndex = -1;
	float _angle = 0.0f;

	void Task(CGameScene* scene);
};

struct animationEvent
{
	int32 _playerIndex = -1;
	uint8 _track = 0;

	void Task(CGameScene* scene);
};

struct InteractionEvent
{
	uint8 _eventId = static_cast<uint8>(-1);

	void Task(CGameScene* scene);
};

struct FrameEvent
{
	int32 _worldFrame = -1;

	void Task(CGameScene* scene);
};

struct DelayEvent
{
	C2S_ATTACK _packet{};

	void Task(CGameScene* scene);
};

using ClientEvent = std::variant<
	moveEvent,
	posEvent,
	rotateEvent,
	animationEvent,
	InteractionEvent,
	FrameEvent,
	DelayEvent>;
