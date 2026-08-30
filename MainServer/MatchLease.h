#pragma once

#include "../Shared/Types.h"

struct MatchLease
{
	uint64 memberGeneration = 0;
	uint64 matchGeneration = 0;

	[[nodiscard]] constexpr bool Matches(const uint64 currentMemberGeneration,
		const uint64 currentMatchGeneration) const noexcept
	{
		return memberGeneration != 0 && matchGeneration != 0 &&
			memberGeneration == currentMemberGeneration && matchGeneration == currentMatchGeneration;
	}
};
