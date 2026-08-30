#include "pch.h"
#include "JobQueue.h"

#include <iostream>
#include <stdexcept>
#include <vector>

class Room {};
class MatchState {};

namespace
{
	std::vector<int> executed;
	bool contextAndLeaseMatched = true;

	class TestEvent final : public QueueEvent
	{
	public:
		TestEvent(const int id, Room& expectedRoom, MatchState& expectedMatch, const MatchLease lease)
			: _id(id), _expectedRoom(&expectedRoom), _expectedMatch(&expectedMatch)
		{
			_lease = lease;
		}

		void Task(Room& room, MatchState& match) override
		{
			contextAndLeaseMatched &= &room == _expectedRoom && &match == _expectedMatch &&
				_lease.Matches(11, 22);
			executed.push_back(_id);
		}
	private:
		int _id;
		Room* _expectedRoom;
		MatchState* _expectedMatch;
	};

	class LifetimeEvent final : public QueueEvent
	{
	public:
		LifetimeEvent(int& destroyed, const bool throws) : _destroyed(destroyed), _throws(throws) {}
		~LifetimeEvent() override { ++_destroyed; }

		void Task(Room&, MatchState&) override
		{
			if (_throws) throw std::runtime_error("expected scheduler test exception");
		}

	private:
		int& _destroyed;
		bool _throws;
	};
}

int main()
{
	Room room;
	MatchState match;
	const MatchLease lease{ 11, 22 };

	{
		ClientEventScheduler scheduler;
		scheduler.PushTask(new TestEvent(1, room, match, lease));
		scheduler.PushTask(new TestEvent(2, room, match, lease));
		scheduler.PushTask(new TestEvent(3, room, match, lease));
		scheduler.PushTask(new TestEvent(4, room, match, lease), 1000.0f);
		scheduler.DoTasks(room, match);

		if (executed != std::vector<int>{ 1, 2, 3 } || !contextAndLeaseMatched)
		{
			std::cerr << "FIFO, execution context, or MatchLease preservation failed\n";
			return 1;
		}
	}

	int destroyed = 0;
	{
		ClientEventScheduler scheduler;
		scheduler.PushTask(new LifetimeEvent(destroyed, false), 1000.0f);
	}
	if (destroyed != 1)
	{
		std::cerr << "scheduler destructor did not clear a pending event\n";
		return 1;
	}

	{
		ClientEventScheduler scheduler;
		scheduler.PushTask(new LifetimeEvent(destroyed, true));
		try
		{
			scheduler.DoTasks(room, match);
			std::cerr << "throwing event did not propagate its exception\n";
			return 1;
		}
		catch (const std::runtime_error&)
		{
		}
	}

	if (destroyed != 2)
	{
		std::cerr << "throwing event was not destroyed\n";
		return 1;
	}
	return 0;
}
