#include "pch.h"
#include "JobQueue.h"

#include <iostream>
#include <vector>

namespace
{
	std::vector<int> executed;

	class TestEvent final : public QueueEvent
	{
	public:
		explicit TestEvent(const int id) : _id(id) {}
		void Task() override { executed.push_back(_id); }
	private:
		int _id;
	};
}

int main()
{
	ClientEventScheduler scheduler;
	scheduler.PushTask(new TestEvent(1));
	scheduler.PushTask(new TestEvent(2));
	scheduler.PushTask(new TestEvent(3));
	scheduler.PushTask(new TestEvent(4), 1000.0f);
	scheduler.DoTasks();

	if (executed != std::vector<int>{ 1, 2, 3 })
	{
		std::cerr << "immediate events were not executed in FIFO order\n";
		return 1;
	}

	scheduler.Clear();
	scheduler.DoTasks();
	return executed == std::vector<int>{ 1, 2, 3 } ? 0 : 1;
}
