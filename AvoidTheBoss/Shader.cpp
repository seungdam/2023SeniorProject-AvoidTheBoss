//-----------------------------------------------------------------------------
// File: Shader.cpp
//-----------------------------------------------------------------------------

#include "pch.h"
#include "Shader.h"
#include "CBullet.h"
#include "CGenerator.h"
#include "DXSampleHelper.h"
#include <stdexcept>

CShader::CShader()
{
}

CShader::~CShader()
{
	ReleaseShaderVariables();

	if (_pd3dPipelineState)
	{
		_pd3dPipelineState->Release();
		_pd3dPipelineState = nullptr;
	};
}

D3D12_SHADER_BYTECODE CShader::CreateVertexShader()
{
	D3D12_SHADER_BYTECODE d3dShaderByteCode;
	d3dShaderByteCode.BytecodeLength = 0;
	d3dShaderByteCode.pShaderBytecode = NULL;

	return(d3dShaderByteCode);
}

D3D12_SHADER_BYTECODE CShader::CreatePixelShader()
{
	D3D12_SHADER_BYTECODE d3dShaderByteCode;
	d3dShaderByteCode.BytecodeLength = 0;
	d3dShaderByteCode.pShaderBytecode = NULL;

	return(d3dShaderByteCode);
}

D3D12_SHADER_BYTECODE CShader::CompileShaderFromFile(const WCHAR *pszFileName, LPCSTR pszShaderName, LPCSTR pszShaderProfile, ID3DBlob **ppd3dShaderBlob)
{
	UINT nCompileFlags = 0;
#if defined(_DEBUG)
	nCompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	ComPtr<ID3DBlob> pd3dErrorBlob;
	const auto shaderPath = GetAssetPath(pszFileName);
	HRESULT hResult = ::D3DCompileFromFile(shaderPath.c_str(), NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, pszShaderName, pszShaderProfile, nCompileFlags, 0, ppd3dShaderBlob, &pd3dErrorBlob);
	if (pd3dErrorBlob)
	{
		::OutputDebugStringA(static_cast<char *>(pd3dErrorBlob->GetBufferPointer()));
	}

	D3D12_SHADER_BYTECODE d3dShaderByteCode = {};
	if (FAILED(hResult) || !*ppd3dShaderBlob)
	{
		std::wstring fileMessage = L"[Shader] Compile failed: ";
		fileMessage += shaderPath.native();
		fileMessage += L"\n";
		::OutputDebugStringW(fileMessage.c_str());

		char message[512]{};
		const HRESULT failure = FAILED(hResult) ? hResult : E_FAIL;
		sprintf_s(message, "[Shader] entry=%s, profile=%s, HRESULT=0x%08lX",
			pszShaderName, pszShaderProfile, static_cast<unsigned long>(failure));
		::OutputDebugStringA(message);
		::OutputDebugStringA("\n");
		throw std::runtime_error(message);
	}
	d3dShaderByteCode.BytecodeLength = (*ppd3dShaderBlob)->GetBufferSize();
	d3dShaderByteCode.pShaderBytecode = (*ppd3dShaderBlob)->GetBufferPointer();

	return(d3dShaderByteCode);
}

D3D12_INPUT_LAYOUT_DESC CShader::CreateInputLayout()
{
	D3D12_INPUT_LAYOUT_DESC d3dInputLayoutDesc;
	d3dInputLayoutDesc.pInputElementDescs = NULL;
	d3dInputLayoutDesc.NumElements = 0;

	return(d3dInputLayoutDesc);
}

D3D12_RASTERIZER_DESC CShader::CreateRasterizerState()
{
	D3D12_RASTERIZER_DESC d3dRasterizerDesc;
	::ZeroMemory(&d3dRasterizerDesc, sizeof(D3D12_RASTERIZER_DESC));
	//	d3dRasterizerDesc.FillMode = D3D12_FILL_MODE_WIREFRAME;
	d3dRasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	d3dRasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	d3dRasterizerDesc.FrontCounterClockwise = FALSE;
	d3dRasterizerDesc.DepthBias = 0;
	d3dRasterizerDesc.DepthBiasClamp = 0.0f;
	d3dRasterizerDesc.SlopeScaledDepthBias = 0.0f;
	d3dRasterizerDesc.DepthClipEnable = TRUE;
	d3dRasterizerDesc.MultisampleEnable = FALSE;
	d3dRasterizerDesc.AntialiasedLineEnable = FALSE;
	d3dRasterizerDesc.ForcedSampleCount = 0;
	d3dRasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	return(d3dRasterizerDesc);
}

D3D12_DEPTH_STENCIL_DESC CShader::CreateDepthStencilState()
{
	D3D12_DEPTH_STENCIL_DESC d3dDepthStencilDesc;
	::ZeroMemory(&d3dDepthStencilDesc, sizeof(D3D12_DEPTH_STENCIL_DESC));
	d3dDepthStencilDesc.DepthEnable = TRUE;
	d3dDepthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	d3dDepthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	d3dDepthStencilDesc.StencilEnable = FALSE;
	d3dDepthStencilDesc.StencilReadMask = 0x00;
	d3dDepthStencilDesc.StencilWriteMask = 0x00;
	d3dDepthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	d3dDepthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	d3dDepthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	d3dDepthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	d3dDepthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	d3dDepthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	d3dDepthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	d3dDepthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;

	return(d3dDepthStencilDesc);
}

D3D12_BLEND_DESC CShader::CreateBlendState()
{
	D3D12_BLEND_DESC d3dBlendDesc;
	::ZeroMemory(&d3dBlendDesc, sizeof(D3D12_BLEND_DESC));
	d3dBlendDesc.AlphaToCoverageEnable = FALSE;
	d3dBlendDesc.IndependentBlendEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].BlendEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].LogicOpEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	d3dBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	return(d3dBlendDesc);
}

void CShader::CreateShader(ID3D12Device5* pd3dDevice, ID3D12GraphicsCommandList4* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature)
{
	auto releaseCreationResources = [this]()
	{
		if (_pd3dVertexShaderBlob)
		{
			_pd3dVertexShaderBlob->Release();
			_pd3dVertexShaderBlob = nullptr;
		}

		if (_pd3dPixelShaderBlob)
		{
			_pd3dPixelShaderBlob->Release();
			_pd3dPixelShaderBlob = nullptr;
		}

		delete[] _d3dPipelineStateDesc.InputLayout.pInputElementDescs;
		_d3dPipelineStateDesc.InputLayout = {};
	};

	HRESULT hResult = E_FAIL;
	try
	{
		::ZeroMemory(&_d3dPipelineStateDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
		_d3dPipelineStateDesc.pRootSignature = pd3dGraphicsRootSignature;
		_d3dPipelineStateDesc.VS = CreateVertexShader();
		_d3dPipelineStateDesc.PS = CreatePixelShader();
		_d3dPipelineStateDesc.RasterizerState = CreateRasterizerState();
		_d3dPipelineStateDesc.BlendState = CreateBlendState();
		_d3dPipelineStateDesc.DepthStencilState = CreateDepthStencilState();
		_d3dPipelineStateDesc.InputLayout = CreateInputLayout();
		_d3dPipelineStateDesc.SampleMask = UINT_MAX;
		_d3dPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		_d3dPipelineStateDesc.NumRenderTargets = 1;
		_d3dPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		_d3dPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		_d3dPipelineStateDesc.SampleDesc.Count = 1;
		_d3dPipelineStateDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

		hResult = pd3dDevice->CreateGraphicsPipelineState(&_d3dPipelineStateDesc, __uuidof(ID3D12PipelineState), (void**)&_pd3dPipelineState);
	}
	catch (...)
	{
		releaseCreationResources();
		throw;
	}

	releaseCreationResources();
	ThrowIfFailed(hResult);
}

void CShader::OnPrepareRender(ID3D12GraphicsCommandList4   *pd3dCommandList, int nPipelineState)
{
	if (_pd3dPipelineState)
	{
		pd3dCommandList->SetPipelineState(_pd3dPipelineState);
	}
}

void CShader::Render(ID3D12GraphicsCommandList4   *pd3dCommandList, CCamera *pCamera, bool bRaster)
{
	OnPrepareRender(pd3dCommandList);
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
CSkyBoxShader::CSkyBoxShader()
{
}

CSkyBoxShader::~CSkyBoxShader()
{
}

D3D12_INPUT_LAYOUT_DESC CSkyBoxShader::CreateInputLayout()
{
	UINT nInputElementDescs = 1;
	D3D12_INPUT_ELEMENT_DESC *pd3dInputElementDescs = new D3D12_INPUT_ELEMENT_DESC[nInputElementDescs];

	pd3dInputElementDescs[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

	D3D12_INPUT_LAYOUT_DESC d3dInputLayoutDesc;
	d3dInputLayoutDesc.pInputElementDescs = pd3dInputElementDescs;
	d3dInputLayoutDesc.NumElements = nInputElementDescs;

	return(d3dInputLayoutDesc);
}

D3D12_DEPTH_STENCIL_DESC CSkyBoxShader::CreateDepthStencilState()
{
	D3D12_DEPTH_STENCIL_DESC d3dDepthStencilDesc;
	d3dDepthStencilDesc.DepthEnable = FALSE;
	d3dDepthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	d3dDepthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_NEVER;
	d3dDepthStencilDesc.StencilEnable = FALSE;
	d3dDepthStencilDesc.StencilReadMask = 0xff;
	d3dDepthStencilDesc.StencilWriteMask = 0xff;
	d3dDepthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	d3dDepthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_INCR;
	d3dDepthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	d3dDepthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	d3dDepthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	d3dDepthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_DECR;
	d3dDepthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	d3dDepthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	return(d3dDepthStencilDesc);
}

D3D12_SHADER_BYTECODE CSkyBoxShader::CreateVertexShader()
{
	return(CShader::CompileShaderFromFile(L"Shaders.hlsl", "VSSkyBox", "vs_5_1", &_pd3dVertexShaderBlob));
}

D3D12_SHADER_BYTECODE CSkyBoxShader::CreatePixelShader()
{
	return(CShader::CompileShaderFromFile(L"Shaders.hlsl", "PSSkyBox", "ps_5_1", &_pd3dPixelShaderBlob));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
CStandardShader::CStandardShader()
{
}

CStandardShader::~CStandardShader()
{
}

D3D12_INPUT_LAYOUT_DESC CStandardShader::CreateInputLayout()
{
	UINT nInputElementDescs = 5;
	D3D12_INPUT_ELEMENT_DESC *pd3dInputElementDescs = new D3D12_INPUT_ELEMENT_DESC[nInputElementDescs];

	pd3dInputElementDescs[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	pd3dInputElementDescs[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	pd3dInputElementDescs[2] = { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 2, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	pd3dInputElementDescs[3] = { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 3, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	pd3dInputElementDescs[4] = { "BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 4, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

	D3D12_INPUT_LAYOUT_DESC d3dInputLayoutDesc;
	d3dInputLayoutDesc.pInputElementDescs = pd3dInputElementDescs;
	d3dInputLayoutDesc.NumElements = nInputElementDescs;

	return(d3dInputLayoutDesc);
}

D3D12_SHADER_BYTECODE CStandardShader::CreateVertexShader()
{
	return(CShader::CompileShaderFromFile(L"Shaders.hlsl", "VSStandard", "vs_5_1", &_pd3dVertexShaderBlob));
}

D3D12_SHADER_BYTECODE CStandardShader::CreatePixelShader()
{

	return(CShader::CompileShaderFromFile(L"Shaders.hlsl", "PSStandard", "ps_5_1", &_pd3dPixelShaderBlob));

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
CSkinnedAnimationStandardShader::CSkinnedAnimationStandardShader()
{
}

CSkinnedAnimationStandardShader::~CSkinnedAnimationStandardShader()
{
}

D3D12_INPUT_LAYOUT_DESC CSkinnedAnimationStandardShader::CreateInputLayout()
{
	UINT nInputElementDescs = 7;
	D3D12_INPUT_ELEMENT_DESC *pd3dInputElementDescs = new D3D12_INPUT_ELEMENT_DESC[nInputElementDescs];

	pd3dInputElementDescs[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	pd3dInputElementDescs[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	pd3dInputElementDescs[2] = { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 2, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	pd3dInputElementDescs[3] = { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 3, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	pd3dInputElementDescs[4] = { "BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 4, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	pd3dInputElementDescs[5] = { "BONEINDEX", 0, DXGI_FORMAT_R32G32B32A32_SINT, 5, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	pd3dInputElementDescs[6] = { "BONEWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 6, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

	D3D12_INPUT_LAYOUT_DESC d3dInputLayoutDesc;
	d3dInputLayoutDesc.pInputElementDescs = pd3dInputElementDescs;
	d3dInputLayoutDesc.NumElements = nInputElementDescs;

	return(d3dInputLayoutDesc);
}

D3D12_SHADER_BYTECODE CSkinnedAnimationStandardShader::CreateVertexShader()
{
	return(CShader::CompileShaderFromFile(L"Shaders.hlsl", "VSSkinnedAnimationStandard", "vs_5_1", &_pd3dVertexShaderBlob));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
CStandardObjectsShader::CStandardObjectsShader()
{
}

CStandardObjectsShader::~CStandardObjectsShader()
{
}

void CStandardObjectsShader::BuildObjects(ID3D12Device5 *pd3dDevice, ID3D12GraphicsCommandList4   *pd3dCommandList, ID3D12RootSignature *pd3dGraphicsRootSignature, CLoadedModelInfo *pModel, void *pContext)
{
}

void CStandardObjectsShader::ReleaseObjects()
{
	if (_ppObjects)
	{
		for (int j = 0; j < _objectCount; j++)
		{
			if (_ppObjects[j])
			{
				//const std::type_info& typeInfo = typeid(m_ppObjects[j]);
				std::cout << "CGameObejct Index: " << j << std::endl;
				_ppObjects[j]->Release();
				_ppObjects[j] = nullptr;
			}
		}
		delete[] _ppObjects;
		_ppObjects = nullptr;
		_objectCount = 0;
	}
}

void CStandardObjectsShader::AnimateObjects(float fTimeElapsed)
{
	_fElapsedTime = fTimeElapsed;
}

void CStandardObjectsShader::ReleaseUploadBuffers()
{
	for (int j = 0; j < _objectCount; j++)
	{
		if (_ppObjects[j])
		{
			_ppObjects[j]->ReleaseUploadBuffers();
		}
	}
}

void CStandardObjectsShader::Render(ID3D12GraphicsCommandList4*pd3dCommandList, CCamera *pCamera,bool bRaster)
{
	CStandardShader::Render(pd3dCommandList, pCamera,bRaster);

	for (int j = 0; j < _objectCount; j++)
	{
		if (_ppObjects[j])
		{
			_ppObjects[j]->Animate(_fElapsedTime);
			_ppObjects[j]->UpdateTransform(NULL);
			_ppObjects[j]->Render(pd3dCommandList, pCamera, bRaster);
		}
	}
}

void CStandardObjectsShader::ResetState()
{
	for (int j = 0; j < _objectCount; j++)
	{
		if (_ppObjects[j])
		{
			_ppObjects[j]->m_bEmpExit = false;
			_ppObjects[j]->ResetState();
		}
	}
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
CSkinnedAnimationObjectsShader::CSkinnedAnimationObjectsShader()
{
}

CSkinnedAnimationObjectsShader::~CSkinnedAnimationObjectsShader()
{
}

void CSkinnedAnimationObjectsShader::BuildObjects(ID3D12Device5 *pd3dDevice, ID3D12GraphicsCommandList4 *pd3dCommandList, ID3D12RootSignature *pd3dGraphicsRootSignature, CLoadedModelInfo *pModel, void *pContext)
{
}

void CSkinnedAnimationObjectsShader::ReleaseObjects()
{
	if (_ppObjects)
	{
		for (int j = 0; j < _objectCounts; j++)
		{
			if (_ppObjects[j])
			{
				_ppObjects[j]->Release();
			}
		}
		delete[] _ppObjects;
		_ppObjects = nullptr;
		_objectCounts = 0;
	}
}

void CSkinnedAnimationObjectsShader::AnimateObjects(float fTimeElapsed)
{
	_fElapsedTime = fTimeElapsed;
}

void CSkinnedAnimationObjectsShader::ReleaseUploadBuffers()
{
	for (int j = 0; j < _objectCounts; j++)
	{
		if (_ppObjects[j])
		{
			_ppObjects[j]->ReleaseUploadBuffers();
		}
	}
}

void CSkinnedAnimationObjectsShader::Render(ID3D12GraphicsCommandList4*pd3dCommandList, CCamera *pCamera, bool bRaster)
{
	CSkinnedAnimationStandardShader::Render(pd3dCommandList, pCamera, bRaster);

	for (int j = 0; j < _objectCounts; j++)
	{
		if (_ppObjects[j])
		{
			_ppObjects[j]->Animate(_fElapsedTime);
			_ppObjects[j]->Render(pd3dCommandList, pCamera, bRaster);
		}
	}
}



CMapObjectsShader::CMapObjectsShader()
{
}

CMapObjectsShader::~CMapObjectsShader()
{
}

void CMapObjectsShader::BuildObjects(ID3D12Device5 * pd3dDevice,ID3D12GraphicsCommandList4     * pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature, CLoadedModelInfo* pModel, void* pContext)
{
	_objectCount = 3;
	_ppObjects = new CGameObject * [_objectCount]{};

	CGameObject* pMap = CGameObject::LoadGeometryFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, "Model/Map/Industry_Map.bin", this,Layout::MAP);
	_ppObjects[0] = new CGameObject();
	_ppObjects[0]->SetChild(pMap ,true);
	_ppObjects[0]->SetPosition(XMFLOAT3(-7.774636f, -0.0f, -1.926502f));

	CGameObject* lever1 = _ppObjects[0]->CGameObject::FindFrame("Generator_Lever");
	CGameObject* lever2 = _ppObjects[0]->CGameObject::FindFrame("Generator_Lever002");
	XMMATRIX xmmtxTranslate = DirectX::XMMatrixTranslation(-2.0f, 0.0f, 0.1f);
	lever1->m_xmf4x4ToParent = Matrix4x4::Multiply(xmmtxTranslate, lever1->m_xmf4x4ToParent);

	XMMATRIX xmmtxTranslate2 = DirectX::XMMatrixTranslation(-1.5f, 0.0f, 0.0f);
	lever2->m_xmf4x4ToParent = Matrix4x4::Multiply(xmmtxTranslate2, lever2->m_xmf4x4ToParent);

	_ppObjects[0]->CGameObject::UpdateTransform(NULL);

	CGameObject* pTile = CGameObject::LoadGeometryFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, "Model/Map/Industry_Field2(1).bin", this, Layout::MAP);
	_ppObjects[1] = new CGameObject();
	_ppObjects[1]->SetChild(pTile ,true);
	_ppObjects[1]->SetPosition(XMFLOAT3(0.0f, 0.0f, 0.0f));

	CGameObject* pCrane = CGameObject::LoadGeometryFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, "Model/Map/Crane.bin", this, Layout::MAP);
	_ppObjects[2] = new CGameObject();
	_ppObjects[2]->SetChild(pCrane ,true);
	_ppObjects[2]->SetPosition(XMFLOAT3(0.0f, 0.0f, 0.0f));

	CreateShaderVariables(pd3dDevice, pd3dCommandList);

	/*if (pMap) delete pMap;
	if (pTile) delete pTile;
	if (pCrane) delete pCrane;*/

}

CBoundsObjectsShader::CBoundsObjectsShader()
{
}

CBoundsObjectsShader::~CBoundsObjectsShader()
{
}

void CBoundsObjectsShader::BuildObjects(ID3D12Device5* pd3dDevice,ID3D12GraphicsCommandList4     * pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature, CLoadedModelInfo* pModel, void* pContext)
{
	_objectCount = 1;
	_ppObjects = new CGameObject * [_objectCount]{};

	CGameObject* pMap = CGameObject::LoadGeometryFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, "Model/Map_Bounding_Box_(5).bin", this, Layout::BOUDS);
	pMap->_type = 1;
	_ppObjects[0] = new CGameObject();
	_ppObjects[0]->SetChild(pMap ,true);
	_ppObjects[0]->SetPosition(XMFLOAT3(0.0f, 0.0f, 0.0f));

	CreateShaderVariables(pd3dDevice, pd3dCommandList);
	//if (pMap) delete pMap;

}

CBulletObjectsShader::CBulletObjectsShader()
{
}

CBulletObjectsShader::~CBulletObjectsShader()
{
}

void CBulletObjectsShader::BuildObjects(ID3D12Device5* pd3dDevice,ID3D12GraphicsCommandList4* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature, CLoadedModelInfo* pModel, void* pContext)
{
	_objectCount = CBullet::PoolCapacity;
	_ppObjects = new CGameObject * [_objectCount]{};

	CGameObject* pBullet = CGameObject::LoadGeometryFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, "Model/총알/green_bullet.bin", this, Layout::BULLET);
	for (int i = 0; i < _objectCount; i++)
	{
		_ppObjects[i] = new CBullet();
		_ppObjects[i]->SetChild(pBullet , true);
		_ppObjects[i]->SetPosition(XMFLOAT3(0.0f, 1.0f, 0.0f));
	}
	CreateShaderVariables(pd3dDevice, pd3dCommandList);
	//if (pBullet) delete pBullet;
}

void CBulletObjectsShader::Render(ID3D12GraphicsCommandList4  * pd3dCommandList, CCamera* pCamera,bool bRaster)
{
	//CStandardShader::Render(pd3dCommandList, pCamera, bRaster);
	for (int j = 0; j < _objectCount; j++)
	{
		if (_ppObjects[j])
		{
			if (_ppObjects[j])
			{
				_ppObjects[j]->Animate(_fElapsedTime);
				_ppObjects[j]->UpdateTransform(NULL);
				if (((CBullet *)_ppObjects[j])->GetOnShoot())
				{
					_ppObjects[j]->Render(pd3dCommandList, pCamera, bRaster);
				}
			}
		}
	}
}

CDoorObjectsShader::CDoorObjectsShader()
{
}

CDoorObjectsShader::~CDoorObjectsShader()
{
}

void CDoorObjectsShader::BuildObjects(ID3D12Device5* pd3dDevice, ID3D12GraphicsCommandList4  * pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature, CLoadedModelInfo* pModel, void* pContext)
{
	_objectCount = 5;
	_ppObjects = new CGameObject * [_objectCount]{};

	CGameObject* pFrontDoor = CGameObject::LoadGeometryFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, "Model/Map/Front_Hanger_Door_Open.bin", this, Layout::DOOR);
	CGameObject* pEmergencyDoor = CGameObject::LoadGeometryFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, "Model/Map/Emergency_Door_Open.bin", this, Layout::DOOR);
	CGameObject* pShutterDoor = CGameObject::LoadGeometryFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, "Model/Map/Shutter_Door_Side.bin", this, Layout::DOOR);

	_ppObjects[0] = new CFrontDoor();
	_ppObjects[0]->SetChild(pFrontDoor ,true);
	_ppObjects[0]->SetPosition(XMFLOAT3(0.0f, 0.0f, 0.0f));
	_ppObjects[0]->OnPrepareAnimate();
	_ppObjects[0]->objLayer = Layout::DOOR;

	_ppObjects[1] = new CEmergencyDoor();
	_ppObjects[1]->SetChild(pEmergencyDoor , true);
	_ppObjects[1]->SetPosition(XMFLOAT3(-25.60735f, 0.01800204f, -22.68291f));
	_ppObjects[1]->Rotate(0.0f, 90.0f, 0.0f);
	_ppObjects[1]->OnPrepareAnimate();
	_ppObjects[1]->objLayer = Layout::DOOR;

	_ppObjects[2] = new CEmergencyDoor();
	_ppObjects[2]->SetChild(pEmergencyDoor , true);
	_ppObjects[2]->SetPosition(XMFLOAT3(25.60001f, 0.01550287f, -21.44026f));
	_ppObjects[2]->Rotate(0.0f, -90.0f, 0.0f);
	_ppObjects[2]->OnPrepareAnimate();
	_ppObjects[2]->objLayer = Layout::DOOR;

	_ppObjects[3] = new CShutterDoor();
	_ppObjects[3]->SetChild(pShutterDoor , true);
	_ppObjects[3]->SetPosition(XMFLOAT3(-0.044f, -0.5005361f, 0.06f));
	_ppObjects[3]->Rotate(-90.0f, 0.0f, 90.0f);
	_ppObjects[3]->OnPrepareAnimate();
	_ppObjects[3]->objLayer = Layout::DOOR;

	_ppObjects[4] = new CShutterDoor();
	_ppObjects[4]->SetChild(pShutterDoor , true);
	_ppObjects[4]->SetPosition(XMFLOAT3(50.43907f, -0.503039f, -0.1099938f));
	_ppObjects[4]->Rotate(-90.0f, 0.0f, 90.0f);
	_ppObjects[4]->OnPrepareAnimate();
	_ppObjects[4]->objLayer = Layout::DOOR;

	CreateShaderVariables(pd3dDevice, pd3dCommandList);
	/*if (pFrontDoor) delete pFrontDoor;
	if (pEmergencyDoor) delete pEmergencyDoor;
	if (pShutterDoor) delete pShutterDoor;*/

}

CSirenObjectsShader::CSirenObjectsShader()
{
}

CSirenObjectsShader::~CSirenObjectsShader()
{
}

void CSirenObjectsShader::BuildObjects(ID3D12Device5* pd3dDevice, ID3D12GraphicsCommandList4  * pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature, CLoadedModelInfo* pModel, void* pContext)
{
	_objectCount = 16;
	_ppObjects = new CGameObject * [_objectCount]{};

	CGameObject* pSiren = CGameObject::LoadGeometryFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, "Model/Map/Siren_Alarm_One.bin", this, Layout::SIREN);

	_ppObjects[0] = new CSiren();
	_ppObjects[0]->SetChild(pSiren , true);
	_ppObjects[0]->SetPosition(XMFLOAT3(23.60255f, 3.744244f, 19.36822f));
	_ppObjects[0]->Rotate(-0.0f, -0.0f, 90.0f);
	_ppObjects[0]->OnPrepareAnimate();
	_ppObjects[0]->objLayer = Layout::SIREN;

	_ppObjects[1] = new CSiren();
	_ppObjects[1]->SetChild(pSiren , true);
	_ppObjects[1]->SetPosition(XMFLOAT3(23.60255f, 3.744244f, 4.83103f));
	_ppObjects[1]->Rotate(-0.0f, -0.0f, 90.0f);
	_ppObjects[1]->OnPrepareAnimate();
	_ppObjects[1]->objLayer = Layout::SIREN;

	_ppObjects[2] = new CSiren();
	_ppObjects[2]->SetChild(pSiren , true);
	_ppObjects[2]->SetPosition(XMFLOAT3(23.60255f, 3.744244f,-0.008120117f));
	_ppObjects[2]->Rotate(-0.0f, -0.0f, 90.0f);
	_ppObjects[2]->OnPrepareAnimate();
	_ppObjects[2]->objLayer = Layout::SIREN;

	_ppObjects[3] = new CSiren();
	_ppObjects[3]->SetChild(pSiren , true);
	_ppObjects[3]->SetPosition(XMFLOAT3(23.60255f, 3.744244f, -14.57508f));
	_ppObjects[3]->Rotate(0.0f, -0.0f, 90.0f);
	_ppObjects[3]->OnPrepareAnimate();
	_ppObjects[3]->objLayer = Layout::SIREN;

	_ppObjects[4] = new CSiren();
	_ppObjects[4]->SetChild(pSiren , true);
	_ppObjects[4]->SetPosition(XMFLOAT3(-23.6237f, 3.744244f, 19.47554f));
	_ppObjects[4]->Rotate(0.0f, 0.0f, -90.0f);
	_ppObjects[4]->OnPrepareAnimate();
	_ppObjects[1]->objLayer = Layout::SIREN;

	_ppObjects[5] = new CSiren();
	_ppObjects[5]->SetChild(pSiren , true);
	_ppObjects[5]->SetPosition(XMFLOAT3(-23.6237f, 3.744244f, 4.938357f));
	_ppObjects[5]->Rotate(0.0f, 0.0f, -90.0f);;
	_ppObjects[5]->OnPrepareAnimate();
	_ppObjects[5]->objLayer = Layout::SIREN;

	_ppObjects[6] = new CSiren();
	_ppObjects[6]->SetChild(pSiren , true);
	_ppObjects[6]->SetPosition(XMFLOAT3(-23.6237f, 3.744244f, 0.09920654f));
	_ppObjects[6]->Rotate(0.0f, 0.0f, -90.0f);
	_ppObjects[6]->OnPrepareAnimate();
	_ppObjects[6]->objLayer = Layout::SIREN;

	_ppObjects[7] = new CSiren();
	_ppObjects[7]->SetChild(pSiren , true);
	_ppObjects[7]->SetPosition(XMFLOAT3(-23.6237f, 3.744244f, -14.46775f));
	_ppObjects[7]->Rotate(0.0f, 0.0f, -90.0f);
	_ppObjects[7]->OnPrepareAnimate();
	_ppObjects[7]->objLayer = Layout::SIREN;

	_ppObjects[8] = new CSiren();
	_ppObjects[8]->SetChild(pSiren , true);
	_ppObjects[8]->SetPosition(XMFLOAT3(-19.46681f, 3.744244f, -23.56555f));
	_ppObjects[8]->Rotate(90.0f, 0.0f, 0.0f);
	_ppObjects[8]->OnPrepareAnimate();
	_ppObjects[8]->objLayer = Layout::SIREN;

	_ppObjects[9] = new CSiren();
	_ppObjects[9]->SetChild(pSiren , true);
	_ppObjects[9]->SetPosition(XMFLOAT3(-4.909707f, 3.744244f, -23.56555f));
	_ppObjects[9]->Rotate(90.0f, 0.0f, 0.0f);
	_ppObjects[9]->OnPrepareAnimate();
	_ppObjects[9]->objLayer = Layout::SIREN;

	_ppObjects[10] = new CSiren();
	_ppObjects[10]->SetChild(pSiren , true);
	_ppObjects[10]->SetPosition(XMFLOAT3(4.786877f, 3.744244f, -23.56555f));
	_ppObjects[10]->Rotate(90.0f, 0.0f, 0.0f);
	_ppObjects[10]->OnPrepareAnimate();
	_ppObjects[10]->objLayer = Layout::SIREN;

	_ppObjects[11] = new CSiren();
	_ppObjects[11]->SetChild(pSiren , true);
	_ppObjects[11]->SetPosition(XMFLOAT3(19.33877f, 3.744244f, -23.56555f));
	_ppObjects[11]->Rotate(90.0f, 0.0f, 0.0f);
	_ppObjects[11]->OnPrepareAnimate();
	_ppObjects[11]->objLayer = Layout::SIREN;

	_ppObjects[12] = new CSiren();
	_ppObjects[12]->SetChild(pSiren , true);
	_ppObjects[12]->SetPosition(XMFLOAT3(4.893795f, 3.744244f, 23.64812f));
	_ppObjects[12]->Rotate(-90.0f, 0.0f, 0.0f);
	_ppObjects[12]->OnPrepareAnimate();
	_ppObjects[12]->objLayer = Layout::SIREN;

	_ppObjects[13] = new CSiren();
	_ppObjects[13]->SetChild(pSiren , true);
	_ppObjects[13]->SetPosition(XMFLOAT3(19.45037f, 3.744244f, 23.64812f));
	_ppObjects[13]->Rotate(-90.0f, -0.0f, 0.0f);
	_ppObjects[13]->OnPrepareAnimate();
	_ppObjects[13]->objLayer = Layout::SIREN;

	_ppObjects[14] = new CSiren();
	_ppObjects[14]->SetChild(pSiren , true);
	_ppObjects[14]->SetPosition(XMFLOAT3(4.897f, 3.721f, 23.64812f));
	_ppObjects[14]->Rotate(-90.0f, 0.0f, 0.0f);
	_ppObjects[14]->OnPrepareAnimate();
	_ppObjects[14]->objLayer = Layout::SIREN;

	_ppObjects[15] = new CSiren();
	_ppObjects[15]->SetChild(pSiren , true);
	_ppObjects[15]->SetPosition(XMFLOAT3(-19.35391f, 3.744244f, 23.64812f));
	_ppObjects[15]->Rotate(-90.0f, 0.0f, 0.0f);
	_ppObjects[15]->OnPrepareAnimate();
	_ppObjects[15]->objLayer = Layout::SIREN;

	CreateShaderVariables(pd3dDevice, pd3dCommandList);
	//if (pSiren) delete pSiren;
}

CGeneratorObjectsShader::CGeneratorObjectsShader()
{
}

CGeneratorObjectsShader::~CGeneratorObjectsShader()
{
}

void CGeneratorObjectsShader::BuildObjects(ID3D12Device5* pd3dDevice, ID3D12GraphicsCommandList4  * pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature, CLoadedModelInfo* pModel, void* pContext)
{
	_objectCount = 3;
	_ppObjects = new CGameObject * [_objectCount]{};

	CGameObject* pGenerator1 = CGameObject::LoadGeometryFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, "Model/Map/Generator.bin", this, Layout::GENERATOR);

	CGameObject* pGenerator2 = CGameObject::LoadGeometryFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, "Model/Map/Generator.bin", this, Layout::GENERATOR);

	CGameObject* pGenerator3 = CGameObject::LoadGeometryFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, "Model/Map/Generator.bin", this, Layout::GENERATOR);

	_ppObjects[0] = new CGenerator();
	_ppObjects[0]->SetChild(pGenerator1 ,true);
	_ppObjects[0]->SetPosition(XMFLOAT3(-22.884f, 0.0f, 2.46665f));
	_ppObjects[0]->Rotate(0.0f, 90.0f, 0.0f);
	_ppObjects[0]->OnPrepareAnimate();
	_ppObjects[0]->SetNormalVector();
	_ppObjects[0]->objLayer = Layout::GENERATOR;

	_ppObjects[1] = new CGenerator();
	_ppObjects[1]->SetChild(pGenerator2 ,true);
	_ppObjects[1]->SetPosition(XMFLOAT3(22.95006f, 0.0f, 2.506552f));
	_ppObjects[1]->Rotate(0.0f, -90.0f, 0.0f);
	_ppObjects[1]->OnPrepareAnimate();
	_ppObjects[1]->SetNormalVector();
	_ppObjects[1]->objLayer = Layout::GENERATOR;

	_ppObjects[2] = new CGenerator();
	_ppObjects[2]->SetChild(pGenerator3 ,true);
	_ppObjects[2]->SetPosition(XMFLOAT3(0.0f, 0.0f, -22.85f));
	_ppObjects[2]->Rotate(0.0f, 0.0f, 0.0f);
	_ppObjects[2]->OnPrepareAnimate();
	_ppObjects[2]->SetNormalVector();
	_ppObjects[2]->objLayer = Layout::GENERATOR;

	CreateShaderVariables(pd3dDevice, pd3dCommandList);

	/*if (pGenerator1)delete pGenerator1;
	if (pGenerator2)delete pGenerator2;
	if (pGenerator3)delete pGenerator3;*/
}



CVirtualObjectsShader::CVirtualObjectsShader()
{
}

CVirtualObjectsShader::~CVirtualObjectsShader()
{
}

void CVirtualObjectsShader::BuildObjects(ID3D12Device5* pd3dDevice, ID3D12GraphicsCommandList4* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature, CLoadedModelInfo* pModel, void* pContext)
{

}

CHitEffectObjectsShader::CHitEffectObjectsShader()
{
}

CHitEffectObjectsShader::~CHitEffectObjectsShader()
{
}

void CHitEffectObjectsShader::BuildObjects(ID3D12Device5* pd3dDevice, ID3D12GraphicsCommandList4* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature, CLoadedModelInfo* pModel, void* pContext)
{
	_objectCount = 1;
	_ppObjects = new CGameObject * [_objectCount]{};

	CGameObject* pHit = CGameObject::LoadGeometryFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, "Model/hit.bin", this, Layout::EFFECT);
	pHit->_type = 1;
	_ppObjects[0] = new CHitEffect();
	_ppObjects[0]->SetChild(pHit ,false);
	_ppObjects[0]->OnPrepareAnimate();
	_ppObjects[0]->SetPosition(XMFLOAT3(0.2f, 1.114f, 3.7f));
	_ppObjects[0]->SetScale(0.5f, 0.5f, 0.5f);

	CreateShaderVariables(pd3dDevice, pd3dCommandList);
	//if (pHit) delete pHit;

}

void CHitEffectObjectsShader::Render(ID3D12GraphicsCommandList4* pd3dCommandList, CCamera* pCamera, bool bRaster)
{
	CStandardShader::Render(pd3dCommandList, pCamera, bRaster);
	for (int j = 0; j < _objectCount; j++)
	{
		if (_ppObjects[j])
		{
			if (_ppObjects[j])
			{
				_ppObjects[j]->Animate(_fElapsedTime);
				_ppObjects[j]->UpdateTransform(NULL);
				if (((CHitEffect *)_ppObjects[j])->GetOnHit())
				{
					_ppObjects[j]->Render(pd3dCommandList, pCamera, bRaster);
				}
			}
		}
	}
}
