#pragma once

//프레임 버퍼의 크기와 종횡비(Aspect Ratio)를 나타내는 상수를 다음과 같이 선언한
#define ASPECT_RATIO (float(FRAME_BUFFER_WIDTH) / float(FRAME_BUFFER_HEIGHT))

//카메라의 종류(모드: Mode)를 나타내는 상수를 다음과 같이 선언한다.
#define FIRST_PERSON_CAMERA		0x01
#define SPACESHIP_CAMERA		0x02
#define THIRD_PERSON_CAMERA		0x03

//---카메라 상수 버퍼를 위한 구조체
struct VS_CB_CAMERA_INFO
{
	XMFLOAT4X4 _view;
	XMFLOAT4X4 _projection;
	XMFLOAT4 _fogOptions;
	XMFLOAT3 _position;

};

class CPlayer;

#define FOG_RENDER_CYCLE_TIME 60
class CCamera final
{
private:
	//카메라의 종류(1인칭 카메라, 스페이스-쉽 카메라, 3인칭 카메라)를 나타낸다.
	DWORD _mode = 0x00;

	//카메라의 위치(월드좌표계) 벡터이다.
	XMFLOAT3 _position;

	//카메라의 로컬 x-축(Right), y-축(Up), z-축(Look)을 나타내는 벡터이다.*/
	XMFLOAT3 _right;
	XMFLOAT3 _up;
	XMFLOAT3 _look;

	//플레이어가 바라볼 위치 벡터이다. 주로 3인칭 카메라에서 사용된다.
	XMFLOAT3 _lookAtWorld;

	//플레이어와 카메라의 오프셋을 나타내는 벡터이다. 주로 3인칭 카메라에서 사용된다.
	XMFLOAT3 _offset;

	//플레이어가 회전할 때 얼마만큼의 시간을 지연시킨 후 카메라를 회전시킬 것인가를 나타낸다.
	float _timeLag = 0.0f;

	//카메라 변환 행렬
	XMFLOAT4X4 _view;
	//투영 변환 행렬
	XMFLOAT4X4 _projection;

	//뷰포트와 씨저 사각형
	D3D12_VIEWPORT _viewport; // 렌더링 할 렌더타겟(후면버퍼) 영역 나타내는 구조체
	D3D12_RECT _scissorRect; // 렌더링에서 제거하지 않을 영역 설정

	ComPtr<ID3D12Resource> _constantBuffer;
	VS_CB_CAMERA_INFO* _mappedConstants = nullptr;
	uint32 _bufferCreateCount = 0;
	int32 _viewerIndex = -1;
	bool _fogEnabled = false;

public:
	CCamera();
	CCamera(const CCamera&) = delete;
	CCamera& operator=(const CCamera&) = delete;
	~CCamera();

	//카메라의 정보를 셰이더 프로그램에게 전달하기 위한 상수 버퍼를 생성하고 갱신한다.
	void CreateShaderVariables(ID3D12Device5* pd3dDevice, ID3D12GraphicsCommandList4* pd3dCommandList);
	void ReleaseShaderVariables();
	void UpdateShaderVariables(
		ID3D12GraphicsCommandList4* pd3dCommandList);

	//카메라 변환 행렬을 생성한다.
	void GenerateViewMatrix();
	void GenerateViewMatrix(XMFLOAT3 xmf3Position, XMFLOAT3 xmf3LookAt, XMFLOAT3 xmf3Up);
	/*카메라가 여러번 회전을 하게 되면 누적된 실수 연산의 부정확성 때문에 카메라의 로컬 x-축(Right), y-축(Up), z- 축(Look)이 서로 직교하지 않을 수 있다. 카메라의 로컬 x-축(Right), y-축(Up), z-축(Look)이 서로 직교하도록 만들
어준다.*/
	void RegenerateViewMatrix();

	//투영 변환 행렬을 생성한다.
	void GenerateProjectionMatrix(float fNearPlaneDistance, float fFarPlaneDistance, float
		fAspectRatio, float fFOVAngle);

	void SetViewport(int xTopLeft, int yTopLeft, int nWidth, int nHeight, float fMinZ =
		0.0f, float fMaxZ = 1.0f);
	void SetScissorRect(LONG xLeft, LONG yTop, LONG xRight, LONG yBottom);

	void SetViewportsAndScissorRects(ID3D12GraphicsCommandList4* pd3dCommandList);

	bool SetMode(DWORD mode);
	void ResetPose(const CPlayer& target);
	DWORD GetMode() const noexcept { return _mode; }
	bool HasShaderVariables() const noexcept { return _constantBuffer && _mappedConstants; }
	uint32 GetBufferCreateCount() const noexcept { return _bufferCreateCount; }
	D3D12_GPU_VIRTUAL_ADDRESS GetBufferAddress() const noexcept
	{
		return _constantBuffer ? _constantBuffer->GetGPUVirtualAddress() : 0;
	}

	void SetViewerIndex(int32 playerIndex) noexcept { _viewerIndex = playerIndex; }
	int32 GetViewerIndex() const noexcept { return _viewerIndex; }
	void SetFogEnabled(bool enabled) noexcept { _fogEnabled = enabled; }
	void ToggleFog() noexcept { _fogEnabled = !_fogEnabled; }
	bool IsFogEnabled() const noexcept { return _fogEnabled; }

	XMFLOAT3 GetPosition() const noexcept { return _position; }

	//1인칭 모드에서 카메라를 x-축, y-축, z-축으로 회전한다.
	void Rotate(const CPlayer& target, float pitch = 0.0f, float yaw = 0.0f, float roll = 0.0f);

	//현재 모드에 맞춰 같은 카메라 객체의 위치와 방향을 갱신한다.
	void Update(const CPlayer& target, float fTimeElapsed);

	//3인칭 카메라에서 카메라가 바라보는 지점을 설정한다. 일반적으로 플레이어를 바라보도록 설정한다.
	void SetLookAt(const XMFLOAT3& lookAt, const XMFLOAT3& up);
};
