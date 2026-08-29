#pragma once
//#include "Shader.h"
//#include "Player.h"
//#include "CEmployee.h"
//#include "CBoss.h"
//#include "CGenerator.h" // 스위치 분리

#include "CScene.h"
#include "ClientPacketEvent.h"
#include "GameUiSnapshot.h"


class Scheduler;
class CGenerator;
class CSound;

class CGameScene : public CScene
{
	friend class ClientSession;
public:
	CGameScene();
	~CGameScene();
	virtual void InitScene() { _timer.Reset(); }
	//씬에서 마우스와 키보드 메시지를 처리한다.
	virtual void OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM	lParam);
	virtual void OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);

	void BuildDefaultLightsAndMaterials();
	virtual void BuildObjects(ID3D12Device5* pd3dDevice, ID3D12GraphicsCommandList4* pd3dCommandList);

	virtual void AnimateObjects();
public : // SceneInterface 상속 함수
	virtual void ProcessInput(HWND& hWnd);
	virtual void Update(HWND& hWnd);
	virtual void Render(ID3D12GraphicsCommandList4* pd3dCommandList, CCamera* pCamera,bool Raster);
	CCamera* GetRenderCamera() override;
	CCamera* GetCameraComponent() noexcept { return &_camera; }
	const CCamera* GetCameraComponent() const noexcept { return &_camera; }
	bool SetCameraMode(DWORD mode);
	void SetFogEnabled(bool enabled) noexcept { _camera.SetFogEnabled(enabled); }
	void ToggleFog() noexcept { _camera.ToggleFog(); }
public: // 오승담 작성 함수
	CPlayer* GetScenePlayerBySid(const int32 sid);
	CPlayer* GetScenePlayerByIdx(const int32 idx);
	CPlayer* GetLocalPlayer();
	int32 GetLocalPlayerIndex() const noexcept { return _localPlayerIndex; }
	[[nodiscard]] GameUiSnapshot CreateUiSnapshot() const;
	CGenerator* GetSceneGenByIdx(const int32 idx);
	bool SetGeneratorInteraction(int32 index, bool interacting, bool advancesProgress);
	bool ApplyGeneratorActivationFromNetwork(int32 index);
	bool ApplyPlayerMove(int32 playerIndex, uint8 key, const XMFLOAT3& direction);
	bool ApplyPlayerPosition(int32 playerIndex, const XMFLOAT3& position);
	bool ApplyPlayerRotation(int32 playerIndex, float angle);
	bool ApplyPlayerAnimation(int32 playerIndex, uint8 track);
	bool ApplyInteraction(uint8 eventId);
	void ApplyWorldFrame(int32 worldFrame) noexcept;

	bool InitGame(const S2C_GAMESTART* packet, int32 sid);

	void StopTimer() { _timer.Stop(); }
	void StartTimer() { _timer.Start(); }
	void AddEvent(ClientEvent event, float afterMilliseconds);

	void ExitReady();

	bool ResetGame();
public:
	WCHAR _frameTextBuffer[20];
	//마지막으로 마우스 버튼을 클릭할 때의 마우스 커서의 위치이다.
	POINT _oldCursorPosition;
private:
	Timer _timer;
	CCamera _camera;
// ========== 서버 처리를 위해 사용하는 변수들 ==============
public:
	// 씬에 있는 오브젝트 관련 변수
	CPlayer* _players[4] = {};
	int16 _localPlayerIndex = -1;
	int16 _lastKeyInput = 0;

	// 발전기
	int _generatorCount = 3;
	CGenerator** _generators = nullptr;

public:
	int32 _activeGeneratorCount = 0;
public:
	Atomic<int32> _remainingPlayerCount = PLAYERNUM;
	Atomic<int32> _exitedPlayerCount = 0;
	bool _employeeExitReady = false;

public:
	Scheduler* _jobQueue;
public:
	XMFLOAT3 _clearPoints[3];
public:
	int32 _currentFrame;
public:
	bool _exitSoundActive = false;

	void ReleaseUploadBuffers() override;
	void ReleaseObjects() override;

private:
	void HandleGeneratorActivated(int32 index, bool notifyServer);
};

