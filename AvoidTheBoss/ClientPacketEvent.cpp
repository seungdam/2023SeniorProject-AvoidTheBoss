#include "pch.h"
#include "ClientPacketEvent.h"

#include "clientIocpCore.h"
#include "GameFramework.h"
#include "GameScene.h"
#include "SceneManager.h"

namespace
{
	CGameScene* GetGameScene() noexcept
	{
		if (mainGame.m_curScene != static_cast<int32>(CGameFramework::SCENESTATE::INGAME))
		{
			return nullptr;
		}

		return static_cast<CGameScene*>(mainGame.GetSceneByIdx(
			static_cast<int32>(CGameFramework::SCENESTATE::INGAME)));
	}
}

void moveEvent::Task()
{
	if (CGameScene* scene = GetGameScene())
		scene->ApplyPlayerMove(_playerIndex, _key, _direction);
}

void posEvent::Task()
{
	if (CGameScene* scene = GetGameScene())
		scene->ApplyPlayerPosition(_playerIndex, _position);
}

void rotateEvent::Task()
{
	if (CGameScene* scene = GetGameScene())
		scene->ApplyPlayerRotation(_playerIndex, _angle);
}

void animationEvent::Task()
{
	if (CGameScene* scene = GetGameScene())
		scene->ApplyPlayerAnimation(_playerIndex, _track);
}

void InteractionEvent::Task()
{
	if (CGameScene* scene = GetGameScene())
		scene->ApplyInteraction(_eventId);
}

void FrameEvent::Task()
{
	if (CGameScene* scene = GetGameScene())
		scene->ApplyWorldFrame(_worldFrame);
}

void DelayEvent::Task()
{
	clientCore.DoSend(&_packet);
}
