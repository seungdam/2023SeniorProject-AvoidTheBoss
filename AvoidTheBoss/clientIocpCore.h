#pragma once
#include "GameFramework.h"
#include "ClientSession.h"
#include "IocpCore.h"
#include "Player.h"
// =============== new Client Session ================
// ===================================================
// ===================================================

extern HWND g_hwnd;

// ========= new Iocp Core ==============

class CCIocpCore : public IocpCore
{
	friend class ClientSession;
public:
	CCIocpCore();
	~CCIocpCore();
	void InitConnect(const char* address);
	void DoConnect(void* loginInfo);
	void DoSend(void* packet) { _client->DoSend(packet); }
	void DispatchPackets() { if (_client) _client->DispatchPackets(); }
	virtual bool Processing(uint32_t timelimit = INFINITE);
	void Disconnect(int32 sid);
private:
	bool PrepareSocket(bool reconnect);
	bool ReconnectIfDrained();
	bool ScheduleReconnect();

	ClientSession* _client;
	SOCKADDR_IN _serveraddr;
	bool _manualDisconnect = false;
	bool _reconnectPending = false;

};

extern class CCIocpCore clientCore;
