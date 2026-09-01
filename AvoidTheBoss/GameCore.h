#pragma once

#include "../Shared/Types.h"
#include "SceneId.h"

#include <atomic>
#include <memory>
#include <windows.h>

struct ID3D12Device5;
struct ID3D12GraphicsCommandList4;

class CScene;
class SceneManager;
class UIManager;
struct ClientFrameSnapshot;

namespace atb
{
class ClientNetworker;
class ClientPacketDispatcher;

class GameCore final
{
public:
	GameCore() noexcept;
	~GameCore() noexcept;

	GameCore(const GameCore&) = delete;
	GameCore& operator=(const GameCore&) = delete;
	GameCore(GameCore&&) = delete;
	GameCore& operator=(GameCore&&) = delete;

	void Initialize(
		ID3D12Device5* device,
		ID3D12GraphicsCommandList4* commandList,
		ClientNetworker& networker,
		UIManager& ui);
	void Shutdown() noexcept;
	void ReleaseUploadBuffers();

	void ProcessInput(HWND window);
	void Update(HWND window);
	void Animate();
	void Render(ID3D12GraphicsCommandList4* commandList, bool raster);
	void ProcessMouseMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
	void ProcessKeyboardMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

	[[nodiscard]] SceneId CurrentScene() const noexcept;
	void ChangeScene(SceneId scene);

	[[nodiscard]] bool TogglePacketDelay() noexcept;
	[[nodiscard]] float PacketEventDelayMs() const noexcept;
	[[nodiscard]] int32 CurrentWorldFrame() const noexcept;
	[[nodiscard]] ClientFrameSnapshot CaptureClientFrameSnapshot(int32 playerIndex) const;
	[[nodiscard]] int32 CurrentLocalPlayerIndex() const noexcept;
	[[nodiscard]] bool IsInitialized() const noexcept;

private:
	friend class ClientPacketDispatcher;
	[[nodiscard]] CScene* Scene(SceneId scene) const noexcept;

	std::unique_ptr<SceneManager> _sceneManager;
	std::atomic<SceneId> _currentScene{ SceneId::Title };
	bool _packetDelayEnabled = false;
};
}
