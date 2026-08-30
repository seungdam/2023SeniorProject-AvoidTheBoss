#pragma once

#include "MatchLease.h"
#include "../Shared/Protocol.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <deque>
#include <mutex>
#include <utility>

enum class LobbyCommandType : uint8
{
	Create,
	Enter,
	SetReady,
	Exit,
	Disconnected,
	Resume,
};

struct LobbyCommand
{
	LobbyCommandType type = LobbyCommandType::Create;
	int32 sid = -1;
	int32 roomNum = -1;
	bool isReady = false;
	uint64 resumeToken = 0;
};

struct GameCommand
{
	int32 sid = -1;
	int32 roomNum = -1;
	MatchLease lease{};
	uint8 packetSize = 0;
	std::array<char, sizeof(_CHAT)> packet{};
};

[[nodiscard]] constexpr bool IsSessionUnbound(const int32 roomNumber) noexcept
{
	return roomNumber == -1;
}

class LobbyCommandQueue
{
public:
	explicit LobbyCommandQueue(const std::size_t capacity) : _capacity(capacity) {}

	[[nodiscard]] bool TryEnqueue(LobbyCommand command)
	{
		std::lock_guard lock(_lock);
		if (_commands.size() >= _capacity && command.type != LobbyCommandType::Disconnected)
			return false;
		// ponytail: Disconnect may cross this soft cap once per retired session;
		// add an active-session admission limit before imposing a hard lifecycle cap.
		_commands.push_back(std::move(command));
		return true;
	}

	[[nodiscard]] std::deque<LobbyCommand> Drain(const std::size_t maxCount)
	{
		std::deque<LobbyCommand> drained;
		std::lock_guard lock(_lock);
		const std::size_t count = std::min(_commands.size(), maxCount);
		for (std::size_t i = 0; i < count; ++i)
		{
			drained.push_back(std::move(_commands.front()));
			_commands.pop_front();
		}
		return drained;
	}

private:
	const std::size_t _capacity;
	std::mutex _lock;
	std::deque<LobbyCommand> _commands;
};

class GameCommandQueue
{
public:
	explicit GameCommandQueue(const std::size_t capacity) : _capacity(capacity) {}

	[[nodiscard]] bool TryEnqueue(GameCommand command)
	{
		std::lock_guard lock(_lock);
		if (_commands.size() >= _capacity) return false;
		_commands.push_back(std::move(command));
		return true;
	}

	[[nodiscard]] std::deque<GameCommand> Drain(const std::size_t maxCount)
	{
		std::deque<GameCommand> drained;
		std::lock_guard lock(_lock);
		const std::size_t count = std::min(_commands.size(), maxCount);
		for (std::size_t i = 0; i < count; ++i)
		{
			drained.push_back(std::move(_commands.front()));
			_commands.pop_front();
		}
		return drained;
	}

private:
	const std::size_t _capacity;
	std::mutex _lock;
	std::deque<GameCommand> _commands;
};
