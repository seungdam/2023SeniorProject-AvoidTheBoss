#pragma once
#include "CoreMacro.h"
#include "IocpObject.h"

#include <utility>

class BaseSession : public IocpObject
{
public:
	~BaseSession() override { Disconnect(); }
	virtual HANDLE GetHandle() const noexcept override { return reinterpret_cast<HANDLE>(_sock); };
	virtual void Processing(class IocpEvent* iocpEvent, int32 numBytes) override {} // 어떤 일감으로  Iocp에 등록했니?
	virtual void OnIocpCompletion(IocpEvent& iocpEvent, uint32_t bytes) override {};
	virtual void OnIocpError(class IocpEvent* iocpEvent, int32 errCode) override { Disconnect(); };

	void Disconnect() noexcept
	{
		SOCKET sock = INVALID_SOCKET;
		{
			std::unique_lock lock(_lock);
			sock = std::exchange(_sock, INVALID_SOCKET);
		}

		if (sock == INVALID_SOCKET) return;

		::shutdown(sock, SD_BOTH);
		::closesocket(sock); // cancels pending overlapped socket I/O; completions can still arrive.
	}
protected:
	int32 _cid = -1;
	int32 _sid = -1;
	SOCKET _sock = INVALID_SOCKET;
	RecvEvent _rev;
	RWLOCK;
};

