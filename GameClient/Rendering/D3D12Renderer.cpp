#include "../Platform/pch.h"
#include "D3D12Renderer.h"

#include "../Platform/D3D12Helpers.h"
#include "../Platform/DXSampleHelper.h"

namespace atb
{
D3D12Renderer::~D3D12Renderer() noexcept
{
	Shutdown();
}

void D3D12Renderer::Initialize(HWND window)
{
	if (IsInitialized())
	{
		throw std::logic_error("D3D12Renderer is already initialized");
	}
	if (!window)
	{
		throw std::invalid_argument("D3D12Renderer requires a valid HWND");
	}

	try
	{
		_window = window;
		RECT clientRect{};
		if (!::GetClientRect(_window, &clientRect))
		{
			ThrowIfFailed(HRESULT_FROM_WIN32(::GetLastError()));
		}
		_width = static_cast<UINT>(clientRect.right - clientRect.left);
		_height = static_cast<UINT>(clientRect.bottom - clientRect.top);

		CreateDevice();
		CreateCommandQueueAndList();
		CreateDescriptorHeaps();
		CreateSwapChain();
		CreateRenderTargetViews();
		CreateDepthStencilView();
	}
	catch (...)
	{
		Shutdown();
		throw;
	}
}

void D3D12Renderer::Shutdown() noexcept
{
	if (_commandQueue && _fence && _fenceEvent)
	{
		try
		{
			WaitForGpuComplete();
		}
		catch (...)
		{
			::OutputDebugStringA("[cleanup] D3D12Renderer GPU wait failed; continuing release\n");
		}
	}

	if (_swapChain)
	{
		_swapChain->SetFullscreenState(FALSE, nullptr);
	}

	_depthStencilBuffer.Reset();
	_dsvHeap.Reset();
	for (auto &backBuffer : _backBuffers)
	{
		backBuffer.Reset();
	}
	_rtvHeap.Reset();

	_commandList.Reset();
	_commandAllocator.Reset();
	_swapChain.Reset();
	_commandQueue.Reset();

	if (_fenceEvent)
	{
		::CloseHandle(_fenceEvent);
		_fenceEvent = nullptr;
	}
	_fence.Reset();
	_device.Reset();
	_factory.Reset();

	_window = nullptr;
	_width = 0;
	_height = 0;
	_frameIndex = 0;
	_rtvDescriptorSize = 0;
	_dsvDescriptorSize = 0;
	_lastSubmittedFenceValue = 0;
	_nextFenceValue = 1;
}

void D3D12Renderer::CreateDevice()
{
	UINT factoryFlags = 0;
#if defined(_DEBUG)
	Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
	if (SUCCEEDED(::D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf()))))
	{
		debugController->EnableDebugLayer();
		factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
	}
#endif

	ThrowIfFailed(::CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(_factory.ReleaseAndGetAddressOf())));

	Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
	for (UINT index = 0;; ++index)
	{
		const HRESULT enumResult = _factory->EnumAdapters1(index, adapter.ReleaseAndGetAddressOf());
		if (enumResult == DXGI_ERROR_NOT_FOUND)
		{
			break;
		}
		ThrowIfFailed(enumResult);

		DXGI_ADAPTER_DESC1 description{};
		ThrowIfFailed(adapter->GetDesc1(&description));
		if (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			continue;
		}

		if (SUCCEEDED(::D3D12CreateDevice(
			adapter.Get(), D3D_FEATURE_LEVEL_12_0,
			IID_PPV_ARGS(_device.ReleaseAndGetAddressOf()))))
		{
			break;
		}
	}

	if (!_device)
	{
		ThrowIfFailed(_factory->EnumWarpAdapter(IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf())));
		ThrowIfFailed(::D3D12CreateDevice(
			adapter.Get(), D3D_FEATURE_LEVEL_12_0,
			IID_PPV_ARGS(_device.ReleaseAndGetAddressOf())));
	}

#if defined(_DEBUG)
	SetName(_device.Get(), L"GameClient D3D12 Device");
	Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
	if (SUCCEEDED(_device.As(&infoQueue)))
	{
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);

		D3D12_MESSAGE_ID deniedMessageIds[] = {
			D3D12_MESSAGE_ID_CREATERESOURCE_STATE_IGNORED
		};
		D3D12_MESSAGE_SEVERITY deniedSeverities[] = {
			D3D12_MESSAGE_SEVERITY_INFO,
			D3D12_MESSAGE_SEVERITY_MESSAGE
		};
		D3D12_INFO_QUEUE_FILTER filter{};
		filter.DenyList.NumIDs = _countof(deniedMessageIds);
		filter.DenyList.pIDList = deniedMessageIds;
		filter.DenyList.NumSeverities = _countof(deniedSeverities);
		filter.DenyList.pSeverityList = deniedSeverities;
		ThrowIfFailed(infoQueue->PushStorageFilter(&filter));
	}
#endif

	ThrowIfFailed(_device->CreateFence(
		0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(_fence.ReleaseAndGetAddressOf())));
	_fenceEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (!_fenceEvent)
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(::GetLastError()));
	}

#if defined(_DEBUG)
	SetName(_fence.Get(), L"GameClient Frame Fence");
#endif

	::gnCbvSrvDescriptorIncrementSize =
		_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	::gnRtvDescriptorIncrementSize =
		_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	::gnDsvDescriptorIncrementSize =
		_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

void D3D12Renderer::CreateCommandQueueAndList()
{
	D3D12_COMMAND_QUEUE_DESC queueDescription{};
	queueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDescription.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(_device->CreateCommandQueue(
		&queueDescription, IID_PPV_ARGS(_commandQueue.ReleaseAndGetAddressOf())));
	ThrowIfFailed(_device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(_commandAllocator.ReleaseAndGetAddressOf())));
	ThrowIfFailed(_device->CreateCommandList(
		0, D3D12_COMMAND_LIST_TYPE_DIRECT, _commandAllocator.Get(), nullptr,
		IID_PPV_ARGS(_commandList.ReleaseAndGetAddressOf())));
	ThrowIfFailed(_commandList->Close());

#if defined(_DEBUG)
	SetName(_commandQueue.Get(), L"GameClient Direct Command Queue");
	SetName(_commandAllocator.Get(), L"GameClient Direct Command Allocator");
	SetName(_commandList.Get(), L"GameClient Direct Command List");
#endif
}

void D3D12Renderer::CreateDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC description{};
	description.NumDescriptors = BackBufferCount;
	description.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	description.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(_device->CreateDescriptorHeap(
		&description, IID_PPV_ARGS(_rtvHeap.ReleaseAndGetAddressOf())));
	_rtvDescriptorSize =
		_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	description.NumDescriptors = 1;
	description.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	ThrowIfFailed(_device->CreateDescriptorHeap(
		&description, IID_PPV_ARGS(_dsvHeap.ReleaseAndGetAddressOf())));
	_dsvDescriptorSize =
		_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

void D3D12Renderer::CreateSwapChain()
{
#ifdef _WITH_CREATE_SWAPCHAIN_FOR_HWND
	DXGI_SWAP_CHAIN_DESC1 description{};
	description.Width = _width;
	description.Height = _height;
	description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	description.SampleDesc.Count = 1;
	description.SampleDesc.Quality = 0;
	description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	description.BufferCount = BackBufferCount;
	description.Scaling = DXGI_SCALING_NONE;
	description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	description.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
#ifdef _WITH_ONLY_RESIZE_BACKBUFFERS
	description.Flags = 0;
#else
	description.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
#endif

	DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreenDescription{};
	fullscreenDescription.RefreshRate.Numerator = 60;
	fullscreenDescription.RefreshRate.Denominator = 1;
	fullscreenDescription.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	fullscreenDescription.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	fullscreenDescription.Windowed = TRUE;

	Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
	ThrowIfFailed(_factory->CreateSwapChainForHwnd(
		_commandQueue.Get(), _window, &description, &fullscreenDescription, nullptr,
		swapChain.ReleaseAndGetAddressOf()));
	ThrowIfFailed(swapChain.As(&_swapChain));
#else
	DXGI_SWAP_CHAIN_DESC description{};
	description.BufferCount = BackBufferCount;
	description.BufferDesc.Width = _width;
	description.BufferDesc.Height = _height;
	description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	description.BufferDesc.RefreshRate.Numerator = 60;
	description.BufferDesc.RefreshRate.Denominator = 1;
	description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	description.OutputWindow = _window;
	description.SampleDesc.Count = 1;
	description.SampleDesc.Quality = 0;
	description.Windowed = TRUE;
	description.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
	ThrowIfFailed(_factory->CreateSwapChain(
		_commandQueue.Get(), &description, swapChain.ReleaseAndGetAddressOf()));
	ThrowIfFailed(swapChain.As(&_swapChain));
#endif

	_frameIndex = _swapChain->GetCurrentBackBufferIndex();
	ThrowIfFailed(_factory->MakeWindowAssociation(_window, DXGI_MWA_NO_ALT_ENTER));
}

void D3D12Renderer::CreateRenderTargetViews()
{
	D3D12_CPU_DESCRIPTOR_HANDLE handle = _rtvHeap->GetCPUDescriptorHandleForHeapStart();
	for (UINT index = 0; index < BackBufferCount; ++index)
	{
		_backBuffers[index].Reset();
		ThrowIfFailed(_swapChain->GetBuffer(
			index, IID_PPV_ARGS(_backBuffers[index].ReleaseAndGetAddressOf())));
		_device->CreateRenderTargetView(_backBuffers[index].Get(), nullptr, handle);
#if defined(_DEBUG)
		SetNameIndexed(_backBuffers[index].Get(), L"GameClient Back Buffer", index);
#endif
		handle.ptr += _rtvDescriptorSize;
	}
}

void D3D12Renderer::CreateDepthStencilView()
{
	D3D12_RESOURCE_DESC resourceDescription{};
	resourceDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDescription.Width = _width;
	resourceDescription.Height = _height;
	resourceDescription.DepthOrArraySize = 1;
	resourceDescription.MipLevels = 1;
	resourceDescription.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	resourceDescription.SampleDesc.Count = 1;
	resourceDescription.SampleDesc.Quality = 0;
	resourceDescription.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDescription.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	D3D12_CLEAR_VALUE clearValue{};
	clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	clearValue.DepthStencil.Depth = 1.0f;

	ThrowIfFailed(_device->CreateCommittedResource(
		&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDescription,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue,
		IID_PPV_ARGS(_depthStencilBuffer.ReleaseAndGetAddressOf())));
#if defined(_DEBUG)
	SetName(_depthStencilBuffer.Get(), L"GameClient Depth Stencil Buffer");
#endif

	D3D12_DEPTH_STENCIL_VIEW_DESC viewDescription{};
	viewDescription.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	viewDescription.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	viewDescription.Flags = D3D12_DSV_FLAG_NONE;
	_device->CreateDepthStencilView(
		_depthStencilBuffer.Get(), &viewDescription,
		_dsvHeap->GetCPUDescriptorHandleForHeapStart());
}

ID3D12GraphicsCommandList4* D3D12Renderer::BeginResourceUpload()
{
	ThrowIfFailed(_commandAllocator->Reset());
	ThrowIfFailed(_commandList->Reset(_commandAllocator.Get(), nullptr));
	return _commandList.Get();
}

void D3D12Renderer::SubmitResourceUploadAndWait()
{
	ThrowIfFailed(_commandList->Close());
	ID3D12CommandList* commandLists[] = { _commandList.Get() };
	_commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
	WaitForGpuComplete();
}

void D3D12Renderer::WaitForPreviousFrame()
{
	WaitForFenceValue(_lastSubmittedFenceValue);
}

ID3D12GraphicsCommandList4* D3D12Renderer::BeginFrame(
	const std::array<float, 4>& clearColor)
{
	ThrowIfFailed(_commandAllocator->Reset());
	ThrowIfFailed(_commandList->Reset(_commandAllocator.Get(), nullptr));

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = _backBuffers[_frameIndex].Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	_commandList->ResourceBarrier(1, &barrier);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = _rtvHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHandle.ptr += static_cast<SIZE_T>(_frameIndex) * _rtvDescriptorSize;
	const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = _dsvHeap->GetCPUDescriptorHandleForHeapStart();

	_commandList->ClearRenderTargetView(rtvHandle, clearColor.data(), 0, nullptr);
	_commandList->ClearDepthStencilView(
		dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
		1.0f, 0, 0, nullptr);
	_commandList->OMSetRenderTargets(1, &rtvHandle, TRUE, &dsvHandle);
	return _commandList.Get();
}

void D3D12Renderer::SubmitFrame()
{
	// D3D11On12 UI inherits RENDER_TARGET and transitions to PRESENT on release.
	ThrowIfFailed(_commandList->Close());
	ID3D12CommandList* commandLists[] = { _commandList.Get() };
	_commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
}

HRESULT D3D12Renderer::Present()
{
#ifdef _WITH_PRESENT_PARAMETERS
	DXGI_PRESENT_PARAMETERS parameters{};
	return _swapChain->Present1(1, 0, &parameters);
#else
#ifdef _WITH_SYNCH_SWAPCHAIN
	return _swapChain->Present(1, 0);
#else
	return _swapChain->Present(0, 0);
#endif
#endif
}

void D3D12Renderer::MoveToNextFrame()
{
	const UINT64 fenceValue = _nextFenceValue++;
	ThrowIfFailed(_commandQueue->Signal(_fence.Get(), fenceValue));
	_lastSubmittedFenceValue = fenceValue;
	_frameIndex = _swapChain->GetCurrentBackBufferIndex();
}

void D3D12Renderer::WaitForGpuComplete()
{
	const UINT64 fenceValue = _nextFenceValue++;
	ThrowIfFailed(_commandQueue->Signal(_fence.Get(), fenceValue));
	WaitForFenceValue(fenceValue);
}

void D3D12Renderer::WaitForFenceValue(UINT64 fenceValue)
{
	if (_fence->GetCompletedValue() >= fenceValue)
	{
		return;
	}
	ThrowIfFailed(_fence->SetEventOnCompletion(fenceValue, _fenceEvent));
	if (::WaitForSingleObject(_fenceEvent, INFINITE) == WAIT_FAILED)
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(::GetLastError()));
	}
}

void D3D12Renderer::ChangeSwapChainState()
{
	WaitForGpuComplete();

	BOOL fullscreen = FALSE;
	ThrowIfFailed(_swapChain->GetFullscreenState(&fullscreen, nullptr));
	ThrowIfFailed(_swapChain->SetFullscreenState(fullscreen ? FALSE : TRUE, nullptr));

	DXGI_MODE_DESC target{};
	target.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	target.Width = _width;
	target.Height = _height;
	target.RefreshRate.Numerator = 60;
	target.RefreshRate.Denominator = 1;
	target.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	target.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	ThrowIfFailed(_swapChain->ResizeTarget(&target));

	for (auto &backBuffer : _backBuffers)
	{
		backBuffer.Reset();
	}
	DXGI_SWAP_CHAIN_DESC description{};
	ThrowIfFailed(_swapChain->GetDesc(&description));
#ifdef _WITH_ONLY_RESIZE_BACKBUFFERS
	ThrowIfFailed(_swapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0));
#else
	ThrowIfFailed(_swapChain->ResizeBuffers(
		BackBufferCount, _width, _height, description.BufferDesc.Format, description.Flags));
#endif
	_frameIndex = _swapChain->GetCurrentBackBufferIndex();
	CreateRenderTargetViews();
}

void D3D12Renderer::CheckRaytracingSupport() const
{
	D3D12_FEATURE_DATA_D3D12_OPTIONS5 options{};
	ThrowIfFailed(_device->CheckFeatureSupport(
		D3D12_FEATURE_D3D12_OPTIONS5, &options, sizeof(options)));
	if (options.RaytracingTier < D3D12_RAYTRACING_TIER_1_0)
	{
		throw std::runtime_error("Raytracing not supported on device");
	}
}

bool D3D12Renderer::IsInitialized() const noexcept
{
	return _device != nullptr;
}

ID3D12Device5* D3D12Renderer::Device() const noexcept
{
	return _device.Get();
}

ID3D12CommandQueue* D3D12Renderer::CommandQueue() const noexcept
{
	return _commandQueue.Get();
}

D3D12Renderer::BackBufferViews D3D12Renderer::BackBuffers() const noexcept
{
	BackBufferViews views{};
	for (UINT index = 0; index < BackBufferCount; ++index)
	{
		views[index] = _backBuffers[index].Get();
	}
	return views;
}

UINT D3D12Renderer::FrameIndex() const noexcept
{
	return _frameIndex;
}

UINT D3D12Renderer::Width() const noexcept
{
	return _width;
}

UINT D3D12Renderer::Height() const noexcept
{
	return _height;
}

UINT64 D3D12Renderer::LastSubmittedFenceValue() const noexcept
{
	return _lastSubmittedFenceValue;
}

UINT64 D3D12Renderer::CompletedFenceValue() const noexcept
{
	return _fence ? _fence->GetCompletedValue() : 0;
}

HRESULT D3D12Renderer::DeviceRemovedReason() const noexcept
{
	return _device ? _device->GetDeviceRemovedReason() : E_POINTER;
}
}
