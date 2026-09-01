#include "pch.h"
#include "SoundManager.h"
#include "CSound.h"
#include "SceneId.h"

CSound* SoundManager::m_pSound = nullptr;

SoundManager::SoundManager()
{
	m_pSound = new CSound();
	m_pSound->SoundSystem();
}

SoundManager::~SoundManager()
{
	delete m_pSound;
	m_pSound = nullptr;
}

void SoundManager::SoundSystem()
{
	m_pSound->SoundSystem();
}

void SoundManager::PlayBackGroundSound(int32 Scene)
{
	switch (Scene)
	{
	case atb::SceneIndex(atb::SceneId::Title):
	case atb::SceneIndex(atb::SceneId::Lobby):
	case atb::SceneIndex(atb::SceneId::Room):
		m_pSound->SoundStop(0);
		m_pSound->PlayBackGroundSound(0,0);
		SoundManager::GetInstance().SetVolum(0, 0.5f);
		break;
	case atb::SceneIndex(atb::SceneId::InGame):
		m_pSound->SoundStop(0);
		m_pSound->PlayBackGroundSound(1, 0);
		break;
	case atb::SceneIndex(atb::SceneId::Result):
		for (int i = 0; i < 3; i++)
		{
			m_pSound->SoundStop(8 + i);
		}
		break;
	}
}

void SoundManager::PlayObjectSound(int32 idx, int32 channel)
{
	m_pSound->PlaySound(idx, channel);
}

void SoundManager::SoundStop(int32 nChannel)
{
	m_pSound->SoundStop(nChannel);
}

void SoundManager::SoundPause(int32 nChannel)
{
	m_pSound->SoundPause(nChannel);
}

void SoundManager::SoundResume(int32 nChannel)
{
	m_pSound->SoundResume(nChannel);
}

void SoundManager::SoundRelease()
{
	m_pSound->SoundRelease();

}

void SoundManager::SetVolum(int nChannel, float volume)
{
	m_pSound->SetVolum(nChannel, volume);
}
