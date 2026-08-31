#pragma once
#include "IocpObject.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

class IocpCore;
class ServerSessionManager;
// AcceptEx lifecycle and accepted-socket activation boundary.

class AcceptManager : public IocpObject
{
public:
	AcceptManager(IocpCore& iocpCore, ServerSessionManager& sessions);
	~AcceptManager();
public:
	// Accept를 받을 준비를 진행해라
	bool InitAccept();
	void StopAccepting() noexcept;
	bool IsDrained() const noexcept;
private:
	class PendingAcceptEvent;

	// 수신관련 진행
	bool RegisterAccept(PendingAcceptEvent& acceptEvent);
	void ProcessAccept(PendingAcceptEvent& acceptEvent);
public: // 인터페이스 구현할 예정
	// 상속하고 있는 iocObject의 추상 함수들을 오버라이딩
	HANDLE GetHandle() const noexcept override;
	void Processing(class IocpEvent* iocpEvent, int32 numOfBytes = 0) override;
	void OnIocpCompletion(class IocpEvent* iocpEvent, uint32_t bytes) override;
	void OnIocpError(class IocpEvent* iocpEvent, int32 errCode) override;

private:
	void FinishAccept() noexcept;

	static constexpr int32 MaxAcceptCount = 1000;

	SOCKET _listenSock = INVALID_SOCKET;
	std::vector<std::unique_ptr<PendingAcceptEvent>> _acceptEvents;
	mutable std::mutex _acceptMutex;
	bool _accepting = false;
	std::atomic<uint32> _outstandingAccepts = 0;
	IocpCore& _iocpCore;
	ServerSessionManager& _sessions;
};

