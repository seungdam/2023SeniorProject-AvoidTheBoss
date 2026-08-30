#include "RoomCommand.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <iostream>
#include <thread>
#include <type_traits>
#include <vector>

namespace
{
	LobbyCommand Lobby(const LobbyCommandType type, const int32 sid)
	{
		LobbyCommand command{};
		command.type = type;
		command.sid = sid;
		return command;
	}

	GameCommand Game(const int32 sid)
	{
		GameCommand command{};
		command.sid = sid;
		return command;
	}

	bool Check(const bool condition, const char* message)
	{
		if (condition) return true;
		std::cerr << message << '\n';
		return false;
	}
}

int main()
{
	static_assert(!std::is_same_v<LobbyCommand, GameCommand>);
	constexpr std::size_t capacity = 3;

	if (!Check(IsSessionUnbound(-1) && !IsSessionUnbound(0) && !IsSessionUnbound(99),
		"room binding admission did not reject every bound room number")) return 1;
	if (!Check(MatchLease{ 11, 22 }.Matches(11, 22),
		"an exact match lease was rejected") ||
		!Check(!MatchLease{ 11, 22 }.Matches(12, 22),
			"a stale member generation was accepted") ||
		!Check(!MatchLease{ 11, 22 }.Matches(11, 23),
			"a stale match generation was accepted") ||
		!Check(!MatchLease{ 0, 22 }.Matches(0, 22) && !MatchLease{ 11, 0 }.Matches(11, 0),
			"an inactive match lease was accepted")) return 1;

	LobbyCommandQueue lobbyCommands(capacity);
	(void)lobbyCommands.TryEnqueue(Lobby(LobbyCommandType::Create, 1));
	(void)lobbyCommands.TryEnqueue(Lobby(LobbyCommandType::Enter, 2));
	(void)lobbyCommands.TryEnqueue(Lobby(LobbyCommandType::SetReady, 3));
	if (!Check(!lobbyCommands.TryEnqueue(Lobby(LobbyCommandType::Resume, 4)),
		"a full lobby queue accepted another ordinary command")) return 1;

	auto firstLobbyBatch = lobbyCommands.Drain(2);
	if (!Check(firstLobbyBatch.size() == 2 && firstLobbyBatch[0].sid == 1 && firstLobbyBatch[1].sid == 2,
		"lobby queue did not preserve FIFO order or the drain limit")) return 1;
	if (!Check(lobbyCommands.TryEnqueue(Lobby(LobbyCommandType::Resume, 4)),
		"lobby queue did not accept a command after draining capacity")) return 1;
	auto secondLobbyBatch = lobbyCommands.Drain(capacity);
	if (!Check(secondLobbyBatch.size() == 2 && secondLobbyBatch[0].sid == 3 && secondLobbyBatch[1].sid == 4,
		"lobby queue did not preserve the undrained tail")) return 1;

	(void)lobbyCommands.TryEnqueue(Lobby(LobbyCommandType::Create, 10));
	(void)lobbyCommands.TryEnqueue(Lobby(LobbyCommandType::Enter, 11));
	(void)lobbyCommands.TryEnqueue(Lobby(LobbyCommandType::SetReady, 12));
	if (!Check(lobbyCommands.TryEnqueue(Lobby(LobbyCommandType::Disconnected, 13)),
		"Disconnected was rejected at the lobby lifecycle soft cap") ||
		!Check(lobbyCommands.TryEnqueue(Lobby(LobbyCommandType::Disconnected, 14)),
			"a second Disconnected was rejected above the lobby lifecycle soft cap") ||
		!Check(!lobbyCommands.TryEnqueue(Lobby(LobbyCommandType::Resume, 15)),
			"an ordinary lobby command crossed the lifecycle soft cap")) return 1;
	auto lifecycleBatch = lobbyCommands.Drain(capacity + 2);
	if (!Check(lifecycleBatch.size() == capacity + 2 && lifecycleBatch[0].sid == 10 &&
		lifecycleBatch[1].sid == 11 && lifecycleBatch[2].sid == 12 &&
		lifecycleBatch[3].sid == 13 && lifecycleBatch[4].sid == 14,
		"lobby lifecycle overflow changed accepted command order")) return 1;

	GameCommandQueue gameCommands(capacity);
	(void)gameCommands.TryEnqueue(Game(21));
	(void)gameCommands.TryEnqueue(Game(22));
	(void)gameCommands.TryEnqueue(Game(23));
	if (!Check(!gameCommands.TryEnqueue(Game(24)),
		"a full game queue accepted another gameplay command")) return 1;
	auto firstGameBatch = gameCommands.Drain(2);
	if (!Check(firstGameBatch.size() == 2 && firstGameBatch[0].sid == 21 && firstGameBatch[1].sid == 22,
		"game queue did not preserve FIFO order or the drain limit")) return 1;
	if (!Check(gameCommands.TryEnqueue(Game(24)),
		"game queue did not accept a command after draining capacity")) return 1;
	auto secondGameBatch = gameCommands.Drain(capacity);
	if (!Check(secondGameBatch.size() == 2 && secondGameBatch[0].sid == 23 && secondGameBatch[1].sid == 24,
		"game queue did not preserve the undrained tail")) return 1;

	GameCommandQueue leasedGameCommands(1);
	GameCommand leasedCommand = Game(25);
	leasedCommand.roomNum = 7;
	leasedCommand.lease = { 31, 41 };
	(void)leasedGameCommands.TryEnqueue(std::move(leasedCommand));
	auto leasedBatch = leasedGameCommands.Drain(1);
	if (!Check(leasedBatch.size() == 1 && leasedBatch.front().roomNum == 7 &&
		leasedBatch.front().lease.Matches(31, 41),
		"game queue did not preserve the captured room/member/match lease")) return 1;

	LobbyCommandQueue independentLobby(1);
	GameCommandQueue independentGame(1);
	(void)independentGame.TryEnqueue(Game(30));
	if (!Check(independentLobby.TryEnqueue(Lobby(LobbyCommandType::Create, 31)),
		"a full game queue affected lobby admission")) return 1;
	if (!Check(independentLobby.Drain(1).size() == 1 && independentGame.Drain(1).size() == 1,
		"draining one command domain affected the other")) return 1;

	constexpr std::size_t producerCount = 8;
	constexpr std::size_t commandsPerProducer = 512;
	constexpr std::size_t productionCapacity = producerCount * commandsPerProducer;
	constexpr std::size_t productionBatch = 512;
	LobbyCommandQueue concurrentLobby(productionCapacity);
	std::atomic_bool enqueueFailed = false;
	std::vector<std::thread> producers;
	producers.reserve(producerCount);
	for (std::size_t producer = 0; producer < producerCount; ++producer)
	{
		producers.emplace_back([&, producer]
		{
			for (std::size_t sequence = 0; sequence < commandsPerProducer; ++sequence)
			{
				const int32 sid = static_cast<int32>(producer * commandsPerProducer + sequence);
				if (!concurrentLobby.TryEnqueue(Lobby(LobbyCommandType::Enter, sid)))
					enqueueFailed.store(true, std::memory_order_relaxed);
			}
		});
	}
	for (auto& producer : producers) producer.join();
	if (!Check(!enqueueFailed.load(std::memory_order_relaxed),
		"the production-sized concurrent lobby burst lost an accepted command") ||
		!Check(!concurrentLobby.TryEnqueue(Lobby(LobbyCommandType::Resume,
			static_cast<int32>(productionCapacity))),
			"the production-sized lobby queue accepted command 4097")) return 1;

	std::array<bool, productionCapacity> seen{};
	std::array<int32, producerCount> lastSequence{};
	lastSequence.fill(-1);
	std::size_t drainedCount = 0;
	for (std::size_t batchIndex = 0; batchIndex < productionCapacity / productionBatch; ++batchIndex)
	{
		auto batch = concurrentLobby.Drain(productionBatch);
		if (!Check(batch.size() == productionBatch,
			"the production-sized lobby queue did not honor its 512-command drain batch")) return 1;
		for (const LobbyCommand& command : batch)
		{
			if (!Check(command.sid >= 0 && static_cast<std::size_t>(command.sid) < productionCapacity,
				"the concurrent lobby burst returned an invalid command")) return 1;
			const auto id = static_cast<std::size_t>(command.sid);
			if (!Check(!seen[id], "the concurrent lobby burst returned a duplicate command")) return 1;
			seen[id] = true;
			const std::size_t producer = id / commandsPerProducer;
			const int32 sequence = static_cast<int32>(id % commandsPerProducer);
			if (!Check(sequence == lastSequence[producer] + 1,
				"the concurrent lobby burst broke producer-local FIFO order")) return 1;
			lastSequence[producer] = sequence;
			++drainedCount;
		}
	}
	if (!Check(drainedCount == productionCapacity &&
		std::all_of(seen.begin(), seen.end(), [](const bool value) { return value; }) &&
		concurrentLobby.Drain(1).empty(),
		"the concurrent lobby burst was not drained exactly once")) return 1;

	return 0;
}
