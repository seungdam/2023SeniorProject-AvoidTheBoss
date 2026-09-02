#pragma once
#include "../Rendering/GameObject.h"
#include "PlayerState.h"

enum class CHARACTER_TYPE: uint8
{
	BOSS = 0, YELLOW_EMP = 1, MASK_EMP, CAP_EMP, GOGGLE_EMP, COUNT
};

class CPlayer : public CGameObject
{
protected:
	PlayerState _state;

	XMFLOAT3 _position = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3 _right = XMFLOAT3(1.0f, 0.0f, 0.0f);
	XMFLOAT3 _up = XMFLOAT3(0.0f, 1.0f, 0.0f);
	XMFLOAT3 _look = XMFLOAT3(0.0f, 0.0f, 1.0f);
	XMFLOAT3 _scale = XMFLOAT3(1.0f, 1.0f, 1.0f);

	float _pitch = 0.0f;
	float _yaw = 0.0f;
	float _roll = 0.0f;

	XMFLOAT3 _velocity = XMFLOAT3(0.0f, 0.0f, 0.0f); // 플레이어 속도
	XMFLOAT3 _gravity = XMFLOAT3(0.0f, 0.0f, 0.0f);  // 중력
	float _friction = 0.0f;                            // 마찰력

public:
	BoundingSphere _boundingSphere;
	CHARACTER_TYPE _characterType;

	bool _moveSoundActive = false;
	virtual void SetOnMoveSound(bool bOnMoveSound)
	{
		_moveSoundActive = bOnMoveSound;
	}
	virtual bool GetOnMoveSound()
	{
		return _moveSoundActive;
	}


public:
	CPlayer();
	virtual ~CPlayer();
	[[nodiscard]] const PlayerState& State() const noexcept { return _state; }
	[[nodiscard]] PLAYER_TYPE GetPlayerType() const noexcept { return _state.playerType; }
	void SetPlayerType(PLAYER_TYPE playerType) noexcept { _state.playerType = playerType; }
	[[nodiscard]] CLIENT_TYPE GetClientType() const noexcept { return _state.clientType; }
	void SetClientType(CLIENT_TYPE clientType) noexcept { _state.clientType = clientType; }
	[[nodiscard]] int16 GetSessionId() const noexcept { return _state.sessionId; }
	void SetPlayerSid(int16 sid) noexcept { _state.sessionId = sid; }
	[[nodiscard]] int32 GetPlayerIndex() const noexcept { return _state.playerIndex; }
	void SetPlayerIndex(int32 playerIndex) noexcept { _state.playerIndex = playerIndex; }
	[[nodiscard]] int32 GetHealth() const noexcept { return _state.health; }
	void SetHealth(int32 health) noexcept { _state.SetHealth(health); }
	[[nodiscard]] bool ApplyDamage() noexcept { return _state.ApplyDamage(); }
	void RestoreHealth() noexcept { _state.RestoreHealth(); }
	[[nodiscard]] bool IsHidden() const noexcept { return _state.hidden; }
	void SetHidden(bool hidden) noexcept { _state.hidden = hidden; }

	XMFLOAT3 GetPosition() const { return _position; }
	XMFLOAT3 GetLookVector() const { return _look; }
	XMFLOAT3 GetUpVector() const { return _up; }
	XMFLOAT3 GetRightVector() const { return _right; }
	void SetDirection(const XMFLOAT3& look)
	{
		_look = look;
		_right = Vector3::CrossProduct(_up, _look, true);
	}
	void SetVelocity(const XMFLOAT3& xmf3Velocity) { _velocity = xmf3Velocity; }
	void SetPosition(const XMFLOAT3& xmf3Position)
	{
		_position = xmf3Position;
	}
	void SetScale(const XMFLOAT3& xmf3Scale) { _scale = xmf3Scale; }
	const XMFLOAT3& GetVelocity() const { return _velocity; }
	float GetYaw() const { return _yaw; }
	float GetPitch() const { return _pitch; }
	float GetRoll() const { return _roll; }

	//플레이어를 회전하는 함수이다.
	virtual void Rotate(float x, float y, float z);

	//플레이어의 위치와 회전축으로부터 월드 변환 행렬을 생성하는 함수이다.
	virtual void OnPrepareRender();
	//플레이어의 카메라가 3인칭 카메라일 때 플레이어(메쉬)를 렌더링한다.
	virtual void Render(ID3D12GraphicsCommandList4* pd3dCommandList, CCamera* pCamera, bool bRaster);

public: //04-29 추가함수
	virtual uint8 ProcessInput() { return 0; };
	virtual void SetAnimationTrack(int32 num) {};
	virtual void Move(const int16& dwDirection, float fDistance);
	virtual void Update(float fTimeElapsed, CLIENT_TYPE ptype);
	virtual void LateUpdate() {};
	// 05-22 추가 함수
	virtual void AnimTrackUpdate(float ,CLIENT_TYPE) {};
	virtual void SetBehavior(PLAYER_BEHAVIOR behavior) noexcept { _state.behavior = static_cast<int32>(behavior); }
	[[nodiscard]] virtual int32 GetBehavior() const noexcept { return _state.behavior; }
	virtual void ResetState();
	virtual void BuildObjects(ID3D12Device5* pd3dDevice, ID3D12GraphicsCommandList4
		* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature) {}
};

class CVirtualPlayer : public CPlayer
{
public:
	CVirtualPlayer();
	CVirtualPlayer(ID3D12Device5* pd3dDevice, ID3D12GraphicsCommandList4* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature);
	virtual ~CVirtualPlayer();
public:
	virtual void Move(DWORD dwDirection, float fDistance);
	virtual void Animate(float fTimeElapsed);
	virtual void Update(float fTimeElapsed);
	virtual void BuildObjects(ID3D12Device5* pd3dDevice, ID3D12GraphicsCommandList4* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature);
	virtual uint8 ProcessInput();
};

struct GEN_INFO
{
	XMFLOAT3 position;
	float radius; //raderArea
};

class CSoundCallbackHandler : public CAnimationCallbackHandler
{
public:
	CSoundCallbackHandler() { }
	~CSoundCallbackHandler() { }

public:
	virtual void HandleCallback(void* pCallbackData, float fTrackPosition);
};

