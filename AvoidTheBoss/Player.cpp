#include "pch.h"
#include "Shader.h"
#include "Player.h"
#include "CBullet.h"


CPlayer::CPlayer()
{
	m_type = 0;
	_position = XMFLOAT3(0.0f, 0.25f, 0.0f);

	_right = XMFLOAT3(1.0f, 0.0f, 0.0f);
	_up = XMFLOAT3(0.0f, 1.0f, 0.0f);
	_look = XMFLOAT3(0.0f, 0.0f, 1.0f);

	_velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
	_gravity = XMFLOAT3(0.0f, 0.0f, 0.0f);

	_friction = 0.0f;

	_pitch = 0.0f;
	_roll = 0.0f;
	_yaw = 0.0f;


}

CPlayer::~CPlayer() = default;

void CPlayer::ResetState()
{
	_state.ResetTransient();
	_right = XMFLOAT3(1.0f, 0.0f, 0.0f);
	_up = XMFLOAT3(0.0f, 1.0f, 0.0f);
	_look = XMFLOAT3(0.0f, 0.0f, 1.0f);
	_pitch = 0.0f;
	_yaw = 0.0f;
	_roll = 0.0f;
	_velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
	_boundingSphere.Center = _position;
}

/*플레이어의 위치를 변경하는 함수이다. 플레이어의 위치는 기본적으로 사용자가 플레이어를 이동하기 위한 키보드를
누를 때 변경된다. 플레이어의 이동 방향(dwDirection)에 따라 플레이어를 fDistance 만큼 이동한다.*/

void CPlayer::Move(const int16& dwDirection, float fDistance)
{
	XMFLOAT3 xmf3Shift = XMFLOAT3(0, 0, 0);
	if (LOBYTE(dwDirection))
	{
		//화살표 키 ‘↑’를 누르면 로컬 z-축 방향으로 이동(전진)한다. ‘↓’를 누르면 반대 방향으로 이동한다.

		if (LOBYTE(dwDirection) & KEY_FORWARD) xmf3Shift = Vector3::Add(xmf3Shift, _look, fDistance);
		if (LOBYTE(dwDirection) & KEY_BACKWARD) xmf3Shift = Vector3::Add(xmf3Shift, _look, -fDistance);
		if (LOBYTE(dwDirection) & KEY_RIGHT) xmf3Shift = Vector3::Add(xmf3Shift, _right, fDistance);
		if (LOBYTE(dwDirection) & KEY_LEFT) xmf3Shift = Vector3::Add(xmf3Shift, _right, -fDistance);
		//플레이어를 현재 위치 벡터에서 xmf3Shift 벡터만큼 이동한다
		_velocity = XMFLOAT3(0, 0, 0);
		SetVelocity(xmf3Shift);
	}
	else SetVelocity(xmf3Shift);
}

void CPlayer::Update(float fTimeElapsed, CLIENT_TYPE ptype)
{
	XMFLOAT3 vel = Vector3::ScalarProduct(_velocity, fTimeElapsed, false);
	_position = Vector3::Add(_position, vel);
	_boundingSphere.Center = GetPosition();
}

//플레이어를 로컬 x-축, y-축, z-축을 중심으로 회전한다.
void CPlayer::Rotate(float x, float y, float z)
{
	if (x != 0.0f)
	{
		_pitch += x;
		if (_pitch > +89.0f) { x -= (_pitch - 89.0f); _pitch = +89.0f; }
		if (_pitch < -89.0f) { x -= (_pitch + 89.0f); _pitch = -89.0f; }
	}
	if (y != 0.0f)
	{
		_yaw += y;
		if (_yaw > 360.0f) _yaw -= 360.0f;
		if (_yaw < 0.0f) _yaw += 360.0f;
	}
	if (z != 0.0f)
	{
		_roll += z;
		if (_roll > +20.0f) { z -= (_roll - 20.0f); _roll = +20.0f; }
		if (_roll < -20.0f) { z -= (_roll + 20.0f); _roll = -20.0f; }
	}

	/*플레이어를 회전한다. 플레이어의 로컬 y-축(Up 벡터)을 기준으로 로컬 z-축(Look 벡터)와
	로컬 x-축(Right 벡터)을 회전시킨다.*/
	if (y != 0.0f)
	{
		XMMATRIX xmmtxRotate = XMMatrixRotationAxis(XMLoadFloat3(&_up),
			XMConvertToRadians(y));
		_look = Vector3::TransformNormal(_look, xmmtxRotate);
		_right = Vector3::TransformNormal(_right, xmmtxRotate);
	}

	_look = Vector3::Normalize(_look);
	_right = Vector3::CrossProduct(_up, _look, true);
	_up = Vector3::CrossProduct(_look, _right, true);
}


void CPlayer::OnPrepareRender()
{
	m_xmf4x4ToParent._11 = _right.x; m_xmf4x4ToParent._12 = _right.y; m_xmf4x4ToParent._13 = _right.z;
	m_xmf4x4ToParent._21 = _up.x; m_xmf4x4ToParent._22 = _up.y; m_xmf4x4ToParent._23 = _up.z;
	m_xmf4x4ToParent._31 = _look.x; m_xmf4x4ToParent._32 = _look.y; m_xmf4x4ToParent._33 = _look.z;
	m_xmf4x4ToParent._41 = _position.x; m_xmf4x4ToParent._42 = _position.y; m_xmf4x4ToParent._43 = _position.z;

	m_xmf4x4ToParent = Matrix4x4::Multiply(XMMatrixScaling(_scale.x, _scale.y, _scale.z), m_xmf4x4ToParent);
}

void CPlayer::Render(ID3D12GraphicsCommandList4 * pd3dCommandList, CCamera* pCamera,bool bRaster)
{
	CGameObject::Render(pd3dCommandList, pCamera, bRaster);
}

CVirtualPlayer::CVirtualPlayer()
{
	SetPosition(XMFLOAT3(0.0f, 0.0f, 0.0f));
}

CVirtualPlayer::CVirtualPlayer(ID3D12Device5* pd3dDevice, ID3D12GraphicsCommandList4* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature)
{
	//CGameObject* pVirtualModel = CGameObject::LoadGeometryFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, /*"Model/Boss_Run.bin"*/"Model///Plane.bin", NULL);
	//SetChild(pVirtualModel, true);
	//if (pVirtualModel) delete pVirtualModel;
}

CVirtualPlayer::~CVirtualPlayer()
{
}

void CVirtualPlayer::Animate(float fTimeElapsed)
{
}

void CVirtualPlayer::Move(DWORD dwDirection, float fDistance)
{

}

void CVirtualPlayer::Update(float fTimeElapsed)
{

}

void CVirtualPlayer::BuildObjects(ID3D12Device5* pd3dDevice, ID3D12GraphicsCommandList4
	* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature)
{

}
uint8 CVirtualPlayer::ProcessInput()
{
	return uint8();
}
#define _WITH_DEBUG_CALLBACK_DATA

void CSoundCallbackHandler::HandleCallback(void*, float)
{
}


