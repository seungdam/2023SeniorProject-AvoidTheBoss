#pragma once

#include "../Shared/Types.h"

namespace atb
{
enum class SceneId : int32
{
	Title = 0,
	Lobby,
	Room,
	InGame,
	Result,
	Count
};

[[nodiscard]] constexpr int32 SceneIndex(const SceneId scene) noexcept
{
	return static_cast<int32>(scene);
}

static_assert(SceneIndex(SceneId::Title) == 0);
static_assert(SceneIndex(SceneId::InGame) == 3);
static_assert(SceneIndex(SceneId::Result) == 4);
static_assert(SceneIndex(SceneId::Count) == 5);
}
