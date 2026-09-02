#pragma once

#include <d2d1_3.h>
#include <d3d11on12.h>
#include <d3d12.h>
#include <dwrite.h>
#include <span>
#include <vector>
#include <wrl/client.h>

namespace atb
{
class D2DRenderer final
{
public:
	D2DRenderer() = default;
	~D2DRenderer() noexcept;

	D2DRenderer(const D2DRenderer&) = delete;
	D2DRenderer& operator=(const D2DRenderer&) = delete;
	D2DRenderer(D2DRenderer&&) = delete;
	D2DRenderer& operator=(D2DRenderer&&) = delete;

	void Initialize(
		ID3D12Device* device,
		ID3D12CommandQueue* commandQueue,
		std::span<ID3D12Resource* const> renderTargets);
	void Shutdown() noexcept;

	void BeginFrame(UINT frameIndex);
	void EndFrame();

	[[nodiscard]] bool IsInitialized() const noexcept;
	[[nodiscard]] ID2D1DeviceContext2* Context() const noexcept;
	[[nodiscard]] IDWriteFactory* WriteFactory() const noexcept;

private:
	Microsoft::WRL::ComPtr<ID3D11Device> _d3d11Device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> _d3d11Context;
	Microsoft::WRL::ComPtr<ID3D11On12Device> _d3d11On12Device;
	Microsoft::WRL::ComPtr<ID2D1Factory3> _d2dFactory;
	Microsoft::WRL::ComPtr<ID2D1Device2> _d2dDevice;
	Microsoft::WRL::ComPtr<ID2D1DeviceContext2> _d2dContext;
	Microsoft::WRL::ComPtr<IDWriteFactory> _writeFactory;
	std::vector<Microsoft::WRL::ComPtr<ID3D11Resource>> _wrappedRenderTargets;
	std::vector<Microsoft::WRL::ComPtr<ID2D1Bitmap1>> _d2dRenderTargets;
	UINT _activeFrameIndex = 0;
	bool _frameActive = false;
};
}
