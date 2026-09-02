#pragma once

#include "../../Shared/Types.h"
#include "../Network/ClientNetworker.h"
#include "../ClientPacketDispatcher.h"
#include "../Rendering/D2DRenderer.h"
#include "../Rendering/D3D12Renderer.h"
#include "../Core/GameCore.h"
#include "Window.h"

#include <memory>

class CScene;
class UIManager;

class CGameFramework
{
private:
	atb::Window _window;
	atb::D3D12Renderer _d3d12Renderer;
	atb::D2DRenderer _d2dRenderer;
	atb::ClientNetworker _networker;
	atb::GameCore _gameCore;
	std::unique_ptr<atb::ClientPacketDispatcher> _packetDispatcher;
	std::unique_ptr<UIManager> _uiRenderer = nullptr;
	bool _raster = true;

public:
	CGameFramework();
	~CGameFramework();

	bool OnCreate(HINSTANCE hInstance, int showCommand);
	void FinalizeClientTest();
	void OnDestroy();
	[[nodiscard]] bool ProcessWindowMessages();
	[[nodiscard]] int ExitCode() const noexcept;

	void ChangeSwapChainState();

	void BuildScenes();
	void ReleaseScenes();

	void ProcessInput();
	void UpdateObject();
	void AnimateObjects();
	void FrameAdvance();
	void Render();

	void OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	void OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	LRESULT CALLBACK OnProcessingWindowMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);

	void CheckRaytracingSupport();
	virtual void OnKeyDown(UINT8 key);
};

extern CGameFramework mainGame;
