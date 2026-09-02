#pragma once

#include "../Shared/Protocol.h"

#include <DirectXMath.h>
#include <variant>

class CGameScene;

struct moveEvent
{
	int32 playerIndex = -1;
	uint8 key = 0;
	DirectX::XMFLOAT3 direction{ 0.0f, 0.0f, 0.0f };

	void Task(CGameScene* scene);
};

struct posEvent
{
	int32 playerIndex = -1;
	DirectX::XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };

	void Task(CGameScene* scene);
};

struct rotateEvent
{
	int32 playerIndex = -1;
	float angle = 0.0f;

	void Task(CGameScene* scene);
};

struct animationEvent
{
	int32 playerIndex = -1;
	uint8 track = 0;

	void Task(CGameScene* scene);
};

struct InteractionEvent
{
	uint8 eventId = static_cast<uint8>(-1);

	void Task(CGameScene* scene);
};

struct FrameEvent
{
	int32 worldFrame = -1;

	void Task(CGameScene* scene);
};

struct DelayEvent
{
	C2S_ATTACK packet{};

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
