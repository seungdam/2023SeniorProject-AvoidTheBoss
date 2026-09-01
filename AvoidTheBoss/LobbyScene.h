#pragma once
#include "SceneInterface.h"
class LobbyScene : public SceneInterface
{
	ID3D12Device* _pd3dDevice;
public:
	LobbyScene(ID3D12Device* device) { _pd3dDevice = device; }
	~LobbyScene()  {}

	virtual void Render() override
	{
		if (_pd3dDevice == nullptr) return;

	}

};

