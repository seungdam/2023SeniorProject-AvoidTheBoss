#include "pch.h"
#include "CEmployee.h"
#include "clientIocpCore.h"
#include "GameFramework.h"

#include "CGenerator.h"
#include "CSound.h"

#include "GameScene.h"
#include "OtherScenes.h"

#include "InputManager.h"
#include "SceneManager.h"
#include "SoundManager.h"

CEmployee::CEmployee(ID3D12Device5* pd3dDevice, ID3D12GraphicsCommandList4* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature, CHARACTER_TYPE nType)
{
	_state.playerType = PLAYER_TYPE::EMPLOYEE;
	_characterType = nType;

	// 1 인칭 애니메이션 로드
		//달리기, 버튼, 느리게 걷기, 대기
		CLoadedModelInfo* pEmployeeModel1v = CGameObject::LoadGeometryAndAnimationFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, g_pstrFirstCharactorRefernece[(int)_characterType], NULL, Layout::PLAYER);
		SetChild(pEmployeeModel1v->m_pModelRootObject, true);

		m_pSkinnedAnimationController2 = new CAnimationController(pd3dDevice, pd3dCommandList, 4, pEmployeeModel1v);
		m_pSkinnedAnimationController2->SetTrackAnimationSet(0, 3);//idle
		m_pSkinnedAnimationController2->SetTrackAnimationSet(1, 0);//run
		m_pSkinnedAnimationController2->SetTrackAnimationSet(2, 2);//slow_walk (절뚝거리기)
		m_pSkinnedAnimationController2->SetTrackAnimationSet(3, 1);//button

	// 3인칭 애니메이션 로드
		CLoadedModelInfo* pEmployeeModel3v = CGameObject::LoadGeometryAndAnimationFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, g_pstrThirdCharactorRefernece[(int)_characterType], NULL, Layout::PLAYER);
		SetChild(pEmployeeModel3v->m_pModelRootObject, true);

		m_pSkinnedAnimationController1 = new CAnimationController(pd3dDevice, pd3dCommandList, 8, pEmployeeModel3v);

		//필요없는 애니메이션
		m_pSkinnedAnimationController1->SetTrackAnimationSet(0, 2);//idle x
		m_pSkinnedAnimationController1->SetTrackAnimationSet(1, 3);//run x
		m_pSkinnedAnimationController1->SetTrackAnimationSet(2, 0);//down (총알 맞고 쓰러짐) x
		m_pSkinnedAnimationController1->SetTrackAnimationSet(3, 1);//down_idle,crawl (쓰러진 상태) ㅇ
		m_pSkinnedAnimationController1->SetTrackAnimationSet(4, 4);//slow_walk (절뚝거리기) x
		m_pSkinnedAnimationController1->SetTrackAnimationSet(5, 5);//stand (쓰러진 상태에서 일어나기) ㅇ
		m_pSkinnedAnimationController1->SetTrackAnimationSet(6, 6);//button ㅇ
		m_pSkinnedAnimationController1->SetTrackAnimationSet(7, 7);//만세 X


	// 게임 시작 전 기본 뷰는 1인칭이다.
	m_pSkinnedAnimationController2->SetTrackEnable(0, false);
	m_pSkinnedAnimationController2->SetTrackEnable(1, false);
	m_pSkinnedAnimationController2->SetTrackEnable(2, false);
	m_pSkinnedAnimationController2->SetTrackEnable(3, true);

	m_pSkinnedAnimationController1->SetTrackEnable(0, false);
	m_pSkinnedAnimationController1->SetTrackEnable(1, false);
	m_pSkinnedAnimationController1->SetTrackEnable(2, false);
	m_pSkinnedAnimationController1->SetTrackEnable(3, false);
	m_pSkinnedAnimationController1->SetTrackEnable(4, false);
	m_pSkinnedAnimationController1->SetTrackEnable(5, false);
	m_pSkinnedAnimationController1->SetTrackEnable(6, false);
	m_pSkinnedAnimationController1->SetTrackEnable(7, false);

	if (pEmployeeModel1v)delete pEmployeeModel1v;
	if (pEmployeeModel3v)delete pEmployeeModel3v;
}

CEmployee::~CEmployee()
{
}

// 04-29 직원 키입력 처리 추가
// 입력 처리 및 플레이어의 행동을 미리 셋팅한다.
uint8 CEmployee::ProcessInput()
{
	// 발전기 상호작용 관련 인풋 처리

	uint8 dir = 0;
	if (!IsSeMiBehavior() && !IsMovable())
	{
		if (InputManager::GetInstance().GetKeyBuffer(KEY_TYPE::W) > 0)  dir |= KEY_FORWARD;
		if (InputManager::GetInstance().GetKeyBuffer(KEY_TYPE::A) > 0)  dir |= KEY_LEFT;
		if (InputManager::GetInstance().GetKeyBuffer(KEY_TYPE::S) > 0)  dir |= KEY_BACKWARD;
		if (InputManager::GetInstance().GetKeyBuffer(KEY_TYPE::D) > 0)  dir |= KEY_RIGHT;

		if (GetBehavior() != (int32)PLAYER_BEHAVIOR::ATTACKED)
		{
			if (dir) SetBehavior(PLAYER_BEHAVIOR::RUN);
			else	 SetBehavior(PLAYER_BEHAVIOR::IDLE);
		}
		// 구조 작업이나 발전기 상호작용을 수행하고 있다면


	}

	GenTasking();
	RescueTasking();

	Move(dir, EMPLOYEE_VELOCITY);
	return dir;

}

void CEmployee::Move(const int16& dwDirection, float fDistance)
{
	if (_state.clientType == CLIENT_TYPE::OTHER_PLAYER)
	{
		if (false == IsSeMiBehavior())
		{
			if ((int32)PLAYER_BEHAVIOR::ATTACKED == GetBehavior() && _attackedAnimationFrames >= 0);
			else if (LOBYTE(dwDirection)) SetBehavior(PLAYER_BEHAVIOR::RUN);
			else if(!LOBYTE(dwDirection)) SetBehavior(PLAYER_BEHAVIOR::IDLE);
		}
	}

	switch (GetBehavior())
	{
	case (int32)PLAYER_BEHAVIOR::RESCUE:
	case (int32)PLAYER_BEHAVIOR::STAND:
	case (int32)PLAYER_BEHAVIOR::DOWN:
	case (int32)PLAYER_BEHAVIOR::CRAWL:
	case (int32)PLAYER_BEHAVIOR::EXIT:
		CPlayer::Move(0, 0);
		break;
	case (int32)PLAYER_BEHAVIOR::IDLE:
	case (int32)PLAYER_BEHAVIOR::RUN:
	case (int32)PLAYER_BEHAVIOR::ATTACKED:
		CPlayer::Move(dwDirection, EMPLOYEE_VELOCITY);
		break;
	}

}

void CEmployee::Update(float fTimeElapsed, CLIENT_TYPE ptype)
{
	CPlayer::Update(fTimeElapsed, ptype);
	LateUpdate(fTimeElapsed,ptype);
}

void CEmployee::LateUpdate(float fTimeElapsed, CLIENT_TYPE ptype)
{
	// ===== 애니메이션 트랙 업데이트 ========
	AnimTrackUpdate();

	// 무적시간 동안 피격 이펙트 재생
	if (_invincible)
	{
		_uiCooldown -= fTimeElapsed;
		if (_uiCooldown <= 0) _uiCooldown = 0.f;
	}
	else
	{
		_uiCooldown = 1.0f;
	}

	// 내가 구해짐을 받고 있다면~
	if (_beingRescued)
	{
		_rescueGauge += _rescueSpeed * fTimeElapsed;
		if (_rescueGauge >= _maxRescueGauge)
		{

			_rescueGauge = 0;
			RestoreHealth();
			_beingRescued = false;

			SetBehavior(PLAYER_BEHAVIOR::STAND);
			_standAnimationFrames = EMPLOYEE_STAND_TIME;

			SC_EVENTPACKET packet;
			packet.type = (uint8)SC_GAME_PACKET_TYPE::GAMEEVENT;
			packet.size = sizeof(SC_EVENTPACKET);
			packet.eventId = (int32)EVENT_TYPE::ALIVE_PLAYER_ONE + _state.playerIndex;
			clientCore.DoSend(&packet);
		}
	}

	// 탈출 후 맵에서 일정 범위 이상 넘어가게 되면 EXIT 상태로 만세 애니메이션 재생
	if (!m_bEmpExit && static_cast<CGameScene*>(mainGame.m_SceneManager->GetSceneByIdx(3))->_employeeExitReady && _state.clientType == CLIENT_TYPE::OWNER)
	{
		// 결과 씬에 넘겨주기
		static_cast<CResultScene*>(mainGame.m_SceneManager->GetSceneByIdx(4))->m_activeCnt = _activatedGeneratorCount;
		static_cast<CResultScene*>(mainGame.m_SceneManager->GetSceneByIdx(4))->m_deadCnt = _deathCount;
		static_cast<CGameScene*>(mainGame.m_SceneManager->GetSceneByIdx(
			static_cast<int32>(CGameFramework::SCENESTATE::INGAME)))->SetCameraMode(FIRST_PERSON_CAMERA);

		if (GetPosition().x < -28 || GetPosition().x > 28 || GetPosition().z > 28 || GetPosition().z < -28)
		{
			m_bEmpExit = true;
			SetBehavior(PLAYER_BEHAVIOR::EXIT);
			_moveSoundActive = false;
			if (CLIENT_TYPE::OWNER == _state.clientType)
				SoundManager::SoundStop(14);
			SC_EVENTPACKET packet;
			packet.size = sizeof(SC_EVENTPACKET);
			packet.type = (uint8)SC_GAME_PACKET_TYPE::GAMEEVENT;
			packet.eventId = _state.playerIndex + (uint8)EVENT_TYPE::EXIT_PLAYER_ONE;

			clientCore.DoSend(&packet); // 탈출 시 전송
		}
	}

	if (ptype == CLIENT_TYPE::OWNER) _velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
	else if (ptype == CLIENT_TYPE::OTHER_PLAYER)
	{
		if (_state.behavior != (int32)PLAYER_BEHAVIOR::SWITCH_INTER) SetGenInteraction(false);
		if (_state.behavior == (int32)PLAYER_BEHAVIOR::EXIT) _state.hidden = true;
	}
}

// ===============애니메이션 트랙 ==================
// 애니메이션 1인칭인덱스 3인칭인덱스
// 걷기 0
void CEmployee::SetIdleAnimTrack()
{
	if (CLIENT_TYPE::OWNER == _state.clientType)
	{
		if (m_pSkinnedAnimationController2 == nullptr) return;
		m_pSkinnedAnimationController2->SetTrackEnable(0, true);
		m_pSkinnedAnimationController2->SetTrackEnable(1, false);
		m_pSkinnedAnimationController2->SetTrackEnable(2, false);
		m_pSkinnedAnimationController2->SetTrackEnable(3, false);

		if (m_pSkinnedAnimationController1 == nullptr) return;
		m_pSkinnedAnimationController1->SetTrackEnable(0, false);
		m_pSkinnedAnimationController1->SetTrackEnable(1, false);
		m_pSkinnedAnimationController1->SetTrackEnable(2, false);
		m_pSkinnedAnimationController1->SetTrackEnable(3, false);
		m_pSkinnedAnimationController1->SetTrackEnable(4, false);
		m_pSkinnedAnimationController1->SetTrackEnable(5, false);
		m_pSkinnedAnimationController1->SetTrackEnable(6, false);
		m_pSkinnedAnimationController1->SetTrackEnable(7, false);
	}
	else if (CLIENT_TYPE::OTHER_PLAYER == _state.clientType)
	{
		if (m_pSkinnedAnimationController2 == nullptr) return;
		m_pSkinnedAnimationController2->SetTrackEnable(0, false);
		m_pSkinnedAnimationController2->SetTrackEnable(1, false);
		m_pSkinnedAnimationController2->SetTrackEnable(2, false);
		m_pSkinnedAnimationController2->SetTrackEnable(3, false);

		if (m_pSkinnedAnimationController1 == nullptr) return;
		m_pSkinnedAnimationController1->SetTrackEnable(0, true);
		m_pSkinnedAnimationController1->SetTrackEnable(1, false);
		m_pSkinnedAnimationController1->SetTrackEnable(2, false);
		m_pSkinnedAnimationController1->SetTrackEnable(3, false);
		m_pSkinnedAnimationController1->SetTrackEnable(4, false);
		m_pSkinnedAnimationController1->SetTrackEnable(5, false);
		m_pSkinnedAnimationController1->SetTrackEnable(6, false);
		m_pSkinnedAnimationController1->SetTrackEnable(7, false);
	}

	m_pSkinnedAnimationController2->SetTrackPosition(0, 0);
	m_pSkinnedAnimationController2->SetTrackPosition(1, 0);
	m_pSkinnedAnimationController2->SetTrackPosition(2, 0);
	m_pSkinnedAnimationController2->SetTrackPosition(3, 0);

	m_pSkinnedAnimationController1->SetTrackPosition(0, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(1, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(2, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(3, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(4, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(5, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(6, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(7, 0);
}

// 달리기 1
void CEmployee::SetRunAnimTrack()
{
	if (CLIENT_TYPE::OWNER == _state.clientType)
	{
		if (m_pSkinnedAnimationController2 == nullptr) return;
		m_pSkinnedAnimationController2->SetTrackEnable(0, false);
		m_pSkinnedAnimationController2->SetTrackEnable(1, true);
		m_pSkinnedAnimationController2->SetTrackEnable(2, false);
		m_pSkinnedAnimationController2->SetTrackEnable(3, false);

		if (m_pSkinnedAnimationController1 == nullptr) return;
		m_pSkinnedAnimationController1->SetTrackEnable(0, false);
		m_pSkinnedAnimationController1->SetTrackEnable(1, false);
		m_pSkinnedAnimationController1->SetTrackEnable(2, false);
		m_pSkinnedAnimationController1->SetTrackEnable(3, false);
		m_pSkinnedAnimationController1->SetTrackEnable(4, false);
		m_pSkinnedAnimationController1->SetTrackEnable(5, false);
		m_pSkinnedAnimationController1->SetTrackEnable(6, false);
		m_pSkinnedAnimationController1->SetTrackEnable(7, false);
	}
	else if (CLIENT_TYPE::OTHER_PLAYER == _state.clientType)
	{
		if (m_pSkinnedAnimationController2 == nullptr) return;
		m_pSkinnedAnimationController2->SetTrackEnable(0, false);
		m_pSkinnedAnimationController2->SetTrackEnable(1, false);
		m_pSkinnedAnimationController2->SetTrackEnable(2, false);
		m_pSkinnedAnimationController2->SetTrackEnable(3, false);

		if (m_pSkinnedAnimationController1 == nullptr) return;
		m_pSkinnedAnimationController1->SetTrackEnable(0, false);
		m_pSkinnedAnimationController1->SetTrackEnable(1, true);
		m_pSkinnedAnimationController1->SetTrackEnable(2, false);
		m_pSkinnedAnimationController1->SetTrackEnable(3, false);
		m_pSkinnedAnimationController1->SetTrackEnable(4, false);
		m_pSkinnedAnimationController1->SetTrackEnable(5, false);
		m_pSkinnedAnimationController1->SetTrackEnable(6, false);
		m_pSkinnedAnimationController1->SetTrackEnable(7, false);
	}
	m_pSkinnedAnimationController2->SetTrackPosition(0, 0);
	m_pSkinnedAnimationController2->SetTrackPosition(1, 0);
	m_pSkinnedAnimationController2->SetTrackPosition(2, 0);
	m_pSkinnedAnimationController2->SetTrackPosition(3, 0);

	m_pSkinnedAnimationController1->SetTrackPosition(0, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(1, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(2, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(3, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(4, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(5, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(6, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(7, 0);
}

// 절뚝거리기 2,4
void CEmployee::SetAttackedAnimTrack()
{
	if (CLIENT_TYPE::OWNER == _state.clientType)
	{
		if (m_pSkinnedAnimationController2 == nullptr) return;
		m_pSkinnedAnimationController2->SetTrackEnable(0, false);
		m_pSkinnedAnimationController2->SetTrackEnable(1, false);
		m_pSkinnedAnimationController2->SetTrackEnable(2, true);
		m_pSkinnedAnimationController2->SetTrackEnable(3, false);

		if (m_pSkinnedAnimationController1 == nullptr) return;
		m_pSkinnedAnimationController1->SetTrackEnable(0, false);
		m_pSkinnedAnimationController1->SetTrackEnable(1, false);
		m_pSkinnedAnimationController1->SetTrackEnable(2, false);
		m_pSkinnedAnimationController1->SetTrackEnable(3, false);
		m_pSkinnedAnimationController1->SetTrackEnable(4, false);
		m_pSkinnedAnimationController1->SetTrackEnable(5, false);
		m_pSkinnedAnimationController1->SetTrackEnable(6, false);
		m_pSkinnedAnimationController1->SetTrackEnable(7, false);
	}
	else if (CLIENT_TYPE::OTHER_PLAYER == _state.clientType)
	{
		if (m_pSkinnedAnimationController2 == nullptr) return;
		m_pSkinnedAnimationController2->SetTrackEnable(0, false);
		m_pSkinnedAnimationController2->SetTrackEnable(1, false);
		m_pSkinnedAnimationController2->SetTrackEnable(2, false);
		m_pSkinnedAnimationController2->SetTrackEnable(3, false);

		if (m_pSkinnedAnimationController1 == nullptr) return;
		m_pSkinnedAnimationController1->SetTrackEnable(0, false);
		m_pSkinnedAnimationController1->SetTrackEnable(1, false);
		m_pSkinnedAnimationController1->SetTrackEnable(2, false);
		m_pSkinnedAnimationController1->SetTrackEnable(3, false);
		m_pSkinnedAnimationController1->SetTrackEnable(4, true);
		m_pSkinnedAnimationController1->SetTrackEnable(5, false);
		m_pSkinnedAnimationController1->SetTrackEnable(6, false);
		m_pSkinnedAnimationController1->SetTrackEnable(7, false);
	}

	m_pSkinnedAnimationController2->SetTrackPosition(0, 0);
	m_pSkinnedAnimationController2->SetTrackPosition(1, 0);
	m_pSkinnedAnimationController2->SetTrackPosition(2, 0);
	m_pSkinnedAnimationController2->SetTrackPosition(3, 0);

	m_pSkinnedAnimationController1->SetTrackPosition(0, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(1, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(2, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(3, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(4, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(5, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(6, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(7, 0);
}

// 발전기 상호작용 3,6
void CEmployee::SetInteractionAnimTrack()
{
	// 발전기 상호작용
	if (CLIENT_TYPE::OWNER == _state.clientType)
	{
		if (m_pSkinnedAnimationController2 == nullptr) return;
		m_pSkinnedAnimationController2->SetTrackEnable(0, false);
		m_pSkinnedAnimationController2->SetTrackEnable(1, false);
		m_pSkinnedAnimationController2->SetTrackEnable(2, false);
		m_pSkinnedAnimationController2->SetTrackEnable(3, true);

		if (m_pSkinnedAnimationController1 == nullptr) return;
		m_pSkinnedAnimationController1->SetTrackEnable(0, false);
		m_pSkinnedAnimationController1->SetTrackEnable(1, false);
		m_pSkinnedAnimationController1->SetTrackEnable(2, false);
		m_pSkinnedAnimationController1->SetTrackEnable(3, false);
		m_pSkinnedAnimationController1->SetTrackEnable(4, false);
		m_pSkinnedAnimationController1->SetTrackEnable(5, false);
		m_pSkinnedAnimationController1->SetTrackEnable(6, false);
		m_pSkinnedAnimationController1->SetTrackEnable(7, false);
	}
	else if (CLIENT_TYPE::OTHER_PLAYER == _state.clientType)
	{
		if (m_pSkinnedAnimationController2 == nullptr) return;
		m_pSkinnedAnimationController2->SetTrackEnable(0, false);
		m_pSkinnedAnimationController2->SetTrackEnable(1, false);
		m_pSkinnedAnimationController2->SetTrackEnable(2, false);
		m_pSkinnedAnimationController2->SetTrackEnable(3, false);

		if (m_pSkinnedAnimationController1 == nullptr) return;
		m_pSkinnedAnimationController1->SetTrackEnable(0, false);
		m_pSkinnedAnimationController1->SetTrackEnable(1, false);
		m_pSkinnedAnimationController1->SetTrackEnable(2, false);
		m_pSkinnedAnimationController1->SetTrackEnable(3, false);
		m_pSkinnedAnimationController1->SetTrackEnable(4, false);
		m_pSkinnedAnimationController1->SetTrackEnable(5, false);
		m_pSkinnedAnimationController1->SetTrackEnable(6, true);
		m_pSkinnedAnimationController1->SetTrackEnable(7, false);
	}
	m_pSkinnedAnimationController2->SetTrackPosition(0, 0);
	m_pSkinnedAnimationController2->SetTrackPosition(1, 0);
	m_pSkinnedAnimationController2->SetTrackPosition(2, 0);
	m_pSkinnedAnimationController2->SetTrackPosition(3, 0);
}

// 쓰러진 상태 x,3
void CEmployee::SetCrawlAnimTrack()
{
	if (CLIENT_TYPE::OWNER == _state.clientType && m_IsFirst)
	{
		// 일단 아이들 상태 애니메이션 재생하도록 한다.
		if (m_pSkinnedAnimationController2 == nullptr) return;
		m_pSkinnedAnimationController2->SetTrackEnable(0, true);
		m_pSkinnedAnimationController2->SetTrackEnable(1, false);
		m_pSkinnedAnimationController2->SetTrackEnable(2, false);
		m_pSkinnedAnimationController2->SetTrackEnable(3, false);

		if (m_pSkinnedAnimationController1 == nullptr) return;
		m_pSkinnedAnimationController1->SetTrackEnable(0, false);
		m_pSkinnedAnimationController1->SetTrackEnable(1, false);
		m_pSkinnedAnimationController1->SetTrackEnable(2, false);
		m_pSkinnedAnimationController1->SetTrackEnable(3, false);
		m_pSkinnedAnimationController1->SetTrackEnable(4, false);
		m_pSkinnedAnimationController1->SetTrackEnable(5, false);
		m_pSkinnedAnimationController1->SetTrackEnable(6, false);
	}
	else if (CLIENT_TYPE::OWNER == _state.clientType && !m_IsFirst)
	{
		// 일단 아이들 상태 애니메이션 재생하도록 한다.
		if (m_pSkinnedAnimationController2 == nullptr) return;
		m_pSkinnedAnimationController2->SetTrackEnable(0, false);
		m_pSkinnedAnimationController2->SetTrackEnable(1, false);
		m_pSkinnedAnimationController2->SetTrackEnable(2, false);
		m_pSkinnedAnimationController2->SetTrackEnable(3, false);

		if (m_pSkinnedAnimationController1 == nullptr) return;
		m_pSkinnedAnimationController1->SetTrackEnable(0, false);
		m_pSkinnedAnimationController1->SetTrackEnable(1, false);
		m_pSkinnedAnimationController1->SetTrackEnable(2, false);
		m_pSkinnedAnimationController1->SetTrackEnable(3, true);
		m_pSkinnedAnimationController1->SetTrackEnable(4, false);
		m_pSkinnedAnimationController1->SetTrackEnable(5, false);
		m_pSkinnedAnimationController1->SetTrackEnable(6, false);
		m_pSkinnedAnimationController1->SetTrackEnable(7, false);
	}

	// 만약 그냥 다른 플레이어라면 ~
	if (CLIENT_TYPE::OTHER_PLAYER == _state.clientType)
	{
		if (m_pSkinnedAnimationController2 == nullptr) return;
		m_pSkinnedAnimationController2->SetTrackEnable(0, false);
		m_pSkinnedAnimationController2->SetTrackEnable(1, false);
		m_pSkinnedAnimationController2->SetTrackEnable(2, false);
		m_pSkinnedAnimationController2->SetTrackEnable(3, false);

		if (m_pSkinnedAnimationController1 == nullptr) return;
		m_pSkinnedAnimationController1->SetTrackEnable(0, false);
		m_pSkinnedAnimationController1->SetTrackEnable(1, false);
		m_pSkinnedAnimationController1->SetTrackEnable(2, false);
		m_pSkinnedAnimationController1->SetTrackEnable(3, true);
		m_pSkinnedAnimationController1->SetTrackEnable(4, false);
		m_pSkinnedAnimationController1->SetTrackEnable(5, false);
		m_pSkinnedAnimationController1->SetTrackEnable(6, false);
		m_pSkinnedAnimationController1->SetTrackEnable(7, false);
	}

	m_pSkinnedAnimationController2->SetTrackPosition(0, 0);
	m_pSkinnedAnimationController2->SetTrackPosition(1, 0);
	m_pSkinnedAnimationController2->SetTrackPosition(2, 0);
	m_pSkinnedAnimationController2->SetTrackPosition(3, 0);

	m_pSkinnedAnimationController1->SetTrackPosition(0, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(1, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(2, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(3, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(4, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(5, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(6, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(7, 0);
}
// 총알 맞고 쓰러짐 x,2
void CEmployee::SetDownAnimTrack()
{
	if (CLIENT_TYPE::OWNER == _state.clientType && m_IsFirst)
	{
		if (m_pSkinnedAnimationController2 == nullptr) return;
		m_pSkinnedAnimationController2->SetTrackEnable(0, true);
		m_pSkinnedAnimationController2->SetTrackEnable(1, false);
		m_pSkinnedAnimationController2->SetTrackEnable(2, false);
		m_pSkinnedAnimationController2->SetTrackEnable(3, false);

		if (m_pSkinnedAnimationController1 == nullptr) return;
		m_pSkinnedAnimationController1->SetTrackEnable(0, false);
		m_pSkinnedAnimationController1->SetTrackEnable(1, false);
		m_pSkinnedAnimationController1->SetTrackEnable(2, false);
		m_pSkinnedAnimationController1->SetTrackEnable(3, false);
		m_pSkinnedAnimationController1->SetTrackEnable(4, false);
		m_pSkinnedAnimationController1->SetTrackEnable(5, false);
		m_pSkinnedAnimationController1->SetTrackEnable(6, false);
		m_pSkinnedAnimationController1->SetTrackEnable(7, false);
	}
	else if (CLIENT_TYPE::OWNER == _state.clientType && !m_IsFirst)
	{
		// 일단 아이들 상태 애니메이션 재생하도록 한다.
		if (m_pSkinnedAnimationController2 == nullptr) return;
		m_pSkinnedAnimationController2->SetTrackEnable(0, false);
		m_pSkinnedAnimationController2->SetTrackEnable(1, false);
		m_pSkinnedAnimationController2->SetTrackEnable(2, false);
		m_pSkinnedAnimationController2->SetTrackEnable(3, false);

		if (m_pSkinnedAnimationController1 == nullptr) return;
		m_pSkinnedAnimationController1->SetTrackEnable(0, false);
		m_pSkinnedAnimationController1->SetTrackEnable(1, false);
		m_pSkinnedAnimationController1->SetTrackEnable(2, true);
		m_pSkinnedAnimationController1->SetTrackEnable(3, false);
		m_pSkinnedAnimationController1->SetTrackEnable(4, false);
		m_pSkinnedAnimationController1->SetTrackEnable(5, false);
		m_pSkinnedAnimationController1->SetTrackEnable(6, false);
		m_pSkinnedAnimationController1->SetTrackEnable(7, false);
	}


	if (CLIENT_TYPE::OTHER_PLAYER == _state.clientType)
	{
		if (m_pSkinnedAnimationController2 == nullptr) return;
		m_pSkinnedAnimationController2->SetTrackEnable(0, false);
		m_pSkinnedAnimationController2->SetTrackEnable(1, false);
		m_pSkinnedAnimationController2->SetTrackEnable(2, false);
		m_pSkinnedAnimationController2->SetTrackEnable(3, false);

		if (m_pSkinnedAnimationController1 == nullptr) return;
		m_pSkinnedAnimationController1->SetTrackEnable(0, false);
		m_pSkinnedAnimationController1->SetTrackEnable(1, false);
		m_pSkinnedAnimationController1->SetTrackEnable(2, true);
		m_pSkinnedAnimationController1->SetTrackEnable(3, false);
		m_pSkinnedAnimationController1->SetTrackEnable(4, false);
		m_pSkinnedAnimationController1->SetTrackEnable(5, false);
		m_pSkinnedAnimationController1->SetTrackEnable(6, false);
		m_pSkinnedAnimationController1->SetTrackEnable(7, false);
	}
	m_pSkinnedAnimationController2->SetTrackPosition(0, 0);
	m_pSkinnedAnimationController2->SetTrackPosition(1, 0);
	m_pSkinnedAnimationController2->SetTrackPosition(2, 0);
	m_pSkinnedAnimationController2->SetTrackPosition(3, 0);

	m_pSkinnedAnimationController1->SetTrackPosition(0, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(1, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(2, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(3, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(4, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(5, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(6, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(7, 0);
}
// 일어나기 x,5
void CEmployee::SetStandAnimTrack()
{
	// 일어나기
	if (CLIENT_TYPE::OWNER == _state.clientType && m_IsFirst)
	{
		if (m_pSkinnedAnimationController2 == nullptr) return;
		m_pSkinnedAnimationController2->SetTrackEnable(0, true); // Idle 애니메이션 재생하도록
		m_pSkinnedAnimationController2->SetTrackEnable(1, false);
		m_pSkinnedAnimationController2->SetTrackEnable(2, false);
		m_pSkinnedAnimationController2->SetTrackEnable(3, false);

		if (m_pSkinnedAnimationController1 == nullptr) return;
		m_pSkinnedAnimationController1->SetTrackEnable(0, false);
		m_pSkinnedAnimationController1->SetTrackEnable(1, false);
		m_pSkinnedAnimationController1->SetTrackEnable(2, false);
		m_pSkinnedAnimationController1->SetTrackEnable(3, false);
		m_pSkinnedAnimationController1->SetTrackEnable(4, false);
		m_pSkinnedAnimationController1->SetTrackEnable(5, false);
		m_pSkinnedAnimationController1->SetTrackEnable(6, false);
		m_pSkinnedAnimationController1->SetTrackEnable(7, false);
	}
	else if (CLIENT_TYPE::OWNER == _state.clientType && !m_IsFirst) // 3인칭
	{
		if (m_pSkinnedAnimationController2 == nullptr) return;
		m_pSkinnedAnimationController2->SetTrackEnable(0, false); // Idle 애니메이션 재생하도록
		m_pSkinnedAnimationController2->SetTrackEnable(1, false);
		m_pSkinnedAnimationController2->SetTrackEnable(2, false);
		m_pSkinnedAnimationController2->SetTrackEnable(3, false);

		if (m_pSkinnedAnimationController1 == nullptr) return;
		m_pSkinnedAnimationController1->SetTrackEnable(0, false);
		m_pSkinnedAnimationController1->SetTrackEnable(1, false);
		m_pSkinnedAnimationController1->SetTrackEnable(2, false);
		m_pSkinnedAnimationController1->SetTrackEnable(3, false);
		m_pSkinnedAnimationController1->SetTrackEnable(4, false);
		m_pSkinnedAnimationController1->SetTrackEnable(5, true);
		m_pSkinnedAnimationController1->SetTrackEnable(6, false);
		m_pSkinnedAnimationController1->SetTrackEnable(7, false);
	}


	if (CLIENT_TYPE::OTHER_PLAYER == _state.clientType)
	{
		if (m_pSkinnedAnimationController2 == nullptr) return;
		m_pSkinnedAnimationController2->SetTrackEnable(0, false);
		m_pSkinnedAnimationController2->SetTrackEnable(1, false);
		m_pSkinnedAnimationController2->SetTrackEnable(2, false);
		m_pSkinnedAnimationController2->SetTrackEnable(3, false);

		if (m_pSkinnedAnimationController1 == nullptr) return;
		m_pSkinnedAnimationController1->SetTrackEnable(0, false);
		m_pSkinnedAnimationController1->SetTrackEnable(1, false);
		m_pSkinnedAnimationController1->SetTrackEnable(2, false);
		m_pSkinnedAnimationController1->SetTrackEnable(3, false);
		m_pSkinnedAnimationController1->SetTrackEnable(4, false);
		m_pSkinnedAnimationController1->SetTrackEnable(5, true);
		m_pSkinnedAnimationController1->SetTrackEnable(6, false);
		m_pSkinnedAnimationController1->SetTrackEnable(7, false);
	}
	m_pSkinnedAnimationController2->SetTrackPosition(0, 0);
	m_pSkinnedAnimationController2->SetTrackPosition(1, 0);
	m_pSkinnedAnimationController2->SetTrackPosition(2, 0);
	m_pSkinnedAnimationController2->SetTrackPosition(3, 0);

	m_pSkinnedAnimationController1->SetTrackPosition(0, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(1, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(2, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(3, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(4, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(5, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(6, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(7, 0);
}

void CEmployee::SetExitMotionAnimTrack()
{
	if (CLIENT_TYPE::OWNER == _state.clientType && m_IsFirst)
	{
		// 일단 아이들 상태 애니메이션 재생하도록 한다.
		if (m_pSkinnedAnimationController2 == nullptr) return;
		m_pSkinnedAnimationController2->SetTrackEnable(0, true);
		m_pSkinnedAnimationController2->SetTrackEnable(1, false);
		m_pSkinnedAnimationController2->SetTrackEnable(2, false);
		m_pSkinnedAnimationController2->SetTrackEnable(3, false);

		if (m_pSkinnedAnimationController1 == nullptr) return;
		m_pSkinnedAnimationController1->SetTrackEnable(0, false);
		m_pSkinnedAnimationController1->SetTrackEnable(1, false);
		m_pSkinnedAnimationController1->SetTrackEnable(2, false);
		m_pSkinnedAnimationController1->SetTrackEnable(3, false);
		m_pSkinnedAnimationController1->SetTrackEnable(4, false);
		m_pSkinnedAnimationController1->SetTrackEnable(5, false);
		m_pSkinnedAnimationController1->SetTrackEnable(6, false);
	}
	else if (CLIENT_TYPE::OWNER == _state.clientType && !m_IsFirst)
	{
		// 일단 아이들 상태 애니메이션 재생하도록 한다.
		if (m_pSkinnedAnimationController2 == nullptr) return;
		m_pSkinnedAnimationController2->SetTrackEnable(0, false);
		m_pSkinnedAnimationController2->SetTrackEnable(1, false);
		m_pSkinnedAnimationController2->SetTrackEnable(2, false);
		m_pSkinnedAnimationController2->SetTrackEnable(3, false);

		if (m_pSkinnedAnimationController1 == nullptr) return;
		m_pSkinnedAnimationController1->SetTrackEnable(0, false);
		m_pSkinnedAnimationController1->SetTrackEnable(1, false);
		m_pSkinnedAnimationController1->SetTrackEnable(2, false);
		m_pSkinnedAnimationController1->SetTrackEnable(3, false);
		m_pSkinnedAnimationController1->SetTrackEnable(4, false);
		m_pSkinnedAnimationController1->SetTrackEnable(5, false);
		m_pSkinnedAnimationController1->SetTrackEnable(6, false);
		m_pSkinnedAnimationController1->SetTrackEnable(7, true);
	}

	// 만약 그냥 다른 플레이어라면 ~
	if (CLIENT_TYPE::OTHER_PLAYER == _state.clientType)
	{
		if (m_pSkinnedAnimationController2 == nullptr) return;
		m_pSkinnedAnimationController2->SetTrackEnable(0, false);
		m_pSkinnedAnimationController2->SetTrackEnable(1, false);
		m_pSkinnedAnimationController2->SetTrackEnable(2, false);
		m_pSkinnedAnimationController2->SetTrackEnable(3, false);

		if (m_pSkinnedAnimationController1 == nullptr) return;
		m_pSkinnedAnimationController1->SetTrackEnable(0, false);
		m_pSkinnedAnimationController1->SetTrackEnable(1, false);
		m_pSkinnedAnimationController1->SetTrackEnable(2, false);
		m_pSkinnedAnimationController1->SetTrackEnable(3, false);
		m_pSkinnedAnimationController1->SetTrackEnable(4, false);
		m_pSkinnedAnimationController1->SetTrackEnable(5, false);
		m_pSkinnedAnimationController1->SetTrackEnable(6, false);
		m_pSkinnedAnimationController1->SetTrackEnable(7, true);
	}

	m_pSkinnedAnimationController2->SetTrackPosition(0, 0);
	m_pSkinnedAnimationController2->SetTrackPosition(1, 0);
	m_pSkinnedAnimationController2->SetTrackPosition(2, 0);
	m_pSkinnedAnimationController2->SetTrackPosition(3, 0);

	m_pSkinnedAnimationController1->SetTrackPosition(0, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(1, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(2, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(3, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(4, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(5, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(6, 0);
	m_pSkinnedAnimationController1->SetTrackPosition(7, 0);
}

void CEmployee::AnimTrackUpdate()
{
	switch (_state.behavior)
	{
	case (int32)PLAYER_BEHAVIOR::IDLE:
		SetIdleAnimTrack();
		if (CLIENT_TYPE::OWNER == _state.clientType)
		{
			if (GetOnMoveSound())
			{
				SetOnMoveSound(false);
				SoundManager::SoundStop(14);
			}
		}
		break;
	case (int32)PLAYER_BEHAVIOR::RUN:
		SetRunAnimTrack();
		if (!GetOnMoveSound() && CLIENT_TYPE::OWNER == _state.clientType )
		{
			SoundManager::GetInstance().PlayObjectSound(12, 14);
			SetOnMoveSound(true);
		}
		break;
	case (int32)PLAYER_BEHAVIOR::RESCUE:
	case (int32)PLAYER_BEHAVIOR::SWITCH_INTER:
		SetInteractionAnimTrack();
		break;
	case (int32)PLAYER_BEHAVIOR::ATTACKED:
		if (_attackedAnimationFrames == EMPLOYEE_ATTACKED_TIME)
		{
			SetAttackedAnimTrack();
			_attackedAnimationFrames--;
			if (CLIENT_TYPE::OWNER == _state.clientType )
				SoundManager::GetInstance().PlayObjectSound(13, 15);
		}
		else
		{
			_attackedAnimationFrames--;
			SetBehavior(PLAYER_BEHAVIOR::ATTACKED);
			if (_attackedAnimationFrames <= 0)
			{
				SetBehavior(PLAYER_BEHAVIOR::IDLE);
				_invincible = false;
				_uiCooldown = 1.0f;
			}
		}
		break;
	case (int32)PLAYER_BEHAVIOR::DOWN:
		if (_downAnimationFrames == EMPLOYEE_DOWN_TIME)
		{
			SetDownAnimTrack();
			_downAnimationFrames--;
		}
		else if (_downAnimationFrames < EMPLOYEE_DOWN_TIME)
		{
			_downAnimationFrames--;
			if (_downAnimationFrames <= 0)
			{
				if (CLIENT_TYPE::OWNER == _state.clientType )
					SoundManager::GetInstance().PlayObjectSound(7, 15);
				SetBehavior(PLAYER_BEHAVIOR::CRAWL);
			}
		}
		break;
	case (int32)PLAYER_BEHAVIOR::CRAWL:
		SetCrawlAnimTrack();
		if (CLIENT_TYPE::OWNER == _state.clientType)
			static_cast<CGameScene*>(mainGame.m_SceneManager->GetSceneByIdx(
				static_cast<int32>(CGameFramework::SCENESTATE::INGAME)))->SetFogEnabled(false);
		break;

	case (int32)PLAYER_BEHAVIOR::EXIT:
		if (GetOnMoveSound()&& CLIENT_TYPE::OWNER == _state.clientType )
		{
			SetOnMoveSound(false);
			SoundManager::SoundStop(14);
		}
		SetExitMotionAnimTrack();
		break;
	case (int32)PLAYER_BEHAVIOR::STAND:
		if (_standAnimationFrames == EMPLOYEE_STAND_TIME)
		{
			SetStandAnimTrack();
			_standAnimationFrames--;
		}
		else
		{
			if(_standAnimationFrames > 0) _standAnimationFrames--;
			if (_standAnimationFrames <= 0)
			{
				if (CLIENT_TYPE::OWNER == _state.clientType)
				{
					auto* gameScene = static_cast<CGameScene*>(mainGame.m_SceneManager->GetSceneByIdx(
						static_cast<int32>(CGameFramework::SCENESTATE::INGAME)));
					gameScene->SetCameraMode(FIRST_PERSON_CAMERA);
					gameScene->SetFogEnabled(true);
				}
				SetBehavior(PLAYER_BEHAVIOR::IDLE);
				_invincible = false;
			}
		}
		break;
	}
}

CGenerator* CEmployee::GetAvailGen()
{
	CGameScene* gs = static_cast<CGameScene*>(mainGame.m_SceneManager->GetSceneByIdx((int32)CGameFramework::SCENESTATE::INGAME));
	if (!gs)
	{
		_inGeneratorArea = false;
		_currentGeneratorIndex = -1;
		return nullptr;
	}

	if (_generatorInteractionActive && _currentGeneratorIndex >= 0)
	{
		_inGeneratorArea = true;
		return gs->GetSceneGenByIdx(_currentGeneratorIndex);
	}

	for (int i = 0; i < 3; ++i)
	{
		XMFLOAT3 distanceVec = Vector3::Subtract(_position, _switches[i].position);
		float distance = Vector3::Length(distanceVec);
		float sumRange = _boundingSphere.Radius + _switches[i].radius;
		if (distance <= sumRange)
		{
			CGenerator* targetGenerator = gs->GetSceneGenByIdx(i);

			if (targetGenerator)
			{
				if (targetGenerator->IsAvailable())
				{
					_inGeneratorArea = true;
					_currentGeneratorIndex = i;
					return targetGenerator;
				}
				else
				{
					_inGeneratorArea = false;
					return nullptr;
				}
			}
			else _inGeneratorArea = false;
		}
	}
	_inGeneratorArea = false;
	_currentGeneratorIndex = -1;
	return nullptr;
}

CEmployee* CEmployee::GetAvailEMP()
{
	CGameScene* gs = static_cast<CGameScene*>(mainGame.m_SceneManager->GetSceneByIdx((int32)CGameFramework::SCENESTATE::INGAME));
	_nearDownedPlayer = false;
	if (!gs) return nullptr;

	for (int i = 1; i < PLAYERNUM; ++i)
	{

		CEmployee* p = static_cast<CEmployee*>(gs->GetScenePlayerByIdx(i));
		if (p == nullptr || i == _state.playerIndex) continue;
		XMFLOAT3 ppos = p->GetPosition();
		ppos = Vector3::Subtract(_position, ppos);
		float dist = Vector3::Length(ppos);
		if (dist < 1.5 && p->GetBehavior() == (int32)PLAYER_BEHAVIOR::CRAWL && !p->GetRescueOn())
		{
			_nearDownedPlayer = true;
			return p;
		}

	}

	return nullptr;
}

// ============== 플레이어 상태 변경 처리 ============ 05-23
void CEmployee::PlayerAttacked()
{
	if (ApplyDamage())
	{
		_invincible = true;


		if (GetHealth() == 0)
		{
			PlayerDown();
		}
		else
		{
			SetBehavior(PLAYER_BEHAVIOR::ATTACKED);
			_attackedAnimationFrames = EMPLOYEE_ATTACKED_TIME;
		}
	}
}

void CEmployee::PlayerDown()
{
	if (CLIENT_TYPE::OWNER == _state.clientType)
	{
		auto* gameScene = static_cast<CGameScene*>(mainGame.m_SceneManager->GetSceneByIdx(
			static_cast<int32>(CGameFramework::SCENESTATE::INGAME)));
		gameScene->SetCameraMode(THIRD_PERSON_CAMERA);
		gameScene->SetFogEnabled(true);
	}

	SetBehavior(PLAYER_BEHAVIOR::DOWN);
	_downAnimationFrames = EMPLOYEE_DOWN_TIME;
}

bool CEmployee::GenTasking()
{
	CGenerator* targetGen = GetAvailGen();
	CGameScene* gameScene = static_cast<CGameScene*>(mainGame.m_SceneManager->GetSceneByIdx(
		static_cast<int32>(CGameFramework::SCENESTATE::INGAME)));

	if(targetGen)std::cout << targetGen->GetIndex() << "Available\n";

	//  F키를 눌렀고, 구하기 상호작용 중이 아닐 때
	if (InputManager::GetInstance().GetKeyBuffer(KEY_TYPE::F) > 0 && !GetIsPlayerOnRescueInter())
	{

		if (targetGen && gameScene)
		{
			if (!gameScene->SetGeneratorInteraction(_currentGeneratorIndex, true, true)) return false;
			SetGenInteraction(true); // 캐릭터 상호작용 애니메이션 재생을 활성화 한다.
			SetBehavior(PLAYER_BEHAVIOR::SWITCH_INTER);

			if (InputManager::GetInstance().GetKeyBuffer(KEY_TYPE::F) == (int8)KEY_STATUS::KEY_PRESS)
			{
				SC_EVENTPACKET packet;
				packet.eventId = _currentGeneratorIndex + (int32)EVENT_TYPE::SWITCH_ONE_START_EVENT;
				packet.size = sizeof(SC_EVENTPACKET);
				packet.type = (uint8)SC_GAME_PACKET_TYPE::GAMEEVENT;
				clientCore.DoSend(&packet);
			}
			return true;
		}
	}
	else if (!InputManager::GetInstance().GetKeyBuffer(KEY_TYPE::F))
	{
			if (InputManager::GetInstance().GetKeyBuffer(KEY_TYPE::F) == (int8)KEY_STATUS::KEY_UP)
			{
				if (GetIsPlayerOnGenInter()) // 내가 상호작용 도중이였다면
				{
					std::cout << "Cancel\n";
					SetGenInteraction(false);
					SetBehavior(PLAYER_BEHAVIOR::IDLE);
					if (gameScene)
						gameScene->SetGeneratorInteraction(_currentGeneratorIndex, false, true);
					//========= 패킷 송신 처리 ==============
					SC_EVENTPACKET packet;
					packet.eventId = _currentGeneratorIndex + (int32)EVENT_TYPE::SWITCH_ONE_END_EVENT;
					packet.size = sizeof(SC_EVENTPACKET);
					packet.type = (uint8)SC_GAME_PACKET_TYPE::GAMEEVENT;
					clientCore.DoSend(&packet);
					_currentGeneratorIndex = -1;
				}
			}
			SetGenInteraction(false);
			SetBehavior(PLAYER_BEHAVIOR::IDLE);
	}
	return false;
}

bool CEmployee::RescueTasking()
{

	// 구조 중인 플레이어가 아닌 쓰러진 플레이어의 인덱스를 가져온다.

	// 1. 현재 켜져 있지 않고, 다른 플레이어에 의해 상호작용 중이지 않은 발전기를 가져온다.
	CEmployee* targetPlayer = GetAvailEMP();

		// 구하는 이벤트에 관한 패킷을 전송하도록 한다.
	if (InputManager::GetKeyBuffer(KEY_TYPE::E) == (int8)KEY_STATUS::KEY_PRESS && !GetIsPlayerOnRescueInter())
	{

		if (targetPlayer)
		{
			SetBehavior(PLAYER_BEHAVIOR::RESCUE);
			SetRescueInteraction(true);
			targetPlayer->_beingRescued = true;
			SC_EVENTPACKET packet;
			packet.eventId = targetPlayer->GetPlayerIndex() + (int32)EVENT_TYPE::RESCUE_PLAYER_ONE;
			packet.size = sizeof(SC_EVENTPACKET);
			packet.type = (uint8)SC_GAME_PACKET_TYPE::GAMEEVENT;
			clientCore.DoSend(&packet);
			_rescuingEmployeeIndex = targetPlayer->GetPlayerIndex();
		}
		return true;
	}
	else if (!InputManager::GetInstance().GetKeyBuffer(KEY_TYPE::E))
	{
		if (InputManager::GetKeyBuffer(KEY_TYPE::E) == (int8)KEY_STATUS::KEY_UP)
		{
			if (GetIsPlayerOnRescueInter())
			{

				CEmployee* rescuedPlayer = static_cast<CEmployee*>(static_cast<CGameScene*>(mainGame.m_SceneManager->GetSceneByIdx(3))->GetScenePlayerByIdx(_rescuingEmployeeIndex));

				SetRescueInteraction(false);

				if (rescuedPlayer)
				{
					if (rescuedPlayer->_beingRescued)
					{
						rescuedPlayer->_beingRescued = false;
						rescuedPlayer->_rescueGauge = 0;
					}
					SC_EVENTPACKET packet;
					packet.eventId = rescuedPlayer->GetPlayerIndex() + (int32)EVENT_TYPE::RESCUE_CANCEL_PLAYER_ONE;
					packet.size = sizeof(SC_EVENTPACKET);
					packet.type = (uint8)SC_GAME_PACKET_TYPE::GAMEEVENT;
					clientCore.DoSend(&packet);
					_rescuingEmployeeIndex = -1;
				}
			}
			SetRescueInteraction(false);
			SetBehavior(PLAYER_BEHAVIOR::IDLE);
		}
	}

	return false;
}

