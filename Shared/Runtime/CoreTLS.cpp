#include "pch.h"
#include "CoreTLS.h" // thread-local worker identity

thread_local uint32 lThreadId = 0;
