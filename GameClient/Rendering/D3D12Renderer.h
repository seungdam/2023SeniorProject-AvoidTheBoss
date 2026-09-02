#pragma once

#include <array>
#include <cstdint>
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

namespace atb
{
class D3D12Renderer final
{
public:
	static constexpr UINT BackBufferCount = 2;
	using BackBufferViews = std::array<ID3D12Resource*, BackBufferCount>;

	D3D12Renderer() = default;
	~D3D12Renderer() noexcept;

	D3D12Renderer(const D3D12Renderer&) = delete;
	D3D12Renderer& operator=(const D3D12Renderer&) = delete;
	D3D12Renderer(D3D12Renderer&&) = delete;
	D3D12Renderer& operator=(D3D12Renderer&&) = delete;

	void Initialize(HWND window);
	void Shutdown() noexcept;

	[[nodiscard]] ID3D12GraphicsCommandList4* BeginResourceUpload();
	void SubmitResourceUploadAndWait();

	void WaitForPreviousFrame();
	[[nodiscard]] ID3D12GraphicsCommandList4* BeginFrame(
		const std::array<float, 4>& clearColor);
	void SubmitFrame();
	[[nodiscard]] HRESULT Present();
	void MoveToNextFrame();
	void WaitForGpuComplete();

	void ChangeSwapChainState();
	void CheckRaytracingSupport() const;

	[[nodiscard]] bool IsInitialized() const noexcept;
	[[nodiscard]] ID3D12Device5* Device() const noexcept;
	[[nodiscard]] ID3D12CommandQueue* CommandQueue() const noexcept;
	[[nodiscard]] BackBufferViews BackBuffers() const noexcept;
	[[nodiscard]] UINT FrameIndex() const noexcept;
	[[nodiscard]] UINT Width() const noexcept;
	[[nodiscard]] UINT Height() const noexcept;
	[[nodiscard]] UINT64 LastSubmittedFenceValue() const noexcept;
	[[nodiscard]] UINT64 CompletedFenceValue() const noexcept;
	[[nodiscard]] HRESULT DeviceRemovedReason() const noexcept;

private:
	void CreateDevice();
	void CreateCommandQueueAndList();
	void CreateDescriptorHeaps();
	void CreateSwapChain();
	void CreateRenderTargetViews();
	void CreateDepthStencilView();
	void WaitForFenceValue(UINT64 fenceValue);

	HWND _window = nullptr;
	UINT _width = 0;
	UINT _height = 0;

	Microsoft::WRL::ComPtr<IDXGIFactory4> _factory;
	Microsoft::WRL::ComPtr<IDXGISwapChain3> _swapChain;
	Microsoft::WRL::ComPtr<ID3D12Device5> _device;

	UINT _frameIndex = 0;

	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, BackBufferCount> _backBuffers;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _rtvHeap;
	UINT _rtvDescriptorSize = 0;

	Microsoft::WRL::ComPtr<ID3D12Resource> _depthStencilBuffer;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _dsvHeap;
	UINT _dsvDescriptorSize = 0;

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> _commandQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> _commandAllocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> _commandList;

	Microsoft::WRL::ComPtr<ID3D12Fence> _fence;
	UINT64 _lastSubmittedFenceValue = 0;
	UINT64 _nextFenceValue = 1;
	HANDLE _fenceEvent = nullptr;
};
}
