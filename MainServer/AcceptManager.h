#pragma once
#include "IocpObject.h"
#include <memory>
#include <vector>

class IocpCore;
class RoomManager;
class ServerSessionManager;
// AcceptEx lifecycle and accepted-socket activation boundary.

class AcceptManager : public IocpObject
{
public:
	AcceptManager(IocpCore& iocpCore, ServerSessionManager& sessions, RoomManager& rooms);
	~AcceptManager();
public:
	// Accept를 받을 준비를 진행해라
	bool InitAccept();
	void CloseSocket();
private:
	class PendingAcceptEvent;

	// 수신관련 진행
	bool RegisterAccept(PendingAcceptEvent& acceptEvent);
	void ProcessAccept(PendingAcceptEvent& acceptEvent);
public: // 인터페이스 구현할 예정
	// 상속하고 있는 iocObject의 추상 함수들을 오버라이딩
	HANDLE GetHandle() const noexcept override;
	void Processing(class IocpEvent* iocpEvent, int32 numOfBytes = 0) override;
	void OnIocpError(class IocpEvent* iocpEvent, int32 errCode) override;

private:
	static constexpr int32 MaxAcceptCount = 1000;

	SOCKET _listenSock = INVALID_SOCKET;
	std::vector<std::unique_ptr<PendingAcceptEvent>> _acceptEvents;
	IocpCore& _iocpCore;
	ServerSessionManager& _sessions;
	RoomManager& _rooms;
};

