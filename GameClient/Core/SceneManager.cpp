#include "../Platform/pch.h"
#include "../SceneManager.h"
#include "../Scenes/CScene.h"
#include "../Scenes/GameScene.h"
#include "../Scenes/OtherScenes.h"
#include "../Audio/CSound.h"

SceneManager::~SceneManager()
{
	ReleaseScene();
}

void SceneManager::Render(
	ID3D12GraphicsCommandList4* pd3dCommandList, const atb::SceneId scene, const bool Raster)
{
	const int32 idx = atb::SceneIndex(scene);
	_pScenes[idx]->Render(pd3dCommandList, _pScenes[idx]->GetRenderCamera(), Raster);
}

void SceneManager::Update(HWND& hWnd, const atb::SceneId scene)
{
	_pScenes[atb::SceneIndex(scene)]->Update(hWnd);
}

void SceneManager::ProcessInput(HWND& hWnd, const atb::SceneId scene)
{
	_pScenes[atb::SceneIndex(scene)]->ProcessInput(hWnd);
}

CScene* SceneManager::GetScene(const atb::SceneId scene) const noexcept
{
	const int32 index = atb::SceneIndex(scene);
	return index >= 0 && index < SceneCount ? _pScenes[index] : nullptr;
}

void SceneManager::ReleaseUpBuffers()
{
	for (auto *scene : _pScenes)
	{
		if (scene)
		{
			scene->ReleaseUploadBuffers();
		}
	}
}
void SceneManager::ReleaseScene()
{
	for (auto& scene : _pScenes)
	{
		if (!scene)
		{
			continue;
		}
		scene->ReleaseObjects();
		delete scene;
		scene = nullptr;
	}
}
void SceneManager::Animate(const atb::SceneId scene)
{
	_pScenes[atb::SceneIndex(scene)]->AnimateObjects();
}

void SceneManager::BuildScene(
	ID3D12Device5* pd3dDevice,
	ID3D12GraphicsCommandList4* pd3dCommandList,
	atb::GameCore& gameCore,
	atb::ClientNetworker& networker,
	UIManager& ui)
{
	// Register each scene before BuildObjects(). If a later build step throws,
	// CGameFramework::OnDestroy() can still reclaim the partially built scene.
	// Known ceiling: objects created inside a recursive loader but never attached
	// to a registered scene still require a future asset-instance ownership split.
#if defined(_DEBUG)
	::OutputDebugStringA("[Phase 0] TITLE scene build\n");
#endif
	_pScenes[atb::SceneIndex(atb::SceneId::Title)] = new CTitleScene(gameCore, networker, ui);
	_pScenes[atb::SceneIndex(atb::SceneId::Title)]->BuildObjects(pd3dDevice, pd3dCommandList);
#if defined(_DEBUG)
	::OutputDebugStringA("[Phase 0] LOBBY scene build\n");
#endif
	_pScenes[atb::SceneIndex(atb::SceneId::Lobby)] = new CLobbyScene(gameCore, networker, ui);
	_pScenes[atb::SceneIndex(atb::SceneId::Lobby)]->BuildObjects(pd3dDevice, pd3dCommandList);
#if defined(_DEBUG)
	::OutputDebugStringA("[Phase 0] ROOM scene build\n");
#endif
	_pScenes[atb::SceneIndex(atb::SceneId::Room)] = new CRoomScene(gameCore, networker, ui);
	_pScenes[atb::SceneIndex(atb::SceneId::Room)]->BuildObjects(pd3dDevice, pd3dCommandList);
#if defined(_DEBUG)
	::OutputDebugStringA("[Phase 0] INGAME scene build\n");
#endif
	_pScenes[atb::SceneIndex(atb::SceneId::InGame)] = new CGameScene(gameCore, networker, ui);
	_pScenes[atb::SceneIndex(atb::SceneId::InGame)]->BuildObjects(pd3dDevice, pd3dCommandList);

#if defined(_DEBUG)
	::OutputDebugStringA("[Phase 0] RESULT scene build\n");
#endif
	_pScenes[atb::SceneIndex(atb::SceneId::Result)] = new CResultScene(gameCore, ui);
	_pScenes[atb::SceneIndex(atb::SceneId::Result)]->BuildObjects(pd3dDevice, pd3dCommandList);
	static_cast<CGameScene*>(_pScenes[atb::SceneIndex(atb::SceneId::InGame)])->SetResultScene(
		*static_cast<CResultScene*>(_pScenes[atb::SceneIndex(atb::SceneId::Result)]));
#if defined(_DEBUG)
	::OutputDebugStringA("[Phase 0] scene build complete\n");
#endif
}

