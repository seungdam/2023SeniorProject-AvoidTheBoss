#include "TracerVisualState.h"

#include <algorithm>
#include <cmath>

namespace
{
[[nodiscard]] bool IsFinite(atb::client::TracerVector3 value) noexcept
{
	return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
}

namespace atb::client
{
TracerVisualState::TracerVisualState(float speedUnitsPerSecond, float maxRange) noexcept
	: _speed(speedUnitsPerSecond), _maxRange(maxRange)
{
}

bool TracerVisualState::Spawn(TracerVector3 origin, TracerVector3 direction) noexcept
{
	Reset();

	const float directionLength = std::hypot(direction.x, direction.y, direction.z);
	if (!IsFinite(origin) || !IsFinite(direction) || !std::isfinite(directionLength) ||
		directionLength <= 0.0f || !std::isfinite(_speed) || _speed <= 0.0f ||
		!std::isfinite(_maxRange) || _maxRange <= 0.0f)
	{
		return false;
	}

	_origin = origin;
	_position = origin;
	_direction = {
		direction.x / directionLength,
		direction.y / directionLength,
		direction.z / directionLength
	};
	_active = true;
	return true;
}

bool TracerVisualState::Tick(float deltaSeconds) noexcept
{
	if (!_active || !std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f)
	{
		return false;
	}

	const float remainingDistance = _maxRange - _travelledDistance;
	const float requestedDistance = _speed * deltaSeconds;
	const float stepDistance = std::isfinite(requestedDistance)
		? (requestedDistance < remainingDistance ? requestedDistance : remainingDistance)
		: remainingDistance;

	_travelledDistance += stepDistance;
	_position = {
		_origin.x + _direction.x * _travelledDistance,
		_origin.y + _direction.y * _travelledDistance,
		_origin.z + _direction.z * _travelledDistance
	};

	if (_travelledDistance >= _maxRange)
	{
		_active = false;
		return true;
	}

	return false;
}

void TracerVisualState::Reset() noexcept
{
	_origin = {};
	_position = {};
	_direction = {};
	_travelledDistance = 0.0f;
	_active = false;
}
}
