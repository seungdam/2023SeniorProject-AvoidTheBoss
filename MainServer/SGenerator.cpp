#include "pch.h"
#include "SGenerator.h"
#include "SPlayer.h"

bool SGenerator::CanInteraction(const SPlayer& player) const
{
	XMFLOAT3 myPlayerpos = player.GetPosition();
	XMFLOAT3 distance = Vector3::Subtract(myPlayerpos,_pos);
	float range = 0.5f + _ActiveRadius;
	if (Vector3::Length(distance) <= range) return true;
	return false;
}
