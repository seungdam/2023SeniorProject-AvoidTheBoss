#pragma once
#include "CoreMacro.h"
#include "IocpObject.h"

#include <utility>

class BaseSession : public IocpObject
{
public:
	~BaseSession() override { Disconnect(); }
	HANDLE GetHandle() const noexcept override
	{
		std::shared_lock lock(_lock);
		return reinterpret_cast<HANDLE>(_sock);
	}
	virtual void Processing(class IocpEvent* iocpEvent, int32 numBytes) override {} // 어떤 일감으로  Iocp에 등록했니?
	virtual void OnIocpCompletion(class IocpEvent* iocpEvent, uint32_t bytes) override
	{
		if (iocpEvent && iocpEvent->_comp == EventType::Send)
			delete static_cast<SendEvent*>(iocpEvent);
		CompleteIO();
	};
	virtual void OnIocpError(class IocpEvent* iocpEvent, int32 errCode) override
	{
		CompleteIO();
		if (iocpEvent && iocpEvent->_comp == EventType::Send)
			delete static_cast<SendEvent*>(iocpEvent);
		Disconnect();
	}

	SOCKET GetSock() const noexcept
	{
		std::shared_lock lock(_lock);
		return _sock;
	}

	void SetSock(const SOCKET sock) noexcept
	{
		std::unique_lock lock(_lock);
		_sock = sock;
	}

	void SetIdentity(const int32 cid, const int32 sid) noexcept
	{
		std::unique_lock lock(_lock);
		_cid = cid;
		_sid = sid;
	}

	int32 GetSid() const noexcept
	{
		std::shared_lock lock(_lock);
		return _sid;
	}

	void BeginIO() noexcept			 { _pendingIO.fetch_add(1, std::memory_order_relaxed); }
	int32 CompleteIO() noexcept		 { return _pendingIO.fetch_sub(1, std::memory_order_acq_rel) - 1; }
	int32 PendingIO() const noexcept { return _pendingIO.load(std::memory_order_acquire); }

	void Disconnect() noexcept
	{
		SOCKET sock = INVALID_SOCKET;
		{
			std::unique_lock lock(_lock);
			sock = std::exchange(_sock, INVALID_SOCKET);
		}

		if (sock == INVALID_SOCKET)
		{
			return;
		}

		::shutdown(sock, SD_BOTH);
		::closesocket(sock); // cancels pending overlapped socket I/O; completions can still arrive.
	}
protected:
	std::atomic<int> _pendingIO = 0;
	int32 _cid = -1;
	int32 _sid = -1;
	SOCKET _sock = INVALID_SOCKET;
	RecvEvent _rev;
	mutable std::shared_mutex _lock;
};
