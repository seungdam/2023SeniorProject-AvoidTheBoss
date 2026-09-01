#include "pch.h"

#include "Player.h"
#include "ClientNetworker.h"
#include "GameCore.h"

#include "InputManager.h"
#include "SoundManager.h"
#include "UIManager.h"
#include "OtherScenes.h"
#include "SoundManager.h"
#include "CSound.h"


ResultUiSnapshot CResultScene::CreateUiSnapshot() const noexcept
{
	return ResultUiSnapshot{ .bossWon = _gameResult == 1 };
}

void CResultScene::Update(HWND& hWnd)
{
	_ui.UpdateResultSceneUI(CreateUiSnapshot());
	_timer.Tick(0.0f);
	if (_showTime > 0)
	{
		_showTime -= _timer.GetTimeElapsed();
	}
	if (_showTime < 0)
	{
		_showTime = 4.0f;
		_gameCore.ChangeScene(atb::SceneId::Lobby);
	}
}

#pragma region Lobby

void CLobbyScene::BuildObjects(ID3D12Device5* pd3dDevice, ID3D12GraphicsCommandList4* pd3dCommandList)
{
}

void CLobbyScene::ProcessInput(HWND& hWnd)
{
}

void CLobbyScene::Update(HWND& hWnd)
{
}

void CLobbyScene::Render(ID3D12GraphicsCommandList4* pd3dCommandList, CCamera* pCamera, bool Raster)
{
}

void CLobbyScene::MouseAction(const POINT& mp)
{
	SoundManager::GetInstance().SoundStop(21);
	SoundManager::GetInstance().PlayObjectSound(21, 21);
	auto& ui = _ui;
	// 체크리스트 충돌체크 처리
	for (int i = 0; i < UIManager::LobbyRoomSlotCount; ++i)
	{
		if (ui.TrySelectLobbyRoomSlot(i, mp))
		{
			if (_rooms[_curPage * 5 + i].status != ROOM_STATUS::EMPTY)
			{
				_selectedRoomNumber = _curPage * 5 + i;
				std::cout << "Selected RM:" << _selectedRoomNumber << "\n";
			}
		}
	}

	if (ui.HitTest(UIManager::UiHitTarget::LobbyEnter, mp))
	{
		//enter
		if (_selectedRoomNumber != -1)
		{
			C2S_ROOM_ENTER packet;
			packet.size = sizeof(C2S_ROOM_ENTER);
			packet.type = (uint8)C_ROOM_PACKET_TYPE::ACQ_ENTER_RM;
			packet.rmNum = _selectedRoomNumber;
			_selectedRoomNumber = -1;
			_networker.Send(&packet);
		}
		else
		{
			std::cout << "There is No Any Room Available\n";
		}
	}
	else if (ui.HitTest(UIManager::UiHitTarget::LobbyCreate, mp))
	{
		//create
		C2S_ROOM_EVENT packet;
		packet.size = sizeof(C2S_ROOM_EVENT);
		packet.type = (uint8)C_ROOM_PACKET_TYPE::ACQ_MK_RM;
		_networker.Send(&packet);
	}
	else if (ui.HitTest(UIManager::UiHitTarget::LobbyLogout, mp))
	{
		if (_networker.Logout())
		{
			_gameCore.ChangeScene(atb::SceneId::Title);
		}
	}
}
void CLobbyScene::ChangePage(int32 newPage)
{
	_prevPage = _curPage;
	_curPage = newPage;
}
void CLobbyScene::UpdateRoomText(int32 index = -1, int32 member = -1)
{

	if (index >= 0 || member >= 0)
	{
		if (member != -1)
		{
			_rooms[_curPage * 5 + index].member = member;
		}
		if (PLAYERNUM == _rooms[_curPage * 5 + index].member)
		{
			_rooms[_curPage * 5 + index].status = ROOM_STATUS::FULL;
		}
		else if (0 == _rooms[_curPage * 5 + index].member)
		{
			_rooms[_curPage * 5 + index].status = ROOM_STATUS::EMPTY;
		}
		else
		{
			_rooms[_curPage * 5 + index].status = ROOM_STATUS::NOT_FULL;
		}
	}
	_ui.UpdateLobbySceneUI(CreateUiSnapshot());
}

LobbyUiSnapshot CLobbyScene::CreateUiSnapshot() const
{
	LobbyUiSnapshot snapshot;
	for (std::size_t index = 0; index < snapshot.rooms.size(); ++index)
	{
		const int32 roomNumber = _curPage * static_cast<int32>(atb::client::ui::LobbyRoomSlotCount) +
			static_cast<int32>(index);
		const auto& room = _rooms[roomNumber];
		if (room.status == ROOM_STATUS::FULL ||
			room.status == ROOM_STATUS::EMPTY ||
			room.status == ROOM_STATUS::INGAME) {
			continue;
}

		snapshot.rooms[index] = LobbyRoomUiSnapshot{
			.roomNumber = roomNumber,
			.memberCount = room.member
		};
	}
	return snapshot;
}

#pragma endregion


#pragma region  Title
void CTitleScene::BuildObjects(ID3D12Device5* pd3dDevice, ID3D12GraphicsCommandList4* pd3dCommandList)
{
	_timer.Reset();
	SoundManager::GetInstance().PlayBackGroundSound(atb::SceneIndex(atb::SceneId::Title));
}
void CTitleScene::MouseAction(const POINT& mp)
{
	SoundManager::GetInstance().SoundStop(20);
	SoundManager::GetInstance().PlayObjectSound(20, 20);
	auto& ui = _ui;

	if (ui.HitTest(UIManager::UiHitTarget::TitleId, mp))
	{
		std::cout << "Focus Change 0\n";
		_focus = 0;
	}
	else if (ui.HitTest(UIManager::UiHitTarget::TitlePassword, mp))
	{
		std::cout << "Focus Change 1\n";
		_focus = 1;
	}

	if (ui.HitTest(UIManager::UiHitTarget::TitleLogin, mp))
	{
		if (ui.CredentialText(0).empty() || ui.CredentialText(1).empty())
		{
			return;
		}

		C2S_LOGIN loginPacket;
		loginPacket.size = sizeof(C2S_LOGIN);
		loginPacket.type = (uint8)C_TITLE_PACKET_TYPE::ACQ_LOGIN;
		lstrcpyn(loginPacket.name, ui.CredentialText(0).c_str(), 10);
		lstrcpyn(loginPacket.pw, ui.CredentialText(1).c_str(), 10);
		_networker.Send(&loginPacket);

	}
	else if (ui.HitTest(UIManager::UiHitTarget::TitleRegister, mp))
	{
		C2S_LOGIN loginPacket;
		loginPacket.size = sizeof(C2S_LOGIN);
		loginPacket.type = (uint8)C_TITLE_PACKET_TYPE::ACQ_REG;
		lstrcpyn(loginPacket.name, ui.CredentialText(0).c_str(), 10);
		lstrcpyn(loginPacket.pw, ui.CredentialText(1).c_str(), 10);
		_networker.Send(&loginPacket);
	}
	else if (ui.HitTest(UIManager::UiHitTarget::TitleQuit, mp))
	{
		::PostQuitMessage(0);
	}
}

void CTitleScene::ProcessInput(HWND& hWnd)
{
	InputManager::GetInstance().InputStatusUpdate();
	auto& ui = _ui;

	// TAB 처리
	if ((int8)KEY_STATUS::KEY_UP == InputManager::GetInstance().GetKeyBuffer(VK_TAB))
	{
		if (0 == _focus)
		{
			_focus = 1;
		}
		else if (1 == _focus)
		{
			_focus = 0;
		}
		std::cout << _focus << "\n";
	}
	// CAP 처리
	if ((int8)KEY_STATUS::KEY_UP == InputManager::GetInstance().GetKeyBuffer(VK_CAPITAL))
	{
		if (_cap)
		{
			_cap = false;
		}
		else
		{
			_cap = true;
		}
	}
	//알파벳 입력 받기
	for (int i = 65; i < 90; ++i)
	{
		if ((int8)KEY_STATUS::KEY_UP == InputManager::GetInstance().GetKeyBuffer(i))
		{
			wchar_t str[2];
			if (_cap)
			{
				str[0] = i;
			}
			else
			{
				str[0] = i + 32;
			}
			str[1] = '\0';
			ui.AppendCredential(_focus, str[0]);
		}
	}
	for (int i = 0; i < 10; ++i)
	{
		if ((int8)KEY_STATUS::KEY_UP == InputManager::GetInstance().GetKeyBuffer((int32)KEY_TYPE::NUM0 + i))
		{
			wchar_t str[2];
			_itow_s(i, str, 10);
			ui.AppendCredential(_focus, str[0]);
		}
	}

	// 텍스트 지우기
	if ((int8)KEY_STATUS::KEY_UP == InputManager::GetInstance().GetKeyBuffer(VK_BACK))
	{
		ui.BackspaceCredential(_focus);
	}
}
void CTitleScene::Update(HWND& hWnd)
{
	_timer.Tick(0.0f);
	const auto expiredFeedback = _ui.TickLoginFeedback(_timer.GetTimeElapsed());
	if (expiredFeedback)
	{
		_loginLock.lock();
		if (_isLogin && *expiredFeedback == UIManager::LoginFeedback::LoginOk)
		{
			_gameCore.ChangeScene(atb::SceneId::Lobby);
			_isLogin = false;
		}
		_loginLock.unlock();
	}
}
void CTitleScene::Render(ID3D12GraphicsCommandList4* pd3dCommandList, CCamera* pCamera,bool Raster)
{


}
#pragma endregion

#pragma region Room

void CRoomScene::BuildObjects(ID3D12Device5* pd3dDevice, ID3D12GraphicsCommandList4* pd3dCommandList)
{
}

void CRoomScene::ProcessInput(HWND& hWnd)
{
}

void CRoomScene::Update(HWND& hWnd)
{
	const RoomUiSnapshot snapshot = CreateUiSnapshot();
	_ui.UpdateRoomSceneUI(snapshot);
}

RoomUiSnapshot CRoomScene::CreateUiSnapshot()
{
	RoomUiSnapshot snapshot;
	std::lock_guard memberLock(_memLock);
	for (std::size_t index = 0; index < snapshot.members.size(); ++index)
	{
		snapshot.members[index].occupied = _members[index]._sid != -1;
		snapshot.members[index].ready = _members[index].isReady;
	}
	return snapshot;
}

void CRoomScene::Render(ID3D12GraphicsCommandList4* pd3dCommandList, CCamera* pCamera, bool Raster)
{
}

void CRoomScene::MouseAction(const POINT& mp)
{
	SoundManager::GetInstance().SoundStop(21);
	SoundManager::GetInstance().PlayObjectSound(21, 21);

	_memLock.lock();
	if (_ui.HitTest(UIManager::UiHitTarget::RoomReady, mp)) // Ready
	{

		for (int i = 0; i < PLAYERNUM; ++i)
		{
			if (_members[i]._sid == CScene::_sid)
			{
				if (!_members[i].isReady)
				{
					_members[i].isReady = true;
					C2S_ROOM_EVENT packet;
					packet.size = sizeof(C2S_ROOM_EVENT);
					packet.type = (uint8)C_ROOM_PACKET_TYPE::ACQ_READY;
					_networker.Send(&packet);
				}
				else
				{
					_members[i].isReady = false;
					C2S_ROOM_EVENT packet;
					packet.size = sizeof(C2S_ROOM_EVENT);
					packet.type = (uint8)C_ROOM_PACKET_TYPE::ACQ_READY_CANCEL;
					_networker.Send(&packet);
				}
				break;
			}
		}

	}
	else if (_ui.HitTest(UIManager::UiHitTarget::RoomLeave, mp))
	{

		C2S_ROOM_EVENT acpacket;
		acpacket.size = sizeof(C2S_ROOM_EVENT);
		acpacket.type = (uint8)C_ROOM_PACKET_TYPE::ACQ_EXIT_ROOM;
		_networker.Send(&acpacket);

		_gameCore.ChangeScene(atb::SceneId::Lobby);
	}
	_memLock.unlock();
}


#pragma endregion
