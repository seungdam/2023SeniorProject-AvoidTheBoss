#include "pch.h"
#include "GameCore.h"

#include "CEmployee.h"
#include "ClientNetworker.h"
#include "ClientTestMode.h"
#include "GameScene.h"
#include "SceneManager.h"
#include "SoundManager.h"

namespace atb
{
GameCore::GameCore() noexcept = default;

GameCore::~GameCore() noexcept
{
	Shutdown();
}

void GameCore::Initialize(
	ID3D12Device5* device,
	ID3D12GraphicsCommandList4* commandList,
	ClientNetworker& networker,
	UIManager& ui)
{
	if (_sceneManager) throw std::logic_error("GameCore is already initialized");
	if (!device || !commandList) throw std::invalid_argument("GameCore requires a device and command list");

	auto sceneManager = std::make_unique<SceneManager>();
	sceneManager->BuildScene(device, commandList, *this, networker, ui);
	_sceneManager = std::move(sceneManager);
	_currentScene.store(SceneId::Title, std::memory_order_release);
	_packetDelayEnabled = false;
}

void GameCore::Shutdown() noexcept
{
	_sceneManager.reset();
	_currentScene.store(SceneId::Title, std::memory_order_release);
	_packetDelayEnabled = false;
}

void GameCore::ReleaseUploadBuffers()
{
	if (_sceneManager) _sceneManager->ReleaseUpBuffers();
}

void GameCore::ProcessInput(HWND window)
{
	if (_sceneManager) _sceneManager->ProcessInput(window, CurrentScene());
}

void GameCore::Update(HWND window)
{
	if (_sceneManager) _sceneManager->Update(window, CurrentScene());
}

void GameCore::Animate()
{
	if (_sceneManager) _sceneManager->Animate(CurrentScene());
}

void GameCore::Render(ID3D12GraphicsCommandList4* commandList, const bool raster)
{
	if (_sceneManager) _sceneManager->Render(commandList, CurrentScene(), raster);
}

void GameCore::ProcessMouseMessage(
	HWND window, const UINT message, const WPARAM wParam, const LPARAM lParam)
{
	if (CScene* scene = Scene(CurrentScene()))
		scene->OnProcessingMouseMessage(window, message, wParam, lParam);
}

void GameCore::ProcessKeyboardMessage(
	HWND window, const UINT message, const WPARAM wParam, const LPARAM lParam)
{
	if (CScene* scene = Scene(CurrentScene()))
		scene->OnProcessingKeyboardMessage(window, message, wParam, lParam);
}

CScene* GameCore::Scene(const SceneId scene) const noexcept
{
	return _sceneManager ? _sceneManager->GetScene(scene) : nullptr;
}

SceneId GameCore::CurrentScene() const noexcept
{
	return _currentScene.load(std::memory_order_acquire);
}

void GameCore::ChangeScene(const SceneId scene)
{
	const int32 sceneIndex = SceneIndex(scene);
	if (sceneIndex < 0 || sceneIndex >= SceneManager::SceneCount)
		throw std::out_of_range("GameCore scene index is out of range");
	_currentScene.store(scene, std::memory_order_release);
	SoundManager::GetInstance().PlayBackGroundSound(sceneIndex);
}

bool GameCore::TogglePacketDelay() noexcept
{
	_packetDelayEnabled = !_packetDelayEnabled;
	return _packetDelayEnabled;
}

float GameCore::PacketEventDelayMs() const noexcept
{
	return _packetDelayEnabled ? 320.0f : 0.0f;
}

int32 GameCore::CurrentWorldFrame() const noexcept
{
	const auto* gameScene = static_cast<const CGameScene*>(
		Scene(SceneId::InGame));
	return gameScene ? gameScene->CurrentWorldFrame() : 0;
}

ClientFrameSnapshot GameCore::CaptureClientFrameSnapshot(const int32 playerIndex) const
{
	ClientFrameSnapshot snapshot;
	snapshot._scene = SceneIndex(CurrentScene());
	auto* gameScene = static_cast<CGameScene*>(Scene(SceneId::InGame));
	CPlayer* player = gameScene ? gameScene->GetScenePlayerByIdx(playerIndex) : nullptr;
	CCamera* camera = gameScene ? gameScene->GetCameraComponent() : nullptr;
	if (player)
	{
		snapshot._health = player->GetHealth();
		snapshot._behavior = player->GetBehavior();
		snapshot._hidden = player->IsHidden();
		if (playerIndex > 0)
			snapshot._rescuing = static_cast<CEmployee*>(player)->GetRescueOn();
	}
	if (camera)
	{
		snapshot._cameraMode = static_cast<int>(camera->GetMode());
		snapshot._cameraIdentity = reinterpret_cast<std::uintptr_t>(camera);
		snapshot._cameraResourcesValid = camera->HasShaderVariables();
		snapshot._cameraBufferAddress = camera->GetBufferAddress();
		snapshot._cameraBufferCreateCount = camera->GetBufferCreateCount();
	}
	if (gameScene)
	{
		CPlayer* localPlayer = gameScene->GetLocalPlayer();
		snapshot._renderCameraStateValid =
			gameScene->GetRenderCamera() == (localPlayer ? camera : nullptr);
		snapshot._localPlayerMatchesDut =
			localPlayer == player && gameScene->GetLocalPlayerIndex() == playerIndex;
		snapshot._cameraViewerMatchesLocal =
			camera && camera->GetViewerIndex() == gameScene->GetLocalPlayerIndex();
	}
	return snapshot;
}

int32 GameCore::CurrentLocalPlayerIndex() const noexcept
{
	if (CurrentScene() != SceneId::InGame) return -1;
	const auto* gameScene = static_cast<const CGameScene*>(Scene(SceneId::InGame));
	return gameScene ? gameScene->GetLocalPlayerIndex() : -1;
}

bool GameCore::IsInitialized() const noexcept
{
	return _sceneManager != nullptr;
}
}
