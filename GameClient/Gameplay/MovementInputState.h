#pragma once

#include "Types.h"

#include <cmath>
#include <optional>

namespace atb
{
	struct MovementInputSample
	{
		uint8 key = 0;
		float aimX = 0.0f;
		float aimZ = 1.0f;
	};

	class MovementInputState final
	{
	public:
		void Sample(const uint8 key, const float aimX, const float aimZ) noexcept
		{
			_current = { key, aimX, aimZ };
		}

		[[nodiscard]] const MovementInputSample& Current() const noexcept { return _current; }

		[[nodiscard]] bool Pending() const noexcept
		{
			return !_lastSent || _lastSent->key != _current.key ||
				std::abs(_lastSent->aimX - _current.aimX) > AimEpsilon ||
				std::abs(_lastSent->aimZ - _current.aimZ) > AimEpsilon;
		}

		void CommitSent() noexcept { _lastSent = _current; }
		void Invalidate() noexcept { _lastSent.reset(); }

	private:
		static constexpr float AimEpsilon = 1.0e-5f;

		MovementInputSample _current;
		std::optional<MovementInputSample> _lastSent;
	};
}
