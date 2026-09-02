#pragma once
#include "CScene.h"
#include "../UI/GameUiSnapshot.h"

class UIManager;

namespace atb
{
class ClientNetworker;
class GameCore;
}

class CLobbyScene : public CScene
{
	static constexpr int32 RoomCapacity = 100;

	struct Room
	{
		int32 member = 0;
		int32 idx = 0;
		ROOM_STATUS status = ROOM_STATUS::EMPTY;
	};
public:
	CLobbyScene(atb::GameCore& gameCore, atb::ClientNetworker& networker, UIManager& ui)
		: _gameCore(gameCore), _networker(networker), _ui(ui) {}
	~CLobbyScene() {}
	virtual void BuildObjects(ID3D12Device5* pd3dDevice, ID3D12GraphicsCommandList4* pd3dCommandList);
	virtual void ProcessInput(HWND& hWnd);
	virtual void Update(HWND& hWnd);
	virtual void Render(ID3D12GraphicsCommandList4* pd3dCommandList, CCamera* pCamera, bool);


	virtual void ReleaseUploadBuffers() {};
	virtual void ReleaseObjects() {}
	virtual void MouseAction(const POINT& mp) override;

	void ChangePage(int32);
	void UpdateRoomText(int32, int32);
	[[nodiscard]] LobbyUiSnapshot CreateUiSnapshot() const;
	void UpdateRoomStatus(int32 rn, int32 mem)
	{
		_rooms[rn].member = mem;
		for (int i = 0; i < 5; ++i)
		{
			UpdateRoomText(i, -1);
		}
	};


private:
	atb::GameCore& _gameCore;
	atb::ClientNetworker& _networker;
	UIManager& _ui;
public:
	int32	 _curPage = 0;
	int32	 _prevPage = 0;
	Room	 _rooms[RoomCapacity];
	int32	 _selectedRoomNumber = -1;
	CPlayer* _pPlayer = NULL;

};

class CTitleScene : public CScene
{

public:
	std::mutex _loginLock;
	bool _isLogin = false;
	CTitleScene(atb::GameCore& gameCore, atb::ClientNetworker& networker, UIManager& ui) : _gameCore(gameCore), _networker(networker), _ui(ui) {}
	~CTitleScene() {}

	virtual void ReleaseUploadBuffers() {};
	virtual void ReleaseObjects() {}
	virtual void BuildObjects(ID3D12Device5* pd3dDevice, ID3D12GraphicsCommandList4* pd3dCommandList);
	virtual void ProcessInput(HWND& hWnd);
	virtual void Update(HWND& hWnd);
	virtual void Render(ID3D12GraphicsCommandList4* pd3dCommandList, CCamera* pCamera, bool);
	virtual void MouseAction(const POINT& mp) override;

private:
	int32	_focus = 0;
	bool	_cap = false;
	Timer	_timer;
	atb::GameCore& _gameCore;
	atb::ClientNetworker& _networker;
	UIManager& _ui;
};

class CRoomScene : public CScene
{
	struct Member
	{
		int32 _sid = -1;
		bool isReady = false;
	};

public:
	CRoomScene(atb::GameCore& gameCore, atb::ClientNetworker& networker, UIManager& ui)
		: _gameCore(gameCore), _networker(networker), _ui(ui)
	{
		for (auto& i : _members)
		{
			i.isReady = false;
		}
	}
	~CRoomScene() {}

	virtual void BuildObjects(ID3D12Device5* pd3dDevice, ID3D12GraphicsCommandList4* pd3dCommandList);
	virtual void ProcessInput(HWND& hWnd);
	virtual void Update(HWND& hWnd);
	[[nodiscard]] RoomUiSnapshot CreateUiSnapshot();
	virtual void UpdateReady(int32 sid, bool val)
	{
		for (auto& i : _members)
		{
			if (sid == i._sid)
			{
				i.isReady = val;
			}
		}
	}
	virtual void ReleaseUploadBuffers() {};
	virtual void ReleaseObjects() {}
	virtual void Render(ID3D12GraphicsCommandList4* pd3dCommandList, CCamera* pCamera,bool);
	virtual void MouseAction(const POINT& mp) override;

private:
	atb::GameCore& _gameCore;
	atb::ClientNetworker& _networker;
	UIManager& _ui;
public:
	Member _members[4];
	std::mutex _memLock;
	int32 _roomNumber = 0;
};

class CResultScene : public CScene
{


public:
	CResultScene(atb::GameCore& gameCore, UIManager& ui)
		: _gameCore(gameCore), _ui(ui) {}
	~CResultScene() {}
	virtual void BuildObjects(ID3D12Device5* pd3dDevice, ID3D12GraphicsCommandList4* pd3dCommandList) {};
	virtual void ProcessInput(HWND& hWnd) {};
	virtual void Update(HWND& hWnd);
	[[nodiscard]] ResultUiSnapshot CreateUiSnapshot() const noexcept;
	virtual void ReleaseUploadBuffers() {};
	virtual void ReleaseObjects() {}
	virtual void Render(ID3D12GraphicsCommandList4* pd3dCommandList, CCamera* pCamera, bool bRaster) {};

	virtual void MouseAction(const POINT& mp) override {};

private:
	atb::GameCore& _gameCore;
	UIManager& _ui;
public:
	int32 _playerIdx = -1;
	int32 _gameResult = 0; // 1  escape 2 arrested
	// 사장
	// 탈출 직원 수
	int32 _exitPlayerCount = 0;
	// 죽인 횟수

	// 직원
	int32 _deathCount;   //  죽은 횟수
	int32 _activeCount; //  발전기 활성화 횟수

	Timer _timer;
	float _showTime = 4.0f; // 결과창 보여주는 시각
};
