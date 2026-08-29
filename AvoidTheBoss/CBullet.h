#pragma once

#include <utility>

#include "GameObject.h"
#include "TracerVisualState.h"

#define BULLET_DISTANCE 3.7f
#define BULLET_NUMBER 1
#define BULLET_SPEED_UNITS_PER_SECOND 18.0f

class CBullet : public CGameObject
{
private:
	atb::client::TracerVisualState _tracer{
		BULLET_SPEED_UNITS_PER_SECOND,
		BULLET_DISTANCE
	};
	bool _spawnRequested = false;
	XMFLOAT4X4 _initialTransform = Matrix4x4::Identity();

public:
	// Observer. CHitEffect is owned by CHitEffectObjectsShader.
	CHitEffect* m_pHitEffect = nullptr;

	CBullet();
	~CBullet() override;

	void Update(float fTimeElapsed) override;
	[[nodiscard]] bool Spawn(const XMFLOAT3& origin, const XMFLOAT3& direction) noexcept;
	void RequestSpawn() noexcept { _spawnRequested = true; }
	[[nodiscard]] bool ConsumeSpawnRequest() noexcept
	{
		return std::exchange(_spawnRequested, false);
	}
	[[nodiscard]] bool GetOnShoot() const noexcept { return _tracer.IsActive(); }

	CHitEffect* GetHitEffect() const noexcept { return m_pHitEffect; }
	void SetHitEffect(CHitEffect* hitEffect) noexcept;

	void ResetState() override
	{
		_spawnRequested = false;
		_tracer.Reset();
		m_xmf4x4ToParent = _initialTransform;
		UpdateTransform(nullptr);
		if (m_pHitEffect) m_pHitEffect->ResetState();
	}
};
