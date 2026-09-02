#include "../Platform/pch.h"

#include "ClientPacketDispatcher.h"
#include "ClientSession.h"

#include "../Core/GameCore.h"
#include "../Diagnostics/ClientTestMode.h"
#include "../UI/UIManager.h"

#include "ClientPacketEvent.h"
#include "../Gameplay/Player.h"

#include "../Scenes/OtherScenes.h"
#include "../Scenes/GameScene.h"

namespace atb
{
ClientPacketDispatcher::ClientPacketDispatcher(GameCore& gameCore, UIManager& ui)
	: _gameCore(gameCore),
	  _ui(ui),
	  _titleScene(static_cast<CTitleScene*>(gameCore.Scene(SceneId::Title))),
	  _gameScene(static_cast<CGameScene*>(gameCore.Scene(SceneId::InGame))),
	  _lobbyScene(static_cast<CLobbyScene*>(gameCore.Scene(SceneId::Lobby))),
	  _roomScene(static_cast<CRoomScene*>(gameCore.Scene(SceneId::Room))),
	  _resultScene(static_cast<CResultScene*>(gameCore.Scene(SceneId::Result)))
{
}

void ClientPacketDispatcher::Apply(ClientSession& session, char* packet) const
{

	auto* pTitleScene = _titleScene;
	auto* pGameScene = _gameScene;
	auto* pLobbyScene = _lobbyScene;
	auto* pRoomScene = _roomScene;
	auto* pResultScene = _resultScene;
	if (!pGameScene)
	{
		return; // something error detected;
	}
	if (!pLobbyScene)
	{
		return; // something error detected;
	}
	if (!pRoomScene)
	{
		return;
	}
	if (!pResultScene)
	{
		return;
	}

	const auto packetType = static_cast<uint8>(packet[1]);
	const auto requiresInGameScene =
		packetType == static_cast<uint8>(S_GAME_PACKET_TYPE::SKEY) ||
		packetType == static_cast<uint8>(S_GAME_PACKET_TYPE::SROT) ||
		packetType == static_cast<uint8>(S_GAME_PACKET_TYPE::SPOS) ||
		packetType == static_cast<uint8>(SC_GAME_PACKET_TYPE::GAMEEVENT) ||
		packetType == static_cast<uint8>(S_GAME_PACKET_TYPE::ANIM) ||
		packetType == static_cast<uint8>(S_GAME_PACKET_TYPE::FRAME);
	if (requiresInGameScene && _gameCore.CurrentScene() != SceneId::InGame)
	{
		return;
	}

	switch (packetType)
	{

	// ================ 로그인 관련 처리 ================
#pragma region Title
	case (uint8)S_TITLE_PACKET_TYPE::LOGIN_OK:
	{
		S2C_LOGIN_OK* lo = (S2C_LOGIN_OK*)packet;
		CScene::_sid = lo->sid;
		CScene::_cid = lo->cid;
		pTitleScene->_loginLock.lock();
		pTitleScene->_isLogin = true;
		pTitleScene->_loginLock.unlock();
		_ui.ShowLoginFeedback(UIManager::LoginFeedback::LoginOk);
		g_clientTestMode.OnLoginOk(session);
	}
	break;
	case (uint8)S_TITLE_PACKET_TYPE::RESUME_OK:
	{
		auto* resume = reinterpret_cast<S2C_RESUME*>(packet);
		if (resume->playerIndex >= PLAYERNUM)
		{
			break;
		}
		if (resume->oldSid != session.GetResumeSid())
		{
			break;
		}
		session.SetSid(resume->newSid);
		session._resumeSid.store(-1, std::memory_order_release);
		CScene::_sid = resume->newSid;
		if (CPlayer *player = pGameScene->GetScenePlayerByIdx(resume->playerIndex))
		{
			player->SetPlayerSid(resume->newSid);
		}
		if (pGameScene->GetLocalPlayerIndex() == resume->playerIndex)
		{
			pGameScene->MarkInputDirty();
		}
	}
	break;
	case (uint8)S_TITLE_PACKET_TYPE::RESUME_FAIL:
	{
		session.ClearResumeToken();
		session._resumeSid.store(-1, std::memory_order_release);
		session.SetSid(-1);
		CScene::_sid = -1;
		if (_gameCore.CurrentScene() == SceneId::InGame)
		{
			pGameScene->ResetGame();
		}
		_gameCore.ChangeScene(SceneId::Title);
	}
	break;
	case (uint8)S_TITLE_PACKET_TYPE::LOGIN_FAIL:
	{
		_ui.ShowLoginFeedback(UIManager::LoginFeedback::LoginFailed);
		g_clientTestMode.OnProtocolFailure("login failed");
	}
	break;
	case (uint8)S_TITLE_PACKET_TYPE::REG_OK:
	{
		_ui.ShowLoginFeedback(UIManager::LoginFeedback::RegistrationOk);
	}
	break;

#pragma endregion
	// ================ 로비씬 패킷      ===============
#pragma region  Lobby
	// ============= 방 관련 패킷 ============
	case (uint8)S_ROOM_PACKET_TYPE::REP_ENTER_OK:
	{
		S2C_ROOM_ENTER* rep = (S2C_ROOM_ENTER*)packet;
		pRoomScene->_roomNumber = rep->rmNum;
		_gameCore.ChangeScene(SceneId::Room);
		g_clientTestMode.OnRoomEntered(session, rep->rmNum);
	}
	break;
	case (uint8)S_ROOM_PACKET_TYPE::REP_ENTER_FAIL:
	{
		g_clientTestMode.OnProtocolFailure("room enter failed");
	}
	break;
	case (uint8)S_ROOM_PACKET_TYPE::MK_RM_FAIL:
	{

	}
	break;
	case (uint8)S_ROOM_PACKET_TYPE::MK_RM_OK:
	{

		_gameCore.ChangeScene(SceneId::Room);
	}
		break;
	case (uint8)S_ROOM_PACKET_TYPE::UPDATE_LIST:
	{

		S2C_ROOM_LIST* rp = (S2C_ROOM_LIST*)packet;;
		pLobbyScene->UpdateRoomStatus(rp->rmNum, rp->member);
		std::cout << "RM" << (int32)rp->rmNum << " MEMBER:" << (int32)rp->member << "\n";
	}
		break;

#pragma endregion
#pragma region Room
	case (uint8)S_ROOM_PACKET_TYPE::REP_READY:
	{
		S2C_ROOM_READY* rp = (S2C_ROOM_READY*)packet;
		pRoomScene->_memLock.lock();
		pRoomScene->UpdateReady(rp->sid, true);
		pRoomScene->_memLock.unlock();

		std::cout << rp->sid << "Ready\n";
	}
		break;
	case (uint8)S_ROOM_PACKET_TYPE::REP_READY_CANCEL:
	{
		S2C_ROOM_READY* rp = (S2C_ROOM_READY*)packet;
		pRoomScene->_memLock.lock();
		pRoomScene->UpdateReady(rp->sid, false);
		pRoomScene->_memLock.unlock();
		std::cout << rp->sid << "Cancel Ready\n";
	}
		break;
	case (uint8)S_ROOM_PACKET_TYPE::ROOM_INFO:
	{

		S2C_ROOM_INFO* rp = (S2C_ROOM_INFO*)packet;
		pRoomScene->_memLock.lock();
		for (int i = 0; i < PLAYERNUM; ++i)
		{
			pRoomScene->_members[i]._sid = rp->sids[i];
		}
		for (int i = 0; i < PLAYERNUM; ++i)
		{
			pRoomScene->_members[i].isReady = rp->rd[i];
		}

		pRoomScene->_memLock.unlock();
	}
	break;
	case (uint8)S_ROOM_PACKET_TYPE::GAME_START:
	{
		// ================= 플레이어 초기 위치 초기화 ==================
		auto* gameStart = reinterpret_cast<S2C_GAMESTART*>(packet);
		if (!g_clientTestMode.ValidateGameStart(gameStart->sids, session.GetSid()))
		{
			break;
		}
		if (!pGameScene->InitGame(gameStart, session.GetSid()))
		{
			g_clientTestMode.OnProtocolFailure("invalid or duplicate GAME_START");
			break;
		}


		// ================= 카메라 셋팅 ================================
		_ui.InitGameSceneUI(pGameScene->CreateUiSnapshot());

		_gameCore.ChangeScene(SceneId::InGame);
		pGameScene->InitScene();
		g_clientTestMode.OnGameStarted();
		pRoomScene->_memLock.lock();

		for (int i = 0; i < PLAYERNUM; ++i)
		{
			pRoomScene->_members[i]._sid = -1;
			pRoomScene->_members[i].isReady = false;
		}
		pRoomScene->_memLock.unlock();
	}
	break;
#pragma endregion
	// ============== 인게임 관련 패킷 =============
#pragma region InGame
	case (uint8)S_GAME_PACKET_TYPE::SKEY:
	{
		S2C_KEY* movePacket = reinterpret_cast<S2C_KEY*>(packet);
		CPlayer* player = pGameScene ->GetScenePlayerBySid(movePacket->sid);
		if (player == nullptr)
		{
			break;
		}

		pGameScene->AddEvent(moveEvent{
			player->GetPlayerIndex(),
			movePacket->key,
			XMFLOAT3(movePacket->x, 0.0f, movePacket->z) },
			_gameCore.PacketEventDelayMs());
	}
	break;
	case (uint8)S_GAME_PACKET_TYPE::SROT:
	{
		S2C_ROTATE* rotatePacket = reinterpret_cast<S2C_ROTATE*>(packet);
		CPlayer* player = pGameScene->GetScenePlayerBySid(rotatePacket->sid);
		if (!player)
		{
			break;
		}
		pGameScene->AddEvent(rotateEvent{ player->GetPlayerIndex(), static_cast<float>(rotatePacket->angle) },
			_gameCore.PacketEventDelayMs());
	}
	break;
	case (uint8)S_GAME_PACKET_TYPE::SPOS: // 미리 계산한 좌표값을 보내준다.
	{
		S2C_POS* posPacket = reinterpret_cast<S2C_POS*>(packet);
		CPlayer* player = pGameScene->GetScenePlayerBySid(posPacket->sid);
		if (player == nullptr)
		{
			break;
		}

		XMFLOAT3 newPos = XMFLOAT3(posPacket->x, player->GetPosition().y, posPacket->z);
		pGameScene->AddEvent(posEvent{ player->GetPlayerIndex(), newPos },
			_gameCore.PacketEventDelayMs());
	}
	break;
	case (uint8)SC_GAME_PACKET_TYPE::GAMEEVENT:
	{
		SC_EVENTPACKET* ev = (SC_EVENTPACKET*)packet;
		if (ev->eventId == (uint8)EVENT_TYPE::BOSS_WIN || ev->eventId == (uint8)EVENT_TYPE::EMP_WIN)
		{
			if (!pGameScene->ResetGame())
			{
				g_clientTestMode.OnProtocolFailure("duplicate or out-of-order game result");
				break;
			}

			std::cout << "Go to Result\n";

			if (ev->eventId == (uint8)EVENT_TYPE::BOSS_WIN)
			{
				pResultScene->_gameResult = 1;
			}
			if (ev->eventId == (uint8)EVENT_TYPE::EMP_WIN)
			{
				pResultScene->_gameResult = 0;
			}

			pResultScene->_timer.Reset();
			_gameCore.ChangeScene(SceneId::Result);
		}
		else
		{
			pGameScene->AddEvent(InteractionEvent{ ev->eventId },
				_gameCore.PacketEventDelayMs());
		}
	}
	break;
	// ================= 플레이어 스위치 애니메이션 관련 패킷 ==================
	case (uint8)S_GAME_PACKET_TYPE::ANIM:
	{
		S2C_ANIMPACKET* sw = (S2C_ANIMPACKET*)packet;
		if (!pGameScene->GetScenePlayerByIdx(sw->idx))
		{
			break;
		}
		pGameScene->AddEvent(animationEvent{ sw->idx, sw->track },
			_gameCore.PacketEventDelayMs());
	}
	break;
	case (uint8)S_GAME_PACKET_TYPE::FRAME:
	{
		S2C_FRAMEPACKET* fp = (S2C_FRAMEPACKET*)packet;
		pGameScene->AddEvent(FrameEvent{ fp->wf }, _gameCore.PacketEventDelayMs());

	}
	break;
#pragma endregion
	}

}
}
