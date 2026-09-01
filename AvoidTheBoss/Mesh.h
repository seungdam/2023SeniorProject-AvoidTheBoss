//------------------------------------------------------- ----------------------
// File: Mesh.h
//-----------------------------------------------------------------------------

#pragma once

class CGameObject;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
class CMesh
{
public:
	static constexpr UINT Position = 0x0001;
	static constexpr UINT Color = 0x0002;
	static constexpr UINT Normal = 0x0004;
	static constexpr UINT Tangent = 0x0008;
	static constexpr UINT TextureCoordinate0 = 0x0010;
	static constexpr UINT TextureCoordinate1 = 0x0020;
	static constexpr UINT BoneIndexWeight = 0x1000;
	static constexpr UINT NormalTangentTexture = Position | Normal | Tangent | TextureCoordinate0;
	static constexpr UINT MaxSkinningBoneCount = 128;

	CMesh(ID3D12Device5 *pd3dDevice,	ID3D12GraphicsCommandList4* pd3dCommandList);
	virtual ~CMesh();

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
	char							m_pstrMeshName[64] = { 0 };

protected:
	UINT							m_nType = 0x00;

	XMFLOAT3						m_xmf3AABBCenter = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3						m_xmf3AABBExtents = XMFLOAT3(0.0f, 0.0f, 0.0f);

	D3D12_PRIMITIVE_TOPOLOGY		m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	UINT							m_nSlot = 0;
	UINT							m_nOffset = 0;

protected:
	int								m_nVertices = 0;

	XMFLOAT3						*m_pxmf3Positions = NULL;

	ID3D12Resource					*m_pd3dPositionBuffer = NULL;
	ID3D12Resource					*m_pd3dPositionUploadBuffer = NULL;
	D3D12_VERTEX_BUFFER_VIEW		m_d3dPositionBufferView;

	int								m_nSubMeshes = 0;
	int								*m_pnSubSetIndices = NULL;
	UINT							**m_ppnSubSetIndices = NULL;

	ID3D12Resource					**m_ppd3dSubSetIndexBuffers = NULL;
	ID3D12Resource					**m_ppd3dSubSetIndexUploadBuffers = NULL;
	D3D12_INDEX_BUFFER_VIEW			*m_pd3dSubSetIndexBufferViews = NULL;

public:
	UINT GetType() { return(m_nType); }

	virtual void CreateShaderVariables(ID3D12Device5 *pd3dDevice, ID3D12GraphicsCommandList4* pd3dCommandList) { }
	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList4  *pd3dCommandList) { }
	virtual void ReleaseShaderVariables() { }

	virtual void ReleaseUploadBuffers();

	virtual void OnPreRender(ID3D12GraphicsCommandList4  *pd3dCommandList, void *pContext);
	virtual void Render(ID3D12GraphicsCommandList4  *pd3dCommandList, int nSubSet, bool bRaster);
	virtual void OnPostRender(ID3D12GraphicsCommandList4  *pd3dCommandList, void *pContext);
};

class CSkyBoxMesh : public CMesh
{
public:
	CSkyBoxMesh(ID3D12Device5 *pd3dDevice, ID3D12GraphicsCommandList4  *pd3dCommandList, float fWidth = 20.0f, float fHeight = 20.0f, float fDepth = 20.0f);
	virtual ~CSkyBoxMesh();
};

class CStandardMesh : public CMesh
{
public:
	CStandardMesh(ID3D12Device5 *pd3dDevice, ID3D12GraphicsCommandList4  *pd3dCommandList);
	virtual ~CStandardMesh();

protected:
	XMFLOAT4						*m_pxmf4Colors = NULL;
	XMFLOAT3						*m_pxmf3Normals = NULL;
	XMFLOAT3						*m_pxmf3Tangents = NULL;
	XMFLOAT3						*m_pxmf3BiTangents = NULL;

	XMFLOAT2						*m_pxmf2TextureCoords0 = NULL;
	XMFLOAT2						*m_pxmf2TextureCoords1 = NULL;

	ID3D12Resource					*m_pd3dTextureCoord0Buffer = NULL;
	ID3D12Resource					*m_pd3dTextureCoord0UploadBuffer = NULL;
	D3D12_VERTEX_BUFFER_VIEW		m_d3dTextureCoord0BufferView;

	ID3D12Resource					*m_pd3dTextureCoord1Buffer = NULL;
	ID3D12Resource					*m_pd3dTextureCoord1UploadBuffer = NULL;
	D3D12_VERTEX_BUFFER_VIEW		m_d3dTextureCoord1BufferView;

	ID3D12Resource					*m_pd3dNormalBuffer = NULL;
	ID3D12Resource					*m_pd3dNormalUploadBuffer = NULL;
	D3D12_VERTEX_BUFFER_VIEW		m_d3dNormalBufferView;

	ID3D12Resource					*m_pd3dTangentBuffer = NULL;
	ID3D12Resource					*m_pd3dTangentUploadBuffer = NULL;
	D3D12_VERTEX_BUFFER_VIEW		m_d3dTangentBufferView;

	ID3D12Resource					*m_pd3dBiTangentBuffer = NULL;
	ID3D12Resource					*m_pd3dBiTangentUploadBuffer = NULL;
	D3D12_VERTEX_BUFFER_VIEW		m_d3dBiTangentBufferView;

public:
	void LoadMeshFromFile(ID3D12Device5 *pd3dDevice, ID3D12GraphicsCommandList4  *pd3dCommandList, FILE *pInFile, CGameObject* pGameobject, Layout objType);

	virtual void ReleaseUploadBuffers();

	virtual void OnPreRender(ID3D12GraphicsCommandList4  *pd3dCommandList, void *pContext);
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
class CSkinnedMesh : public CStandardMesh
{
public:
	CSkinnedMesh(ID3D12Device5 *pd3dDevice, ID3D12GraphicsCommandList4  *pd3dCommandList);
	virtual ~CSkinnedMesh();

protected:
	int								m_nBonesPerVertex = 4;

	XMINT4							*m_pxmn4BoneIndices = NULL;
	XMFLOAT4						*m_pxmf4BoneWeights = NULL;

	ID3D12Resource					*m_pd3dBoneIndexBuffer = NULL;
	ID3D12Resource					*m_pd3dBoneIndexUploadBuffer = NULL;
	D3D12_VERTEX_BUFFER_VIEW		m_d3dBoneIndexBufferView;

	ID3D12Resource					*m_pd3dBoneWeightBuffer = NULL;
	ID3D12Resource					*m_pd3dBoneWeightUploadBuffer = NULL;
	D3D12_VERTEX_BUFFER_VIEW		m_d3dBoneWeightBufferView;

public:
	int								m_nSkinningBones = 0;

	char							(*m_ppstrSkinningBoneNames)[64];
	CGameObject						**m_ppSkinningBoneFrameCaches = NULL; //[m_nSkinningBones]

	XMFLOAT4X4						*m_pxmf4x4BindPoseBoneOffsets = NULL; //Transposed

	ID3D12Resource					*m_pd3dcbBindPoseBoneOffsets = NULL;
	XMFLOAT4X4						*m_pcbxmf4x4MappedBindPoseBoneOffsets = NULL;

	ID3D12Resource					*m_pd3dcbSkinningBoneTransforms = NULL; //Pointer Only
	XMFLOAT4X4						*m_pcbxmf4x4MappedSkinningBoneTransforms = NULL;

public:
	void PrepareSkinning(CGameObject *pModelRootObject);
	void LoadSkinInfoFromFile(ID3D12Device5 *pd3dDevice, ID3D12GraphicsCommandList4  *pd3dCommandList, FILE *pInFile);

	virtual void CreateShaderVariables(ID3D12Device5 *pd3dDevice, ID3D12GraphicsCommandList4  *pd3dCommandList);
	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList4  *pd3dCommandList);
	virtual void ReleaseShaderVariables();

	virtual void ReleaseUploadBuffers();

	virtual void OnPreRender(ID3D12GraphicsCommandList4  *pd3dCommandList, void *pContext);
};
