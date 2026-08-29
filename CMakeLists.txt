cmake_minimum_required(VERSION 3.26)

project(AvoidTheBoss LANGUAGES CXX RC)

include(CTest)

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

add_library(SharedHeaders INTERFACE)
target_include_directories(SharedHeaders INTERFACE Shared)

add_library(SharedRuntime STATIC
    Shared/Runtime/CoreTLS.cpp
    Shared/Runtime/GameTimer.cpp
    Shared/Runtime/IocpCore.cpp
    Shared/Runtime/IocpEvent.cpp
    Shared/Runtime/SocketUtil.cpp
    Shared/Runtime/ThreadManager.cpp
)

target_include_directories(SharedRuntime PUBLIC Shared/Runtime)
target_precompile_headers(SharedRuntime PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/Shared/Runtime/pch.h>"
)
target_link_libraries(SharedRuntime PUBLIC SharedHeaders ws2_32)
set_target_properties(SharedRuntime PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<CONFIG>"
)

add_library(CoreEngine STATIC
    MainServer/AcceptManager.cpp
    MainServer/CGameManager.cpp
    MainServer/CollisionDetector.cpp
    MainServer/CSIocpCore.cpp
    MainServer/JobQueue.cpp
    MainServer/packetEvent.cpp
    MainServer/RoomManager.cpp
    MainServer/Session.cpp
    MainServer/SGenerator.cpp
    MainServer/SPlayer.cpp
    MainServer/WorldRewinder.cpp
)

target_include_directories(CoreEngine PUBLIC MainServer)
target_compile_definitions(CoreEngine PRIVATE _LIB)
target_precompile_headers(CoreEngine PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/MainServer/pch.h>"
)
target_link_libraries(CoreEngine PUBLIC SharedRuntime)
set_target_properties(CoreEngine PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<CONFIG>"
)

add_executable(GameServer
    MainServer/newMain.cpp
)

target_include_directories(GameServer PRIVATE MainServer)
target_compile_definitions(GameServer PRIVATE _CONSOLE)
target_precompile_headers(GameServer PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/MainServer/pch.h>"
)
target_link_libraries(GameServer PRIVATE CoreEngine)

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
    AvoidTheBoss/ClientTestMode.cpp
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
	AvoidTheBoss/TracerVisualState.cpp
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
target_compile_definitions(GameClient PRIVATE _WINDOWS)
target_precompile_headers(GameClient PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/AvoidTheBoss/pch.h>"
)
target_link_libraries(GameClient PRIVATE
    SharedRuntime
    d2d1
    d3d11
    d3d12
    d3dcompiler
    dwrite
    dxgi
    dxguid
    shell32
    windowscodecs
)

target_link_options(GameClient PRIVATE
    "$<$<CONFIG:Debug>:/ENTRY:wWinMainCRTStartup>"
    "$<$<CONFIG:Debug>:/SUBSYSTEM:CONSOLE>"
)

set_target_properties(GameClient PROPERTIES
    OUTPUT_NAME AvoidTheBoss
    JOB_POOL_LINK msvc_link_pool
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/client/$<CONFIG>"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/AvoidTheBoss"
)

set(client_asset_output "$<TARGET_FILE_DIR:GameClient>/Assets")
# ponytail: scans assets on each GameClient build; add per-file dependencies only if this becomes measurable.
add_custom_target(GameClientAssets
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${client_asset_output}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/AvoidTheBoss/Shaders.hlsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/AvoidTheBoss/Light.hlsl"
        "${client_asset_output}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/AvoidTheBoss/Model"
        "${client_asset_output}/Model"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/AvoidTheBoss/Skybox"
        "${client_asset_output}/Skybox"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/AvoidTheBoss/UI"
        "${client_asset_output}/UI"
    VERBATIM
)
add_dependencies(GameClient GameClientAssets)

add_executable(ProtocolScenarioHarness
    tests/ProtocolScenarioHarness.cpp
)
target_link_libraries(ProtocolScenarioHarness PRIVATE ws2_32)
target_link_libraries(ProtocolScenarioHarness PRIVATE SharedHeaders)
set_target_properties(ProtocolScenarioHarness PROPERTIES
    JOB_POOL_LINK msvc_link_pool
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>"
)

add_executable(ProjectileLogicTests
	AvoidTheBoss/TracerVisualState.cpp
	tests/ProjectileLogicTests.cpp
)
target_include_directories(ProjectileLogicTests PRIVATE AvoidTheBoss)
set_target_properties(ProjectileLogicTests PROPERTIES
	JOB_POOL_LINK msvc_link_pool
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>"
)

add_executable(GameObjectOwnershipTests
	tests/GameObjectOwnershipTests.cpp
)
target_include_directories(GameObjectOwnershipTests PRIVATE AvoidTheBoss)
set_target_properties(GameObjectOwnershipTests PROPERTIES
	JOB_POOL_LINK msvc_link_pool
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>"
)

add_executable(PlayerStateTests
	tests/PlayerStateTests.cpp
)
target_include_directories(PlayerStateTests PRIVATE AvoidTheBoss)
set_target_properties(PlayerStateTests PROPERTIES
	JOB_POOL_LINK msvc_link_pool
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>"
)

add_executable(SchedulerTests
	AvoidTheBoss/CJobQueue.cpp
	tests/SchedulerTests.cpp
)
target_include_directories(SchedulerTests PRIVATE AvoidTheBoss)
target_link_libraries(SchedulerTests PRIVATE SharedHeaders)
set_target_properties(SchedulerTests PROPERTIES
	JOB_POOL_LINK msvc_link_pool
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>"
)

if(BUILD_TESTING)
	add_test(NAME ProjectileLogicTests COMMAND ProjectileLogicTests)
	add_test(NAME GameObjectOwnershipTests COMMAND GameObjectOwnershipTests)
	add_test(NAME PlayerStateTests COMMAND PlayerStateTests)
	add_test(NAME SchedulerTests COMMAND SchedulerTests)
	set_tests_properties(
		ProjectileLogicTests
		GameObjectOwnershipTests
		PlayerStateTests
		SchedulerTests
		PROPERTIES LABELS unit
	)
endif()

add_custom_target(CameraLifecycleE2E
    COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
        -File "${CMAKE_CURRENT_SOURCE_DIR}/tools/run-camera-lifecycle-e2e.ps1"
        -ServerExe "$<TARGET_FILE:GameServer>"
        -ClientExe "$<TARGET_FILE:GameClient>"
        -HarnessExe "$<TARGET_FILE:ProtocolScenarioHarness>"
        -OutputRoot "${CMAKE_BINARY_DIR}/e2e"
        -TimeoutSeconds 180
    DEPENDS GameServer GameClient ProtocolScenarioHarness
    USES_TERMINAL
    VERBATIM
)

if(BUILD_TESTING AND CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_test(NAME CameraLifecycleE2E
		COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
			-File "${CMAKE_CURRENT_SOURCE_DIR}/tools/run-camera-lifecycle-e2e.ps1"
			-ServerExe "$<TARGET_FILE:GameServer>"
			-ClientExe "$<TARGET_FILE:GameClient>"
			-HarnessExe "$<TARGET_FILE:ProtocolScenarioHarness>"
			-OutputRoot "${CMAKE_BINARY_DIR}/e2e"
			-TimeoutSeconds 180
	)
	set_tests_properties(CameraLifecycleE2E PROPERTIES
		LABELS integration
		RUN_SERIAL TRUE
		TIMEOUT 220
	)
endif()
