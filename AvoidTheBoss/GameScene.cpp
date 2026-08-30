#include "pch.h"
#include "DXSampleHelper.h"
#include "GameScene.h"
#include "GameFramework.h"
#include "UIManager.h"
#include "SceneManager.h"
#include "InputManager.h"
#include "SoundManager.h"
#include "CSound.h"

//네트워크 관련
#include "clientIocpCore.h"
#include "ClientEventScheduler.h"

// 객체 관련
#include "CBullet.h"
#include "CBoss.h"
#include "CEmployee.h"
#include "CGenerator.h"

#include <utility>




#define MAPVOLUME 50

CGameScene::CGameScene()
{
	_currentFrame = 0;
	_jobQueue = new ClientEventScheduler();
}

CGameScene::~CGameScene()
{
	delete _jobQueue;
}

void CGameScene::ReleaseUploadBuffers()
{
	CScene::ReleaseUploadBuffers();
	for (auto* player : _players)
		if (player) player->ReleaseUploadBuffers();
}

void CGameScene::ReleaseObjects()
{
	if (_jobQueue) _jobQueue->Clear();
	_camera.ReleaseShaderVariables();

	for (auto& player : _players)
	{
		if (player) player->Release();
		player = nullptr;
	}
	delete[] _generators;
	_generators = nullptr;

	CScene::ReleaseObjects();
	CMaterial::ReleaseShaders();
}

void CGameScene::OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	switch (nMessageID)
	{
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
		//마우스 캡쳐를 하고 현재 마우스 위치를 가져온다.
		::SetCapture(hWnd);
		::GetCursorPos(&_oldCursorPosition);
		break;
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
		//마우스 캡쳐를 해제한다.
		::ReleaseCapture();
		break;
	case WM_MOUSEMOVE:
		break;
	default:
		break;
	}
}

// 특수키 처리를 위한 것
void CGameScene::OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	switch (nMessageID)
	{
	case WM_KEYUP:
		switch (wParam)
		{
		//case VK_ESCAPE:
		//	::PostQuitMessage(0);
		//	break;
		case VK_RETURN:
			break;
			/*‘F1’ 키를 누르면 1인칭 카메라, ‘F3’ 키를 누르면 3인칭 카메라로 변경한다.*/
		case VK_F9:
			//“F9” 키가 눌려지면 윈도우 모드와 전체화면 모드의 전환을 처리한다.
			//mainGame.ChangeSwapChainState();
			break;
		case VK_F1:
			mainGame.m_activeDelay = !mainGame.m_activeDelay;
			if (!mainGame.m_activeDelay) _jobQueue->Clear();
			break;
		}
		break;
	default:
		break;
	}
}


void CGameScene::BuildDefaultLightsAndMaterials()
{
	m_nLights = 11;
	m_pLights = new LIGHT[m_nLights];
	::ZeroMemory(m_pLights, sizeof(LIGHT) * m_nLights);

	m_xmf4GlobalAmbient = XMFLOAT4(0.15f, 0.15f, 0.15f, 1.0f);

	// 비상구 조명
	XMFLOAT4 fAmbientExist = XMFLOAT4(0.0f, 0.7f, 0.1f, 1.0f);
	XMFLOAT4 f4DiffuseExist = XMFLOAT4(0.0f, 0.7f, 0.1f, 1.0f);
	m_pLights[0].m_bEnable = true;
	m_pLights[0].m_nType = POINT_LIGHT;
	m_pLights[0].m_fRange = 4.5f;
	m_pLights[0].m_xmf4Ambient = fAmbientExist;
	m_pLights[0].m_xmf4Diffuse = f4DiffuseExist;
	m_pLights[0].m_xmf4Specular = XMFLOAT4(0.5f, 0.5f, 0.5f, 0.0f);
	m_pLights[0].m_xmf3Position = XMFLOAT3(24.6359f, 1.168867f, -21.98898f);
	m_pLights[0].m_xmf3Attenuation = XMFLOAT3(1.0f, 0.001f, 0.0001f);

	m_pLights[10].m_bEnable = true;
	m_pLights[10].m_nType = POINT_LIGHT;
	m_pLights[10].m_fRange = 4.5f;
	m_pLights[10].m_xmf4Ambient = fAmbientExist;
	m_pLights[10].m_xmf4Diffuse = f4DiffuseExist;
	m_pLights[10].m_xmf4Specular = XMFLOAT4(0.5f, 0.5f, 0.5f, 0.0f);
	m_pLights[10].m_xmf3Position = XMFLOAT3(-24.6359f, 1.168867f, -21.98898f);
	m_pLights[10].m_xmf3Attenuation = XMFLOAT3(1.0f, 0.001f, 0.0001f);

	// 입구 문틈 햇빛 효과
	m_pLights[1].m_bEnable = true;
	m_pLights[1].m_nType = SPOT_LIGHT;
	m_pLights[1].m_fRange = 35.0f;
	m_pLights[1].m_xmf4Ambient = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
	m_pLights[1].m_xmf4Diffuse = XMFLOAT4(1.0f, 0.53f, 0.27f, 1.0f);
	m_pLights[1].m_xmf4Specular = XMFLOAT4(0.3f, 0.3f, 0.3f, 0.0f);
	m_pLights[1].m_xmf3Position = XMFLOAT3(-0.0f, 18.0f, 23.0f);
	m_pLights[1].m_xmf3Direction = XMFLOAT3(0.0f, -1.0f, -0.1f);
	m_pLights[1].m_xmf3Attenuation = XMFLOAT3(1.0f, 0.01f, 0.0001f);
	m_pLights[1].m_fFalloff = 40.0f;
	m_pLights[1].m_fPhi = (float)cos(XMConvertToRadians(150.0f));
	m_pLights[1].m_fTheta = (float)cos(XMConvertToRadians(20.0f));

	// 전역 조명
	m_pLights[2].m_bEnable = true;
	m_pLights[2].m_nType = DIRECTIONAL_LIGHT;
	m_pLights[2].m_xmf4Ambient = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
	m_pLights[2].m_xmf4Diffuse = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
	m_pLights[2].m_xmf4Specular = XMFLOAT4(0.4f, 0.4f, 0.4f, 0.0f);
	m_pLights[2].m_xmf3Direction = XMFLOAT3(0.0f, -1.0f, -1.0f);

	// 발전기 조명
	XMFLOAT4 fAmbientGen = XMFLOAT4(0.7f, 0.3f, 0.3f, 1.0f);
	XMFLOAT4 f4DiffuseGen = XMFLOAT4(0.7f, 0.3f, 0.3f, 1.0f);
	m_pLights[4].m_bEnable = true;
	m_pLights[4].m_nType = POINT_LIGHT;
	m_pLights[4].m_fRange = 3.5f;
	m_pLights[4].m_xmf4Ambient = fAmbientGen;
	m_pLights[4].m_xmf4Diffuse = f4DiffuseGen;
	m_pLights[4].m_xmf4Specular = XMFLOAT4(0.5f, 0.5f, 0.5f, 0.0f);
	m_pLights[4].m_xmf3Position = XMFLOAT3(0.874719f, 1.083242f, -23.05909f);
	m_pLights[4].m_xmf3Attenuation = XMFLOAT3(1.0f, 0.001f, 0.0001f);

	m_pLights[5].m_bEnable = true;
	m_pLights[5].m_nType = POINT_LIGHT;
	m_pLights[5].m_fRange = 3.5f;
	m_pLights[5].m_xmf4Ambient = fAmbientGen;
	m_pLights[5].m_xmf4Diffuse = f4DiffuseGen;
	m_pLights[5].m_xmf4Specular = XMFLOAT4(0.5f, 0.5f, 0.5f, 0.0f);
	m_pLights[5].m_xmf3Position = XMFLOAT3(23.08867f, 1.083242f, 3.35997f);
	m_pLights[5].m_xmf3Attenuation = XMFLOAT3(1.0f, 0.001f, 0.0001f);

	m_pLights[6].m_bEnable = true;
	m_pLights[6].m_nType = POINT_LIGHT;
	m_pLights[6].m_fRange = 3.5f;
	m_pLights[6].m_xmf4Ambient = fAmbientGen;
	m_pLights[6].m_xmf4Diffuse = f4DiffuseGen;
	m_pLights[6].m_xmf4Specular = XMFLOAT4(0.5f, 0.5f, 0.5f, 0.0f);
	m_pLights[6].m_xmf3Position = XMFLOAT3(-23.12724f, 1.146619f, 1.614123f);
	m_pLights[6].m_xmf3Attenuation = XMFLOAT3(1.0f, 0.001f, 0.0001f);

	// 창문 햇살 조명
	XMFLOAT4 fAmbientWin = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f); // 기본 색상 (발산광)
	XMFLOAT4 f4DiffuseWin = XMFLOAT4(1.0f, 0.53f, 0.27f, 1.0f); // 간접광 색상
	XMFLOAT3 fDirectionWin = XMFLOAT3(0.0f, -1.0f, -1.0f); // 라이트 방향
	float fFalloff = 0.5f; // 빛나는 영역 - 없어지는 영역관 부드러움 설정 (1.0f이상은 선명하다)
	float fRangeWin = 20.0f; // 원 크기
	float fphiWin = (float)cos(XMConvertToRadians(25.0f)); // 강도를 감쇠하기 시작하는 각도 (스포트라이트의 내부 원뿔 각도)
	float fThetaWin = (float)cos(XMConvertToRadians(10.0f)); // 강도를 감쇠하기 시작하는 각도(외부 원뿔 각도)
	m_pLights[7].m_bEnable = false;
	m_pLights[7].m_nType = SPOT_LIGHT;
	m_pLights[7].m_fRange = fRangeWin;
	m_pLights[7].m_xmf4Ambient = fAmbientWin;
	m_pLights[7].m_xmf4Diffuse = f4DiffuseWin;
	m_pLights[7].m_xmf4Specular = XMFLOAT4(0.3f, 0.3f, 0.3f, 0.0f);
	m_pLights[7].m_xmf3Position = XMFLOAT3(0.0f, 20.0f, 20.0f);
	m_pLights[7].m_xmf3Direction = XMFLOAT3(0.0f, -1.0f, 1.0f);;
	m_pLights[7].m_xmf3Attenuation = XMFLOAT3(1.0f, 0.01f, 0.0001f);
	m_pLights[7].m_fFalloff = fFalloff;
	m_pLights[7].m_fPhi = fphiWin;
	m_pLights[7].m_fTheta = fThetaWin;

	m_pLights[8].m_bEnable = true;
	m_pLights[8].m_nType = SPOT_LIGHT;
	m_pLights[8].m_fRange = fRangeWin + 10.0f;
	m_pLights[8].m_xmf4Ambient = fAmbientWin;
	m_pLights[8].m_xmf4Diffuse = f4DiffuseWin;
	m_pLights[8].m_xmf4Specular = XMFLOAT4(0.3f, 0.3f, 0.3f, 0.0f);
	m_pLights[8].m_xmf3Position = XMFLOAT3(0.0f, 23.0f, -18.0f);
	m_pLights[8].m_xmf3Direction = fDirectionWin;
	m_pLights[8].m_xmf3Attenuation = XMFLOAT3(1.0f, 0.01f, 0.0001f);
	m_pLights[8].m_fFalloff = fFalloff;
	m_pLights[8].m_fPhi = (float)cos(XMConvertToRadians(fphiWin + 25.0f));
	m_pLights[8].m_fTheta = fThetaWin;

	m_pLights[9].m_bEnable = false;
	m_pLights[9].m_nType = SPOT_LIGHT;
	m_pLights[9].m_fRange = fRangeWin;
	m_pLights[9].m_xmf4Ambient = fAmbientWin;
	m_pLights[9].m_xmf4Diffuse = f4DiffuseWin;
	m_pLights[9].m_xmf4Specular = XMFLOAT4(0.3f, 0.3f, 0.3f, 0.0f);
	m_pLights[9].m_xmf3Position = XMFLOAT3(-10.0f, 23.0f, -20.0f);
	m_pLights[9].m_xmf3Direction = fDirectionWin;
	m_pLights[9].m_xmf3Attenuation = XMFLOAT3(1.0f, 0.01f, 0.0001f);
	m_pLights[9].m_fFalloff = fFalloff;
	m_pLights[9].m_fPhi = fphiWin;
	m_pLights[9].m_fTheta = fThetaWin;
}

void CGameScene::BuildObjects(ID3D12Device5* pd3dDevice,ID3D12GraphicsCommandList4*  pd3dCommandList)
{
	m_pd3dGraphicsRootSignature = CreateGraphicsRootSignature(pd3dDevice);

	CreateCbvSrvDescriptorHeaps(pd3dDevice, 0, 282);
	// 맵 106+ 스카이박스 2 + 크래인 12 + 바닥 2 + 사이렌 6*16+ 발전기 16*3 + 셔터도어 4*2 + 비상구1*2 + 정문 4 + 캐릭터?? + 히트 1 + 총알 1
	CMaterial::PrepareShaders(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);
	BuildDefaultLightsAndMaterials();

	m_pSkyBox = new CSkyBox(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);
	m_pSkyBox->SetScale(50.0f, 50.0f, 50.0f);
	m_pSkyBox->SetPosition(0.0f, 100.0f, 0.0f);
	m_pSkyBox->Rotate(0.0f, 150.0f, 0.0f);

	for (int i = 0; i < PLAYERNUM; ++i)
	{
		if (i == (int)(CHARACTER_TYPE::BOSS))
		{
			_players[i] = new CBoss(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);
		}
		else
		{
			_players[i] = new CEmployee(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature, (CHARACTER_TYPE)(i));
		}
		_players[i]->SetPlayerIndex(i);
	}
	m_nShaders = 6;
	m_ppShaders = new CShader * [m_nShaders]{};

	CMapObjectsShader* pMapShader = new CMapObjectsShader();
	m_ppShaders[0] = pMapShader;
	pMapShader->CreateShader(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);
	pMapShader->BuildObjects(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature, NULL, NULL);

	CBulletObjectsShader* pBulletObjectShader = new CBulletObjectsShader();
	m_ppShaders[1] = pBulletObjectShader;
	pBulletObjectShader->CreateShader(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);
	pBulletObjectShader->BuildObjects(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature, NULL, NULL);

	CDoorObjectsShader* pDoorObjectShader = new CDoorObjectsShader();
	m_ppShaders[2] = pDoorObjectShader;
	pDoorObjectShader->CreateShader(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);
	pDoorObjectShader->BuildObjects(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature, NULL, NULL);

	CSirenObjectsShader* pSirenObjectShader = new CSirenObjectsShader();
	m_ppShaders[3] = pSirenObjectShader;
	pSirenObjectShader->CreateShader(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);
	pSirenObjectShader->BuildObjects(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature, NULL, NULL);

	CGeneratorObjectsShader* pGeneratorObjectsShader = new CGeneratorObjectsShader();
	m_ppShaders[4] = pGeneratorObjectsShader;
	pGeneratorObjectsShader->CreateShader(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);
	pGeneratorObjectsShader->BuildObjects(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature, NULL, NULL);

	CHitEffectObjectsShader* pHitEffectObjectsShader = new CHitEffectObjectsShader();
	m_ppShaders[5] = pHitEffectObjectsShader;
	pHitEffectObjectsShader->CreateShader(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);
	pHitEffectObjectsShader->BuildObjects(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature, NULL, NULL);

	_generators = new CGenerator * [_generatorCount];

	for (int i = 0; i < _generatorCount; ++i)
	{
		_generators[i] = ((CGenerator*)pGeneratorObjectsShader->m_ppObjects[i]);
		_generators[i]->SetIndex(i);
	}

	for (int i = 0; i < PLAYERNUM; ++i)
	{
		if (i == (int)(CHARACTER_TYPE::BOSS))
		{
			if (m_ppShaders[1] && m_ppShaders[5])
			{
				((CBullet*)(pBulletObjectShader->m_ppObjects[0]))->SetHitEffect((CHitEffect*)pHitEffectObjectsShader->m_ppObjects[0]);
				((CBoss*)_players[i])->_bullet = (CBullet*)pBulletObjectShader->m_ppObjects[0];
			}
		}
		else
		{
			((CEmployee*)_players[i])->_switches[0].position = XMFLOAT3(-23.12724f, 1.146619f, 1.814123f);
			((CEmployee*)_players[i])->_switches[0].radius = 0.5f;
			((CEmployee*)_players[i])->_switches[1].position = XMFLOAT3(23.08867f, 1.083242f, 3.155997f);
			((CEmployee*)_players[i])->_switches[1].radius = 0.5f;
			((CEmployee*)_players[i])->_switches[2].position = XMFLOAT3(0.6774719f, 1.083242f, -23.05909f);
			((CEmployee*)_players[i])->_switches[2].radius = 0.5f;
		}
	}
	ThrowIfFailed(_camera.SetMode(FIRST_PERSON_CAMERA) ? S_OK : E_INVALIDARG);
	_camera.CreateShaderVariables(pd3dDevice, pd3dCommandList);
	CreateShaderVariables(pd3dDevice, pd3dCommandList);

	//m_ppGameObjects = nullptr;
}

void CGameScene::ProcessInput(HWND& hWnd)
{
	CPlayer* localPlayer = GetLocalPlayer();
	if (!localPlayer) return;

	//if (hWnd != ::GetActiveWindow()) return;

	uint8 keyInput = 0;
	InputManager::GetInstance().InputStatusUpdate();
	InputManager::GetInstance().MouseInputStatusUpdate();


	// ============= 마우스 버튼 관련 처리 ================
	float cxDelta = 0.0f, cyDelta = 0.0f;
	if (InputManager::GetInstance().GetKeyBuffer(KEY_TYPE::MLBUTTON) > 0 )
	{

		POINT ptCursorPos;
		if (::GetCapture() == hWnd)
		{
			::SetCursor(NULL);
			::GetCursorPos(&ptCursorPos);
			cxDelta = (float)(ptCursorPos.x - _oldCursorPosition.x) / 3.0f;
			cyDelta = (float)(ptCursorPos.y - _oldCursorPosition.y) / 3.0f;
			::SetCursorPos(_oldCursorPosition.x, _oldCursorPosition.y);
		}
		if (cxDelta != 0)
		{
			localPlayer->Rotate(0.0f, cxDelta, 0.0f);
			_camera.Rotate(*localPlayer, 0.0f, cxDelta, 0.0f);
		}
	}

	//============  플레이어에게 최종 키입력 처리 ============
	keyInput = localPlayer->ProcessInput(); // 입력된 키를 기반으로 인풋 처리 진행
	if (InputManager::GetInstance().GetKeyBuffer(KEY_TYPE::G) == static_cast<uint8>(KEY_STATUS::KEY_PRESS))
		ToggleFog();

	// ============ 패킷 송신 파트 ===================
	// 이동 키 입력에 변화가 있거나 키 입력 중 회전을 수행하는 경우에만.. 이동 관련 패킷을 전송한다.
	if (_lastKeyInput != keyInput || (keyInput && cxDelta != 0))
	{
		C2S_KEY packet; // 키 입력 + 방향 정보를 보낸다.
		packet.size = sizeof(C2S_KEY);
		packet.type = (uint8)C_GAME_PACKET_TYPE::CKEY;
		packet.key = keyInput;
		const XMFLOAT3 look = localPlayer->GetLookVector();
		packet.x = look.x;
		packet.z = look.z;
		clientCore.DoSend(&packet);
	}
	_lastKeyInput = keyInput;
}

void CGameScene::Update(HWND& hWnd)

{
	_timer.Tick(0);

	_jobQueue->DoTasks();
	CPlayer* localPlayer = GetLocalPlayer();

	for (int k = 0; k < PLAYERNUM; ++k)
	{
		_players[k]->m_IsFirst = _players[k] == localPlayer && _camera.GetMode() == FIRST_PERSON_CAMERA;
		if (k == _localPlayerIndex) _players[k]->Update(_timer.GetTimeElapsed(), CLIENT_TYPE::OWNER);
		else _players[k]->Update(_timer.GetTimeElapsed(), CLIENT_TYPE::OTHER_PLAYER);
	}
	if (localPlayer)
	{
		_camera.Update(*localPlayer, _timer.GetTimeElapsed());
		_camera.RegenerateViewMatrix();
	}
	for (int k = 0; k < _generatorCount; ++k)
	{
		if (_generators[k]->TickState(_timer.GetTimeElapsed()) == GeneratorTransition::Activated)
			HandleGeneratorActivated(k, true);
	}



	mainGame.m_UIRenderer->UpdateGameSceneUI(CreateUiSnapshot());

	if (_employeeExitReady) ExitReady();
	// 평균 프레임 레이트 출력
	std::wstring str = L"[";
	str.append(std::to_wstring(m_sid));
	str.append(L"] ");
	str.append(L"- WorldFrame: ");
	str.append(std::to_wstring(_currentFrame));

	::SetWindowText(hWnd, str.c_str());
}

void CGameScene::AnimateObjects()
{
	for (int i = 0; i < m_nShaders; i++) if (m_ppShaders[i]) m_ppShaders[i]->AnimateObjects(_timer.GetTimeElapsed());

	for (int i = 0; i < PLAYERNUM; i++)
	{
		if(_players[i])
			_players[i]->Animate(_timer.GetTimeElapsed());
	}
	//if (m_pLights)
	//{
	//	m_pLights[1].m_xmf3Position = _players[_localPlayerIndex]->GetPosition();
	//	m_pLights[1].m_xmf3Direction = _players[_localPlayerIndex]->GetLookVector();
	//}
}

void CGameScene::Render(ID3D12GraphicsCommandList4* pd3dCommandList, CCamera* pCamera, bool bRaster)
{
	if (!pCamera) return;

	if (m_pd3dGraphicsRootSignature) pd3dCommandList->SetGraphicsRootSignature(m_pd3dGraphicsRootSignature);
	if (m_pd3dCbvSrvDescriptorHeap) pd3dCommandList->SetDescriptorHeaps(1, &m_pd3dCbvSrvDescriptorHeap);

	pCamera->SetViewportsAndScissorRects(pd3dCommandList);
	pCamera->UpdateShaderVariables(pd3dCommandList);

	UpdateShaderVariables(pd3dCommandList);

	D3D12_GPU_VIRTUAL_ADDRESS d3dcbLightsGpuVirtualAddress = m_pd3dcbLights->GetGPUVirtualAddress();
	pd3dCommandList->SetGraphicsRootConstantBufferView(2, d3dcbLightsGpuVirtualAddress); //Lights

	if (m_pSkyBox) m_pSkyBox->Render(pd3dCommandList, pCamera,bRaster);

	for (int i = 0; i < m_nGameObjects; i++) if (m_ppGameObjects[i]) m_ppGameObjects[i]->Render(pd3dCommandList, pCamera, bRaster);

	for (int i = 0; i < m_nShaders; i++) if (m_ppShaders[i]) m_ppShaders[i]->Render(pd3dCommandList, pCamera, bRaster);

	for (int i = 0; i < m_nHierarchicalGameObjects; i++)
	{
		if (m_ppHierarchicalGameObjects[i])
		{
			m_ppHierarchicalGameObjects[i]->Animate(m_fElapsedTime);
			if (!m_ppHierarchicalGameObjects[i]->m_IsFirst)
			{
				if (!m_ppHierarchicalGameObjects[i]->m_pSkinnedAnimationController) m_ppHierarchicalGameObjects[i]->UpdateTransform(NULL);
				if (!m_ppHierarchicalGameObjects[i]->m_pSkinnedAnimationController1) m_ppHierarchicalGameObjects[i]->UpdateTransform(NULL);
			}
			else
			{
				if (!m_ppHierarchicalGameObjects[i]->m_pSkinnedAnimationController2) m_ppHierarchicalGameObjects[i]->UpdateTransform(NULL);
			}

			m_ppHierarchicalGameObjects[i]->Render(pd3dCommandList, pCamera, bRaster);
		}
	}

	for (int i = 0; i < PLAYERNUM; ++i)
	{
		if (!_players[i]->IsHidden()) _players[i]->Render(pd3dCommandList, pCamera, bRaster);
	}
}

CPlayer* CGameScene::GetScenePlayerBySid(const int32 sid)
{
	for (int i = 0; i < PLAYERNUM; ++i)
	{
		if (_players[i]->GetSessionId() == sid)
		{
			return _players[i];
		}
	}
	return nullptr;
}

CPlayer* CGameScene::GetScenePlayerByIdx(const int32 idx)
{
	if (idx < 0 || idx >= PLAYERNUM) return nullptr;
	return _players[idx];
}

CPlayer* CGameScene::GetLocalPlayer()
{
	return GetScenePlayerByIdx(_localPlayerIndex);
}

GameUiSnapshot CGameScene::CreateUiSnapshot() const
{
	GameUiSnapshot snapshot;
	for (std::size_t index = 0; index < snapshot.players.size(); ++index)
	{
		const CPlayer* player = _players[index];
		if (player) snapshot.players[index] = PlayerUiSnapshot{ player->GetHealth() };
	}

	if (_localPlayerIndex < 0 || _localPlayerIndex >= static_cast<int16>(snapshot.players.size()) ||
		!snapshot.players[_localPlayerIndex])
	{
		return snapshot;
	}

	snapshot.localPlayerIndex = static_cast<std::size_t>(_localPlayerIndex);
	if (_localPlayerIndex == 0) return snapshot;

	const auto* employee = dynamic_cast<const CEmployee*>(_players[_localPlayerIndex]);
	if (!employee) return snapshot;

	EmployeeUiSnapshot employeeSnapshot;
	employeeSnapshot.invincible = employee->_invincible;
	employeeSnapshot.uiCooldown = employee->_uiCooldown;
	employeeSnapshot.inGeneratorArea = employee->GetIsInGenArea();
	employeeSnapshot.generatorInteractionActive = employee->GetIsPlayerOnGenInter();
	employeeSnapshot.rescueTargetAvailable = employee->HasRescueTarget();
	employeeSnapshot.rescueInteractionActive = employee->GetIsPlayerOnRescueInter();
	employeeSnapshot.beingRescued = employee->GetRescueOn();

	const int32 generatorIndex = employee->GetCurrentGeneratorIndex();
	if (_generators && generatorIndex >= 0 && generatorIndex < _generatorCount && _generators[generatorIndex])
	{
		employeeSnapshot.generatorGauge = _generators[generatorIndex]->GetProgress();
	}

	if (employeeSnapshot.rescueInteractionActive)
	{
		const int32 targetIndex = employee->GetRescuingEmployeeIndex();
		if (targetIndex > 0 && targetIndex < PLAYERNUM)
		{
			if (const auto* target = dynamic_cast<const CEmployee*>(_players[targetIndex]))
				employeeSnapshot.rescueGauge = target->GetRescueGauge();
		}
	}
	else if (employeeSnapshot.beingRescued)
	{
		employeeSnapshot.rescueGauge = employee->GetRescueGauge();
	}

	snapshot.localEmployee = employeeSnapshot;
	return snapshot;
}

CCamera* CGameScene::GetRenderCamera()
{
	return GetLocalPlayer() ? &_camera : nullptr;
}

bool CGameScene::SetCameraMode(const DWORD mode)
{
	if (!_camera.SetMode(mode)) return false;
	if (CPlayer* localPlayer = GetLocalPlayer())
	{
		_camera.Update(*localPlayer, 0.0f);
		_camera.RegenerateViewMatrix();
	}
	return true;
}

CGenerator* CGameScene::GetSceneGenByIdx(const int32 idx)
{
	if (!_generators || idx < 0 || idx >= _generatorCount) return nullptr;
	return _generators[idx];
}

bool CGameScene::ApplyPlayerMove(const int32 playerIndex, const uint8 key,
	const XMFLOAT3& direction)
{
	CPlayer* player = GetScenePlayerByIdx(playerIndex);
	if (!player) return false;

	player->SetDirection(direction);
	if (player->GetPlayerType() == PLAYER_TYPE::BOSS)
		static_cast<CBoss*>(player)->Move(key, BOSS_VELOCITY);
	else
		static_cast<CEmployee*>(player)->Move(key, EMPLOYEE_VELOCITY);
	return true;
}

bool CGameScene::ApplyPlayerPosition(const int32 playerIndex, const XMFLOAT3& position)
{
	CPlayer* player = GetScenePlayerByIdx(playerIndex);
	if (!player) return false;

	XMFLOAT3 offset = Vector3::Subtract(player->GetPosition(), position);
	if (Vector3::Length(offset) > 0.2f) player->SetPosition(position);
	return true;
}

bool CGameScene::ApplyPlayerRotation(const int32 playerIndex, const float angle)
{
	CPlayer* player = GetScenePlayerByIdx(playerIndex);
	if (!player) return false;

	player->Rotate(0.0f, angle, 0.0f);
	return true;
}

bool CGameScene::ApplyPlayerAnimation(const int32 playerIndex, const uint8 track)
{
	CPlayer* player = GetScenePlayerByIdx(playerIndex);
	if (!player) return false;

	if (playerIndex == 0)
	{
		if (track != static_cast<uint8>(ANIMTRACK::ATTACK_ANIM)) return false;
		static_cast<CBoss*>(player)->SetAttackAnimOtherClient();
		return true;
	}

	if (track == static_cast<uint8>(ANIMTRACK::GEN_ANIM))
		player->SetBehavior(PLAYER_BEHAVIOR::SWITCH_INTER);
	else if (track == static_cast<uint8>(ANIMTRACK::RESCUE))
		player->SetBehavior(PLAYER_BEHAVIOR::RESCUE);
	else
		player->SetBehavior(PLAYER_BEHAVIOR::IDLE);
	return true;
}

bool CGameScene::ApplyInteraction(const uint8 eventId)
{
	switch (static_cast<EVENT_TYPE>(eventId))
	{
	case EVENT_TYPE::SWITCH_ONE_START_EVENT:
	case EVENT_TYPE::SWITCH_TWO_START_EVENT:
	case EVENT_TYPE::SWITCH_THREE_START_EVENT:
		return SetGeneratorInteraction(
			eventId - static_cast<uint8>(EVENT_TYPE::SWITCH_ONE_START_EVENT), true, false);

	case EVENT_TYPE::SWITCH_ONE_END_EVENT:
	case EVENT_TYPE::SWITCH_TWO_END_EVENT:
	case EVENT_TYPE::SWITCH_THREE_END_EVENT:
		return SetGeneratorInteraction(
			eventId - static_cast<uint8>(EVENT_TYPE::SWITCH_ONE_END_EVENT), false, false);

	case EVENT_TYPE::SWITCH_ONE_ACTIVATE_EVENT:
	case EVENT_TYPE::SWITCH_TWO_ACTIVATE_EVENT:
	case EVENT_TYPE::SWITCH_THREE_ACTIVATE_EVENT:
		return ApplyGeneratorActivationFromNetwork(
			eventId - static_cast<uint8>(EVENT_TYPE::SWITCH_ONE_ACTIVATE_EVENT));

	case EVENT_TYPE::HIDE_PLAYER_ONE:
	case EVENT_TYPE::HIDE_PLAYER_TWO:
	case EVENT_TYPE::HIDE_PLAYER_THREE:
	case EVENT_TYPE::HIDE_PLAYER_FOUR:
	{
		CPlayer* player = GetScenePlayerByIdx(
			eventId - static_cast<uint8>(EVENT_TYPE::HIDE_PLAYER_ONE));
		if (!player) return false;
		player->SetHealth(0);
		player->SetHidden(true);
		return true;
	}

	case EVENT_TYPE::ATTACK_EVENT:
	{
		auto* boss = static_cast<CBoss*>(GetScenePlayerByIdx(0));
		if (!boss) return false;
		boss->SetAttackAnimOtherClient();
		return true;
	}

	case EVENT_TYPE::ATTACKED_PLAYER_TWO:
	case EVENT_TYPE::ATTACKED_PLAYER_THREE:
	case EVENT_TYPE::ATTACKED_PLAYER_FOUR:
	{
		auto* employee = static_cast<CEmployee*>(GetScenePlayerByIdx(
			eventId - static_cast<uint8>(EVENT_TYPE::ATTACKED_PLAYER_ONE)));
		if (!employee) return false;
		employee->PlayerAttacked();
		return true;
	}

	case EVENT_TYPE::RESCUE_PLAYER_TWO:
	case EVENT_TYPE::RESCUE_PLAYER_THREE:
	case EVENT_TYPE::RESCUE_PLAYER_FOUR:
	{
		auto* employee = static_cast<CEmployee*>(GetScenePlayerByIdx(
			eventId - static_cast<uint8>(EVENT_TYPE::RESCUE_PLAYER_ONE)));
		if (!employee) return false;
		employee->RescueOn(true);
		return true;
	}

	case EVENT_TYPE::RESCUE_CANCEL_PLAYER_TWO:
	case EVENT_TYPE::RESCUE_CANCEL_PLAYER_THREE:
	case EVENT_TYPE::RESCUE_CANCEL_PLAYER_FOUR:
	{
		auto* employee = static_cast<CEmployee*>(GetScenePlayerByIdx(
			eventId - static_cast<uint8>(EVENT_TYPE::RESCUE_CANCEL_PLAYER_ONE)));
		if (!employee) return false;
		employee->RescueOn(false);
		return true;
	}

	case EVENT_TYPE::ALIVE_PLAYER_TWO:
	case EVENT_TYPE::ALIVE_PLAYER_THREE:
	case EVENT_TYPE::ALIVE_PLAYER_FOUR:
	{
		auto* employee = static_cast<CEmployee*>(GetScenePlayerByIdx(
			eventId - static_cast<uint8>(EVENT_TYPE::ALIVE_PLAYER_ONE)));
		if (!employee) return false;
		employee->SetBehavior(PLAYER_BEHAVIOR::STAND);
		employee->_standAnimationFrames = EMPLOYEE_STAND_TIME;
		employee->RestoreHealth();
		return true;
	}

	case EVENT_TYPE::EXIT_PLAYER_TWO:
	case EVENT_TYPE::EXIT_PLAYER_THREE:
	case EVENT_TYPE::EXIT_PLAYER_FOUR:
	{
		auto* employee = static_cast<CEmployee*>(GetScenePlayerByIdx(
			eventId - static_cast<uint8>(EVENT_TYPE::EXIT_PLAYER_ONE)));
		if (!employee) return false;
		employee->SetBehavior(PLAYER_BEHAVIOR::EXIT);
		return true;
	}

	default:
		return false;
	}
}

void CGameScene::ApplyWorldFrame(const int32 worldFrame) noexcept
{
	mainGame.m_curFrame = worldFrame;
	_currentFrame = worldFrame;
}

bool CGameScene::SetGeneratorInteraction(const int32 index, const bool interacting,
	const bool advancesProgress)
{
	CGenerator* generator = GetSceneGenByIdx(index);
	if (!generator) return false;

	const bool phaseChanged = interacting
		? generator->BeginInteraction(advancesProgress)
		: generator->EndInteraction();
	if (!phaseChanged) return true;

	if (interacting)
		SoundManager::GetInstance().PlayObjectSound(17, 8 + index);
	else
		SoundManager::SoundStop(8 + index);
	return true;
}

bool CGameScene::ApplyGeneratorActivationFromNetwork(const int32 index)
{
	CGenerator* generator = GetSceneGenByIdx(index);
	if (!generator || !generator->Activate()) return false;

	HandleGeneratorActivated(index, false);
	return true;
}

void CGameScene::HandleGeneratorActivated(const int32 index, const bool notifyServer)
{
	if (notifyServer)
	{
		SC_EVENTPACKET packet{};
		packet.type = static_cast<uint8>(SC_GAME_PACKET_TYPE::GAMEEVENT);
		packet.size = sizeof(SC_EVENTPACKET);
		packet.eventId = static_cast<uint8>(EVENT_TYPE::SWITCH_ONE_ACTIVATE_EVENT) + index;
		clientCore.DoSend(&packet);

		CPlayer* localPlayer = GetLocalPlayer();
		if (localPlayer && localPlayer->GetPlayerType() == PLAYER_TYPE::EMPLOYEE)
			++static_cast<CEmployee*>(localPlayer)->_activatedGeneratorCount;
	}

	if (_activeGeneratorCount < _generatorCount) ++_activeGeneratorCount;
	_employeeExitReady = _activeGeneratorCount >= GENCNT;
	SoundManager::GetInstance().SetVolum(8 + index, 0.1f);
}

bool CGameScene::InitGame(const S2C_GAMESTART* packet, int32 sid)
{
	if (!packet || sid < 0 || _localPlayerIndex >= 0) return false;
	_jobQueue->Clear();

	int16 resolvedPlayerIdx = -1;
	for (int i = 0; i < PLAYERNUM; ++i)
	{
		if (!_players[i] || packet->sids[i] < 0) return false;
		for (int j = 0; j < i; ++j)
			if (packet->sids[i] == packet->sids[j]) return false;
		if (packet->sids[i] == sid) resolvedPlayerIdx = static_cast<int16>(i);
	}

	CPlayer* localPlayer = GetScenePlayerByIdx(resolvedPlayerIdx);
	if (!localPlayer) return false;

	for (int i = 0; i < PLAYERNUM; ++i)
	{
		_players[i]->SetPlayerSid(packet->sids[i]);
		std::cout << "[" << _players[i]->GetSessionId() << "|";
	}
	_localPlayerIndex = resolvedPlayerIdx;
	std::cout << "]\n";

	std::cout << "PLAYER_IDX: " << _localPlayerIndex << "\n";

	_players[0]->SetPosition(XMFLOAT3(0, 0, -18));
	if(_players[1] != nullptr)_players[1]->SetPosition(XMFLOAT3(10, 0, -18));
	if(_players[2] != nullptr)_players[2]->SetPosition(XMFLOAT3(15, 0, -18));
	if(_players[3] != nullptr)_players[3]->SetPosition(XMFLOAT3(20, 0, -18));
	_camera.SetViewerIndex(_localPlayerIndex);
	_camera.SetFogEnabled(true);
	SetCameraMode(FIRST_PERSON_CAMERA);
	localPlayer->SetClientType(CLIENT_TYPE::OWNER);
	return true;
}

void CGameScene::AddEvent(ClientEvent event, const float afterMilliseconds)
{
	_jobQueue->PushTask(std::move(event), afterMilliseconds);
}

void CGameScene::ExitReady()
{
	if (_employeeExitReady) // 탈출 성공 시 , 해야할 일 처리
	{
		if (!_exitSoundActive)
		{
			SoundManager::GetInstance().PlayObjectSound(15, 7);//Emergency_Door_Open
			SoundManager::GetInstance().PlayObjectSound(16, 11);
			SoundManager::GetInstance().PlayObjectSound(18, 12);//Hangar_Door_Open
			SoundManager::GetInstance().PlayObjectSound(19, 13);//Shutter_Open
			_exitSoundActive = true;
		}
		for (int j = 0; j < m_nShaders; j++)
		{
			CStandardObjectsShader* pShaderObjects = (CStandardObjectsShader*)m_ppShaders[j];
			for (int i = 0; i < pShaderObjects->m_nObjects; i++)
			{
				if (pShaderObjects->m_ppObjects[i])
				{
					if ((pShaderObjects->m_ppObjects[i]->objLayer == Layout::SIREN) || (pShaderObjects->m_ppObjects[i]->objLayer == Layout::DOOR))
						pShaderObjects->m_ppObjects[i]->m_bEmpExit = true;
				}
			}
		}
	}
}

bool CGameScene::ResetGame()
{
	CPlayer* localPlayer = GetLocalPlayer();
	if (!localPlayer) return false;

	_jobQueue->Clear();
	// 플레이어 상태 초기화
	for (auto& i : _players) if(i) i->ResetState();
	if (m_ppShaders)
		for (int i = 0; i < m_nShaders; ++i)
			if (m_ppShaders[i]) m_ppShaders[i]->ResetState();

	if (!SetCameraMode(FIRST_PERSON_CAMERA)) return false;
	_camera.ResetPose(*localPlayer);
	_camera.SetViewerIndex(-1);
	_camera.SetFogEnabled(false);
	// 발전기 상태 초기화

	for (int i = 0; i < _generatorCount; ++i)
	{
		SoundManager::SoundStop(8 + i);
		_generators[i]->ResetState();
	}
	localPlayer->SetClientType(CLIENT_TYPE::OTHER_PLAYER);
	_localPlayerIndex = -1;
	_employeeExitReady = false;
	_exitSoundActive = false;
	_lastKeyInput = 0;
	ApplyWorldFrame(0);
	_exitedPlayerCount = 0;
	_remainingPlayerCount = 0;
	_activeGeneratorCount = 0;
	for (const int channel : { 7, 11, 12, 13, 14, 15 }) SoundManager::SoundStop(channel);

#if defined(_DEBUG)
	const XMFLOAT3 right = localPlayer->GetRightVector();
	const XMFLOAT3 up = localPlayer->GetUpVector();
	const XMFLOAT3 look = localPlayer->GetLookVector();
	assert(localPlayer->GetPitch() == 0.0f && localPlayer->GetYaw() == 0.0f && localPlayer->GetRoll() == 0.0f);
	assert(right.x == 1.0f && right.y == 0.0f && right.z == 0.0f);
	assert(up.x == 0.0f && up.y == 1.0f && up.z == 0.0f);
	assert(look.x == 0.0f && look.y == 0.0f && look.z == 1.0f);
	const XMFLOAT3 expectedCameraPosition = Vector3::Add(
		localPlayer->GetPosition(), XMFLOAT3(0.0f, 1.25f * UNIT, 0.0f));
	XMFLOAT3 cameraOffset = Vector3::Subtract(_camera.GetPosition(), expectedCameraPosition);
	assert(Vector3::Length(cameraOffset) < 0.0001f);
	assert(_camera.GetMode() == FIRST_PERSON_CAMERA);
	assert(mainGame.m_curFrame.load() == 0 && _currentFrame == 0);
	const auto* bullet = static_cast<const CBoss*>(_players[0])->_bullet;
	assert(!bullet || !bullet->GetOnShoot());
	assert(!bullet || !bullet->GetHitEffect() || !bullet->GetHitEffect()->GetOnHit());
#endif

	// 모든 조형 객체 ResetState()호출

	return true;
}

