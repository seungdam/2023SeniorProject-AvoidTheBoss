#include "pch.h"
#include "CJobQueue.h"

#include <iostream>
#include <vector>

namespace
{
	std::vector<int32> executedFrames;

	bool Check(const bool condition, const char* message)
	{
		if (!condition) std::cerr << "FAIL: " << message << '\n';
		return condition;
	}
}

void moveEvent::Task() {}
void posEvent::Task() {}
void rotateEvent::Task() {}
void animationEvent::Task() {}
void InteractionEvent::Task() {}
void DelayEvent::Task() {}
void FrameEvent::Task() { executedFrames.push_back(_worldFrame); }

int main()
{
	bool passed = true;
	JobPriorityQueue ordered;
	ordered.push(ScheduledClientEvent{ 10, 2, FrameEvent{ 2 } });
	ordered.push(ScheduledClientEvent{ 10, 0, FrameEvent{ 0 } });
	ordered.push(ScheduledClientEvent{ 10, 1, FrameEvent{ 1 } });
	for (int expected = 0; expected < 3; ++expected)
	{
		passed &= Check(std::get<FrameEvent>(ordered.top()._event)._worldFrame == expected,
			"equal-deadline events did not preserve FIFO sequence");
		ordered.pop();
	}

	Scheduler scheduler;
	scheduler.PushTask(FrameEvent{ 1 }, 0.0f);
	scheduler.PushTask(FrameEvent{ 2 }, 0.0f);
	scheduler.PushTask(FrameEvent{ 3 }, 0.0f);
	scheduler.DoTasks();
	passed &= Check(executedFrames == std::vector<int32>{ 1, 2, 3 },
		"Scheduler::DoTasks did not execute immediate events in FIFO order");

	executedFrames.clear();
	scheduler.PushTask(FrameEvent{ 4 }, 0.0f);
	scheduler.Clear();
	scheduler.DoTasks();
	passed &= Check(executedFrames.empty(), "Scheduler::Clear left a pending event");
	return passed ? 0 : 1;
}
