#pragma once

#include "../Shared/Types.h"

enum class RoomCommandType : uint8
{
	Create,
	Enter,
	SetReady,
	Exit,
	Disconnected,
	Resume,
};

struct RoomCommand
{
	RoomCommandType type = RoomCommandType::Create;
	int32 sid = -1;
	int32 roomNum = -1;
	bool isReady = false;
	uint64 resumeToken = 0;
};
