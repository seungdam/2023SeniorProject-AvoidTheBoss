#include "../Platform/pch.h"
#include "CBoss.h"
#include "CEmployee.h"
#include "CBullet.h"

#include "InputManager.h"
#include "../Audio/SoundManager.h"
#include "../Scenes/GameScene.h"

CBoss::CBoss(
	ID3D12Device5* pd3dDevice,
	ID3D12GraphicsCommandList4* pd3dCommandList,
	ID3D12RootSignature* pd3dGraphicsRootSignature,
	CGameScene& ownerScene)
	: _ownerScene(ownerScene)
{
	_type = 0;

	_state.playerType = PLAYER_TYPE::BOSS;
	_characterType = CHARACTER_TYPE::BOSS;

		CLoadedModelInfo* pBossArmModel = CGameObject::LoadGeometryAndAnimationFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, "Model/Character/Boss_Idle_First.bin", NULL, Layout::PLAYER);
		SetChild(pBossArmModel->m_pModelRootObject, true);
		m_pSkinnedAnimationController2 = new CAnimationController(pd3dDevice, pd3dCommandList, 4, pBossArmModel);

		m_pSkinnedAnimationController2->SetTrackAnimationSet(0, 0);//Idle
		m_pSkinnedAnimationController2->SetTrackAnimationSet(1, 1);//Run
		m_pSkinnedAnimationController2->SetTrackAnimationSet(2, 2);//Shoot
		m_pSkinnedAnimationController2->SetTrackAnimationSet(3, 3);//RunningShoot

		CLoadedModelInfo* pBossUpperModel = CGameObject::LoadGeometryAndAnimationFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, "Model/Character/Boss_Shooting_Run_UpperBody.bin", NULL, Layout::PLAYER);
		SetChild(pBossUpperModel->m_pModelRootObject, true);
		CLoadedModelInfo* pBossLowerModel = CGameObject::LoadGeometryAndAnimationFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, "Model/Character/Boss_Shooting_Run_LowerBody.bin", NULL, Layout::PLAYER);
		SetChild(pBossLowerModel->m_pModelRootObject, true);
		m_pSkinnedAnimationController =  new CAnimationController(pd3dDevice, pd3dCommandList, 4, pBossUpperModel);
		m_pSkinnedAnimationController1 = new CAnimationController(pd3dDevice, pd3dCommandList, 4, pBossLowerModel);

		m_pSkinnedAnimationController->SetTrackAnimationSet(0, 1);//Idle
		m_pSkinnedAnimationController->SetTrackAnimationSet(1, 2);//Run
		m_pSkinnedAnimationController->SetTrackAnimationSet(2, 3);//Shoot 2
		m_pSkinnedAnimationController->SetTrackAnimationSet(3, 0);//RunningShoot

		m_pSkinnedAnimationController1->SetTrackAnimationSet(0, 1);//Idle
		m_pSkinnedAnimationController1->SetTrackAnimationSet(1, 0);//Run
		m_pSkinnedAnimationController1->SetTrackAnimationSet(2, 2);//Run
		m_pSkinnedAnimationController1->SetTrackAnimationSet(3, 3);//Run

	m_pSkinnedAnimationController2->SetTrackEnable(0, true);
	m_pSkinnedAnimationController2->SetTrackEnable(1, false);
	m_pSkinnedAnimationController2->SetTrackEnable(2, false);
	m_pSkinnedAnimationController2->SetTrackEnable(3, false);

	m_pSkinnedAnimationController->SetTrackEnable(0, false);
	m_pSkinnedAnimationController->SetTrackEnable(1, false);
	m_pSkinnedAnimationController->SetTrackEnable(2, false);
	m_pSkinnedAnimationController->SetTrackEnable(3, false);

	m_pSkinnedAnimationController1->SetTrackEnable(0, false);
	m_pSkinnedAnimationController1->SetTrackEnable(1, false);
	m_pSkinnedAnimationController1->SetTrackEnable(2, false);
	m_pSkinnedAnimationController1->SetTrackEnable(3, false);

	if (pBossArmModel)
	{
		delete pBossArmModel;
	}
	if (pBossUpperModel)
	{
		delete pBossUpperModel;
	}
	if (pBossLowerModel)
	{
		delete pBossLowerModel;
	}
}

CBoss::~CBoss()
{
}

void CBoss::Rotate(float x, float y, float z)
{
	CPlayer::Rotate(x, y, z);
	_bullet->Rotate(x, y, z);
	_bullet->m_pHitEffect->Rotate(x, y, z);
}

void CBoss::PrepareAnimate()
{
}

void CBoss::Move(const int16& dwDirection, float fDistance)
{
	CPlayer::Move(dwDirection, BOSS_VELOCITY);
}

void CBoss::ResetState()
{
	CPlayer::ResetState();
	 _runAttackAnimationTime = 0;
	 _standAttackAnimationTime = 0;
	 _isAttacking = false;
	 _moveSoundActive = false;
	 if (_bullet)
	 {
		 _bullet->ResetState();
	 }
	 SoundManager::SoundStop(5);
}

void CBoss::SetAttackAnimOtherClient()
{
	//if (!GetOnAttack())
	//{
		if (!Vector3::IsZero(_velocity))
		{
			SetBehavior(PLAYER_BEHAVIOR::RUN_ATTACK);
			SetRunAttackAnimTime();
		}
		else
		{
			SetBehavior(PLAYER_BEHAVIOR::ATTACK);
			SetAttackAnimTime();
		}
		SetOnAttack(true);
	    if (_bullet)
	    {
		    _bullet->RequestSpawn();
	    }
	//}
}

void CBoss::Update(float fTimeElapsed, CLIENT_TYPE ptype)
{
	CPlayer::Update(fTimeElapsed, ptype);

	if (_bullet)
	{
		if (_bullet->ConsumeSpawnRequest())
		{
			const bool spawned = _bullet->Spawn(GetPosition(), GetLookVector());
			assert(spawned);
			(void)spawned;
		}
		_bullet->Update(fTimeElapsed);
	}

	AnimationLogicUpdate(); // 쿨타임 계산

	AimationStateUpdate(); // 애니메이션 트랙 상태 결정

	AnimTrackUpdate(); // 애니메이션 트랙 상태 변경

	LateUpdate(fTimeElapsed, ptype);


}

void CBoss::LateUpdate(float fTimeElapsed, CLIENT_TYPE ptype)
{
	if (ptype == CLIENT_TYPE::OWNER)
	{
		_velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
	}
	else if (ptype != CLIENT_TYPE::OTHER_PLAYER && _bullet)
	{
		SetAttackAnimOtherClient();
	}
}


void CBoss::AnimationLogicUpdate()
{
	if (GetOnAttack())
	{
		if (_standAttackAnimationTime >= CBoss::StandAttackFrameCount)
		{
			SetOnAttack(false);
			SetAttackAnimTime();
			return;
		}
		_standAttackAnimationTime++;

	}
}

void CBoss::AimationStateUpdate()
{
	if (GetOnAttack())
	{
		if (Vector3::IsZero(_velocity))
		{
			SetBehavior(PLAYER_BEHAVIOR::ATTACK);
		}
		else
		{
			SetBehavior(PLAYER_BEHAVIOR::RUN_ATTACK);
		}
	}
	else
	{
		if (Vector3::IsZero(_velocity))
		{
			SetBehavior(PLAYER_BEHAVIOR::IDLE);
		}
		else
		{
			SetBehavior(PLAYER_BEHAVIOR::RUN);
		}
	}
}

void CBoss::SetIdleAnimTrack()
{
	if (CLIENT_TYPE::OWNER == _state.clientType)
	{
		if (m_pSkinnedAnimationController2 == nullptr)
		{
			return;
		}
		m_pSkinnedAnimationController2->SetTrackEnable(0, true); // 아이들
		m_pSkinnedAnimationController2->SetTrackEnable(1, false);
		m_pSkinnedAnimationController2->SetTrackEnable(2, false);
		m_pSkinnedAnimationController2->SetTrackEnable(3, false);

		m_pSkinnedAnimationController2->SetTrackPosition(0, 0.0f);
		m_pSkinnedAnimationController2->SetTrackPosition(1, 0.0f);
		m_pSkinnedAnimationController2->SetTrackPosition(2, 0.0f);
		m_pSkinnedAnimationController2->SetTrackPosition(3, 0.0f);

		if (m_pSkinnedAnimationController == nullptr)
		{
			return;
		}
		m_pSkinnedAnimationController->SetTrackEnable(0, false); // 아이들
		m_pSkinnedAnimationController->SetTrackEnable(1, false);
		m_pSkinnedAnimationController->SetTrackEnable(2, false);
		m_pSkinnedAnimationController->SetTrackEnable(3, false);

		m_pSkinnedAnimationController->SetTrackPosition(0, 0.0f);
		m_pSkinnedAnimationController->SetTrackPosition(1, 0.0f);
		m_pSkinnedAnimationController->SetTrackPosition(2, 0.0f);
		m_pSkinnedAnimationController->SetTrackPosition(3, 0.0f);

		// ===============  하체 ===========================
		if (m_pSkinnedAnimationController1 == nullptr)
		{
			return;
		}
		m_pSkinnedAnimationController1->SetTrackEnable(0, false); // IDLE
		m_pSkinnedAnimationController1->SetTrackEnable(1, false);  // Run

		m_pSkinnedAnimationController1->SetTrackPosition(0, 0.0f);
		m_pSkinnedAnimationController1->SetTrackPosition(1, 0.0f);
	}
	else if (CLIENT_TYPE::OTHER_PLAYER == _state.clientType)
	{
		if (m_pSkinnedAnimationController2 == nullptr)
		{
			return;
		}
		m_pSkinnedAnimationController2->SetTrackEnable(0, false); // 아이들
		m_pSkinnedAnimationController2->SetTrackEnable(1, false);
		m_pSkinnedAnimationController2->SetTrackEnable(2, false);
		m_pSkinnedAnimationController2->SetTrackEnable(3, false);

		m_pSkinnedAnimationController2->SetTrackPosition(0, 0.0f);
		m_pSkinnedAnimationController2->SetTrackPosition(1, 0.0f);
		m_pSkinnedAnimationController2->SetTrackPosition(2, 0.0f);
		m_pSkinnedAnimationController2->SetTrackPosition(3, 0.0f);

		if (m_pSkinnedAnimationController == nullptr)
		{
			return;
		}
		m_pSkinnedAnimationController->SetTrackEnable(0, true); // 아이들
		m_pSkinnedAnimationController->SetTrackEnable(1, false);
		m_pSkinnedAnimationController->SetTrackEnable(2, false);
		m_pSkinnedAnimationController->SetTrackEnable(3, false);

		m_pSkinnedAnimationController->SetTrackPosition(0, 0.0f);
		m_pSkinnedAnimationController->SetTrackPosition(1, 0.0f);
		m_pSkinnedAnimationController->SetTrackPosition(2, 0.0f);
		m_pSkinnedAnimationController->SetTrackPosition(3, 0.0f);

		// ===============  하체 ===========================
		if (m_pSkinnedAnimationController1 == nullptr)
		{
			return;
		}
		m_pSkinnedAnimationController1->SetTrackEnable(0, true); // IDLE
		m_pSkinnedAnimationController1->SetTrackEnable(1, false);  // Run

		m_pSkinnedAnimationController1->SetTrackPosition(0, 0.0f);
		m_pSkinnedAnimationController1->SetTrackPosition(1, 0.0f);
	}
}

void CBoss::SetRunAnimTrack()
{
	if (CLIENT_TYPE::OWNER == _state.clientType)
	{
		if (m_pSkinnedAnimationController2 == nullptr)
		{
			return;
		}
		m_pSkinnedAnimationController2->SetTrackEnable(0, false);
		m_pSkinnedAnimationController2->SetTrackEnable(1, true);
		m_pSkinnedAnimationController2->SetTrackEnable(2, false);
		m_pSkinnedAnimationController2->SetTrackEnable(3, false);

		m_pSkinnedAnimationController2->SetTrackPosition(0, 0.0f);
		m_pSkinnedAnimationController2->SetTrackPosition(1, 0.0f);
		m_pSkinnedAnimationController2->SetTrackPosition(2, 0.0f);
		m_pSkinnedAnimationController2->SetTrackPosition(3, 0.0f);

		if (m_pSkinnedAnimationController == nullptr)
		{
			return;
		}
		// ================= 상체 =========================
		m_pSkinnedAnimationController->SetTrackEnable(0, false);
		m_pSkinnedAnimationController->SetTrackEnable(1, false); // 달리기
		m_pSkinnedAnimationController->SetTrackEnable(2, false);
		m_pSkinnedAnimationController->SetTrackEnable(3, false);

		m_pSkinnedAnimationController->SetTrackPosition(0, 0.0f);
		m_pSkinnedAnimationController->SetTrackPosition(1, 0.0f);
		m_pSkinnedAnimationController->SetTrackPosition(2, 0.0f);
		m_pSkinnedAnimationController->SetTrackPosition(3, 0.0f);

		// ===============  하체 ===========================
		if (m_pSkinnedAnimationController1 == nullptr)
		{
			return;
		}
		m_pSkinnedAnimationController1->SetTrackEnable(0, false); // IDLE
		m_pSkinnedAnimationController1->SetTrackEnable(1, false);  // Run

		m_pSkinnedAnimationController1->SetTrackPosition(0, 0.0f);
		m_pSkinnedAnimationController1->SetTrackPosition(1, 0.0f);
	}
	else if (CLIENT_TYPE::OTHER_PLAYER == _state.clientType)
	{
		if (m_pSkinnedAnimationController2 == nullptr)
		{
			return;
		}
		m_pSkinnedAnimationController2->SetTrackEnable(0, false);
		m_pSkinnedAnimationController2->SetTrackEnable(1, false);
		m_pSkinnedAnimationController2->SetTrackEnable(2, false);
		m_pSkinnedAnimationController2->SetTrackEnable(3, false);

		m_pSkinnedAnimationController2->SetTrackPosition(0, 0.0f);
		m_pSkinnedAnimationController2->SetTrackPosition(1, 0.0f);
		m_pSkinnedAnimationController2->SetTrackPosition(2, 0.0f);
		m_pSkinnedAnimationController2->SetTrackPosition(3, 0.0f);

		if (m_pSkinnedAnimationController == nullptr)
		{
			return;
		}
		// ================= 상체 =========================
		m_pSkinnedAnimationController->SetTrackEnable(0, false);
		m_pSkinnedAnimationController->SetTrackEnable(1, true); // 달리기
		m_pSkinnedAnimationController->SetTrackEnable(2, false);
		m_pSkinnedAnimationController->SetTrackEnable(3, false);

		m_pSkinnedAnimationController->SetTrackPosition(0, 0.0f);
		m_pSkinnedAnimationController->SetTrackPosition(1, 0.0f);
		m_pSkinnedAnimationController->SetTrackPosition(2, 0.0f);
		m_pSkinnedAnimationController->SetTrackPosition(3, 0.0f);

		// ===============  하체 ===========================
		if (m_pSkinnedAnimationController1 == nullptr)
		{
			return;
		}
		m_pSkinnedAnimationController1->SetTrackEnable(0, false); // IDLE
		m_pSkinnedAnimationController1->SetTrackEnable(1, true);  // Run

		m_pSkinnedAnimationController1->SetTrackPosition(0, 0.0f);
		m_pSkinnedAnimationController1->SetTrackPosition(1, 0.0f);
	}

}

void CBoss::SetAttackAnimTrack()
{
	if (CLIENT_TYPE::OWNER == _state.clientType)
	{
		if (m_pSkinnedAnimationController2 == nullptr)
		{
			return;
		}
		m_pSkinnedAnimationController2->SetTrackEnable(0, false); // 아이들
		m_pSkinnedAnimationController2->SetTrackEnable(1, false);
		m_pSkinnedAnimationController2->SetTrackEnable(2, true);
		m_pSkinnedAnimationController2->SetTrackEnable(3, false);

		m_pSkinnedAnimationController2->SetTrackPosition(0, 0.0f);
		m_pSkinnedAnimationController2->SetTrackPosition(1, 0.0f);
		m_pSkinnedAnimationController2->SetTrackPosition(2, 0.0f);
		m_pSkinnedAnimationController2->SetTrackPosition(3, 0.0f);

		m_pSkinnedAnimationController2->SetTrackPosition(0, 0.0f);
		m_pSkinnedAnimationController2->SetTrackPosition(1, 0.0f);
		m_pSkinnedAnimationController2->SetTrackPosition(2, 0.0f);
		m_pSkinnedAnimationController2->SetTrackPosition(3, 0.0f);

		if (m_pSkinnedAnimationController == nullptr)
		{
			return;
		}
		m_pSkinnedAnimationController->SetTrackEnable(0, false);
		m_pSkinnedAnimationController->SetTrackEnable(1, false);
		m_pSkinnedAnimationController->SetTrackEnable(2, false); // 공격
		m_pSkinnedAnimationController->SetTrackEnable(3, false);

		m_pSkinnedAnimationController->SetTrackPosition(0, 0.0f);
		m_pSkinnedAnimationController->SetTrackPosition(1, 0.0f);
		m_pSkinnedAnimationController->SetTrackPosition(2, 0.0f);
		m_pSkinnedAnimationController->SetTrackPosition(3, 0.0f);

		// ===============  하체 ===========================
		if (m_pSkinnedAnimationController1 == nullptr)
		{
			return;
		}
		m_pSkinnedAnimationController1->SetTrackEnable(0, false); // IDLE
		m_pSkinnedAnimationController1->SetTrackEnable(1, false);  // Run

		m_pSkinnedAnimationController1->SetTrackPosition(0, 0.0f);
		m_pSkinnedAnimationController1->SetTrackPosition(1, 0.0f);

	}
	else if (CLIENT_TYPE::OTHER_PLAYER == _state.clientType)
	{
		if (m_pSkinnedAnimationController2 == nullptr)
		{
			return;
		}
		m_pSkinnedAnimationController2->SetTrackEnable(0, false); // 아이들
		m_pSkinnedAnimationController2->SetTrackEnable(1, false);
		m_pSkinnedAnimationController2->SetTrackEnable(2, false);
		m_pSkinnedAnimationController2->SetTrackEnable(3, false);

		m_pSkinnedAnimationController2->SetTrackPosition(0, 0.0f);
		m_pSkinnedAnimationController2->SetTrackPosition(1, 0.0f);
		m_pSkinnedAnimationController2->SetTrackPosition(2, 0.0f);
		m_pSkinnedAnimationController2->SetTrackPosition(3, 0.0f);

		if (m_pSkinnedAnimationController == nullptr)
		{
			return;
		}
		m_pSkinnedAnimationController->SetTrackEnable(0, false);
		m_pSkinnedAnimationController->SetTrackEnable(1, false);
		m_pSkinnedAnimationController->SetTrackEnable(2, true); // 공격
		m_pSkinnedAnimationController->SetTrackEnable(3, false);

		m_pSkinnedAnimationController->SetTrackPosition(0, 0.0f);
		m_pSkinnedAnimationController->SetTrackPosition(1, 0.0f);
		m_pSkinnedAnimationController->SetTrackPosition(2, 0.0f);
		m_pSkinnedAnimationController->SetTrackPosition(3, 0.0f);

		// ===============  하체 ===========================
		if (m_pSkinnedAnimationController1 == nullptr)
		{
			return;
		}
		m_pSkinnedAnimationController1->SetTrackEnable(0, true); // IDLE
		m_pSkinnedAnimationController1->SetTrackEnable(1, false);  // Run

		m_pSkinnedAnimationController1->SetTrackPosition(0, 0.0f);
		m_pSkinnedAnimationController1->SetTrackPosition(1, 0.0f);
	}
}

void CBoss::SetRunAttackAnimTrack()
{
	if (CLIENT_TYPE::OWNER == _state.clientType)
	{
		if (m_pSkinnedAnimationController2 == nullptr)
		{
			return;
		}
		m_pSkinnedAnimationController2->SetTrackEnable(0, false);
		m_pSkinnedAnimationController2->SetTrackEnable(1, false);
		m_pSkinnedAnimationController2->SetTrackEnable(2, false);
		m_pSkinnedAnimationController2->SetTrackEnable(3, true);

		m_pSkinnedAnimationController2->SetTrackPosition(0, 0.0f);
		m_pSkinnedAnimationController2->SetTrackPosition(1, 0.0f);
		m_pSkinnedAnimationController2->SetTrackPosition(2, 0.0f);
		m_pSkinnedAnimationController2->SetTrackPosition(3, 0.0f);

		if (m_pSkinnedAnimationController == nullptr)
		{
			return;
		}
		m_pSkinnedAnimationController->SetTrackEnable(0, false);
		m_pSkinnedAnimationController->SetTrackEnable(1, false);
		m_pSkinnedAnimationController->SetTrackEnable(2, false);
		m_pSkinnedAnimationController->SetTrackEnable(3, false);

		m_pSkinnedAnimationController->SetTrackPosition(0, 0.0f);
		m_pSkinnedAnimationController->SetTrackPosition(1, 0.0f);
		m_pSkinnedAnimationController->SetTrackPosition(2, 0.0f);
		m_pSkinnedAnimationController->SetTrackPosition(3, 0.0f);

		if (m_pSkinnedAnimationController1 == nullptr)
		{
			return;
		}
		// ===============  하체 ===========================
		m_pSkinnedAnimationController1->SetTrackEnable(0, false); // IDLE
		m_pSkinnedAnimationController1->SetTrackEnable(1, false);  // Run

		m_pSkinnedAnimationController1->SetTrackPosition(0, 0.0f);
		m_pSkinnedAnimationController1->SetTrackPosition(1, 0.0f);
	}
	else if (CLIENT_TYPE::OTHER_PLAYER == _state.clientType)
	{
		if (m_pSkinnedAnimationController2 == nullptr)
		{
			return;
		}
		m_pSkinnedAnimationController2->SetTrackEnable(0, false);
		m_pSkinnedAnimationController2->SetTrackEnable(1, false);
		m_pSkinnedAnimationController2->SetTrackEnable(2, false);
		m_pSkinnedAnimationController2->SetTrackEnable(3, false);

		m_pSkinnedAnimationController2->SetTrackPosition(0, 0.0f);
		m_pSkinnedAnimationController2->SetTrackPosition(1, 0.0f);
		m_pSkinnedAnimationController2->SetTrackPosition(2, 0.0f);
		m_pSkinnedAnimationController2->SetTrackPosition(3, 0.0f);

		if (m_pSkinnedAnimationController == nullptr)
		{
			return;
		}
		m_pSkinnedAnimationController->SetTrackEnable(0, false);
		m_pSkinnedAnimationController->SetTrackEnable(1, false);
		m_pSkinnedAnimationController->SetTrackEnable(2, false);
		m_pSkinnedAnimationController->SetTrackEnable(3, true);

		m_pSkinnedAnimationController->SetTrackPosition(0, 0.0f);
		m_pSkinnedAnimationController->SetTrackPosition(1, 0.0f);
		m_pSkinnedAnimationController->SetTrackPosition(2, 0.0f);
		m_pSkinnedAnimationController->SetTrackPosition(3, 0.0f);

		if (m_pSkinnedAnimationController1 == nullptr)
		{
			return;
		}
		// ===============  하체 ===========================
		m_pSkinnedAnimationController1->SetTrackEnable(0, false); // IDLE
		m_pSkinnedAnimationController1->SetTrackEnable(1, true);  // Run

		m_pSkinnedAnimationController1->SetTrackPosition(0, 0.0f);
		m_pSkinnedAnimationController1->SetTrackPosition(1, 0.0f);
	}
}

void CBoss::AnimTrackUpdate()
{
	switch (_state.behavior)
	{
		case (int32)PLAYER_BEHAVIOR::IDLE:
			SetIdleAnimTrack();
			if (GetOnMoveSound()&&CLIENT_TYPE::OWNER == _state.clientType)
			{
				SoundManager::GetInstance().SoundStop(5);
				SetOnMoveSound(false);
			}
			break;
		case(int32)PLAYER_BEHAVIOR::RUN:
			SetRunAnimTrack();
			if (!GetOnMoveSound()&& CLIENT_TYPE::OWNER == _state.clientType)
			{
				SoundManager::GetInstance().PlayObjectSound(10, 5);
				SetOnMoveSound(true);
			}
			break;
		case (int32)PLAYER_BEHAVIOR::ATTACK:
		    if (CLIENT_TYPE::OWNER == _state.clientType)
		    {
			    SoundManager::GetInstance().SoundStop(5);
		    }
		    SetAttackAnimTrack();
			break;
		case (int32)PLAYER_BEHAVIOR::RUN_ATTACK:
			SetRunAttackAnimTrack();
			break;
	}
}


uint8 CBoss::ProcessInput()
{
	uint8 dir = 0;

	if (InputManager::GetInstance().GetKeyBuffer(KEY_TYPE::W) > 0)
	{
		dir |= KEY_FORWARD;
	}
	if (InputManager::GetInstance().GetKeyBuffer(KEY_TYPE::A) > 0)
	{
		dir |= KEY_LEFT;
	}
	if (InputManager::GetInstance().GetKeyBuffer(KEY_TYPE::S) > 0)
	{
		dir |= KEY_BACKWARD;
	}
	if (InputManager::GetInstance().GetKeyBuffer(KEY_TYPE::D) > 0)
	{
		dir |= KEY_RIGHT;
	}

	if (dir)
	{
		SetBehavior(PLAYER_BEHAVIOR::RUN);
	}
	else
	{
		SetBehavior(PLAYER_BEHAVIOR::IDLE);
	}

	// 1. 공격 키를 눌렀을 경우 처리
	if (InputManager::GetInstance().GetKeyBuffer(KEY_TYPE::SPACE) == (uint8)KEY_STATUS::KEY_PRESS && !GetOnAttack())
	{
		SetOnAttack(true);
		if (CLIENT_TYPE::OWNER == _state.clientType)
		{
			SoundManager::GetInstance().PlayObjectSound(2, 6);
		}

		C2S_ATTACK packet;
		packet.type = (uint8)C_GAME_PACKET_TYPE::CATTACK;
		packet.size = sizeof(C2S_ATTACK);
		packet.wf = _ownerScene.CurrentWorldFrame();

		SC_EVENTPACKET epacket{};
		epacket.size = sizeof(epacket);
		epacket.type = (uint8)SC_GAME_PACKET_TYPE::GAMEEVENT;
		epacket.eventId = (uint8)EVENT_TYPE::ATTACK_ANIM;

		_ownerScene.SendPacket(&epacket);

		XMFLOAT3 bossPos = GetPosition();
		XMFLOAT3 bossDir = GetLookVector();
		float rayDist = 5.0f;

		if (PLAYERNUM >= 1)
		{
			for (int i = 1; i < PLAYERNUM; ++i)
			{
				CEmployee* targetPlayer = static_cast<CEmployee*>(_ownerScene.GetScenePlayerByIdx(i));
				if (targetPlayer)
				{
					if (targetPlayer->_boundingSphere.Intersects(XMLoadFloat3(&bossPos), XMLoadFloat3(&bossDir), rayDist) && !targetPlayer->_invincible)
					{
						packet.tidx = i;
						_ownerScene.AddEvent(DelayEvent{ packet }, 0.0f);
						break;
					}
				}
			}
		}
		if (_bullet)
		{
			_bullet->RequestSpawn();
		}
		if (CLIENT_TYPE::OWNER == _state.clientType)
		{
			SoundManager::GetInstance().PlayObjectSound(4, 6);
		}
	}
	Move(dir, BOSS_VELOCITY);
	return dir;
}

