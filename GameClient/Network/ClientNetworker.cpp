#include "../Platform/pch.h"
#include "ClientNetworker.h"

#include "clientIocpCore.h"

namespace atb
{
namespace
{
	// ponytail: fixed local-client retry cadence; add backoff/jitter when production telemetry needs it.
	constexpr uint32 NetworkPollIntervalMs = 250;
}

ClientNetworker::ClientNetworker() = default;

ClientNetworker::~ClientNetworker() noexcept
{
	Shutdown();
}

void ClientNetworker::Initialize(const char* ipv4Address)
{
	if (_core)
	{
		throw std::logic_error("ClientNetworker is already initialized");
	}
	if (!ipv4Address || *ipv4Address == '\0')
	{
		throw std::invalid_argument("ClientNetworker requires an IPv4 address");
	}

	auto core = std::make_unique<ClientIocpCore>();
	core->InitConnect(ipv4Address);
	_core = std::move(core);

	try
	{
		_core->DoConnect();
		_worker.Launch(
		    [this]
		    {
			    while (_core->Processing(NetworkPollIntervalMs))
			    {
			    }
		    });
	}
	catch (...)
	{
		_core->Disconnect();
		while (_core->Processing(NetworkPollIntervalMs))
		{
		}
		_core.reset();
		throw;
	}
}

void ClientNetworker::Shutdown() noexcept
{
	if (!_core)
	{
		return;
	}
	_core->Disconnect();
	_worker.Join();
	_core.reset();
}

bool ClientNetworker::Logout()
{
	return _core && _core->Logout();
}

bool ClientNetworker::Send(void* packet)
{
	return _core && _core->DoSend(packet);
}

void ClientNetworker::DispatchPackets(ClientPacketDispatcher& dispatcher)
{
	if (_core)
	{
		_core->DispatchPackets(dispatcher);
	}
}

bool ClientNetworker::IsInitialized() const noexcept
{
	return _core != nullptr;
}
}
