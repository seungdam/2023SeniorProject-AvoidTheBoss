#include "pch.h"
#include "AcceptManager.h"
#include "SocketUtil.h"


#include "IocpCore.h"
#include "RoomManager.h"
#include "ServerSessionManager.h"
#include "ServerSession.h"
#include "IocpEvent.h"

#include <array>

using namespace std;

class AcceptManager::PendingAcceptEvent final : public IocpEvent
{
public:
	static constexpr DWORD AddressBytes = sizeof(SOCKADDR_IN) + 16;

	PendingAcceptEvent() : IocpEvent(EventType::Accept) {}

	std::shared_ptr<ServerSession> session;
	std::array<char, 2 * AddressBytes> addressBuffer{};
};

AcceptManager::AcceptManager(IocpCore& iocpCore, ServerSessionManager& sessions, RoomManager& rooms)
	: _iocpCore(iocpCore), _sessions(sessions), _rooms(rooms)
{
}

AcceptManager::~AcceptManager()
{
	SocketUtil::Close(_listenSock);
}

HANDLE AcceptManager::GetHandle() const noexcept
{
	return reinterpret_cast<HANDLE>(_listenSock);
}

void AcceptManager::Processing(IocpEvent* iocpEvent, int32 numOfBytes)
{
	// iocpEvent를 복원한다.
	ASSERT_CRASH(iocpEvent->_comp == EventType::Accept);
	auto& acceptEvent = *static_cast<PendingAcceptEvent*>(iocpEvent);
	ProcessAccept(acceptEvent);
}

void AcceptManager::OnIocpError(IocpEvent* iocpEvent, int32 errCode)
{
	if (!iocpEvent || iocpEvent->_comp != EventType::Accept) return;
	auto& acceptEvent = *static_cast<PendingAcceptEvent*>(iocpEvent);
	acceptEvent.session.reset();
	if (_listenSock != INVALID_SOCKET) (void)RegisterAccept(acceptEvent);
}


bool AcceptManager::InitAccept()
{
	_listenSock = SocketUtil::CreateSocket();
	if (_listenSock == INVALID_SOCKET)
		return false;

	if (_iocpCore.Register(this) == false)
		return false;

	if (SocketUtil::SetReuseAddress(_listenSock, true) == false) // 재사용 가능한 주소인지 확인
		return false;

	if (SocketUtil::SetLinger(_listenSock, 0, 0) == false)
		return false;

	if (SocketUtil::Bind(_listenSock) == false)
		return false;

	if (SocketUtil::Listen(_listenSock) == false)
		return false;

	int32 registeredAccepts = 0;
	for (int32 i = 0; i < MaxAcceptCount; ++i)
	{
		auto acceptEvent = std::make_unique<PendingAcceptEvent>();
		if (RegisterAccept(*acceptEvent)) ++registeredAccepts;
		_acceptEvents.push_back(std::move(acceptEvent));
	}

	return registeredAccepts > 0;
}

void AcceptManager::CloseSocket()
{
	SocketUtil::Close(_listenSock);
}

// Accept Event를 걸어줘 Iocp에서 처리할 수 있는 일감을 던져주는 역할을 수행한다.
// SocketUtil::AcceptEx를 여기서 사용할 것임.
bool AcceptManager::RegisterAccept(PendingAcceptEvent& acceptEvent)
{
	acceptEvent.Init();
	acceptEvent.session = _sessions.AcquireSession();

	DWORD recvBytes(0);

	// 1. 리슨 소켓
	// 2. 클라 소켓
	// 3. 초기 수신 데이터는 사용하지 않음: 첫 패킷은 일반 WSARecv에서 처리한다.
	// 4. 5. 원격 주소와 로컬 주소를 담기 위한 버퍼 사이즈로 SOCKADDR_IN + 16 크기로 고정됨
	const bool retVal = SocketUtil::AcceptEx(_listenSock, acceptEvent.session->GetSock(),
		acceptEvent.addressBuffer.data(), 0,
		PendingAcceptEvent::AddressBytes, PendingAcceptEvent::AddressBytes, OUT &recvBytes,
		static_cast<LPOVERLAPPED>(&acceptEvent));

	if (!retVal)
	{
		const int32 errorCode = ::WSAGetLastError();
		if (errorCode != WSA_IO_PENDING)
		{
			acceptEvent.session.reset();
			std::cout << "Accept Error " << errorCode << std::endl;
			return false;
		}
	}
	return true;
}




// Accept 처리 완료 시 , 후처리를 진행한다. callBack 처리
void AcceptManager::ProcessAccept(PendingAcceptEvent& acceptEvent)
{
	auto session = std::move(acceptEvent.session);
	ASSERT_CRASH(session);


	//클라이언트 소켓과 서버 리슨 소켓과 옵션을 동일하게 맞춰준다.

	if (false == SocketUtil::SetUpdateAcceptSocket(session->GetSock(), _listenSock))
	{
		session.reset();
		(void)RegisterAccept(acceptEvent);
		return;
	}
	if (!_iocpCore.Register(session.get()))
	{
		session.reset();
		(void)RegisterAccept(acceptEvent);
		return;
	}

	// Publish only after IOCP association; no session I/O is posted before activation.
	const std::optional<int32> sid = _sessions.ActivateSession(session);
	if (!sid)
	{
		session.reset();
		(void)RegisterAccept(acceptEvent);
		return;
	}
	std::cout << *sid << " Accept Success\n";


	S2C_ROOM_LIST packet;
	packet.size = sizeof(S2C_ROOM_LIST);
	packet.type = (uint8)S_ROOM_PACKET_TYPE::UPDATE_LIST;
	for (int32 i = 0; i < 5; ++i)
	{
		packet.member = _rooms.GetRoom(session->_curPage * 5 + i).GetMemberCount();
		packet.rmNum = session->_curPage * 5 + i;
		if (!session->DoSend(&packet))
		{
			_sessions.RequestRemoveSession(*sid);
			(void)RegisterAccept(acceptEvent);
			return;
		}
	}
	if (!session->DoRecv()) _sessions.RequestRemoveSession(*sid);
	(void)RegisterAccept(acceptEvent); // 다시 acceptEvent를 등록한다.
}
