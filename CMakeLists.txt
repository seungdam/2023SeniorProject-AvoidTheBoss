cmake_minimum_required(VERSION 3.24)

project(AvoidTheBoss LANGUAGES CXX RC)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "This project requires Windows and the MSVC toolchain.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Only x64 builds are supported. Select the amd64/x64 toolchain.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

# MSVC can reject simultaneous linker PDB writes under Ninja (LNK1201).
# Compilation stays parallel; only the two final link steps are serialized.
set_property(GLOBAL PROPERTY JOB_POOLS msvc_link_pool=1)

add_compile_options(
    "$<$<COMPILE_LANGUAGE:CXX>:/W3>"
    "$<$<COMPILE_LANGUAGE:CXX>:/permissive->"
    "$<$<COMPILE_LANGUAGE:CXX>:/utf-8>"
    "$<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>"
    "$<$<COMPILE_LANGUAGE:CXX>:/sdl>"
)

add_compile_definitions(UNICODE _UNICODE)

add_library(CoreEngine STATIC
    CoreEngine/ASession.cpp
    CoreEngine/CoreGlobal.cpp
    CoreEngine/CoreMacro.cpp
    CoreEngine/CorePch.cpp
    CoreEngine/CoreTLS.cpp
    CoreEngine/CTimer.cpp
    CoreEngine/DeadLockFinder.cpp
    CoreEngine/IocpCore.cpp
    CoreEngine/IocpEvent.cpp
    CoreEngine/IocpObject.cpp
    CoreEngine/pch.cpp
    CoreEngine/protocol.cpp
    CoreEngine/RWLock.cpp
    CoreEngine/SocketUtil.cpp
    CoreEngine/ThreadManager.cpp
)

target_include_directories(CoreEngine PUBLIC CoreEngine)
target_compile_definitions(CoreEngine PRIVATE _LIB)
target_precompile_headers(CoreEngine PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/CoreEngine/pch.h>"
)
target_link_libraries(CoreEngine PUBLIC ws2_32 mswsock winmm)
set_target_properties(CoreEngine PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<CONFIG>"
)

add_executable(GameServer
    MainServer/AcceptManager.cpp
    MainServer/CGameManager.cpp
    MainServer/CollisionDetector.cpp
    MainServer/CSIocpCore.cpp
    MainServer/JobQueue.cpp
    MainServer/newMain.cpp
    MainServer/packetEvent.cpp
    MainServer/pch.cpp
    MainServer/RoomManager.cpp
    MainServer/Session.cpp
    MainServer/SGenerator.cpp
    MainServer/SPlayer.cpp
    MainServer/WorldRewinder.cpp
)

target_include_directories(GameServer PRIVATE MainServer)
target_compile_definitions(GameServer PRIVATE _CONSOLE)
target_precompile_headers(GameServer PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/MainServer/pch.h>"
)
target_link_libraries(GameServer PRIVATE CoreEngine)

# Legacy pch.h still names Debug/CoreEngine.lib or Release/CoreEngine.lib.
# CMake needs the parent search path until those legacy pragmas are removed.
target_link_directories(GameServer PRIVATE "${CMAKE_BINARY_DIR}/lib")

set_target_properties(GameServer PROPERTIES
    OUTPUT_NAME GAMESERVER
    JOB_POOL_LINK msvc_link_pool
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/server/$<CONFIG>"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/MainServer"
)

add_custom_command(TARGET GameServer POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/MainServer/bounding_boxes.txt"
        "${CMAKE_CURRENT_SOURCE_DIR}/MainServer/bounding_boxes2.txt"
        "$<TARGET_FILE_DIR:GameServer>"
    VERBATIM
)

set(client_dxr_sources
    AvoidTheBoss/DXRHelpers/nv_helpers_dx12/BottomLevelASGenerator.cpp
    AvoidTheBoss/DXRHelpers/nv_helpers_dx12/RaytracingPipelineGenerator.cpp
    AvoidTheBoss/DXRHelpers/nv_helpers_dx12/RootSignatureGenerator.cpp
    AvoidTheBoss/DXRHelpers/nv_helpers_dx12/ShaderBindingTableGenerator.cpp
    AvoidTheBoss/DXRHelpers/nv_helpers_dx12/TopLevelASGenerator.cpp
)

add_executable(GameClient WIN32
    AvoidTheBoss/AvoidTheBoss.cpp
    AvoidTheBoss/Camera.cpp
    AvoidTheBoss/CBoss.cpp
    AvoidTheBoss/CBullet.cpp
    AvoidTheBoss/CEmployee.cpp
    AvoidTheBoss/CGenerator.cpp
    AvoidTheBoss/clientIocpCore.cpp
    AvoidTheBoss/ClientPacketEvent.cpp
    AvoidTheBoss/ClientSession.cpp
    AvoidTheBoss/CollisionDetector.cpp
    AvoidTheBoss/CScene.cpp
    AvoidTheBoss/CSound.cpp
    AvoidTheBoss/DDSTextureLoader12.cpp
    AvoidTheBoss/DXSampleHelper.cpp
    AvoidTheBoss/GameFramework.cpp
    AvoidTheBoss/GameObject.cpp
    AvoidTheBoss/GameScene.cpp
    AvoidTheBoss/CJobQueue.cpp
    AvoidTheBoss/InputManager.cpp
    AvoidTheBoss/Mesh.cpp
    AvoidTheBoss/OtherScenes.cpp
    AvoidTheBoss/pch.cpp
    AvoidTheBoss/Player.cpp
    AvoidTheBoss/SceneManager.cpp
    AvoidTheBoss/Shader.cpp
    AvoidTheBoss/SoundManager.cpp
    AvoidTheBoss/UIManager.cpp
    AvoidTheBoss/AvoidTheBoss.rc
    AvoidTheBoss/Light.hlsl
    AvoidTheBoss/Shaders.hlsl
    ${client_dxr_sources}
)

set_source_files_properties(${client_dxr_sources} PROPERTIES
    SKIP_PRECOMPILE_HEADERS ON
)
set_source_files_properties(
    AvoidTheBoss/Light.hlsl
    AvoidTheBoss/Shaders.hlsl
    PROPERTIES HEADER_FILE_ONLY ON
)

target_include_directories(GameClient PRIVATE
    AvoidTheBoss
    AvoidTheBoss/DXRHelpers
    AvoidTheBoss/DXRHelpers/nv_helpers_dx12
)
target_include_directories(GameClient SYSTEM PRIVATE AvoidTheBoss/FMOD/inc)
target_compile_definitions(GameClient PRIVATE _WINDOWS)
target_precompile_headers(GameClient PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/AvoidTheBoss/pch.h>"
)
target_link_libraries(GameClient PRIVATE
    CoreEngine
    "${CMAKE_CURRENT_SOURCE_DIR}/AvoidTheBoss/FMOD/lib/fmod_vc.lib"
    d2d1
    d3d11
    d3d12
    d3dcompiler
    dwrite
    dxgi
    dxguid
    windowscodecs
)

# Compatibility paths for the two legacy #pragma comment(lib, ...) directives.
target_link_directories(GameClient PRIVATE
    "${CMAKE_BINARY_DIR}/lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/AvoidTheBoss"
)

set_target_properties(GameClient PROPERTIES
    OUTPUT_NAME AvoidTheBoss
    JOB_POOL_LINK msvc_link_pool
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/client/$<CONFIG>"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/AvoidTheBoss"
)

add_custom_command(TARGET GameClient POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/AvoidTheBoss/FMOD/lib/fmod.dll"
        "$<TARGET_FILE_DIR:GameClient>/fmod.dll"
    VERBATIM
)
