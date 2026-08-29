#include "PlayerState.h"

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <cassert>

int main()
{
	PlayerState state;
	assert(state.playerType == PLAYER_TYPE::NONE);
	assert(state.clientType == CLIENT_TYPE::OTHER_PLAYER);
	assert(state.sessionId == -1);
	assert(state.playerIndex == -1);
	assert(state.health == PlayerState::MaxHealth);
	assert(state.behavior == 0);
	assert(!state.hidden);

	state.SetHealth(PlayerState::MaxHealth + 10);
	assert(state.health == PlayerState::MaxHealth);
	assert(state.ApplyDamage());
	assert(state.health == PlayerState::MaxHealth - 1);
	state.SetHealth(-10);
	assert(state.health == 0);
	assert(!state.ApplyDamage());
	state.RestoreHealth();
	assert(state.health == PlayerState::MaxHealth);

	state.playerType = PLAYER_TYPE::BOSS;
	state.clientType = CLIENT_TYPE::OWNER;
	state.sessionId = 7;
	state.playerIndex = 2;
	state.health = 1;
	state.behavior = 9;
	state.hidden = true;
	state.ResetTransient();
	assert(state.playerType == PLAYER_TYPE::BOSS);
	assert(state.clientType == CLIENT_TYPE::OWNER);
	assert(state.sessionId == 7);
	assert(state.playerIndex == 2);
	assert(state.health == PlayerState::MaxHealth);
	assert(state.behavior == 0);
	assert(!state.hidden);

	return 0;
}
