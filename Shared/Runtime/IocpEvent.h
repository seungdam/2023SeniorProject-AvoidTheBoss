#pragma once


#include "../NetworkConfig.h"
#include "../Types.h"
#include "NetworkPlatform.h"

#include <cstring>

//==========================
//        IOCP EVENT TYPE
// =========================
enum  class EventType : int8
{
	Accept,
	Connect,
	Disconnect,
	Send,
	Recv
};


//==========================
//        IOCP EVENT: 어떤 사유로 iocp에 일감이 등록되었는가
// =========================
// 상속을 받으면 무조건 첫번째 offset이 wsaoverlapped 가 된다.
// 우리가 수업시간에 overexten을 만들 때, 첫 멤버 변수로 wsaoverlapped를 넣은것과 같다.
class IocpEvent : public WSAOVERLAPPED
{
public:
	IocpEvent(EventType type);
	void Init();
public:
	EventType _comp;
};


class ConnectEvent : public IocpEvent
{
public:
	ConnectEvent() : IocpEvent(EventType::Connect) { Init(); };
public:
	char _buf[BUFSIZE / 2] = {};
};


// ==== 송 수신 관련 이벤트 ====
class SendEvent : public IocpEvent
{
public:
	SendEvent(char* packet) : IocpEvent(EventType::Send)
	{
		_sWsaBuf.buf = _sbuf;
		_sWsaBuf.len = packet[0];
		memcpy(_sbuf, packet, packet[0]);
	};
public:
	int32 _sid = -1;
	int32 _cid = -1;
	WSABUF _sWsaBuf;
	char _sbuf[BUFSIZE] = {};
};

class RecvEvent : public IocpEvent
{
public:
	RecvEvent() : IocpEvent(EventType::Recv)
	{
		Init();
		_rWsaBuf.buf = _rbuf;
		_rWsaBuf.len = BUFSIZE;
	};
public:
	int32 _sid = -1;
	int32 _cid = -1;
	WSABUF _rWsaBuf;
	char _rbuf[BUFSIZE] = { };
};

