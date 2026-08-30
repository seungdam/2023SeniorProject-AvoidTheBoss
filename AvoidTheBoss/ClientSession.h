#pragma once
#include "BaseSession.h"
#include <deque>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <vector>

class IocpEvent;
class ClientEventScheduler;

class ClientSession : public BaseSession
{

public:
	ClientSession();
	virtual ~ClientSession();
public:
	// 세션 인터페이스
	virtual void Processing(class IocpEvent* iocpEvent, int32 numOfBytes = 0) override;
public:
	// 세션 정보를 얻어 내거나 세팅할 수 있는 함수들
	bool DoSend(void* packet);
	bool DoRecv();
	void DispatchPackets();
	void SetIdentity(int32 cid, int32 sid);
	void SetSid(int32 sid);
	int32 GetSid();
	std::pair<int32, int32> GetIdentity();
	void RequestStop() { _stopping.store(true); }
	bool IsStopping() const { return _stopping.load(); }
	void Stop();
public:
	int16 _myRm = -1;
	int32 _prevRemain = 0;
	int16  _loginOk = -3;
private:
	bool QueuePacket(const char* packet, std::size_t packetSize);
	void ApplyPacket(char* packet);

	std::mutex _packetMutex;
	std::deque<std::vector<char>> _pendingPackets;
	std::atomic_bool _stopping = false;
};
