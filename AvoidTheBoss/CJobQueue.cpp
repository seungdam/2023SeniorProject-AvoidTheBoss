#include "pch.h"
#include "CJobQueue.h"

Scheduler::Scheduler()
{
	_BeginTickPoint = Clock::now(); // 스케줄러 시작 시각
	_CurrentTick = GetCurrentTick(); // 현재 틱 초기화
}

Scheduler::~Scheduler()
{
	Clear();
}

void Scheduler::PushTask(queueEvent* task, float after)
{
	_CurrentTick = GetCurrentTick();
	task->generateTime = _CurrentTick + static_cast<int64>(after);
	_TaskQueue.push(task);
}

void Scheduler::PushTask(queueEvent* task)
{
	task->generateTime = _CurrentTick;
	_normalQueue.push(task);
}

void Scheduler::DoTasks()
{
	// 틱 갱신
	int32 cycleCnt = 0;
	while (!_TaskQueue.empty())
	{
		if (cycleCnt > 100) break; // 아직 실행할 수 없는 작업은 다음 프레임으로 넘긴다.
		_CurrentTick = GetCurrentTick();
		queueEvent* jobElem = _TaskQueue.top(); // 가장 먼저 실행할 이벤트
		if (_CurrentTick < jobElem->generateTime)
		{
			// 우선순위를 갱신한 뒤 다시 대기시킨다.
			_TaskQueue.pop();
			_TaskQueue.push(jobElem);
			cycleCnt += 1;
			continue;
		}
		jobElem->Task();
		_TaskQueue.pop();
		delete jobElem;

	}

}

void Scheduler::Clear()
{
	while (!_TaskQueue.empty())
	{
		queueEvent* jobElem = _TaskQueue.top();
		_TaskQueue.pop();
		delete jobElem;
	}
	while (!_normalQueue.empty())
	{
		delete _normalQueue.front();
		_normalQueue.pop();
	}
}

void Scheduler::DoNormalTasks()
{
	// 일반 작업 처리
	while (!_normalQueue.empty())
	{
		queueEvent* jobElem = _normalQueue.front();
		_normalQueue.pop();
		if (jobElem != nullptr)
		{
			jobElem->Task();
			delete jobElem;
		}
	}

}
