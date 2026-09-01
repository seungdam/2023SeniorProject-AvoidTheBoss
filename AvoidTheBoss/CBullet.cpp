#include "pch.h"
#include "GameObject.h"
#include "CBullet.h"

CBullet::CBullet() = default;
CBullet::~CBullet() = default;

void CBullet::SetHitEffect(CHitEffect* hitEffect) noexcept
{
	m_pHitEffect = hitEffect;
	_initialTransform = m_xmf4x4ToParent;
	if (m_pHitEffect)
	{
		m_pHitEffect->CaptureInitialTransform();
	}
}

void CBullet::Update(float fTimeElapsed)
{
	if (_tracer.IsActive())
	{
		const bool reachedEnd = _tracer.Tick(fTimeElapsed);
		const auto& position = _tracer.Position();
		SetPosition(XMFLOAT3(position.x, position.y, position.z));

		if (reachedEnd && m_pHitEffect)
		{
			m_pHitEffect->SetPosition(position.x, 1.1f, position.z);
			m_pHitEffect->SetOnHit(true);
		}
	}

	if (m_pHitEffect)
	{
		m_pHitEffect->Update(fTimeElapsed);
	}
}

bool CBullet::Spawn(const XMFLOAT3& origin, const XMFLOAT3& direction) noexcept
{
	const bool spawned = _tracer.Spawn(
		{ origin.x, origin.y, origin.z },
		{ direction.x, direction.y, direction.z });
	if (spawned)
	{
		SetPosition(origin);
	}
	return spawned;
}
