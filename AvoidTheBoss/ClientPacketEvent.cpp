#include "pch.h"
#include "ClientPacketEvent.h"

#include "GameScene.h"

namespace
{
	bool CanApplyTo(CGameScene* scene) noexcept
	{
		return scene && scene->IsActive();
	}
}

void moveEvent::Task(CGameScene* scene)
{
	if (CanApplyTo(scene))
		scene->ApplyPlayerMove(_playerIndex, _key, _direction);
}

void posEvent::Task(CGameScene* scene)
{
	if (CanApplyTo(scene))
		scene->ApplyPlayerPosition(_playerIndex, _position);
}

void rotateEvent::Task(CGameScene* scene)
{
	if (CanApplyTo(scene))
		scene->ApplyPlayerRotation(_playerIndex, _angle);
}

void animationEvent::Task(CGameScene* scene)
{
	if (CanApplyTo(scene))
		scene->ApplyPlayerAnimation(_playerIndex, _track);
}

void InteractionEvent::Task(CGameScene* scene)
{
	if (CanApplyTo(scene))
		scene->ApplyInteraction(_eventId);
}

void FrameEvent::Task(CGameScene* scene)
{
	if (CanApplyTo(scene))
		scene->ApplyWorldFrame(_worldFrame);
}

void DelayEvent::Task(CGameScene* scene)
{
	if (scene) scene->SendPacket(&_packet);
}
