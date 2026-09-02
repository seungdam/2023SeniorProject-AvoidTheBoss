#pragma once

#include "../../Shared/Types.h"

#include <cstdio>
#include <d3d12.h>

extern UINT gnCbvSrvDescriptorIncrementSize;
extern UINT gnRtvDescriptorIncrementSize;
extern UINT gnDsvDescriptorIncrementSize;

ID3D12Resource* CreateBufferResource(
	ID3D12Device5* pd3dDevice,
	ID3D12GraphicsCommandList4* pd3dCommandList,
	void* pData,
	UINT nBytes,
	D3D12_HEAP_TYPE d3dHeapType = D3D12_HEAP_TYPE_UPLOAD,
	D3D12_RESOURCE_STATES d3dResourceStates = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
	ID3D12Resource** ppd3dUploadBuffer = nullptr);

ID3D12Resource* CreateTextureResourceFromDDSFile(
	ID3D12Device5* pd3dDevice,
	ID3D12GraphicsCommandList4* pd3dCommandList,
	const wchar_t* pszFileName,
	ID3D12Resource** ppd3dUploadBuffer,
	D3D12_RESOURCE_STATES d3dResourceStates = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

BYTE ReadStringFromFile(FILE* pInFile, char* pstrToken);
int ReadIntegerFromFile(FILE* pInFile);
float ReadFloatFromFile(FILE* pInFile);
