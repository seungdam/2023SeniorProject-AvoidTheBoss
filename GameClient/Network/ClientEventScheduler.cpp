#include "../Platform/pch.h"
#include "ClientEventScheduler.h"

#include <utility>

ClientEventScheduler::ClientEventScheduler(CGameScene* ownerScene)
	: _ownerScene(ownerScene)
{
	Reset();
}

void ClientEventScheduler::Reset() noexcept
{
	Clear();
	_beginTickPoint = Clock::now();
}

void ClientEventScheduler::PushTask(ClientEvent event, const float afterMilliseconds)
{
	const int64 delay = afterMilliseconds > 0.0f
		? static_cast<int64>(afterMilliseconds)
		: 0;
	_taskQueue.push(ScheduledClientEvent{
		GetCurrentTick() + delay,
		_nextSequence++,
		std::move(event) });
}

void ClientEventScheduler::DoTasks()
{
	while (!_taskQueue.empty())
	{
		if (GetCurrentTick() < _taskQueue.top()._executeAt)
		{
			break;
		}

		ScheduledClientEvent scheduled = _taskQueue.top();
		_taskQueue.pop();
		std::visit([this](auto& event) { event.Task(_ownerScene); }, scheduled._event);
	}
}

void ClientEventScheduler::Clear() noexcept
{
	while (!_taskQueue.empty())
	{
		_taskQueue.pop();
	}
	_nextSequence = 0;
}
