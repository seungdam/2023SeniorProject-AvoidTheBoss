#include "../Platform/pch.h"
#include "SoundManager.h"
#include "CSound.h"
#include "../Core/SceneId.h"

CSound* SoundManager::_pSound = nullptr;

SoundManager::SoundManager()
{
	_pSound = new CSound();
	_pSound->SoundSystem();
}

SoundManager::~SoundManager()
{
	delete _pSound;
	_pSound = nullptr;
}

void SoundManager::SoundSystem()
{
	_pSound->SoundSystem();
}

void SoundManager::PlayBackGroundSound(int32 Scene)
{
	switch (Scene)
	{
	case atb::SceneIndex(atb::SceneId::Title):
	case atb::SceneIndex(atb::SceneId::Lobby):
	case atb::SceneIndex(atb::SceneId::Room):
		_pSound->SoundStop(0);
		_pSound->PlayBackGroundSound(0,0);
		SoundManager::GetInstance().SetVolum(0, 0.5f);
		break;
	case atb::SceneIndex(atb::SceneId::InGame):
		_pSound->SoundStop(0);
		_pSound->PlayBackGroundSound(1, 0);
		break;
	case atb::SceneIndex(atb::SceneId::Result):
		for (int i = 0; i < 3; i++)
		{
			_pSound->SoundStop(8 + i);
		}
		break;
	}
}

void SoundManager::PlayObjectSound(int32 idx, int32 channel)
{
	_pSound->PlaySound(idx, channel);
}

void SoundManager::SoundStop(int32 nChannel)
{
	_pSound->SoundStop(nChannel);
}

void SoundManager::SoundPause(int32 nChannel)
{
	_pSound->SoundPause(nChannel);
}

void SoundManager::SoundResume(int32 nChannel)
{
	_pSound->SoundResume(nChannel);
}

void SoundManager::SoundRelease()
{
	_pSound->SoundRelease();

}

void SoundManager::SetVolum(int nChannel, float volume)
{
	_pSound->SetVolum(nChannel, volume);
}
