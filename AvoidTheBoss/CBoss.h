#pragma once
#include "Player.h"
class CBullet;

class CBoss : public CPlayer
{
	friend class ClientSession;
private:
	CGameObject* _rightHand = NULL;
private: // 06/11 --> 06/13 추가
	int32 _runAttackAnimationTime = 0;
	int32 _standAttackAnimationTime = 0;
	bool  _isAttacking = false ;
public:
	int _bulletCount = 50;
	CBullet* _bullet = NULL;

	CBoss(ID3D12Device5* pd3dDevice, ID3D12GraphicsCommandList4* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature);
	virtual ~CBoss();

	virtual void Rotate(float x, float y, float z);
	virtual void PrepareAnimate();
	// ========== 플레이어 조작 관련 ============
	virtual uint8 ProcessInput(); // 04-29 추가
	virtual void Move(const int16& dwDirection, float fDistance);
	virtual void Update(float fTimeElapsed, CLIENT_TYPE ptype);
	virtual void LateUpdate(float ,CLIENT_TYPE);


	// ============= 애니메이션 트랙 셋팅 관련 ============ // 05-22 추가 함수
	void AnimationLogicUpdate();
	void AimationStateUpdate();

	void SetIdleAnimTrack();
	void SetRunAnimTrack();
	void SetAttackAnimTrack();
	void SetRunAttackAnimTrack();
	virtual void AnimTrackUpdate();

	void SetOnAttack(bool value) { _isAttacking = value; }
	bool GetOnAttack() { return _isAttacking; }
	void SetAttackAnimTime()
	{
		_standAttackAnimationTime = 0;
	}
	void SetRunAttackAnimTime()
	{
		_runAttackAnimationTime = 0;
	}

	virtual void ResetState();
	void SetAttackAnimOtherClient();
};
