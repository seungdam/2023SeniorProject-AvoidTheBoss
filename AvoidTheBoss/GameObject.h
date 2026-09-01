//------------------------------------------------------- ----------------------
// File: Object.h
//-----------------------------------------------------------------------------

#pragma once

#include "Mesh.h"
#include "Camera.h"
#include "GameObjectOwnership.h"


extern std::vector<DirectX::BoundingBox> bv;


class CSound;
class CShader;
class CStandardShader;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
struct SRVROOTARGUMENTINFO
{
	int								m_nRootParameterIndex = 0;
	D3D12_GPU_DESCRIPTOR_HANDLE		m_d3dSrvGpuDescriptorHandle = {};
};

class CTexture
{
public:
	static constexpr UINT Texture2D = 0x01;
	static constexpr UINT Texture2DResources = 0x02;
	static constexpr UINT Texture2DArray = 0x03;
	static constexpr UINT TextureCube = 0x04;
	static constexpr UINT Buffer = 0x05;

	CTexture(int nTextureResources = 1, UINT nResourceType = Texture2D, int nSamplers = 0);
	virtual ~CTexture() = default;

	CTexture(const CTexture&) = delete;
	CTexture& operator=(const CTexture&) = delete;

private:
	int								m_nReferences = 0;

	UINT							m_nTextureType = Texture2D;

	int								m_nTextures = 0;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_textures;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_textureUploadBuffers;
	std::vector<SRVROOTARGUMENTINFO> m_rootArgumentInfos;

	std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> m_samplerGpuDescriptorHandles;

public:
	void AddRef() { m_nReferences++; }
	void Release()
	{
		if (--m_nReferences <= 0)
		{
			delete this;
		}
	}

	void SetRootArgument(int nIndex, UINT nRootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE d3dsrvGpuDescriptorHandle);
	void SetSampler(int nIndex, D3D12_GPU_DESCRIPTOR_HANDLE d3dSamplerGpuDescriptorHandle);

	void UpdateShaderVariables(ID3D12GraphicsCommandList4   *pd3dCommandList);
	void UpdateShaderVariable(ID3D12GraphicsCommandList4   *pd3dCommandList, int nIndex);
	void ReleaseShaderVariables();

	void LoadTextureFromFile(ID3D12Device5 *pd3dDevice, ID3D12GraphicsCommandList4   *pd3dCommandList, const wchar_t *pszFileName, UINT nIndex, bool bIsDDSFile=true);

	int GetTextures() const noexcept { return(m_nTextures); }
	ID3D12Resource *GetTexture(int nIndex) const { return(m_textures.at(static_cast<std::size_t>(nIndex)).Get()); }
	UINT GetTextureType() const noexcept { return(m_nTextureType); }

	void ReleaseUploadBuffers();
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
class CGameObject;

class CMaterial
{
public:
	static constexpr UINT AlbedoMap = 0x01;
	static constexpr UINT SpecularMap = 0x02;
	static constexpr UINT NormalMap = 0x04;
	static constexpr UINT MetallicMap = 0x08;
	static constexpr UINT EmissionMap = 0x10;
	static constexpr UINT DetailAlbedoMap = 0x20;
	static constexpr UINT DetailNormalMap = 0x40;

	CMaterial(int nTextures);
	virtual ~CMaterial();

private:
	int								m_nReferences = 0;

public:
	void AddRef() { m_nReferences++; }
	void Release()
	{
		if (--m_nReferences <= 0)
		{
			delete this;
		}
	}

	void SetShader(CShader *pShader);
	void SetMaterialType(UINT nType) { m_nType |= nType; }
	void SetTexture(CTexture *pTexture, UINT nTexture = 0);

	virtual void UpdateShaderVariable(ID3D12GraphicsCommandList4   *pd3dCommandList);

	virtual void ReleaseUploadBuffers();

public:
	CShader							*_pShader = nullptr;
	XMFLOAT4						_xmf4AlbedoColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	XMFLOAT4						_xmf4EmissiveColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	XMFLOAT4						_xmf4SpecularColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	XMFLOAT4						_xmf4AmbientColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

	UINT							m_nType = 0x00;

	float							_fGlossiness = 0.0f;
	float							_fSmoothness = 0.0f;
	float							m_fSpecularHighlight = 0.0f;
	float							m_fMetallic = 0.0f;
	float							m_fGlossyReflection = 0.0f;

public:
	int 							_textureCount = 0;
	_TCHAR							(*_ppStrTextureNames)[64] = NULL;
	CTexture						**_ppTextures = nullptr; //0:Albedo, 1:Specular, 2:Metallic, 3:Normal, 4:Emission, 5:DetailAlbedo, 6:DetailNormal

	void LoadTextureFromFile(ID3D12Device5 *pd3dDevice, ID3D12GraphicsCommandList4  *pd3dCommandList, UINT nType, UINT nRootParameter, _TCHAR *pwstrTextureName, CTexture **ppTexture, CGameObject *pParent, FILE *pInFile, CShader *pShader);

public:
	static CShader					*_pStandardShader;
	static CShader					*_pSkinnedAnimationShader;

	static void PrepareShaders(ID3D12Device5 *pd3dDevice, ID3D12GraphicsCommandList4  *pd3dCommandList, ID3D12RootSignature *pd3dGraphicsRootSignature);
	static void ReleaseShaders();

	void SetStandardShader() { CMaterial::SetShader(_pStandardShader); }
	void SetSkinnedAnimationShader() { CMaterial::SetShader(_pSkinnedAnimationShader); }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
struct CALLBACKKEY
{
   float  							m_fTime = 0.0f;
   const void  							*m_pCallbackData = NULL;
};

class CAnimationCallbackHandler
{

public:
	CAnimationCallbackHandler() { }
	~CAnimationCallbackHandler() { }

public:
   virtual void HandleCallback(const void *pCallbackData, float fTrackPosition) { }
};

class CAnimationSet
{
public:
	static constexpr int Once = 0;
	static constexpr int PingPong = 2;
	static constexpr float CallbackEpsilon = 0.0165f;

	CAnimationSet(float fLength, int nFramesPerSecond, int nKeyFrameTransforms, int nSkinningBones, char *pstrName);
	~CAnimationSet();

public:
	char							m_pstrAnimationSetName[64];

	float							m_fLength = 0.0f;
	int								m_nFramesPerSecond = 0; //m_fTicksPerSecond

	int								m_nKeyFrames = 0;
	float							*m_pfKeyFrameTimes = NULL;
	XMFLOAT4X4						**m_ppxmf4x4KeyFrameTransforms = NULL;

#ifdef _WITH_ANIMATION_SRT
	int								m_nKeyFrameScales = 0;
	float							*m_pfKeyFrameScaleTimes = NULL;
	XMFLOAT3						**m_ppxmf3KeyFrameScales = NULL;
	int								m_nKeyFrameRotations = 0;
	float							*m_pfKeyFrameRotationTimes = NULL;
	XMFLOAT4						**m_ppxmf4KeyFrameRotations = NULL;
	int								m_nKeyFrameTranslations = 0;
	float							*m_pfKeyFrameTranslationTimes = NULL;
	XMFLOAT3						**m_ppxmf3KeyFrameTranslations = NULL;
#endif

	float 							m_fPosition = 0.0f;
    int 							m_nType = Once; //Once, Loop, PingPong

	int 							m_nCallbackKeys = 0;
	CALLBACKKEY 					*m_pCallbackKeys = NULL;

	CAnimationCallbackHandler 		*m_pAnimationCallbackHandler = NULL;

public:
	void SetPosition(float fTrackPosition);

	XMFLOAT4X4 GetSRT(int nBone);

	void SetCallbackKeys(int nCallbackKeys);
	void SetCallbackKey(int nKeyIndex, float fTime, void *pData);
	void SetAnimationCallbackHandler(CAnimationCallbackHandler *pCallbackHandler);

	void HandleCallback();
}; //애니메이션 1개

class CAnimationSets //애니메이션 집합
{
private:
	int								m_nReferences = 0;

public:
	void AddRef() { m_nReferences++; }
	void Release()
	{
		if (--m_nReferences <= 0)
		{
			delete this;
		}
	}

public:
	CAnimationSets(int nAnimationSets);
	~CAnimationSets();

public:
	int								m_nAnimationSets = 0;
	CAnimationSet					**m_pAnimationSets = NULL;

	int								m_nAnimatedBoneFrames = 0;
	CGameObject						**m_ppAnimatedBoneFrameCaches = NULL; //[m_nAnimatedBoneFrames]

public:
	void SetCallbackKeys(int nAnimationSet, int nCallbackKeys);
	void SetCallbackKey(int nAnimationSet, int nKeyIndex, float fTime, const void *pData);
	void SetAnimationCallbackHandler(int nAnimationSet, CAnimationCallbackHandler *pCallbackHandler);
};

class CAnimationTrack //동작 제어로 성능 개선
{
public:
	CAnimationTrack() { }
	~CAnimationTrack() { }

public:
    BOOL 							m_bEnable = true;
    float 							m_fSpeed = 1.0f;
    float 							m_fPosition = 0.0f;
    float 							m_fWeight = 1.0f;

	int 							m_nAnimationSet = 0;

public:
	void SetAnimationSet(int nAnimationSet) { m_nAnimationSet = nAnimationSet; }

	void SetEnable(bool bEnable) { m_bEnable = bEnable; }
	BOOL GetEnable() { return m_bEnable; }
	void SetSpeed(float fSpeed) { m_fSpeed = fSpeed; }
	void SetWeight(float fWeight) { m_fWeight = fWeight; }
	void SetPosition(float fPosition) { m_fPosition = fPosition; }
};

class CLoadedModelInfo
{
public:
	CLoadedModelInfo() { }
	~CLoadedModelInfo();

    CGameObject						*m_pModelRootObject = NULL;

	int 							m_nSkinnedMeshes = 0;
	CSkinnedMesh					**m_ppSkinnedMeshes = NULL; //[SkinnedMeshes], Skinned Mesh Cache

	CAnimationSets					*m_pAnimationSets = NULL;

public:
	void PrepareSkinning();
};

class CAnimationController //애니메이션 블렌딩 / 애니메이션 집합 전체 옵션의 자료구조
{
public:
	CAnimationController(ID3D12Device5 *pd3dDevice, ID3D12GraphicsCommandList4  *pd3dCommandList, int nAnimationTracks, CLoadedModelInfo *pModel);
	~CAnimationController();

public:
    float 							m_fTime = 0.0f;

    int 							m_nAnimationTracks = 0;
    CAnimationTrack 				*m_pAnimationTracks = NULL;

	CAnimationSets					*m_pAnimationSets = NULL;

	int 							m_nSkinnedMeshes = 0;
	CSkinnedMesh					**m_ppSkinnedMeshes = NULL; //[SkinnedMeshes], Skinned Mesh Cache

	ID3D12Resource					**m_ppd3dcbSkinningBoneTransforms = NULL; //[SkinnedMeshes]
	XMFLOAT4X4						**m_ppcbxmf4x4MappedSkinningBoneTransforms = NULL; //[SkinnedMeshes]

public:
	void UpdateShaderVariables(ID3D12GraphicsCommandList4  *pd3dCommandList);

	void SetTrackAnimationSet(int nAnimationTrack, int nAnimationSet);

	void SetTrackEnable(int nAnimationTrack, bool bEnable);
	bool GetTrackEnable(int nAnimationTrack);

	void SetTrackPosition(int nAnimationTrack, float fPosition);
	void SetTrackSpeed(int nAnimationTrack, float fSpeed);
	void SetTrackWeight(int nAnimationTrack, float fWeight);

	void SetCallbackKeys(int nAnimationSet, int nCallbackKeys);
	void SetCallbackKey(int nAnimationSet, int nKeyIndex, float fTime, const void *pData);
	void SetAnimationCallbackHandler(int nAnimationSet, CAnimationCallbackHandler *pCallbackHandler);

	void AdvanceTime(float fElapsedTime, CGameObject *pRootGameObject);
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
class CGameObject
{
private:
	game_object_ownership::ReferenceCount m_references;

public:
	void AddRef() noexcept;
	void Release() noexcept;

public:
	CGameObject();
	CGameObject(int nMaterials);
    // Transitional intrusive ownership: heap objects must normally be released
    // through Release(). Direct delete is only safe while no references exist.
    virtual ~CGameObject();

public:
	//CSound*							m_pSound;
	bool							m_IsFirst = false;
	Layout							objLayer;
	bool							m_bEmpExit = false;

	char							m_pstrFrameName[64];

	CMesh							*m_pMesh = NULL;

	int								m_nMaterials = 0;
	CMaterial						**m_ppMaterials = NULL;

	XMFLOAT4X4						m_xmf4x4ToParent;
	XMFLOAT4X4						m_xmf4x4World;

	CGameObject 					*m_pParent = NULL;
	CGameObject 					*m_pChild = NULL;
	CGameObject 					*m_pSibling = NULL;

	BoundingBox						m_pAABB;
	int								_type = -1;

	void SetMesh(CMesh *pMesh);
	void SetShader(CShader *pShader);
	void SetShader(int nMaterial, CShader *pShader);
	void SetMaterial(int nMaterial, CMaterial *pMaterial);

	void SetChild(CGameObject *pChild, bool bReferenceUpdate=false);

	virtual void BuildMaterials(ID3D12Device5 *pd3dDevice, ID3D12GraphicsCommandList4  *pd3dCommandList) { }

	virtual void OnPrepareAnimate() { }
	virtual void Animate(float fTimeElapsed);
	virtual void Update(float fTimeElapsed) { }

	virtual void OnPrepareRender() { }
	virtual void Render(ID3D12GraphicsCommandList4 *pd3dCommandList, CCamera* pCamera, bool bRaster);

	virtual void CreateShaderVariables(ID3D12Device5 *pd3dDevice, ID3D12GraphicsCommandList4  *pd3dCommandList);
	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList4  *pd3dCommandList);
	virtual void ReleaseShaderVariables();

	virtual void UpdateShaderVariable(ID3D12GraphicsCommandList4  *pd3dCommandList, XMFLOAT4X4 *pxmf4x4World);
	virtual void UpdateShaderVariable(ID3D12GraphicsCommandList4  *pd3dCommandList, CMaterial *pMaterial);

	virtual void ReleaseUploadBuffers();
	virtual void SetNormalVector(){}

	const XMFLOAT3 GetPosition();

	XMFLOAT3 GetLook();
	XMFLOAT3 GetUp();
	XMFLOAT3 GetRight();

	XMFLOAT3 GetParentUp();

	void SetPosition(float x, float y, float z);
	void SetPosition(XMFLOAT3 xmf3Position);
	void SetScale(float x, float y, float z);

	void Rotate(float fPitch = 10.0f, float fYaw = 10.0f, float fRoll = 10.0f);
	void Rotate(XMFLOAT3 *pxmf3Axis, float fAngle);
	void Rotate(XMFLOAT4 *pxmf4Quaternion);

	CGameObject *GetParent() { return(m_pParent); }
	void UpdateTransform(XMFLOAT4X4 *pxmf4x4Parent=NULL);
	CGameObject *FindFrame(const char *pstrFrameName);

	CTexture *FindReplicatedTexture(_TCHAR *pstrTextureName);

	UINT GetMeshType() { return((m_pMesh) ? m_pMesh->GetType() : 0x00); }

public:
	int	m_nAnimControllerIndex = 0;
	CAnimationController 			*m_pSkinnedAnimationController = NULL;
	CAnimationController			*m_pSkinnedAnimationController1 = NULL;
	CAnimationController			*m_pSkinnedAnimationController2 = NULL;

	CSkinnedMesh *FindSkinnedMesh(char *pstrSkinnedMeshName);
	void FindAndSetSkinnedMesh(CSkinnedMesh **ppSkinnedMeshes, int *pnSkinnedMesh);

	void SetTrackAnimationSet(int nAnimationTrack, int nAnimationSet);
	void SetTrackAnimationPosition(int nAnimationTrack, float fPosition);

	void LoadMaterialsFromFile(ID3D12Device5 *pd3dDevice, ID3D12GraphicsCommandList4  *pd3dCommandList, CGameObject *pParent, FILE *pInFile, CShader *pShader);

	static void LoadAnimationFromFile(FILE *pInFile, CLoadedModelInfo *pLoadedModel);
	static CGameObject *LoadFrameHierarchyFromFile(ID3D12Device5 *pd3dDevice, ID3D12GraphicsCommandList4  *pd3dCommandList, ID3D12RootSignature *pd3dGraphicsRootSignature, CGameObject *pParent, FILE *pInFile, CShader *pShader, int *pnSkinnedMeshes, Layout objType);

	static CLoadedModelInfo *LoadGeometryAndAnimationFromFile(ID3D12Device5 *pd3dDevice, ID3D12GraphicsCommandList4  *pd3dCommandList, ID3D12RootSignature *pd3dGraphicsRootSignature, const char *pstrFileName, CShader *pShader, Layout objType);

	static CGameObject* LoadGeometryFromFile(ID3D12Device5* pd3dDevice, ID3D12GraphicsCommandList4 * pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature, const char* pstrFileName, CShader* pShader, Layout objType);

	static void PrintFrameInfo(CGameObject *pGameObject, CGameObject *pParent);
	virtual void ResetState(){}
};

// Adopts one existing CGameObject ownership claim; it does not call AddRef().
using GameObjectReleaser = game_object_ownership::Releaser<CGameObject>;
using GameObjectOwner = game_object_ownership::Owner<CGameObject>;


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
class CSkyBox : public CGameObject
{
public:
	CSkyBox(ID3D12Device5 *pd3dDevice,
		ID3D12GraphicsCommandList4   *pd3dCommandList, ID3D12RootSignature *pd3dGraphicsRootSignature);
	virtual ~CSkyBox();

	virtual void Render(ID3D12GraphicsCommandList4    *pd3dCommandList, CCamera *pCamera, bool bRaster);
};


class CSiren : public CGameObject
{
private:
	CGameObject* m_ppSirenCap = NULL;
	CGameObject* m_ppSirenBell = NULL;
	XMFLOAT4X4 _initialSirenCapTransform = Matrix4x4::Identity();
	XMFLOAT4X4 _initialSirenBellTransform = Matrix4x4::Identity();

	float m_AnimationDegree = 1080.0f;
public:
	CSiren();
	virtual ~CSiren();

	virtual void OnPrepareAnimate();
	virtual void Animate(float fTimeElapsed);
	virtual void ResetState()
	{
		m_bEmpExit = false;
		m_AnimationDegree = 1080.0f;

		if (m_ppSirenBell)
		{
			m_ppSirenBell->m_xmf4x4ToParent = _initialSirenBellTransform;
		}
		if (m_ppSirenCap)
		{
			m_ppSirenCap->m_xmf4x4ToParent = _initialSirenCapTransform;
		}
		UpdateTransform(NULL);
	}
};

class CFrontDoor : public CGameObject
{
private:
	static constexpr float InitialAnimationDistance = 5.0f;

	float m_AnimationDistance = InitialAnimationDistance;
	CGameObject* m_pLeftDoorFrame = NULL;
	CGameObject* m_pRightDoorFrame = NULL;
	XMFLOAT4X4 _initialLeftDoorTransform = Matrix4x4::Identity();
	XMFLOAT4X4 _initialRightDoorTransform = Matrix4x4::Identity();
public:
	CFrontDoor();
	virtual ~CFrontDoor();

	virtual void OnPrepareAnimate();
	virtual void Animate(float fTimeElapsed);

	virtual void ResetState()
	{
		m_bEmpExit = false;
		m_AnimationDistance = InitialAnimationDistance;
		if (m_pLeftDoorFrame)
		{
			m_pLeftDoorFrame->m_xmf4x4ToParent = _initialLeftDoorTransform;
		}
		if (m_pRightDoorFrame)
		{
			m_pRightDoorFrame->m_xmf4x4ToParent = _initialRightDoorTransform;
		}
		UpdateTransform(NULL);
	}
};

class CEmergencyDoor : public CGameObject
{
private:
	float m_AnimationDegree = 120.0f;
	XMFLOAT4X4 _initialTransform = Matrix4x4::Identity();
public:
	CEmergencyDoor();
	virtual ~CEmergencyDoor();

	virtual void OnPrepareAnimate();
	virtual void Animate(float fTimeElapsed);

	virtual void ResetState()
	{
		m_bEmpExit = false;
		m_AnimationDegree = 120.0f;
		m_xmf4x4ToParent = _initialTransform;
		UpdateTransform(NULL);
	}
};

class CShutterDoor : public CGameObject
{
private:
	float m_AnimationDistance = 1.5f;//1.9f;
	CGameObject* m_pShutter = NULL;
	XMFLOAT4X4 _initialShutterTransform = Matrix4x4::Identity();
public:
	CShutterDoor();
	virtual ~CShutterDoor();

	virtual void OnPrepareAnimate();
	virtual void Animate(float fTimeElapsed);

	virtual void ResetState()
	{
		m_bEmpExit = false;
		m_AnimationDistance = 1.5f;
		if (m_pShutter)
		{
			m_pShutter->m_xmf4x4ToParent = _initialShutterTransform;
		}
		UpdateTransform(NULL);
	}
};

class CHitEffect : public CGameObject
{
private:
	static constexpr float MaxScale = 1.0f;
	static constexpr float ScaleIncrement = 0.1f;

private:
	float		m_fDistance = 0.0f;
	XMFLOAT3	m_xmf3Look = XMFLOAT3(0.0f, 0.0f, 1.0f);
	XMFLOAT3	m_xmf3Right = XMFLOAT3(1.0f, 0.0f, 0.0f);
	XMFLOAT3	m_xmf3Up = XMFLOAT3(0.0f, 1.0f, 0.0f);
	XMFLOAT3	m_xmf3Velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);

	float scaleFactor = 0.0f;
	bool m_bOnHit = false;
	CGameObject* m_pHit = NULL;
	XMFLOAT4X4 _initialTransform = Matrix4x4::Identity();
public:
	CHitEffect();
	virtual ~CHitEffect();

	virtual void OnPrepareAnimate();
	virtual void Animate(float fTimeElapsed);
	virtual void Update(float fTimeElapsed);
	void ResetState() override;
	void CaptureInitialTransform() noexcept { _initialTransform = m_xmf4x4ToParent; }

	void SetOnHit(bool hit) { m_bOnHit = hit; }
	bool GetOnHit() const noexcept { return m_bOnHit; }

	XMFLOAT3 GetLookVector() { return(m_xmf3Look); }
	XMFLOAT3 GetUpVector() { return(m_xmf3Up); }
	XMFLOAT3 GetRightVector() { return(m_xmf3Right); }
	void SetDirection(const XMFLOAT3& look)
	{
		m_xmf3Look = look;
		m_xmf3Right = Vector3::CrossProduct(m_xmf3Up, m_xmf3Look, true);
	}
};
