#pragma once

class SPlayer;

class SGenerator
{
public:
	XMFLOAT3 _pos;
	Atomic<bool> _IsOnInteraction = false;
	Atomic<bool> _IsActive = false;
	float _maxGuage = 100;
	float _curGuage = 0.f;
	float _GuageOffset = 1.0f;
	std::mutex _lock;
	float _ActiveRadius = 1.25f;
	int32 _idx;
public:
	SGenerator()
	{
		_pos = XMFLOAT3(0.0f, 0.0f, 0.0f);
		_idx = 0;
	}
	~SGenerator() {}

	void GenInteractionOn(bool value)
	{
		_IsOnInteraction = value;
	}

	void GenActivate(bool value)
	{
		_IsActive = value;
	}

	void ResetState()
	{
		_IsActive = false;
		_IsOnInteraction = false;
		_curGuage = 0.f;
	}

	bool CanInteraction(const SPlayer& player) const;

};
