#pragma once
#include "BaseSession.h"
#include <deque>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <vector>

class IocpEvent;
class ClientEventScheduler;
namespace atb { class ClientPacketDispatcher; }

class ClientSession : public BaseSession
{
	friend class atb::ClientPacketDispatcher;

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
	void DispatchPackets(atb::ClientPacketDispatcher& dispatcher);
	void SetIdentity(int32 cid, int32 sid);
	void SetSid(int32 sid);
	int32 GetSid();
	std::pair<int32, int32> GetIdentity();
	void ResetForReconnect(SOCKET socket);
	bool HasResumeToken() const noexcept { return _resumeToken.load(std::memory_order_acquire) != 0; }
	uint64 GetResumeToken() const noexcept { return _resumeToken.load(std::memory_order_acquire); }
	int32 GetResumeSid() const noexcept { return _resumeSid.load(std::memory_order_acquire); }
	void ClearResumeToken() noexcept { _resumeToken.store(0, std::memory_order_release); }
	void RequestStop() { _stopping.store(true); }
	bool IsStopping() const { return _stopping.load(); }
	void Stop();
public:
	int16 _myRoomNumber = -1;
	int32 _prevRemainBytes = 0;
	int16  _loginOk = -3;
private:
	bool QueuePacket(const char* packet, std::size_t packetSize);

	std::mutex _packetMutex;
	std::deque<std::vector<char>> _pendingPackets;
	std::atomic_bool _stopping = false;
	std::atomic<uint64> _resumeToken = 0;
	std::atomic<int32> _resumeSid = -1;
};
