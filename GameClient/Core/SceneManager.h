#pragma once
#include "../SceneId.h"

class CSound;
class CScene;
class UIManager;

namespace atb
{
class ClientNetworker;
class GameCore;
}

class SceneManager
{
public:
	static constexpr int32 SceneCount = atb::SceneIndex(atb::SceneId::Count);

	SceneManager() = default;
	~SceneManager();

	void Render(ID3D12GraphicsCommandList4* pd3dCommandList, atb::SceneId scene, bool);
	void Update(HWND& hWnd, atb::SceneId scene);
	void Animate(atb::SceneId scene);
	void ProcessInput(HWND& hWnd, atb::SceneId scene);


	void BuildScene(
		ID3D12Device5* pd3dDevice,
		ID3D12GraphicsCommandList4* pd3dCommandList,
		atb::GameCore& gameCore,
		atb::ClientNetworker& networker,
		UIManager& ui);
	void ReleaseUpBuffers();
	void ReleaseScene();

private:
	friend class atb::GameCore;
	CScene* GetScene(atb::SceneId scene) const noexcept;

	CScene* _pScenes[SceneCount]{};
};

