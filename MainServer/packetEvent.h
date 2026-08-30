#pragma once
#include "MatchLease.h"
#include "../Shared/GameCommon.h"
// 0 1 2

class MatchState;
class Room;

class QueueEvent
{
public:
	int64 generateTime = 0;
	uint64 sequence = 0;
	int32 _sid = -1;
	MatchLease _lease{};
public:
	QueueEvent() {};
	virtual ~QueueEvent() {};
	virtual void Task(Room&, MatchState&) {};

protected:
	[[nodiscard]] bool IsCurrent(Room& room, MatchState& match) const;
};

class moveEvent : public QueueEvent // 33 ms 마다 전송한다.
{
public:
	moveEvent() { };
	moveEvent(int32 sid, uint8 key, XMFLOAT3 dir) : _key(key), _dir(dir) { _sid = sid; };
	virtual ~moveEvent() {};
	XMFLOAT3 _dir {0,0,0};
	uint8 _key = 0;
public:
	void Task(Room& room, MatchState& match) override;
};



class InteractionEvent : public QueueEvent
{
public:
	InteractionEvent() {};
	virtual ~InteractionEvent() {};
	uint8 eventId = -1;
public:
	void Task(Room& room, MatchState& match) override;
};

class AttackEvent : public QueueEvent
{
public:
	int16 _tidx;
	int32 _wf;
public:
	AttackEvent():_wf(0), _tidx(0) {};
	virtual ~AttackEvent() {}
	void Task(Room& room, MatchState& match) override;
 };
