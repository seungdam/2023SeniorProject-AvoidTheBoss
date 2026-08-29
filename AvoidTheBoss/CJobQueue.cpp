#include "pch.h"
#include "CJobQueue.h"

#include <utility>

Scheduler::Scheduler()
{
	Reset();
}

void Scheduler::Reset() noexcept
{
	Clear();
	_beginTickPoint = Clock::now();
}

void Scheduler::PushTask(ClientEvent event, const float afterMilliseconds)
{
	const int64 delay = afterMilliseconds > 0.0f
		? static_cast<int64>(afterMilliseconds)
		: 0;
	_taskQueue.push(ScheduledClientEvent{
		GetCurrentTick() + delay,
		_nextSequence++,
		std::move(event) });
}

void Scheduler::DoTasks()
{
	while (!_taskQueue.empty())
	{
		if (GetCurrentTick() < _taskQueue.top()._executeAt) break;

		ScheduledClientEvent scheduled = _taskQueue.top();
		_taskQueue.pop();
		std::visit([](auto& event) { event.Task(); }, scheduled._event);
	}
}

void Scheduler::Clear() noexcept
{
	while (!_taskQueue.empty()) _taskQueue.pop();
	_nextSequence = 0;
}
