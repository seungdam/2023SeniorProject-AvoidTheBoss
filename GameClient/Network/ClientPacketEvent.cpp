#include "../Platform/pch.h"
#include "../ClientPacketEvent.h"

#include "../Scenes/GameScene.h"

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
	{
		scene->ApplyPlayerMove(playerIndex, key, direction);
	}
}

void posEvent::Task(CGameScene* scene)
{
	if (CanApplyTo(scene))
	{
		scene->ApplyPlayerPosition(playerIndex, position);
	}
}

void rotateEvent::Task(CGameScene* scene)
{
	if (CanApplyTo(scene))
	{
		scene->ApplyPlayerRotation(playerIndex, angle);
	}
}

void animationEvent::Task(CGameScene* scene)
{
	if (CanApplyTo(scene))
	{
		scene->ApplyPlayerAnimation(playerIndex, track);
	}
}

void InteractionEvent::Task(CGameScene* scene)
{
	if (CanApplyTo(scene))
	{
		scene->ApplyInteraction(eventId);
	}
}

void FrameEvent::Task(CGameScene* scene)
{
	if (CanApplyTo(scene))
	{
		scene->ApplyWorldFrame(worldFrame);
	}
}

void DelayEvent::Task(CGameScene* scene)
{
	if (scene)
	{
		scene->SendPacket(&packet);
	}
}
