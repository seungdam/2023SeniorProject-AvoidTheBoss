#include "pch.h"
#include "Window.h"

#include "ClientConfig.h"
#include "resource.h"

#include <system_error>

namespace
{
constexpr int MaxLoadString = 100;

[[noreturn]] void ThrowError(const DWORD error, const char* operation)
{
	throw std::system_error(
		static_cast<int>(error),
		std::system_category(),
		operation);
}
}

namespace atb
{
Window::~Window() noexcept
{
	Shutdown();
}

void Window::Initialize(HINSTANCE instance, MessageHandler messageHandler)
{
	if (_window || _classAtom) throw std::logic_error("Window is already initialized");
	if (!instance || !messageHandler) throw std::invalid_argument("Window requires an instance and message handler");

	auto pendingMessageHandler = std::move(messageHandler);
	_instance = instance;
	_exitCode = 0;

	wchar_t title[MaxLoadString]{};
	wchar_t className[MaxLoadString]{};
	if (!::LoadStringW(instance, IDS_APP_TITLE, title, MaxLoadString) ||
		!::LoadStringW(instance, IDC_AVOIDTHEBOSS, className, MaxLoadString))
	{
		const DWORD error = ::GetLastError();
		Shutdown();
		ThrowError(error ? error : ERROR_RESOURCE_NAME_NOT_FOUND, "LoadStringW");
	}

	WNDCLASSEXW windowClass{};
	windowClass.cbSize = sizeof(windowClass);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = WindowProcedure;
	windowClass.hInstance = instance;
	windowClass.hIcon = ::LoadIconW(instance, MAKEINTRESOURCEW(IDI_AVOIDTHEBOSS));
	windowClass.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
	windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	windowClass.lpszClassName = className;
	windowClass.hIconSm = ::LoadIconW(instance, MAKEINTRESOURCEW(IDI_SMALL));

	_classAtom = ::RegisterClassExW(&windowClass);
	if (!_classAtom)
	{
		const DWORD error = ::GetLastError();
		Shutdown();
		ThrowError(error, "RegisterClassExW");
	}

	// Preserve the original borderless result without changing style during WM_CREATE.
	constexpr DWORD style = WS_MINIMIZEBOX | WS_SYSMENU;
	RECT rectangle{ 0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT };
	if (!::AdjustWindowRect(&rectangle, style, FALSE))
	{
		const DWORD error = ::GetLastError();
		Shutdown();
		ThrowError(error, "AdjustWindowRect");
	}

	_window = ::CreateWindowExW(
		0,
		MAKEINTATOM(_classAtom),
		title,
		style,
		0,
		0,
		rectangle.right - rectangle.left,
		rectangle.bottom - rectangle.top,
		nullptr,
		nullptr,
		instance,
		this);
	if (!_window)
	{
		const DWORD error = ::GetLastError();
		Shutdown();
		ThrowError(error, "CreateWindowExW");
	}

	_accelerators = ::LoadAcceleratorsW(instance, MAKEINTRESOURCEW(IDC_AVOIDTHEBOSS));
	_messageHandler = std::move(pendingMessageHandler);
}

void Window::Show(const int showCommand) const
{
	if (!_window) throw std::logic_error("Window is not initialized");
	::ShowWindow(_window, showCommand);
	::UpdateWindow(_window);
}

void Window::Shutdown() noexcept
{
	if (_window && ::IsWindow(_window)) ::DestroyWindow(_window);
	_window = nullptr;
	_accelerators = nullptr;
	_messageHandler = {};

	if (_classAtom && _instance)
		::UnregisterClassW(MAKEINTATOM(_classAtom), _instance);
	_classAtom = 0;
	_instance = nullptr;
}

bool Window::ProcessMessages()
{
	MSG message{};
	while (::PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
	{
		if (message.message == WM_QUIT)
		{
			_exitCode = static_cast<int>(message.wParam);
			return false;
		}

		if (!_accelerators || !::TranslateAcceleratorW(message.hwnd, _accelerators, &message))
		{
			::TranslateMessage(&message);
			::DispatchMessageW(&message);
		}
	}
	return true;
}

int Window::ExitCode() const noexcept
{
	return _exitCode;
}

HWND Window::Handle() const noexcept
{
	return _window;
}

LRESULT CALLBACK Window::WindowProcedure(
	HWND window, const UINT message, const WPARAM wParam, const LPARAM lParam)
{
	Window* self = nullptr;
	if (message == WM_NCCREATE)
	{
		const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
		self = static_cast<Window*>(create->lpCreateParams);
		self->_window = window;
		::SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
	}
	else
	{
		self = reinterpret_cast<Window*>(::GetWindowLongPtrW(window, GWLP_USERDATA));
	}

	if (!self) return ::DefWindowProcW(window, message, wParam, lParam);
	const LRESULT result = self->HandleMessage(window, message, wParam, lParam);
	if (message == WM_NCDESTROY)
	{
		::SetWindowLongPtrW(window, GWLP_USERDATA, 0);
		self->_window = nullptr;
	}
	return result;
}

LRESULT Window::HandleMessage(
	HWND window, const UINT message, const WPARAM wParam, const LPARAM lParam)
{
	switch (message)
	{
	case WM_SIZE:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MOUSEMOVE:
	case WM_KEYDOWN:
	case WM_KEYUP:
		return _messageHandler ? _messageHandler(window, message, wParam, lParam) : 0;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDM_EXIT)
		{
			::DestroyWindow(window);
			return 0;
		}
		break;

	case WM_PAINT:
	{
		PAINTSTRUCT paint{};
		::BeginPaint(window, &paint);
		::EndPaint(window, &paint);
		return 0;
	}

	case WM_DESTROY:
		::PostQuitMessage(0);
		return 0;

	default:
		break;
	}

	return ::DefWindowProcW(window, message, wParam, lParam);
}
}
