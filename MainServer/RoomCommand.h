#pragma once

#include "../Shared/Protocol.h"

#include <array>

enum class RoomCommandType : uint8
{
	Create,
	Enter,
	SetReady,
	Exit,
	Disconnected,
	Resume,
	GamePacket,
};

struct RoomCommand
{
	RoomCommandType type = RoomCommandType::Create;
	int32 sid = -1;
	int32 roomNum = -1;
	bool isReady = false;
	uint64 resumeToken = 0;
	bool reliable = false;
	uint8 packetSize = 0;
	std::array<char, sizeof(_CHAT)> packet{};
};
