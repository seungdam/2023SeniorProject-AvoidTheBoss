#include "IocpCore.h"

#include <array>
#include <iostream>
#include <latch>
#include <thread>

namespace
{
	bool Check(const bool condition, const char* message)
	{
		if (condition) return true;
		std::cerr << message << '\n';
		return false;
	}

	int Run()
	{
		IocpCore iocpCore;
		if (!Check(!iocpCore.Processing(0), "empty IOCP did not preserve the false timeout result"))
			return 1;

		constexpr std::size_t WorkerCount = 4;
		std::latch ready(WorkerCount);
		std::array<bool, WorkerCount> woke{};
		std::array<std::thread, WorkerCount> workers;

		for (std::size_t i = 0; i < WorkerCount; ++i)
		{
			workers[i] = std::thread([&iocpCore, &ready, &woke, i]()
				{
					ready.count_down();
					woke[i] = !iocpCore.Processing();
				});
		}

		ready.wait();
		bool passed = true;
		for (std::size_t i = 0; i < WorkerCount; ++i)
			passed &= Check(iocpCore.PostWakeup(), "failed to post an IOCP wakeup packet");
		for (auto& worker : workers) worker.join();
		for (const bool didWake : woke)
			passed &= Check(didWake, "an IOCP worker did not receive a wakeup result");

		return passed ? 0 : 1;
	}
}

int main()
{
	return Run();
}
