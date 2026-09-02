#pragma once

class CGameScene;
class CLobbyScene;
class CResultScene;
class CRoomScene;
class CTitleScene;
class ClientSession;
class UIManager;

namespace atb
{
class GameCore;

class ClientPacketDispatcher final
{
public:
	ClientPacketDispatcher(GameCore& gameCore, UIManager& ui);
	void Apply(ClientSession& session, char* packet) const;

private:
	GameCore& _gameCore;
	UIManager& _ui;
	CTitleScene* _titleScene = nullptr;
	CGameScene* _gameScene = nullptr;
	CLobbyScene* _lobbyScene = nullptr;
	CRoomScene* _roomScene = nullptr;
	CResultScene* _resultScene = nullptr;
};
}
