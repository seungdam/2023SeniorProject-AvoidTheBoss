#pragma once

#include "../Shared/Types.h"

enum class RoomCommandType : uint8
{
	Create,
	Enter,
	SetReady,
	Exit,
};

struct RoomCommand
{
	RoomCommandType type = RoomCommandType::Create;
	int32 sid = -1;
	int16 roomNum = -1;
	bool isReady = false;
};
