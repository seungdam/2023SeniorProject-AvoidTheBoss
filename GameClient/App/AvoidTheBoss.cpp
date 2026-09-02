#include "../Platform/pch.h"

#include "../Diagnostics/ClientTestMode.h"
#include "GameFramework.h"
#include "SocketUtil.h"

#if defined(_DEBUG)
#include <crtdbg.h>
#endif

int APIENTRY wWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR commandLine,
	_In_ int showCommand)
{
#if defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	std::set_terminate([]
		{
			try
			{
			    if (const auto exception = std::current_exception())
			    {
				    std::rethrow_exception(exception);
			    }
		    }
			catch (const std::exception& error)
			{
				::OutputDebugStringA("[terminate] ");
				::OutputDebugStringA(error.what());
				::OutputDebugStringA("\n");
			}
			catch (...)
			{
				::OutputDebugStringA("[terminate] unknown exception\n");
			}
			std::abort();
		});
#endif

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(commandLine);
	::SetConsoleTitleW(L"Client");

	if (!g_clientTestMode.Configure())
	{
		::OutputDebugStringA("[E2E] Invalid camera test arguments\n");
		return 2;
	}
	const bool isClientTest = g_clientTestMode.Enabled();

	SocketUtil::Init();
	::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

	try
	{
		mainGame.OnCreate(hInstance, showCommand);
	}
	catch (const std::exception& error)
	{
		::OutputDebugStringA("[initialization] ");
		::OutputDebugStringA(error.what());
		::OutputDebugStringA("\n");
		mainGame.OnDestroy();
		SocketUtil::Clear();
		::CoUninitialize();
		return 1;
	}
	catch (...)
	{
		::OutputDebugStringA("[initialization] unknown exception\n");
		mainGame.OnDestroy();
		SocketUtil::Clear();
		::CoUninitialize();
		return 1;
	}

	while (mainGame.ProcessWindowMessages())
	{
		mainGame.FrameAdvance();
	}
	const int exitCode = mainGame.ExitCode();

	mainGame.OnDestroy();
	std::cout << "Quit Client\n";
	SocketUtil::Clear();
	::CoUninitialize();

	return isClientTest ? g_clientTestMode.FinalizeProcess() : exitCode;
}
