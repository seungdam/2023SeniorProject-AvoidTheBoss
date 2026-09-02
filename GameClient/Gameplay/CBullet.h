#pragma once

#include <utility>

#include "../Rendering/GameObject.h"
#include "TracerVisualState.h"

class CBullet : public CGameObject
{
public:
	static constexpr float MaxRange = 3.7f;
	static constexpr std::size_t PoolCapacity = 1;
	static constexpr float SpeedUnitsPerSecond = 18.0f;

private:
	atb::client::TracerVisualState _tracer{
		SpeedUnitsPerSecond,
		MaxRange
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
		if (m_pHitEffect)
		{
			m_pHitEffect->ResetState();
		}
	}
};
