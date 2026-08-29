#include "pch.h"
#include "JobQueue.h"

ClientEventScheduler::ClientEventScheduler()
{
	_BeginTickPoint = Clock::now(); // 시작 시점은 지금
	_CurrentTick = GetCurrentTick(); // 현재 틱 값 초기화
}

void ClientEventScheduler::PushTask(QueueEvent* task, float after)
{
	if (!task) return;
	_CurrentTick = GetCurrentTick();
	task->generateTime = _CurrentTick + static_cast<int64>(after);
	task->sequence = _nextSequence++;
	_TaskQueue.push(task);
}

void ClientEventScheduler::PushTask(QueueEvent* task)
{
	if (!task) return;
	_CurrentTick = GetCurrentTick();
	task->generateTime = _CurrentTick;
	task->sequence = _nextSequence++;
	_TaskQueue.push(task);
}

void ClientEventScheduler::DoTasks()
{
	/// tick update
	_CurrentTick = GetCurrentTick(); // 현재 틱값을 구한다.

	while (!_TaskQueue.empty())
	{
		QueueEvent* jobElem = _TaskQueue.top(); // 가장 우선적으로 나와야할 이벤트에 대해서
		if (_CurrentTick < jobElem->generateTime) break;
		_TaskQueue.pop();
		jobElem->Task(); // 만약 호출할 시점이 됐다면 해당 잡을 수행하고 queue에서 제거
		delete jobElem;

	}

}

void ClientEventScheduler::Clear()
{
	while (!_TaskQueue.empty())
	{
		delete _TaskQueue.top();
		_TaskQueue.pop();
	}
}
