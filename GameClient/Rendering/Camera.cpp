#include "../Platform/pch.h"
#include "../Gameplay/Player.h"
#include "Camera.h"
#include "../Platform/DXSampleHelper.h"

CCamera::CCamera()
{
	_view = Matrix4x4::Identity();
	_projection = Matrix4x4::Identity();
	_viewport = { 0, 0, atb::client::config::DefaultWindowWidth , atb::client::config::DefaultWindowHeight, 0.0f, 1.0f }; // 화면 영역 설정, 깊이 값 설정 0~1.0f (Z값)
	_scissorRect = { 0, 0, atb::client::config::DefaultWindowWidth , atb::client::config::DefaultWindowHeight };
	_position = XMFLOAT3(0.0f, 0.0f, 0.0f);
	_right = XMFLOAT3(1.0f, 0.0f, 0.0f);
	_look = XMFLOAT3(0.0f, 0.0f, 1.0f);
	_up = XMFLOAT3(0.0f, 1.0f, 0.0f);
	_offset = XMFLOAT3(0.0f, 0.0f, 0.0f);
	_lookAtWorld = XMFLOAT3(0.0f, 0.0f, 0.0f);
}

CCamera::~CCamera()
{
	ReleaseShaderVariables();
}

void CCamera::CreateShaderVariables(ID3D12Device5* pd3dDevice, ID3D12GraphicsCommandList4* pd3dCommandList)
{
	ReleaseShaderVariables();
	UINT ncbElementBytes = ((sizeof(VS_CB_CAMERA_INFO) + 255) & ~255); //256의 배수
	_constantBuffer.Attach(::CreateBufferResource(pd3dDevice, pd3dCommandList, NULL, ncbElementBytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, NULL));
	if (!_constantBuffer)
	{
		ThrowIfFailed(E_OUTOFMEMORY);
	}
	ThrowIfFailed(_constantBuffer->Map(0, NULL, reinterpret_cast<void**>(&_mappedConstants)));
	++_bufferCreateCount;
}

void CCamera::ReleaseShaderVariables()
{
	if (_constantBuffer)
	{
		_constantBuffer->Unmap(0, NULL);
		_constantBuffer.Reset();
		_mappedConstants = nullptr;
	}
}

void CCamera::UpdateShaderVariables(ID3D12GraphicsCommandList4* pd3dCommandList)
{
	XMFLOAT4X4 xmf4x4View;
	XMStoreFloat4x4(&xmf4x4View, XMMatrixTranspose(XMLoadFloat4x4(&_view)));
	::memcpy(&_mappedConstants->_view, &xmf4x4View, sizeof(XMFLOAT4X4));

	XMFLOAT4X4 xmf4x4Projection;
	XMStoreFloat4x4(&xmf4x4Projection, XMMatrixTranspose(XMLoadFloat4x4(&_projection))); // 터지는 부분
	::memcpy(&_mappedConstants->_projection, &xmf4x4Projection, sizeof(XMFLOAT4X4));

	::memcpy(&_mappedConstants->_position, &_position, sizeof(XMFLOAT3));

	XMFLOAT4 fogOption; // id , on off, start, end;

	if (_viewerIndex == 0 && _fogEnabled)
	{
		fogOption = XMFLOAT4{1.f, 2.f, 12.f, 1};
	}
	else if (_viewerIndex > 0 && _fogEnabled)
	{
		fogOption = XMFLOAT4{1.f, 2.f, 7.f, 1};
	}
	else if (_viewerIndex < 0 || !_fogEnabled)
	{
		fogOption = XMFLOAT4{-1.f, 0.f, 0.f, 1};
	}

	::memcpy(&_mappedConstants->_fogOptions, &fogOption, sizeof(XMFLOAT4));


	D3D12_GPU_VIRTUAL_ADDRESS d3dGpuVirtualAddress = _constantBuffer->GetGPUVirtualAddress();
	pd3dCommandList->SetGraphicsRootConstantBufferView(0, d3dGpuVirtualAddress);
}

bool CCamera::SetMode(const DWORD mode)
{
	switch (mode)
	{
	case CCamera::FirstPersonMode:
		_offset = XMFLOAT3(0.0f, 1.25f * UNIT, 0.0f);
		GenerateProjectionMatrix(0.01f, 5000.0f, CCamera::DefaultAspectRatio, 60.0f);
		break;
	case CCamera::ThirdPersonMode:
		_offset = XMFLOAT3(0.0f, 1.7f * UNIT, -5.0f * UNIT);
		GenerateProjectionMatrix(1.01f, 5000.0f, CCamera::DefaultAspectRatio, 60.0f);
		break;
	default:
		return false;
	}

	_mode = mode;
	_timeLag = 0.0f;
	SetViewport(0, 0, atb::client::config::DefaultWindowWidth, atb::client::config::DefaultWindowHeight, 0.0f, 1.0f);
	SetScissorRect(0, 0, atb::client::config::DefaultWindowWidth, atb::client::config::DefaultWindowHeight);
	return true;
}

/*카메라 변환 행렬을 생성한다. 카메라의 위치 벡터, 카메라가 바라보는 지점, 카메라의 Up 벡터(로컬 y-축 벡터)를
파라메터로 사용하는 XMMatrixLookAtLH() 함수를 사용한다.*/
void CCamera::ResetPose(const CPlayer& target)
{
	_view = Matrix4x4::Identity();
	_position = XMFLOAT3(0.0f, 0.0f, 0.0f);
	_right = XMFLOAT3(1.0f, 0.0f, 0.0f);
	_up = XMFLOAT3(0.0f, 1.0f, 0.0f);
	_look = XMFLOAT3(0.0f, 0.0f, 1.0f);
	_lookAtWorld = target.GetPosition();
	Update(target, 0.0f);
	RegenerateViewMatrix();
}

void CCamera::GenerateViewMatrix()
{
	_view = Matrix4x4::LookAtLH(_position, _lookAtWorld, _up);
}

void CCamera::GenerateViewMatrix(XMFLOAT3 xmf3Position, XMFLOAT3 xmf3LookAt, XMFLOAT3 xmf3Up)
{
	_position = xmf3Position;
	_lookAtWorld = xmf3LookAt;
	_up = xmf3Up;

	GenerateViewMatrix();
}

void CCamera::RegenerateViewMatrix()
{
	/*카메라의 z - 축을 기준으로 카메라의 좌표축들이 직교하도록 카메라 변환 행렬을 갱신한다. = up / right / lookAt 벡터 직교정규화 */
	//카메라의 z-축 벡터를 정규화한다.
	_look = Vector3::Normalize(_look);
	//카메라의 z-축과 y-축에 수직인 벡터를 x-축으로 설정한다.
	_right = Vector3::CrossProduct(_up, _look, true);
	//카메라의 z-축과 x-축에 수직인 벡터를 y-축으로 설정한다.
	_up = Vector3::CrossProduct(_look, _right, true);

	_view._11 = _right.x; _view._12 = _up.x; _view._13 =
		_look.x;
	_view._21 = _right.y; _view._22 = _up.y; _view._23 =
		_look.y;
	_view._31 = _right.z; _view._32 = _up.z; _view._33 =
		_look.z;
	_view._41 = -Vector3::DotProduct(_position, _right);
	_view._42 = -Vector3::DotProduct(_position, _up);
	_view._43 = -Vector3::DotProduct(_position, _look);
}

void CCamera::GenerateProjectionMatrix(float fNearPlaneDistance, float fFarPlaneDistance, float fAspectRatio, float fFOVAngle)
{
	_projection = Matrix4x4::PerspectiveFovLH(XMConvertToRadians(fFOVAngle),
		fAspectRatio, fNearPlaneDistance, fFarPlaneDistance);
}

void CCamera::SetViewport(int xTopLeft, int yTopLeft, int nWidth, int nHeight, float fMinZ, float fMaxZ)
{
	_viewport.TopLeftX = float(xTopLeft);
	_viewport.TopLeftY = float(yTopLeft);
	_viewport.Width = float(nWidth);
	_viewport.Height = float(nHeight);
	_viewport.MinDepth = fMinZ;
	_viewport.MaxDepth = fMaxZ;
}

void CCamera::SetScissorRect(LONG xLeft, LONG yTop, LONG xRight, LONG yBottom)
{
	_scissorRect.left = xLeft;
	_scissorRect.top = yTop;
	_scissorRect.right = xRight;
	_scissorRect.bottom = yBottom;
}

void CCamera::SetViewportsAndScissorRects(ID3D12GraphicsCommandList4 * pd3dCommandList)
{
	//뷰포트와 씨저 사각형을 설정한다.
	pd3dCommandList->RSSetViewports(1, &_viewport);
	pd3dCommandList->RSSetScissorRects(1, &_scissorRect);
}
void CCamera::Update(const CPlayer& target, const float fTimeElapsed)
{
	XMFLOAT4X4 xmf4x4Rotate = Matrix4x4::Identity();
	XMFLOAT3 xmf3Right = target.GetRightVector();
	XMFLOAT3 xmf3Up = target.GetUpVector();
	XMFLOAT3 xmf3Look = target.GetLookVector();

	//플레이어의 로컬 축으로부터 플레이어와 같은 방향의 회전 행렬을 만든다.
	xmf4x4Rotate._11 = xmf3Right.x; xmf4x4Rotate._21 = xmf3Up.x; xmf4x4Rotate._31 = xmf3Look.x;
	xmf4x4Rotate._12 = xmf3Right.y; xmf4x4Rotate._22 = xmf3Up.y; xmf4x4Rotate._32 = xmf3Look.y;
	xmf4x4Rotate._13 = xmf3Right.z; xmf4x4Rotate._23 = xmf3Up.z; xmf4x4Rotate._33 = xmf3Look.z;

	XMFLOAT3 xmf3Offset = Vector3::TransformCoord(_offset, xmf4x4Rotate);
	XMFLOAT3 xmf3Position = Vector3::Add(target.GetPosition(), xmf3Offset);

	if (_mode == CCamera::FirstPersonMode)
	{
		_look = target.GetLookVector();
		_position = xmf3Position;
		return;
	}

	if (_mode != CCamera::ThirdPersonMode)
	{
		return;
	}

	XMFLOAT3 xmf3Direction = Vector3::Subtract(xmf3Position, _position);
	const float fLength = Vector3::Length(xmf3Direction);
	if (fLength > 0.0f)
	{
		xmf3Direction = Vector3::Normalize(xmf3Direction);
		const float fTimeLagScale = (_timeLag > 0.0f) ? fTimeElapsed / _timeLag : 1.0f;
		float fDistance = fLength * fTimeLagScale;
		if (fDistance > fLength || fLength < 0.01f)
		{
			fDistance = fLength;
		}
		_position = Vector3::Add(_position, xmf3Direction, fDistance);
	}

	//정지한 프레임에도 플레이어를 바라보도록 방향 축을 갱신한다.
	SetLookAt(target.GetPosition(), target.GetUpVector());
}

void CCamera::Rotate(const CPlayer& target, const float x, const float y, const float z)
{
	if (_mode != CCamera::FirstPersonMode)
	{
		return;
	}

	//x축회전 - 카메라 로컬 x 기준 고개 위아래, y축회전 - 플레이어 y축 기준 좌우 회전, z축회전 - 플레이어 로컬 z축 기준 회전 (린 Lean - 몸통을 그래도 한 채 카메라만 살짝 내밀어 적 살피기인데, 그냥 쵸파가 빼꼼 볼때 사선으로 서서 오브젝트에 가려진 시야를 넓히는 것과 비슷하다)

	if (x != 0.0f)
	{
		//카메라의 로컬 x-축을 기준으로 회전하는 행렬을 생성한다. 사람의 경우 고개를 끄떡이는 동작이다.
		XMMATRIX xmmtxRotate = XMMatrixRotationAxis(XMLoadFloat3(&_right),
			XMConvertToRadians(x));

		//카메라의 로컬 x-축, y-축, z-축을 회전 행렬을 사용하여 회전한다.
		_look = Vector3::TransformNormal(_look, xmmtxRotate);
		_up = Vector3::TransformNormal(_up, xmmtxRotate);
		_right = Vector3::TransformNormal(_right, xmmtxRotate);
	}
	if (y != 0.0f)
	{
		//플레이어의 로컬 y-축을 기준으로 회전하는 행렬을 생성한다.
		XMFLOAT3 xmf3Up = target.GetUpVector();
		XMMATRIX xmmtxRotate = XMMatrixRotationAxis(XMLoadFloat3(&xmf3Up),
			XMConvertToRadians(y));

		//카메라의 로컬 x-축, y-축, z-축을 회전 행렬을 사용하여 회전한다.
		_look = Vector3::TransformNormal(_look, xmmtxRotate);
		_up = Vector3::TransformNormal(_up, xmmtxRotate);
		_right = Vector3::TransformNormal(_right, xmmtxRotate);
	}
	if (z != 0.0f)
	{
		//플레이어의 로컬 z-축을 기준으로 회전하는 행렬을 생성한다.
		XMFLOAT3 xmf3Look = target.GetLookVector();
		XMMATRIX xmmtxRotate = XMMatrixRotationAxis(XMLoadFloat3(&xmf3Look),
			XMConvertToRadians(z));

		//카메라의 위치 벡터를 플레이어 좌표계로 표현한다(오프셋 벡터).
		_position = Vector3::Subtract(_position, target.GetPosition());

		//오프셋 벡터 벡터를 회전한다.
		_position = Vector3::TransformCoord(_position, xmmtxRotate);

		//회전한 카메라의 위치를 월드 좌표계로 표현한다.
		_position = Vector3::Add(_position, target.GetPosition());

		//카메라의 로컬 x-축, y-축, z-축을 회전한다.
		_look = Vector3::TransformNormal(_look, xmmtxRotate);
		_up = Vector3::TransformNormal(_up, xmmtxRotate);
		_right = Vector3::TransformNormal(_right, xmmtxRotate);
	}
}

void CCamera::SetLookAt(const XMFLOAT3& xmf3LookAt, const XMFLOAT3& up)
{
	//현재 카메라의 위치에서 플레이어를 바라보기 위한 카메라 변환 행렬을 생성한다.
	XMFLOAT4X4 mtxLookAt = Matrix4x4::LookAtLH(_position, xmf3LookAt, up);

	//카메라 변환 행렬에서 카메라의 x-축, y-축, z-축을 구한다.
	_right = XMFLOAT3(mtxLookAt._11, mtxLookAt._21, mtxLookAt._31);
	_up = XMFLOAT3(mtxLookAt._12, mtxLookAt._22, mtxLookAt._32);
	_look = XMFLOAT3(mtxLookAt._13, mtxLookAt._23, mtxLookAt._33);
}
