#include "pch.h"
#include "GameFramework.h"

#include "ClientTestMode.h"
#include "SoundManager.h"
#include "UIManager.h"

CGameFramework mainGame;

CGameFramework::CGameFramework()
{
	SoundManager::GetInstance();
}

CGameFramework::~CGameFramework() = default;

bool CGameFramework::OnCreate(HINSTANCE hInstance, const int showCommand)
{
	_window.Initialize(
		hInstance,
		[this](HWND window, UINT message, WPARAM wParam, LPARAM lParam)
		{
			return OnProcessingWindowMessage(window, message, wParam, lParam);
		});
	_d3d12Renderer.Initialize(_window.Handle());
	BuildScenes();
	_window.Show(showCommand);
	_networker.Initialize("127.0.0.1");

#ifdef _WITH_SWAPCHAIN_FULLSCREEN_STATE
	ChangeSwapChainState();
#endif
	return true;
}

void CGameFramework::OnDestroy()
{
	_networker.Shutdown();

	if (_d3d12Renderer.IsInitialized())
	{
		try
		{
			_d3d12Renderer.WaitForGpuComplete();
		}
		catch (...)
		{
			::OutputDebugStringA("[cleanup] GPU wait failed; continuing resource release\n");
		}
	}

	SoundManager::GetInstance().SoundRelease();
	ReleaseScenes();
	FinalizeClientTest();
	_d3d12Renderer.Shutdown();

#if defined(_DEBUG)
	ComPtr<IDXGIDebug1> dxgiDebug;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(dxgiDebug.GetAddressOf()))))
	{
		::OutputDebugStringW(L"\n[Phase 0] DXGI live object report\n");
		dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL);
	}
#endif

	_window.Shutdown();
	g_clientTestMode.OnCleanupSequenceCompleted();
}

bool CGameFramework::ProcessWindowMessages()
{
	return _window.ProcessMessages();
}

int CGameFramework::ExitCode() const noexcept
{
	return _window.ExitCode();
}

void CGameFramework::BuildScenes()
{
	auto* commandList = _d3d12Renderer.BeginResourceUpload();
	auto backBuffers = _d3d12Renderer.BackBuffers();
	_d2dRenderer.Initialize(
		_d3d12Renderer.Device(),
		_d3d12Renderer.CommandQueue(),
		backBuffers);

#if defined(_DEBUG)
	::OutputDebugStringA("[Phase 0] UI initialization begin\n");
#endif
	_uiRenderer = new UIManager(
		_d2dRenderer.Context(),
		_d2dRenderer.WriteFactory(),
		_d3d12Renderer.Width(),
		_d3d12Renderer.Height());
#if defined(_DEBUG)
	::OutputDebugStringA("[Phase 0] UI initialization complete\n");
#endif

	_gameCore.Initialize(_d3d12Renderer.Device(), commandList, _networker, *_uiRenderer);
	_packetDispatcher = std::make_unique<atb::ClientPacketDispatcher>(_gameCore, *_uiRenderer);
	_d3d12Renderer.SubmitResourceUploadAndWait();
	_gameCore.ReleaseUploadBuffers();
}

void CGameFramework::ReleaseScenes()
{
	_packetDispatcher.reset();
	_gameCore.Shutdown();
	delete _uiRenderer;
	_uiRenderer = nullptr;
	_d2dRenderer.Shutdown();
}

void CGameFramework::ProcessInput()
{
	_gameCore.ProcessInput(_window.Handle());
}

void CGameFramework::UpdateObject()
{
	_gameCore.Update(_window.Handle());
}

void CGameFramework::AnimateObjects()
{
	_gameCore.Animate();
}

void CGameFramework::FrameAdvance()
{
	// Packet handlers may replace GPU-backed scene objects, so wait before dispatch.
	_d3d12Renderer.WaitForPreviousFrame();

	if (_packetDispatcher) _networker.DispatchPackets(*_packetDispatcher);
	ProcessInput();
	UpdateObject();
	AnimateObjects();

	if (g_clientTestMode.Enabled())
	{
		ClientFrameSnapshot snapshot =
			_gameCore.CaptureClientFrameSnapshot(g_clientTestMode.DutPlayerIndex());
		snapshot._submittedFence = _d3d12Renderer.LastSubmittedFenceValue();
		snapshot._completedFence = _d3d12Renderer.CompletedFenceValue();
		if (g_clientTestMode.Pump(snapshot))
		{
			::PostQuitMessage(0);
			return;
		}
	}

	Render();
	const int32 currentScene = atb::SceneIndex(_gameCore.CurrentScene());
	const int32 localPlayerIndex = _gameCore.CurrentLocalPlayerIndex();
	_d2dRenderer.BeginFrame(_d3d12Renderer.FrameIndex());
	_uiRenderer->Render2D(currentScene, localPlayerIndex);
	_d2dRenderer.EndFrame();

	const HRESULT presentResult = _d3d12Renderer.Present();
	if (g_clientTestMode.Enabled()) g_clientTestMode.OnPresent(presentResult);
	_d3d12Renderer.MoveToNextFrame();
}

void CGameFramework::FinalizeClientTest()
{
	if (!g_clientTestMode.Enabled()) return;

	std::uint32_t errorCount = 0;
	bool infoQueueAvailable = false;

#if defined(_DEBUG)
	ComPtr<ID3D12InfoQueue> infoQueue;
	auto* device = _d3d12Renderer.Device();
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
		_d3d12Renderer.DeviceRemovedReason(),
		_d3d12Renderer.CompletedFenceValue(),
		_d3d12Renderer.LastSubmittedFenceValue());
}

void CGameFramework::Render()
{
	const std::array<float, 4> clearColor = _raster
		? std::array<float, 4>{ 0.0f, 0.125f, 0.3f, 1.0f }
		: std::array<float, 4>{ 0.6f, 0.8f, 0.4f, 1.0f };
	auto* commandList = _d3d12Renderer.BeginFrame(clearColor);
	_gameCore.Render(commandList, true);
	_d3d12Renderer.SubmitFrame();
}

void CGameFramework::OnProcessingMouseMessage(
	HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	_gameCore.ProcessMouseMessage(hWnd, nMessageID, wParam, lParam);
}

void CGameFramework::OnProcessingKeyboardMessage(
	HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	if (nMessageID == WM_KEYUP && wParam == VK_ESCAPE) ::PostQuitMessage(0);
	// F9 remains disabled until D3D11On12 wrapped targets support resize.
	_gameCore.ProcessKeyboardMessage(hWnd, nMessageID, wParam, lParam);
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
	_d3d12Renderer.CheckRaytracingSupport();
}

void CGameFramework::OnKeyDown(const UINT8 key)
{
	if (key == VK_NUMPAD0) _raster = !_raster;
}

void CGameFramework::ChangeSwapChainState()
{
	_d3d12Renderer.ChangeSwapChainState();
}
