#include "../Platform/pch.h"
#include "../InputManager.h"


void InputManager::InputStatusUpdate()
{
	for (int i = 65; i < 91; ++i)
	{
		Update(i);
	}

	for (int i = 0; i < 10; ++i)
	{
		Update((int32)KEY_TYPE::NUM0 + i);
	}

	Update(VK_BACK);
	Update(VK_SPACE);
	Update(VK_TAB);
	Update(VK_CAPITAL);
}

void InputManager::MouseInputStatusUpdate()
{
	if (::GetCapture())
	{
		Update(0x01);
	}
}

void InputManager::SetKeyPress(int32 key)
{
	if (_keyBuffer[key] <= 0 )
	{

		_keyBuffer[key] = (int8)KEY_STATUS::KEY_PRESS;
	}
	else if(_keyBuffer[key] == (int8)KEY_STATUS::KEY_PRESS)
	{

		_keyBuffer[key] = (int8)KEY_STATUS::KEY_DOWN;
	}
}

void InputManager::SetKeyUp(int32 key)
{
	if (_keyBuffer[key] > 0)
	{
		_keyBuffer[key] = (int8)KEY_STATUS::KEY_UP;
	}
	else if(_keyBuffer[key] == (int8)KEY_STATUS::KEY_UP)
	{
		_keyBuffer[key] = (int8)KEY_STATUS::KEY_NONE;
	}
}

void InputManager::Update(int32 key)
{

	if (::GetAsyncKeyState(key) & 0x8000) // 키를 이전부터 누르고 있었던 경우
	{
		SetKeyPress(key);
	}
	else
	{
		SetKeyUp(key);
	}

}
