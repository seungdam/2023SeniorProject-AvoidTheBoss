#pragma once
#include "../ClientSession.h"
#include "IocpCore.h"

#include <atomic>
#include <memory>

// ========= new Iocp Core ==============

namespace atb { class ClientPacketDispatcher; }

class ClientIocpCore final : public IocpCore
{
	friend class ClientSession;
public:
	ClientIocpCore();
	~ClientIocpCore() override;
	void InitConnect(const char* address);
	void DoConnect();
	bool Logout();
	bool DoSend(void* packet);
	void DispatchPackets(atb::ClientPacketDispatcher& dispatcher);
	bool Processing(uint32_t timelimit = INFINITE) override;
	void Disconnect();
private:
	bool PrepareSocket(bool reconnect);
	bool ReconnectIfDrained();
	bool ScheduleReconnect();

	std::unique_ptr<ClientSession> _client;
	SOCKADDR_IN _serveraddr;
	std::atomic_bool _manualDisconnect = false;
	bool _reconnectPending = false;

};
