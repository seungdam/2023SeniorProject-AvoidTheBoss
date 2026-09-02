#pragma once
#include "Player.h"


class CGenerator;
class CGameScene;

class CEmployee : public CPlayer
{
public:
	static constexpr int32 AttackedAnimationFrameCount = 30;
	static constexpr int32 DownAnimationFrameCount = 20;
	static constexpr int32 StandAnimationFrameCount = 30;

	bool _generatorInteractionActive = false; // F키를 눌렀다 땠는지 확인하는 용도
	bool _rescueInteractionActive = false;
public:
	bool _invincible = false;
	float _uiCooldown = 1.0f;

private:
	CGameScene& _ownerScene;
	bool _inGeneratorArea = false;
	bool _nearDownedPlayer = false; // Down된 플레이어와 인접해 있는가?
	//bool m_bIsDown
protected:
	float _maxRescueGauge = 100;
	float _rescueGauge = 0;
	float _rescueSpeed = 10.0f;
	int32 _rescuingEmployeeIndex = -1;
	bool _beingRescued = false;
private:
	int32 _currentGeneratorIndex = -1;
public:
	int32 _deathCount = 0;
	int32 _activatedGeneratorCount = 0;

	int32 _attackedAnimationFrames = 0;
	int32 _downAnimationFrames = 0;
	int32 _standAnimationFrames = 0;
public:
	CEmployee(ID3D12Device5* pd3dDevice,
		ID3D12GraphicsCommandList4
		* pd3dCommandList,
		ID3D12RootSignature* pd3dGraphicsRootSignature,
		CHARACTER_TYPE nType,
		CGameScene& ownerScene);
	virtual ~CEmployee();

	// ========== 플레이어 조작 관련 ===================
	virtual uint8 ProcessInput();
	virtual void Move(const int16& dwDirection, float fDistance);
	virtual void Update(float fTimeElapsed, CLIENT_TYPE ptype);
	virtual void LateUpdate(float fTimeElapsed, CLIENT_TYPE ptype);

	void SetGenInteraction(bool value) { _generatorInteractionActive = value; }
	bool GetIsPlayerOnGenInter() const noexcept { return _generatorInteractionActive; }
	void SetRescueInteraction(bool value) { _rescueInteractionActive = value; }
	bool GetIsPlayerOnRescueInter() const noexcept { return _rescueInteractionActive; }

	// ============= 애니메이션 트랙 셋팅 관련 ============

	bool IsMovable()
	{
		return (_state.behavior == (int32)PLAYER_BEHAVIOR::RESCUE || _state.behavior == (int32)PLAYER_BEHAVIOR::SWITCH_INTER || _state.behavior == (int32)PLAYER_BEHAVIOR::CRAWL
			|| _state.behavior == (int32)PLAYER_BEHAVIOR::EXIT);
	}
	bool IsSeMiBehavior() // 스탠드, 크라울, 다운 상태
	{
		return (_state.behavior == (int32)PLAYER_BEHAVIOR::DOWN  || _state.behavior == (int32)PLAYER_BEHAVIOR::STAND);
	}

	// 깨우기
	void RescueOn(bool value)
	{
		if (_beingRescued != value)
		{
			_beingRescued = value;
		}
	}
	void ResetRescueGuage() { _rescueGauge = 0; }
	bool GetRescueOn() const noexcept { return _beingRescued; }

	virtual void ResetState()
	{
		CPlayer::ResetState();
		_attackedAnimationFrames = 0;
		_downAnimationFrames = 0;
		_standAnimationFrames = 0;

		_generatorInteractionActive = false; // F키를 눌렀다 땠는지 확인하는 용도
		_rescueInteractionActive = false;

		_invincible = false;
		_uiCooldown = 1.0f;


		_inGeneratorArea = false;
		_currentGeneratorIndex = -1;
		_deathCount = 0;
		_activatedGeneratorCount = 0;
		_nearDownedPlayer = false; // Down된 플레이어와 인접해 있는가?


	    _rescueGauge = 0;
		_rescuingEmployeeIndex = -1;
		_beingRescued = false;

		_moveSoundActive = false;
		m_bEmpExit = false;

	}

	// 총알 맞고 쓰러짐 x,2
	// 피격 2,4
	// 느리게 걷기 2,4

	//down (총알 맞고 쓰러짐) x
	//down_idle (쓰러진 상태) ㅇ
	//slow_walk,crawl (절뚝거리기) x

	void SetIdleAnimTrack();	// 걷기 0
	void SetRunAnimTrack(); 	// 달리기 1
	void SetDownAnimTrack();	// 총알 맞고 쓰러짐 x,2
	void SetAttackedAnimTrack();// 절뚝거리기 2,4
	void SetCrawlAnimTrack();	// 쓰러진 상태 x,3
	void SetStandAnimTrack(); 	// 일어나기 x,5
	void SetInteractionAnimTrack(); 	// 발전기 상호작용 3,6
	void SetExitMotionAnimTrack();

	virtual void AnimTrackUpdate();


	// ================ 캐릭터 상태 반환 ============ 05-23 추가함수
public: // 05-23 추가 함수
	void PlayerAttacked();
	void PlayerDown();
	bool GenTasking();
	bool RescueTasking();


	bool GetIsInGenArea() const noexcept { return _inGeneratorArea; }
	bool HasRescueTarget() const noexcept { return _nearDownedPlayer; }
	float GetRescueGauge() const noexcept { return _rescueGauge; }
	int32 GetRescuingEmployeeIndex() const noexcept { return _rescuingEmployeeIndex; }
	int32 GetCurrentGeneratorIndex() const noexcept { return _currentGeneratorIndex; }
	CGenerator* GetAvailGen();
	CEmployee* GetAvailEMP();
public: // 05-24 추가함수
	GEN_INFO _switches[3];
};
