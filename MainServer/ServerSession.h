#pragma once

#include "IocpEvent.h"
#include "BaseSession.h"
#include "MatchLease.h"
#include "RoomCommand.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>

// 서버에서 클라이언트 소켓을 관리할 클래스
// 마찬가지로 Iocp에 등록할 대상이기 때문에 IocpObject에 해당된다


struct ServerSessionRoutes
{
	std::function<void(int32)> requestRemove;
	std::function<bool(LobbyCommand)> enqueueLobby;
	std::function<bool(GameCommand)> enqueueGame;
};

class ServerSession : public BaseSession, public std::enable_shared_from_this<ServerSession>
{
public:
	struct GameBinding
	{
		int32 roomNumber = -1;
		MatchLease lease{};
	};

	explicit ServerSession(ServerSessionRoutes routes);
	virtual ~ServerSession();
public:
	// 세션 인터페이스
	virtual void Processing(class IocpEvent* iocpEvent, int32 numOfBytes = 0) override;
	void OnIocpCompletion(IocpEvent* iocpEvent, uint32_t bytes) override;
	void OnIocpError(IocpEvent* iocpEvent, int32 errCode) override;
public:
	// 세션 정보를 얻어 내거나 세팅할 수 있는 함수들
	bool DoSend(void* packet);
	bool DoRecv();
	void DoSendLoginPacket(bool isSuccess);
	void ProcessPacket(char*);
	uint64 GetResumeToken() const noexcept { return _resumeToken.load(std::memory_order_acquire); }
	void SetResumeToken(uint64 token) noexcept { _resumeToken.store(token, std::memory_order_release); }
	int32 GetRoomNumber() const
	{
		std::lock_guard lock(_bindingLock);
		return _gameBinding.roomNumber;
	}
	GameBinding GetGameBinding() const
	{
		std::lock_guard lock(_bindingLock);
		return _gameBinding;
	}
	bool BindRoom(int32 roomNumber, uint64 memberGeneration, uint64 matchGeneration = 0)
	{
		std::lock_guard lock(_bindingLock);
		if (_retired) return false;
		_gameBinding = { roomNumber, { memberGeneration, matchGeneration } };
		return true;
	}
	bool BindMatch(uint64 matchGeneration)
	{
		std::lock_guard lock(_bindingLock);
		if (_retired || _gameBinding.roomNumber == -1) return false;
		_gameBinding.lease.matchGeneration = matchGeneration;
		return true;
	}
	void ClearRoomBinding()
	{
		std::lock_guard lock(_bindingLock);
		_gameBinding = {};
	}
	void RetireGameBinding()
	{
		std::lock_guard lock(_bindingLock);
		_retired = true;
		_gameBinding = {};
	}

public:
	int32 _curPage = 0;
	int32 _cbPrevRemainPacket = 0;

private:
	void RequestRemoval();

	ServerSessionRoutes _routes;
	mutable std::mutex _bindingLock;
	GameBinding _gameBinding{};
	bool _retired = false;
	std::atomic<uint64> _resumeToken = 0;
};


