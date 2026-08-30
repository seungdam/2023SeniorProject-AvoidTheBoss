#pragma once

#include "IocpEvent.h"
#include "BaseSession.h"

#include <atomic>

// 서버에서 클라이언트 소켓을 관리할 클래스
// 마찬가지로 Iocp에 등록할 대상이기 때문에 IocpObject에 해당된다


class ServerSession : public BaseSession
{
public:
	ServerSession();
	virtual ~ServerSession();
public:
	// 세션 인터페이스
	virtual void Processing(class IocpEvent* iocpEvent, int32 numOfBytes = 0) override;
	void OnIocpError(IocpEvent* iocpEvent, int32 errCode) override;
public:
	// 세션 정보를 얻어 내거나 세팅할 수 있는 함수들
	bool DoSend(void* packet);
	bool DoRecv();
	void DoSendLoginPacket(bool isSuccess);
	void ProcessPacket(char*);
	uint64 GetResumeToken() const noexcept { return _resumeToken.load(std::memory_order_acquire); }
	void SetResumeToken(uint64 token) noexcept { _resumeToken.store(token, std::memory_order_release); }
public:
	std::atomic<int16> _myRoomNumber = -1;
	int32 _curPage = 0;
	int32 _cbPrevRemainPacket = 0;
	std::atomic<uint64> _resumeToken = 0;
};


