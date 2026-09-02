#pragma once
#include "../Rendering/GameObject.h"
#include "../Rendering/Shader.h"

class CSound;

struct LIGHT
{
	static constexpr int PointType = 1;
	static constexpr int SpotType = 2;
	static constexpr int DirectionalType = 3;

	XMFLOAT4				xmf4Ambient;
	XMFLOAT4				xmf4Diffuse;
	XMFLOAT4				xmf4Specular;
	XMFLOAT3				xmf3Position;
	XMFLOAT3				xmf3Attenuation;
	XMFLOAT3				xmf3Direction;

	float 					fFalloff;
	float 					theta; //cos(m_fTheta)
	float					pi; //cos(m_fPhi)
	bool					isEnable;
	int						type;
	float					range;
	float					padding;
};
struct LIGHTS
{
	static constexpr std::size_t MaxLightCount = 16;

	LIGHT					lights[MaxLightCount];
	XMFLOAT4				xmf4GlobalAmbient;
	int						lightCount;
};

class CSound;

class CScene
{
public:
	static int32 _sid;
	static int32 _cid;
protected:
	ID3D12RootSignature* _pd3dGraphicsRootSignature = NULL;
public:
	LIGHT*								_pLights = nullptr;
	LIGHTS*								_pcbMappedLights = nullptr;

	int									_lightCount = 0;
	XMFLOAT4							_xmf4GlobalAmbient;
	ID3D12Resource*						_pd3dcbLights = nullptr;

	int									_gameObjectCounts = 0;
	CGameObject							(**_ppGameObjects) = nullptr;

	int									_hierarchicalGameObjectCount = 0;
	CGameObject							(**_ppHierarchicalGameObjects) = nullptr;

	int									_shaderCount = 0;
	CShader								(**_ppShaders) = nullptr;
	CSkyBox*							_pSkyBox = nullptr;

	float								_fElapsedTime = 0.0f;
protected:

	static ID3D12DescriptorHeap*		_pd3dCbvSrvDescriptorHeap;
	static D3D12_CPU_DESCRIPTOR_HANDLE	_d3dCbvCPUDescriptorStartHandle;
	static D3D12_GPU_DESCRIPTOR_HANDLE	_d3dCbvGPUDescriptorStartHandle;
	static D3D12_CPU_DESCRIPTOR_HANDLE	_d3dSrvCPUDescriptorStartHandle;
	static D3D12_GPU_DESCRIPTOR_HANDLE	_d3dSrvGPUDescriptorStartHandle;

	static D3D12_CPU_DESCRIPTOR_HANDLE	_d3dCbvCPUDescriptorNextHandle;
	static D3D12_GPU_DESCRIPTOR_HANDLE	_d3dCbvGPUDescriptorNextHandle;
	static D3D12_CPU_DESCRIPTOR_HANDLE	_d3dSrvCPUDescriptorNextHandle;
	static D3D12_GPU_DESCRIPTOR_HANDLE	_d3dSrvGPUDescriptorNextHandle;
public:
	CScene() = default;
	virtual ~CScene() = default;

	static void CreateCbvSrvDescriptorHeaps(ID3D12Device5* pd3dDevice, int nConstantBufferViews, int nShaderResourceViews);
	static D3D12_GPU_DESCRIPTOR_HANDLE CreateConstantBufferViews(ID3D12Device5* pd3dDevice, int nConstantBufferViews, ID3D12Resource* pd3dConstantBuffers, UINT nStride);
	static D3D12_GPU_DESCRIPTOR_HANDLE CreateShaderResourceViews(ID3D12Device5* pd3dDevice, CTexture* pTexture, UINT nRootParameter, bool bAutoIncrement);

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUCbvDescriptorStartHandle() { return(_d3dCbvCPUDescriptorStartHandle); }
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUCbvDescriptorStartHandle() { return(_d3dCbvGPUDescriptorStartHandle); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUSrvDescriptorStartHandle() { return(_d3dSrvCPUDescriptorStartHandle); }
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUSrvDescriptorStartHandle() { return(_d3dSrvGPUDescriptorStartHandle); }

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUCbvDescriptorNextHandle() { return(_d3dCbvCPUDescriptorNextHandle); }
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUCbvDescriptorNextHandle() { return(_d3dCbvGPUDescriptorNextHandle); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUSrvDescriptorNextHandle() { return(_d3dSrvCPUDescriptorNextHandle); }
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUSrvDescriptorNextHandle() { return(_d3dSrvGPUDescriptorNextHandle); }


	virtual void ProcessInput(HWND& hWnd) {}
	virtual void Render(ID3D12GraphicsCommandList4* pd3dCommandList, CCamera* pCamera,bool Raster) {};
	virtual CCamera* GetRenderCamera() { return nullptr; }
	virtual void Update(HWND& hWnd) {}
	virtual void AnimateObjects() {}
	virtual void BuildObjects(ID3D12Device5* pd3dDevice, ID3D12GraphicsCommandList4* pd3dCommandList) {}

	ID3D12RootSignature* CreateGraphicsRootSignature(ID3D12Device5* pd3dDevice);
	ID3D12RootSignature* GetGraphicsRootSignature() { return(_pd3dGraphicsRootSignature); }


	virtual void CreateShaderVariables(ID3D12Device5* pd3dDevice, ID3D12GraphicsCommandList4* pd3dCommandList);
	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList4* pd3dCommandList);
	virtual void ReleaseShaderVariables();

	virtual void ReleaseUploadBuffers();
	virtual void ReleaseObjects();

	virtual void OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM	lParam);
	virtual void OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	virtual void MouseAction(const POINT& mp) {};
};




