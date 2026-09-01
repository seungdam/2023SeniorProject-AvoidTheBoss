#pragma once

#include <cstddef>

namespace atb::client
{
struct TracerVector3 final
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};

class TracerVisualState final
{
public:
	TracerVisualState(float speedUnitsPerSecond, float maxRange) noexcept;

	[[nodiscard]] bool Spawn(TracerVector3 origin, TracerVector3 direction) noexcept;
	// True only on the tick that reaches the configured maximum range.
	[[nodiscard]] bool Tick(float deltaSeconds) noexcept;
	void Reset() noexcept;

	[[nodiscard]] bool IsActive() const noexcept { return _active; }
	[[nodiscard]] const TracerVector3& Origin() const noexcept { return _origin; }
	[[nodiscard]] const TracerVector3& Position() const noexcept { return _position; }
	[[nodiscard]] const TracerVector3& Direction() const noexcept { return _direction; }
	[[nodiscard]] float TravelledDistance() const noexcept { return _travelledDistance; }
	[[nodiscard]] float Speed() const noexcept { return _speed; }
	[[nodiscard]] float MaxRange() const noexcept { return _maxRange; }

private:
	float _speed = 0.0f;
	float _maxRange = 0.0f;
	TracerVector3 _origin{};
	TracerVector3 _position{};
	TracerVector3 _direction{};
	float _travelledDistance = 0.0f;
	bool _active = false;
};
}
