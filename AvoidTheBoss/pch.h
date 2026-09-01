// header.h: 표준 시스템 포함 파일
// 또는 프로젝트 특정 포함 파일이 들어 있는 포함 파일입니다.
//

#pragma once

#include "../Shared/GameCommon.h"
#include "../Shared/Protocol.h"
#include "../Shared/Runtime/CoreMacro.h"
#include "../Shared/Runtime/GameTimer.h"
#include "../Shared/Runtime/NetworkPlatform.h"
#include "ClientConfig.h"
#include "D3D12Helpers.h"
#include "d3dx12.h"

#include <d2d1_3.h>
#include <d3d11.h>
#include <d3d11on12.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dwrite.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <shellapi.h>
#include <wrl.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iosfwd>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <queue>
#include <set>
#include <shared_mutex>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <thread>
#include <tchar.h>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

using Microsoft::WRL::ComPtr;


