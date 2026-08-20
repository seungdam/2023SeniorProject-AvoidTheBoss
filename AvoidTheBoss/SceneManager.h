#pragma once
class CSound;
class CScene;

class SceneManager
{
public:
	enum class SCENESTATE { TITLE = 0, LOBBY = 1, ROOM = 2, INGAME = 3, RESULT = 4 };
	static constexpr int32 SceneCount = 5;
protected:

	CScene* m_pScenes[SceneCount];
public:
	SceneManager()
	{
		for (int i = 0; i < SceneCount; ++i) m_pScenes[i] = nullptr;
	}
	~SceneManager();

	void Render(ID3D12GraphicsCommandList4* pd3dCommandList, int32, bool);
	void Update(HWND& hWnd, int32);
	void Animate(int32);
	void ProcessInput(HWND& hWnd, int32);


	void BuildScene(ID3D12Device5* pd3dDevice, ID3D12GraphicsCommandList4* pd3dCommandList);
	void ReleaseUpBuffers();
	void ReleaseScene();


	void ResetScene();
	CScene* ChangeScene(int32 idx);
	CScene* GetSceneByIdx(int32 idx)
	{
		assert(idx >= 0 && idx < SceneCount);
		return (idx >= 0 && idx < SceneCount) ? m_pScenes[idx] : nullptr;
	}
};

