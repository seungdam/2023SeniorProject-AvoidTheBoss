#pragma once
#include "../Rendering/GameObject.h"
#include "GeneratorState.h"

class CGenerator : public CGameObject
{
public:
	static constexpr int ButtonAnimationFrameCount = 50;
	static constexpr int PipeAnimationFrameCount = 16;
	static constexpr int BodyAnimationFrameCount = 16;

	float radius = 0.0f;
	int m_nPipe = 3;
	XMFLOAT4 	xmf4NormalVector;

	CGameObject** m_ppPipe = NULL;
	CGameObject* m_pButton = NULL;
	CGameObject* m_pBody = NULL;
private:
	GeneratorState _state;

public:
	bool m_nPipeStartAnimation[3];
public:
	int  m_nButtonAnimationCount = ButtonAnimationFrameCount;
	int  m_nGenerPipeAnimationCount[3];
	int  m_nGenerBodyAnimationCount = 0;
	float m_AnimationDegree = 0.0f;
public:
	CGenerator();
	CGenerator(const CGenerator&) = delete;
	CGenerator& operator=(const CGenerator&) = delete;
	virtual ~CGenerator() { delete[] m_ppPipe; };
	float GetRadius() { return radius; }
	void SetIndex(int index) noexcept { _state.index = index; }
	int GetIndex() const noexcept { return _state.index; }
	float GetProgress() const noexcept { return _state.progress; }

	virtual void SetNormalVector();
	void LogicUpdate();
	bool BeginInteraction(bool advancesProgress) noexcept { return _state.BeginInteraction(advancesProgress); }
	bool EndInteraction() noexcept { return _state.EndInteraction(); }
	bool Activate() noexcept { return _state.Activate(); }
	GeneratorTransition TickState(float deltaSeconds) noexcept { return _state.Tick(deltaSeconds); }
	bool IsAvailable() const noexcept { return _state.IsAvailable(); }
	bool IsAnimating() const noexcept { return _state.IsAnimating(); }
	bool IsActivated() const noexcept { return _state.IsActivated(); }

	virtual void OnPrepareAnimate();
	virtual void Animate(float fTimeElapsed);

	void PipelineAnimate(float fTimeElapsed);
	void BodyAnimate(float fTimeElapsed);

	virtual void ResetState()
	{
		_state.Reset();

		m_ppPipe[0]->SetPosition(0.09313595f, 1.839607f,-0.1005933f);
		m_ppPipe[1]->SetPosition(0.09313595f, 1.839607f,-0.4108887f);
		m_ppPipe[2]->SetPosition(0.09313595f, 1.839607f,-0.7232422f);
		m_pBody->SetPosition(0.0f, 0.0f, 0.0f);

		for (int i = 0; i < m_nPipe; i++)
		{
			m_nPipeStartAnimation[i] = false;
			m_nGenerPipeAnimationCount[i] = 0;
		}
		m_nGenerBodyAnimationCount = 0;

	};
};


