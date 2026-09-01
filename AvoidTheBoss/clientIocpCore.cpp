#include "pch.h"
#include "ClientIocpCore.h"
#include "SocketUtil.h"
#include "IocpEvent.h"

ClientIocpCore::ClientIocpCore()
{
	_client = std::make_unique<ClientSession>();
	::ZeroMemory(&_serveraddr, sizeof(sockaddr_in));
}

ClientIocpCore::~ClientIocpCore() = default;

bool ClientIocpCore::DoSend(void* packet)
{
	return _client && _client->DoSend(packet);
}

void ClientIocpCore::DispatchPackets(atb::ClientPacketDispatcher& dispatcher)
{
	if (_client) _client->DispatchPackets(dispatcher);
}

void ClientIocpCore::InitConnect(const char* address)
{
	_serveraddr.sin_family = AF_INET;
	if (::inet_pton(AF_INET, address, &_serveraddr.sin_addr) != 1)
		throw std::invalid_argument("CCIocpCore requires a valid IPv4 address");
	_serveraddr.sin_port = htons(PORTNUM);
	if (!PrepareSocket(false))
		throw std::runtime_error("CCIocpCore failed to prepare the client socket");
}

bool ClientIocpCore::Logout()
{
	if (!_client) return false;

	C2S_LOGOUT packet{};
	packet.size = sizeof(packet);
	packet.type = static_cast<uint8>(C_TITLE_PACKET_TYPE::ACQ_LOGOUT);
	packet.sid = static_cast<uint16>(_client->GetSid());
	if (!_client->DoSend(&packet)) return false;
	_client->ClearResumeToken();
	return true;
}

bool ClientIocpCore::PrepareSocket(const bool reconnect)
{
	if (!_client) return false;
	const SOCKET socket = SocketUtil::CreateSocket();
	if (socket == INVALID_SOCKET) return false;

	if (reconnect) _client->ResetForReconnect(socket);
	else
	{
		_client->SetSock(socket);
		_client->SetIdentity(-1, -1);
	}

	SOCKADDR_IN localAddress{};
	localAddress.sin_family = AF_INET;
	localAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	localAddress.sin_port = htons(0);
	if (::bind(socket, reinterpret_cast<sockaddr*>(&localAddress), sizeof(localAddress)) == SOCKET_ERROR ||
		!Register(static_cast<IocpObject*>(_client.get())))
	{
		_client->Stop();
		return false;
	}
	return true;
}

bool ClientIocpCore::ReconnectIfDrained()
{
	if (!_reconnectPending || !_client || _manualDisconnect.load(std::memory_order_acquire) ||
		_client->PendingIO() != 0) return false;
	if (!PrepareSocket(true)) return true;

	if (_manualDisconnect.load(std::memory_order_acquire))
	{
		_client->Stop();
		return _client->PendingIO() > 0;
	}

	_reconnectPending = false;
	DoConnect();
	return true;
}

bool ClientIocpCore::ScheduleReconnect()
{
	if (_manualDisconnect.load(std::memory_order_acquire) || !_client) return false;
	_reconnectPending = true;
	return true;
}

void ClientIocpCore::DoConnect()
{
	if (!_client || _manualDisconnect.load(std::memory_order_acquire) || _client->IsStopping()) return;
	_client->SetSid(0);
	auto* connectEvent = new ConnectEvent();
	_client->BeginIO();
	const bool connected = SocketUtil::ConnectEx(
		_client->GetSock(),
		reinterpret_cast<sockaddr*>(&_serveraddr),
		sizeof(_serveraddr),
		nullptr,
		0,
		nullptr,
		static_cast<LPWSAOVERLAPPED>(connectEvent));

	if (!connected)
	{
		const int32 errorCode = ::WSAGetLastError();
		if (errorCode != WSA_IO_PENDING)
		{
			_client->CompleteIO();
			delete connectEvent;
			std::cout << errorCode << "\nConnect Error\n";
			_client->Stop();
			ScheduleReconnect();
		}
	}
}

bool ClientIocpCore::Processing(uint32_t timelimit)
{
		DWORD numOfBytes(0); // 몇 바이트가 전송되었는가?
		IocpObject* iocpObject = nullptr; // 일감이 완료된 iocpObject의 종류를 복원하기 위한 IocpObject
		IocpEvent* iocpEvent = nullptr; // 일감이 완료된 iocpEvent의 종류(Accept인가?)

		BOOL retVal = ::GetQueuedCompletionStatus(_hIocp, OUT & numOfBytes, reinterpret_cast<PULONG_PTR>(&iocpObject), // 하지만 이렇게 iocpObject를 인자로 넘겨주게 되면, 다른 스레드에서 이 오브젝트를 삭제했을 때, 문제가 생길 수도 있다. -->
			//애초에 iocpEvent에서 해당 iocp객체들에 관한 정보(해당 이벤트를 호출한 주인 iocp객체들)을 담고 있도록하자.
			OUT reinterpret_cast<LPOVERLAPPED*>(&iocpEvent), timelimit);
		if (!iocpObject && !iocpEvent)
		{
			if (!_client) return false;
			if (_manualDisconnect.load(std::memory_order_acquire))
			{
				_client->Stop();
				return _client->PendingIO() > 0;
			}
			if (_reconnectPending && _client->PendingIO() == 0)
				return ReconnectIfDrained();
			return true;
		}

		ClientSession* session = static_cast<ClientSession*>(iocpObject);
		session->CompleteIO();

		if (!retVal) // 실패했다면 에러코드 확인
		{
			int32 errCode = ::WSAGetLastError();
			switch (errCode)
			{
			case ERROR_CONNECTION_REFUSED:
			case WSAECONNREFUSED:
				if (iocpEvent && iocpEvent->_comp == EventType::Connect)
					delete static_cast<ConnectEvent*>(iocpEvent);
				else if (iocpEvent && iocpEvent->_comp == EventType::Send)
					delete static_cast<SendEvent*>(iocpEvent);
				session->Stop();
				return session->PendingIO() > 0 || ScheduleReconnect();
			default:
				std::cout << ::WSAGetLastError() << "\n";
				if (iocpEvent && iocpEvent->_comp == EventType::Connect)
					delete static_cast<ConnectEvent*>(iocpEvent);
				else if (iocpEvent && iocpEvent->_comp == EventType::Send)
					delete static_cast<SendEvent*>(iocpEvent);
				session->Stop();
				return session->PendingIO() > 0 || ScheduleReconnect();
			}
		}

		// 클라이언트가 정상적으로 종료한 경우
		if (numOfBytes == 0 && (iocpEvent->_comp == EventType::Recv || iocpEvent->_comp == EventType::Send))
		{
			//Disconnect
			if (iocpEvent->_comp == EventType::Send) delete static_cast<SendEvent*>(iocpEvent);
			session->Stop();
			return session->PendingIO() > 0 || ScheduleReconnect();
		}
		iocpObject->Processing(iocpEvent, numOfBytes); // 성공하면 전반적인 프로세싱을 시작해보자;
		if (session->IsStopping())
		{
			session->Stop();
			return session->PendingIO() > 0 || ScheduleReconnect();
		}
		return true;

}

void ClientIocpCore::Disconnect()
{
	std::cout << "Disconnect Client" << std::endl;
	if (_client != nullptr)
	{
		_manualDisconnect.store(true, std::memory_order_release);
		_client->RequestStop();
		(void)PostWakeup();
	}
}
