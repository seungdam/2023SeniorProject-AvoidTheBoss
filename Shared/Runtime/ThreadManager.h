#pragma once
#include "../Types.h"

#include <functional>
#include <thread>
#include <vector>

class ThreadManager
{
public:
	ThreadManager() { InitTLS(); }
	~ThreadManager() { Join(); }


	void Launch(std::function<void(void)> callback);
	void Join();
	//TLS init
	static void InitTLS();
	static void DestroyTLS();
private:
	Mutex _m;
	std::vector<std::thread> _threads;
};



