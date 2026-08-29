#pragma once
#include "IocpEvent.h" // WSAOVERLAPPED event contract

class IocpObject
{
public:
	virtual ~IocpObject() = default;
	virtual HANDLE GetHandle() const noexcept = 0;
	virtual void Processing(class IocpEvent* iocpEvent, int32 numBytes) = 0; // 어떤 일감으로  Iocp에 등록했니?
	// iocpEvent에 따라 적절한 일을 처리
	virtual void OnIocpError(class IocpEvent* iocpEvent, int32 errCode) {}
	virtual void OnIocpCompletion(class IocpEvent* iocpEvent, uint32_t bytes) {}
};
