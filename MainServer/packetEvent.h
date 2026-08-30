#pragma once
#include "IocpCore.h"
#include "ServerSession.h"
#include "ServerIocpCore.h"
// 0 1 2


class QueueEvent
{
public:
	int64 generateTime = 0;
	uint64 sequence = 0;
	int32 _sid = -1;
	int32 _roomNum = -1;
	uint64 _memberGeneration = 0;
public:
	QueueEvent() {};
	virtual ~QueueEvent() {};
	virtual void Task() {};
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
	virtual void Task();
};



class InteractionEvent : public QueueEvent
{
public:
	InteractionEvent() {};
	virtual ~InteractionEvent() {};
	uint8 eventId = -1;
public:
	virtual void Task();
};

class AttackEvent : public QueueEvent
{
public:
	int16 _tidx;
	int32 _wf;
public:
	AttackEvent():_wf(0), _tidx(0) {};
	virtual ~AttackEvent() {}
	virtual void Task();
 };
