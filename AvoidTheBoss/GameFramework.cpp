#include "pch.h"
#include "GameFramework.h"

#include "CEmployee.h"
#include "ClientTestMode.h"
#include "GameScene.h"
#include "SceneManager.h"
#include "SoundManager.h"
#include "UIManager.h"
#include "clientIocpCore.h"

CGameFramework mainGame;

CGameFramework::CGameFramework()
{
	m_curScene = static_cast<int32>(SCENESTATE::TITLE);
	SoundManager::GetInstance();
}

CGameFramework::~CGameFramework() = default;

CScene* CGameFramework::GetSceneByIdx(const int32 index) const noexcept
{
	return m_SceneManager ? m_SceneManager->GetSceneByIdx(index) : nullptr;
}

bool CGameFramework::OnCreate(HINSTANCE hInstance, const int showCommand)
{
	m_window.Initialize(
		hInstance,
		[this](HWND window, UINT message, WPARAM wParam, LPARAM lParam)
		{
			return OnProcessingWindowMessage(window, message, wParam, lParam);
		});
	m_d3d12Renderer.Initialize(m_window.Handle());
	BuildScenes();
	m_window.Show(showCommand);

#ifdef _WITH_SWAPCHAIN_FULLSCREEN_STATE
	ChangeSwapChainState();
#endif
	return true;
}

void CGameFramework::OnDestroy()
{
	if (m_d3d12Renderer.IsInitialized())
	{
		try
		{
			m_d3d12Renderer.WaitForGpuComplete();
		}
		catch (...)
		{
			::OutputDebugStringA("[cleanup] GPU wait failed; continuing resource release\n");
		}
	}

	SoundManager::GetInstance().SoundRelease();
	ReleaseScenes();
	FinalizeClientTest();
	m_d3d12Renderer.Shutdown();

#if defined(_DEBUG)
	ComPtr<IDXGIDebug1> dxgiDebug;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(dxgiDebug.GetAddressOf()))))
	{
		::OutputDebugStringW(L"\n[Phase 0] DXGI live object report\n");
		dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL);
	}
#endif

	m_window.Shutdown();
	g_clientTestMode.OnCleanupSequenceCompleted();
}

bool CGameFramework::ProcessWindowMessages()
{
	return m_window.ProcessMessages();
}

int CGameFramework::ExitCode() const noexcept
{
	return m_window.ExitCode();
}

void CGameFramework::BuildScenes()
{
	auto* commandList = m_d3d12Renderer.BeginResourceUpload();
	auto backBuffers = m_d3d12Renderer.BackBuffers();
	m_d2dRenderer.Initialize(
		m_d3d12Renderer.Device(),
		m_d3d12Renderer.CommandQueue(),
		backBuffers);

#if defined(_DEBUG)
	::OutputDebugStringA("[Phase 0] UI initialization begin\n");
#endif
	m_UIRenderer = new UIManager(
		m_d2dRenderer.Context(),
		m_d2dRenderer.WriteFactory(),
		m_d3d12Renderer.Width(),
		m_d3d12Renderer.Height());
#if defined(_DEBUG)
	::OutputDebugStringA("[Phase 0] UI initialization complete\n");
#endif

	m_SceneManager = new SceneManager();
	m_SceneManager->BuildScene(m_d3d12Renderer.Device(), commandList);
	m_d3d12Renderer.SubmitResourceUploadAndWait();
	m_SceneManager->ReleaseUpBuffers();
}

void CGameFramework::ReleaseScenes()
{
	delete m_UIRenderer;
	m_UIRenderer = nullptr;
	m_d2dRenderer.Shutdown();

	delete m_SceneManager;
	m_SceneManager = nullptr;
}

void CGameFramework::ProcessInput()
{
	HWND window = m_window.Handle();
	m_SceneManager->ProcessInput(window, m_curScene);
}

void CGameFramework::UpdateObject()
{
	HWND window = m_window.Handle();
	m_SceneManager->Update(window, m_curScene);
}

void CGameFramework::AnimateObjects()
{
	m_SceneManager->Animate(m_curScene);
}

void CGameFramework::FrameAdvance()
{
	// Packet handlers may replace GPU-backed scene objects, so wait before dispatch.
	m_d3d12Renderer.WaitForPreviousFrame();

	clientCore.DispatchPackets();
	ProcessInput();
	UpdateObject();
	AnimateObjects();

	if (g_clientTestMode.Enabled())
	{
		ClientFrameSnapshot snapshot;
		snapshot._scene = m_curScene.load();
		snapshot._submittedFence = m_d3d12Renderer.LastSubmittedFenceValue();
		snapshot._completedFence = m_d3d12Renderer.CompletedFenceValue();
		const int playerIndex = g_clientTestMode.DutPlayerIndex();
		auto* gameScene = static_cast<CGameScene*>(m_SceneManager->GetSceneByIdx(
			static_cast<int>(SCENESTATE::INGAME)));
		CPlayer* player = gameScene ? gameScene->GetScenePlayerByIdx(playerIndex) : nullptr;
		CCamera* camera = gameScene ? gameScene->GetCameraComponent() : nullptr;
		if (player)
		{
			snapshot._health = player->GetHealth();
			snapshot._behavior = player->GetBehavior();
			snapshot._hidden = player->IsHidden();
			if (playerIndex > 0)
				snapshot._rescuing = static_cast<CEmployee*>(player)->GetRescueOn();
		}
		if (camera)
		{
			snapshot._cameraMode = static_cast<int>(camera->GetMode());
			snapshot._cameraIdentity = reinterpret_cast<std::uintptr_t>(camera);
			snapshot._cameraResourcesValid = camera->HasShaderVariables();
			snapshot._cameraBufferAddress = camera->GetBufferAddress();
			snapshot._cameraBufferCreateCount = camera->GetBufferCreateCount();
		}
		if (gameScene)
		{
			CPlayer* localPlayer = gameScene->GetLocalPlayer();
			snapshot._renderCameraStateValid =
				gameScene->GetRenderCamera() == (localPlayer ? camera : nullptr);
			snapshot._localPlayerMatchesDut =
				localPlayer == player && gameScene->GetLocalPlayerIndex() == playerIndex;
			snapshot._cameraViewerMatchesLocal =
				camera && camera->GetViewerIndex() == gameScene->GetLocalPlayerIndex();
		}
		if (g_clientTestMode.Pump(snapshot))
		{
			::PostQuitMessage(0);
			return;
		}
	}

	Render();
	const int32 currentScene = m_curScene.load();
	int32 localPlayerIndex = -1;
	if (currentScene == static_cast<int32>(SCENESTATE::INGAME))
	{
		auto* gameScene = static_cast<CGameScene*>(m_SceneManager->GetSceneByIdx(currentScene));
		if (gameScene) localPlayerIndex = gameScene->GetLocalPlayerIndex();
	}
	m_d2dRenderer.BeginFrame(m_d3d12Renderer.FrameIndex());
	m_UIRenderer->Render2D(currentScene, localPlayerIndex);
	m_d2dRenderer.EndFrame();

	const HRESULT presentResult = m_d3d12Renderer.Present();
	if (g_clientTestMode.Enabled()) g_clientTestMode.OnPresent(presentResult);
	m_d3d12Renderer.MoveToNextFrame();
}

void CGameFramework::FinalizeClientTest()
{
	if (!g_clientTestMode.Enabled()) return;

	std::uint32_t errorCount = 0;
	bool infoQueueAvailable = false;

#if defined(_DEBUG)
	ComPtr<ID3D12InfoQueue> infoQueue;
	auto* device = m_d3d12Renderer.Device();
	if (!device || FAILED(device->QueryInterface(IID_PPV_ARGS(infoQueue.GetAddressOf()))))
	{
		++errorCount;
		g_clientTestMode.OnD3DMessage("ID3D12InfoQueue is unavailable");
	}
	else
	{
		infoQueueAvailable = true;
		if (infoQueue->GetNumMessagesDiscardedByMessageCountLimit() != 0)
		{
			++errorCount;
			g_clientTestMode.OnD3DMessage(
				"D3D12 InfoQueue discarded messages at its storage limit");
		}
		const UINT64 messageCount = infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();
		for (UINT64 index = 0; index < messageCount; ++index)
		{
			SIZE_T messageSize = 0;
			if (FAILED(infoQueue->GetMessage(index, nullptr, &messageSize)) || messageSize == 0)
			{
				++errorCount;
				g_clientTestMode.OnD3DMessage(
					"failed to query a D3D12 InfoQueue message size");
				continue;
			}

			std::vector<std::byte> storage(messageSize);
			auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
			if (FAILED(infoQueue->GetMessage(index, message, &messageSize)))
			{
				++errorCount;
				g_clientTestMode.OnD3DMessage("failed to retrieve a D3D12 InfoQueue message");
				continue;
			}
			if (message->Severity == D3D12_MESSAGE_SEVERITY_ERROR ||
				message->Severity == D3D12_MESSAGE_SEVERITY_CORRUPTION)
			{
				++errorCount;
				g_clientTestMode.OnD3DMessage(message->pDescription);
			}
		}
	}
#endif

	g_clientTestMode.FinalizeGraphics(
		infoQueueAvailable,
		errorCount,
		m_d3d12Renderer.DeviceRemovedReason(),
		m_d3d12Renderer.CompletedFenceValue(),
		m_d3d12Renderer.LastSubmittedFenceValue());
}

void CGameFramework::Render()
{
	const std::array<float, 4> clearColor = m_raster
		? std::array<float, 4>{ 0.0f, 0.125f, 0.3f, 1.0f }
		: std::array<float, 4>{ 0.6f, 0.8f, 0.4f, 1.0f };
	auto* commandList = m_d3d12Renderer.BeginFrame(clearColor);
	m_SceneManager->Render(commandList, m_curScene, true);
	m_d3d12Renderer.SubmitFrame();
}

void CGameFramework::ChangeScene(const SCENESTATE state)
{
	m_curScene.store(static_cast<int32>(state));
	if (m_curScene <= static_cast<int32>(SCENESTATE::RESULT))
		SoundManager::GetInstance().PlayBackGroundSound(m_curScene);
}

void CGameFramework::OnProcessingMouseMessage(
	HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	m_SceneManager->GetSceneByIdx(m_curScene)->OnProcessingMouseMessage(
		hWnd, nMessageID, wParam, lParam);
}

void CGameFramework::OnProcessingKeyboardMessage(
	HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	if (nMessageID == WM_KEYUP && wParam == VK_ESCAPE) ::PostQuitMessage(0);
	// F9 remains disabled until D3D11On12 wrapped targets support resize.
	m_SceneManager->GetSceneByIdx(m_curScene)->OnProcessingKeyboardMessage(
		hWnd, nMessageID, wParam, lParam);
}

LRESULT CGameFramework::OnProcessingWindowMessage(
	HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	switch (nMessageID)
	{
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MOUSEMOVE:
		OnProcessingMouseMessage(hWnd, nMessageID, wParam, lParam);
		break;
	case WM_KEYDOWN:
		OnKeyDown(static_cast<UINT8>(wParam));
		break;
	case WM_KEYUP:
		OnProcessingKeyboardMessage(hWnd, nMessageID, wParam, lParam);
		break;
	default:
		break;
	}
	return 0;
}

void CGameFramework::CheckRaytracingSupport()
{
	m_d3d12Renderer.CheckRaytracingSupport();
}

void CGameFramework::OnKeyDown(const UINT8 key)
{
	if (key == VK_NUMPAD0) m_raster = !m_raster;
}

void CGameFramework::ChangeSwapChainState()
{
	m_d3d12Renderer.ChangeSwapChainState();
}
