#pragma once

#include <functional>
#include <windows.h>

namespace atb
{
class Window final
{
public:
	using MessageHandler = std::function<LRESULT(HWND, UINT, WPARAM, LPARAM)>;

	Window() = default;
	~Window() noexcept;

	Window(const Window&) = delete;
	Window& operator=(const Window&) = delete;
	Window(Window&&) = delete;
	Window& operator=(Window&&) = delete;

	void Initialize(HINSTANCE instance, MessageHandler messageHandler);
	void Show(int showCommand) const;
	void Shutdown() noexcept;

	[[nodiscard]] bool ProcessMessages();
	[[nodiscard]] int ExitCode() const noexcept;
	[[nodiscard]] HWND Handle() const noexcept;

private:
	static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

	HINSTANCE _instance = nullptr;
	HWND _window = nullptr;
	HACCEL _accelerators = nullptr;
	ATOM _classAtom = 0;
	MessageHandler _messageHandler;
	int _exitCode = 0;
};
}
