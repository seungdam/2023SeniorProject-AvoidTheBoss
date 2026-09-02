#include "../Platform/pch.h"
#include "D2DRenderer.h"

#include "../Platform/DXSampleHelper.h"

namespace atb
{
D2DRenderer::~D2DRenderer() noexcept
{
	Shutdown();
}

void D2DRenderer::Initialize(
	ID3D12Device* device,
	ID3D12CommandQueue* commandQueue,
	const std::span<ID3D12Resource* const> renderTargets)
{
	if (IsInitialized())
	{
		throw std::logic_error("D2DRenderer is already initialized");
	}
	if (!device || !commandQueue || renderTargets.empty())
	{
		throw std::invalid_argument("D2DRenderer requires a device, queue, and render targets");
	}

	try
	{
		constexpr UINT deviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
		IUnknown* queues[] = { commandQueue };
		ThrowIfFailed(::D3D11On12CreateDevice(
			device,
			deviceFlags,
			nullptr,
			0,
			queues,
			_countof(queues),
			0,
			_d3d11Device.ReleaseAndGetAddressOf(),
			_d3d11Context.ReleaseAndGetAddressOf(),
			nullptr));
		ThrowIfFailed(_d3d11Device.As(&_d3d11On12Device));

		D2D1_FACTORY_OPTIONS factoryOptions{};
		ThrowIfFailed(::D2D1CreateFactory(
			D2D1_FACTORY_TYPE_SINGLE_THREADED,
			__uuidof(ID2D1Factory3),
			&factoryOptions,
			reinterpret_cast<void**>(_d2dFactory.ReleaseAndGetAddressOf())));

		Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
		ThrowIfFailed(_d3d11Device.As(&dxgiDevice));
		ThrowIfFailed(_d2dFactory->CreateDevice(dxgiDevice.Get(), _d2dDevice.ReleaseAndGetAddressOf()));
		ThrowIfFailed(_d2dDevice->CreateDeviceContext(
			D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
			_d2dContext.ReleaseAndGetAddressOf()));
		_d2dContext->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

		ThrowIfFailed(::DWriteCreateFactory(
			DWRITE_FACTORY_TYPE_SHARED,
			__uuidof(IDWriteFactory),
			reinterpret_cast<IUnknown**>(_writeFactory.ReleaseAndGetAddressOf())));

		_wrappedRenderTargets.resize(renderTargets.size());
		_d2dRenderTargets.resize(renderTargets.size());
		const auto bitmapProperties = D2D1::BitmapProperties1(
			D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
			D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED));

		for (std::size_t index = 0; index < renderTargets.size(); ++index)
		{
			if (!renderTargets[index])
			{
				throw std::invalid_argument("D2DRenderer received a null render target");
			}

			D3D11_RESOURCE_FLAGS flags{ D3D11_BIND_RENDER_TARGET };
			ThrowIfFailed(_d3d11On12Device->CreateWrappedResource(
				renderTargets[index],
				&flags,
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PRESENT,
				IID_PPV_ARGS(_wrappedRenderTargets[index].ReleaseAndGetAddressOf())));

			Microsoft::WRL::ComPtr<IDXGISurface> surface;
			ThrowIfFailed(_wrappedRenderTargets[index].As(&surface));
			ThrowIfFailed(_d2dContext->CreateBitmapFromDxgiSurface(
				surface.Get(),
				&bitmapProperties,
				_d2dRenderTargets[index].ReleaseAndGetAddressOf()));
		}
	}
	catch (...)
	{
		Shutdown();
		throw;
	}
}

void D2DRenderer::Shutdown() noexcept
{
	if (_frameActive)
	{
		_d2dContext->EndDraw();
		ID3D11Resource* resources[] = { _wrappedRenderTargets[_activeFrameIndex].Get() };
		_d3d11On12Device->ReleaseWrappedResources(resources, _countof(resources));
		_d3d11Context->Flush();
		_frameActive = false;
	}

	if (_d2dContext)
	{
		_d2dContext->SetTarget(nullptr);
	}
	if (_d3d11Context)
	{
		_d3d11Context->Flush();
	}
	_d2dRenderTargets.clear();
	_wrappedRenderTargets.clear();
	_writeFactory.Reset();
	_d2dContext.Reset();
	_d2dDevice.Reset();
	_d2dFactory.Reset();
	_d3d11On12Device.Reset();
	_d3d11Context.Reset();
	_d3d11Device.Reset();
}

void D2DRenderer::BeginFrame(const UINT frameIndex)
{
	if (!IsInitialized())
	{
		throw std::logic_error("D2DRenderer is not initialized");
	}
	if (_frameActive)
	{
		throw std::logic_error("D2DRenderer frame is already active");
	}
	if (frameIndex >= _wrappedRenderTargets.size())
	{
		throw std::out_of_range("D2DRenderer frame index is out of range");
	}

	_activeFrameIndex = frameIndex;
	ID3D11Resource* resources[] = { _wrappedRenderTargets[frameIndex].Get() };
	_d2dContext->SetTarget(_d2dRenderTargets[frameIndex].Get());
	_d3d11On12Device->AcquireWrappedResources(resources, _countof(resources));
	_d2dContext->BeginDraw();
	_frameActive = true;
}

void D2DRenderer::EndFrame()
{
	if (!_frameActive)
	{
		throw std::logic_error("D2DRenderer frame is not active");
	}

	const HRESULT drawResult = _d2dContext->EndDraw();
	ID3D11Resource* resources[] = { _wrappedRenderTargets[_activeFrameIndex].Get() };
	_d3d11On12Device->ReleaseWrappedResources(resources, _countof(resources));
	_d3d11Context->Flush();
	_frameActive = false;
	ThrowIfFailed(drawResult);
}

bool D2DRenderer::IsInitialized() const noexcept
{
	return _d2dContext && !_wrappedRenderTargets.empty();
}

ID2D1DeviceContext2* D2DRenderer::Context() const noexcept
{
	return _d2dContext.Get();
}

IDWriteFactory* D2DRenderer::WriteFactory() const noexcept
{
	return _writeFactory.Get();
}
}
