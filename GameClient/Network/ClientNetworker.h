#pragma once

#include "ThreadManager.h"

#include <memory>

class ClientIocpCore;

namespace atb
{
class ClientPacketDispatcher;

class ClientNetworker final
{
public:
	ClientNetworker();
	~ClientNetworker() noexcept;

	ClientNetworker(const ClientNetworker&) = delete;
	ClientNetworker& operator=(const ClientNetworker&) = delete;
	ClientNetworker(ClientNetworker&&) = delete;
	ClientNetworker& operator=(ClientNetworker&&) = delete;

	void Initialize(const char* ipv4Address);
	void Shutdown() noexcept;
	bool Logout();

	bool Send(void* packet);
	void DispatchPackets(ClientPacketDispatcher& dispatcher);
	[[nodiscard]] bool IsInitialized() const noexcept;

private:
	std::unique_ptr<ClientIocpCore> _core;
	ThreadManager _worker;
};
}
