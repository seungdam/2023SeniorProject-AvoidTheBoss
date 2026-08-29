#pragma once

#include "../Shared/Types.h"
#include "D3D12Renderer.h"

class CScene;
class UIManager;
class SceneManager;

class CGameFramework
{
public:
	enum class SCENESTATE { TITLE = 0, LOBBY = 1, ROOM = 2, INGAME = 3, RESULT = 4 };

	friend class CEmployee;
	friend class CBoss;
	friend class ClientSession;
	friend class CGameScene;
	friend class CRoomScene;
	friend class CLobbyScene;
	friend class CTitleScene;
	friend class UIManager;
	friend class CGenerator;

private:
	HINSTANCE m_hInstance = nullptr;
	HWND m_hWnd = nullptr;
	atb::D3D12Renderer m_d3d12Renderer;

protected:
	SceneManager* m_SceneManager = nullptr;
	UIManager* m_UIRenderer = nullptr;

public:
	Atomic<int32> m_curFrame = 0;
	bool m_activeDelay = false;
	Atomic<int32> m_curScene = 3;

	CGameFramework();
	~CGameFramework();
	CScene* GetSceneByIdx(int32 index) const noexcept;

	bool OnCreate(HINSTANCE hInstance, HWND hMainWnd);
	void FinalizeClientTest();
	void OnDestroy();

	void ChangeSwapChainState();

	void BuildScenes();
	void ReleaseScenes();

	void ProcessInput();
	void UpdateObject();
	void AnimateObjects();
	void FrameAdvance();
	void Render();

	void ChangeScene(SCENESTATE ss);
	void OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	void OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	LRESULT CALLBACK OnProcessingWindowMessage(
		HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);

	void CheckRaytracingSupport();
	virtual void OnKeyDown(UINT8 key);
	bool m_raster = true;
};

extern CGameFramework mainGame;
