#include "pch.h"
#include "Camera.h"
#include "ClientTestMode.h"

#include "ClientSession.h"
#include "SceneId.h"

ClientTestMode g_clientTestMode;

namespace
{
	bool ParseInteger(const std::wstring& value, long& result)
	{
		wchar_t* end = nullptr;
		result = std::wcstol(value.c_str(), &end, 10);
		return end != value.c_str() && *end == L'\0';
	}
}

bool ClientTestMode::Configure()
{
#if !defined(_DEBUG)
	if (std::wstring_view(::GetCommandLineW()).find(L"--e2e-camera") != std::wstring_view::npos)
	{
		::OutputDebugStringA("[E2E] Camera lifecycle test requires a Debug client build\n");
		return false;
	}
	return true;
#else
	int argumentCount = 0;
	wchar_t** arguments = ::CommandLineToArgvW(::GetCommandLineW(), &argumentCount);
	if (!arguments)
	{
		return false;
	}

	for (int i = 1; i < argumentCount; ++i)
	{
		const std::wstring argument = arguments[i];
		if (argument == L"--e2e-camera")
		{
			_enabled = true;
		}
		else if (argument.starts_with(L"--room="))
		{
			long room = -1;
			if (!ParseInteger(argument.substr(7), room) || room < 0 || room > INT_MAX)
			{
				::LocalFree(arguments);
				return false;
			}
			_roomNumber = static_cast<int>(room);
		}
		else if (argument.starts_with(L"--telemetry="))
		{
			_telemetryPath = argument.substr(12);
		}
		else if (argument.starts_with(L"--timeout-seconds="))
		{
			long seconds = 0;
			if (!ParseInteger(argument.substr(18), seconds) || seconds < 1 || seconds > 3'600)
			{
				::LocalFree(arguments);
				return false;
			}
			_timeoutMs = static_cast<std::uint64_t>(seconds) * 1'000;
		}
	}
	::LocalFree(arguments);

	if (!_enabled)
	{
		return true;
	}
	if (_roomNumber < 0 || _telemetryPath.empty())
	{
		return false;
	}

	std::error_code error;
	if (const auto parent = _telemetryPath.parent_path(); !parent.empty())
	{
		std::filesystem::create_directories(parent, error);
	}
	if (error)
	{
		return false;
	}

	_log.open(_telemetryPath, std::ios::out | std::ios::trunc);
	if (!_log)
	{
		return false;
	}

	_startedAtMs = ::GetTickCount64();
	std::lock_guard lock(_mutex);
	LogLocked("configured", "room=" + std::to_string(_roomNumber));
	return true;
#endif
}

void ClientTestMode::OnConnected(ClientSession& session)
{
	if (!_enabled)
	{
		return;
	}
	{
		std::lock_guard lock(_mutex);
		_session = &session;
	}

	C2S_LOGIN packet{};
	packet.size = sizeof(packet);
	packet.type = static_cast<uint8>(C_TITLE_PACKET_TYPE::ACQ_LOGIN);
	wcscpy_s(packet.name, L"e2e_dut");
	wcscpy_s(packet.pw, L"e2e_dut");
	if (!session.DoSend(&packet))
	{
		Fail("failed to send login");
	}
}

void ClientTestMode::OnLoginOk(ClientSession& session)
{
	if (!_enabled)
	{
		return;
	}
	{
		std::lock_guard lock(_mutex);
		if (_roomRequested)
		{
			return;
		}
		_roomRequested = true;
		LogLocked("login_ok", "requesting room=" + std::to_string(_roomNumber));
	}

	C2S_ROOM_ENTER packet{};
	packet.size = sizeof(packet);
	packet.type = static_cast<uint8>(C_ROOM_PACKET_TYPE::ACQ_ENTER_RM);
	packet.rmNum = _roomNumber;
	if (!session.DoSend(&packet))
	{
		Fail("failed to send room enter");
	}
}

void ClientTestMode::OnRoomEntered(ClientSession& session, const int roomNumber)
{
	if (!_enabled)
	{
		return;
	}
	int match = 0;
	{
		std::lock_guard lock(_mutex);
		if (roomNumber != _roomNumber)
		{
			FailLocked("entered unexpected room=" + std::to_string(roomNumber));
			return;
		}
		if (_stage == Stage::AwaitGame)
		{
			if (_readySent)
			{
				return;
			}
			_readySent = true;
			match = 1;
		}
		else if (_stage == Stage::AwaitSecondRoom)
		{
			if (_secondReadySent)
			{
				return;
			}
			_secondReadySent = true;
			_stage = Stage::AwaitSecondGame;
			match = 2;
		}
		else
		{
			FailLocked("room entry arrived outside an expected match setup");
			return;
		}
		LogLocked("room_entered", "match=" + std::to_string(match) + " room=" + std::to_string(roomNumber));
	}

	C2S_ROOM_EVENT packet{};
	packet.size = sizeof(packet);
	packet.type = static_cast<uint8>(C_ROOM_PACKET_TYPE::ACQ_READY);
	if (!session.DoSend(&packet))
	{
		Fail("failed to send ready");
	}
}

void ClientTestMode::OnProtocolFailure(const char* reason)
{
	if (_enabled)
	{
		Fail(reason);
	}
}

bool ClientTestMode::ValidateGameStart(const std::int16_t* sids, const std::int32_t ownSid)
{
	if (!_enabled)
	{
		return true;
	}
	if (!sids)
	{
		Fail("GAME_START has no roster");
		return false;
	}

	int ownIndex = -1;
	for (int i = 0; i < PLAYERNUM; ++i)
	{
		if (sids[i] < 0)
		{
			Fail("GAME_START contains an invalid sid");
			return false;
		}
		for (int j = 0; j < i; ++j)
		{
			if (sids[i] == sids[j])
			{
				Fail("GAME_START contains duplicate sids");
				return false;
			}
		}
		if (sids[i] == ownSid)
		{
			ownIndex = i;
		}
	}

	std::lock_guard lock(_mutex);
	if (_stage != Stage::AwaitGame && _stage != Stage::AwaitSecondGame)
	{
		FailLocked("GAME_START arrived outside an expected match setup");
		return false;
	}
	if (ownIndex != 1)
	{
		FailLocked("DUT did not receive employee slot 1");
		return false;
	}
	_dutPlayerIndex = ownIndex;
	_rosterCount = PLAYERNUM;
	_ownSidPresent = true;
	return true;
}

void ClientTestMode::OnGameStarted()
{
	if (!_enabled)
	{
		return;
	}
	std::lock_guard lock(_mutex);
	if (_stage == Stage::AwaitGame)
	{
		_stage = Stage::AwaitInitialFirst;
	}
	else if (_stage == Stage::AwaitSecondGame)
	{
		_stage = Stage::AwaitSecondInitialFirst;
	}
	else
	{
		FailLocked("duplicate or out-of-order GAME_START");
		return;
	}
	++_gameStartCount;
	_gameStarted = true;
	LogLocked("game_start", "match=" + std::to_string(_gameStartCount) + " dut_slot=1");
}

int ClientTestMode::DutPlayerIndex() const
{
	std::lock_guard lock(_mutex);
	return _dutPlayerIndex;
}

bool ClientTestMode::Pump(const ClientFrameSnapshot& snapshot)
{
	if (!_enabled)
	{
		return false;
	}
	std::lock_guard lock(_mutex);
	_lastScene = snapshot._scene;

	if (_stage != Stage::CandidatePass && _stage != Stage::Failed &&
		::GetTickCount64() - _startedAtMs > _timeoutMs) {
		FailLocked("client scenario timeout");
}

	const int firstPerson = static_cast<int>(CCamera::FirstPersonMode);
	const int thirdPerson = static_cast<int>(CCamera::ThirdPersonMode);
	const int inGame = atb::SceneIndex(atb::SceneId::InGame);
	const int result = atb::SceneIndex(atb::SceneId::Result);
	const int lobby = atb::SceneIndex(atb::SceneId::Lobby);
	const bool validatingFirstMatch = _stage >= Stage::AwaitInitialFirst && _stage <= Stage::AwaitResult;
	const bool validatingSecondMatch = _stage == Stage::AwaitSecondInitialFirst ||
		_stage == Stage::AwaitSecondPresent;
	if (validatingFirstMatch || validatingSecondMatch)
	{
		if (snapshot._cameraIdentity == 0)
		{
			FailLocked("game scene camera identity is unavailable");
		}
		else if (_cameraIdentity != 0 && snapshot._cameraIdentity != _cameraIdentity)
		{
			FailLocked("game scene camera object changed during a mode transition");
		}
		else if (snapshot._cameraBufferAddress == 0)
		{
			FailLocked("game scene camera buffer is unavailable");
		}
		else if (_cameraBufferAddress != 0 && snapshot._cameraBufferAddress != _cameraBufferAddress)
		{
			FailLocked("game scene camera buffer changed during a mode transition");
		}
		else if (snapshot._cameraBufferCreateCount != 1)
		{
			FailLocked("game scene camera buffer was not created exactly once");
		}
		else if (!snapshot._cameraResourcesValid)
		{
			FailLocked("game scene camera shader variables are unavailable");
		}
		else if (snapshot._scene == inGame && (!snapshot._renderCameraStateValid || !snapshot._localPlayerMatchesDut ||
		                                       !snapshot._cameraViewerMatchesLocal))
		{
			FailLocked("in-game scene camera is not bound to the DUT player");
		}
		else if (snapshot._scene == result &&
		         (!snapshot._renderCameraStateValid || !snapshot._cameraViewerMatchesLocal))
		{
			FailLocked("ResetGame left invalid scene camera state");
		}

		if (snapshot._cameraMode != firstPerson && snapshot._cameraMode != thirdPerson)
		{
			FailLocked("game scene camera mode is invalid");
		}
		else if (_lastCameraMode < 0)
		{
			_lastCameraMode = snapshot._cameraMode;
		}
		else if (snapshot._cameraMode != _lastCameraMode)
		{
			++_cameraModeTransitions;
			_lastCameraMode = snapshot._cameraMode;
		}

		if (_cameraIdentity == 0)
		{
			_cameraIdentity = snapshot._cameraIdentity;
		}
		if (_cameraBufferAddress == 0)
		{
			_cameraBufferAddress = snapshot._cameraBufferAddress;
		}
		_cameraBufferCreateCount = snapshot._cameraBufferCreateCount;
	}

	switch (_stage)
	{
	case Stage::AwaitInitialFirst:
		if (snapshot._scene == inGame && snapshot._health == 3 && snapshot._cameraMode == firstPerson)
		{
			_stage = Stage::AwaitFirstDown;
			LogLocked("state", "camera=first scene=ingame hp=3");
		}
		break;
	case Stage::AwaitFirstDown:
		if (snapshot._scene == inGame && snapshot._health == 0 && snapshot._cameraMode == thirdPerson)
		{
			_stage = Stage::AwaitRevive;
			LogLocked("state", "camera=third scene=ingame hp=0");
		}
		break;
	case Stage::AwaitRevive:
		if (snapshot._scene == inGame && snapshot._health == 3 && snapshot._cameraMode == firstPerson &&
			snapshot._behavior == static_cast<int>(PLAYER_BEHAVIOR::IDLE))
		{
			_stage = Stage::AwaitSecondDown;
			LogLocked("state", "camera=first scene=ingame hp=3 behavior=0");
		}
		break;
	case Stage::AwaitSecondDown:
		if (snapshot._scene == inGame && snapshot._health == 0 && snapshot._cameraMode == thirdPerson)
		{
			_stage = Stage::AwaitResult;
			LogLocked("state", "camera=third scene=ingame hp=0");
		}
		break;
	case Stage::AwaitResult:
		if (snapshot._scene == result && snapshot._health == 3 &&
			snapshot._behavior == static_cast<int>(PLAYER_BEHAVIOR::IDLE) && !snapshot._hidden &&
			snapshot._cameraMode == firstPerson)
		{
			_stage = Stage::AwaitLobby;
			LogLocked("scene", "scene=result");
			LogLocked("state", "camera=first hp=3");
		}
		break;
	case Stage::AwaitSecondInitialFirst:
		if (snapshot._scene == inGame && snapshot._health == 3 &&
			snapshot._behavior == static_cast<int>(PLAYER_BEHAVIOR::IDLE) && !snapshot._hidden &&
			snapshot._cameraMode == firstPerson)
		{
			_stage = Stage::AwaitSecondPresent;
		}
		break;
	case Stage::AwaitLobby:
		if (snapshot._scene == lobby)
		{
			_stage = Stage::AwaitLobbyPresent;
		}
		break;
	default:
		break;
	}

	// The supervisor closes a successful DUT after the harness records its result.
	// Only failures self-terminate, always from this main-thread pump.
	return _stage == Stage::Failed;
}

void ClientTestMode::OnPresent(const long result)
{
	if (!_enabled)
	{
		return;
	}
	std::lock_guard lock(_mutex);
	if (_lastScene == atb::SceneIndex(atb::SceneId::InGame))
	{
		++_ingamePresents;
	}
	_presentResult = result;
	if (result < 0)
	{
		FailLocked("swap-chain Present failed");
		return;
	}
	if (_stage == Stage::AwaitLobbyPresent)
	{
		if (_cameraModeTransitions != 4)
		{
			FailLocked("unexpected camera mode transition count");
			return;
		}
		_cameraValid = true;
		_stage = Stage::AwaitSecondRoom;
		LogLocked("scene", "scene=lobby");

		C2S_ROOM_ENTER packet{};
		packet.size = sizeof(packet);
		packet.type = static_cast<uint8>(C_ROOM_PACKET_TYPE::ACQ_ENTER_RM);
		packet.rmNum = _roomNumber;
		if (!_session || !_session->DoSend(&packet))
		{
			FailLocked("failed to request the second match room");
		}
	}
	else if (_stage == Stage::AwaitSecondPresent)
	{
		_secondMatchValidated = true;
		_stage = Stage::CandidatePass;
		LogLocked("second_match_validated", "match=2 camera=first scene=ingame hp=3 behavior=0 hidden=0");
	}
}

void ClientTestMode::OnD3DMessage(const char* message)
{
	if (!_enabled)
	{
		return;
	}
	std::lock_guard lock(_mutex);
	LogLocked("d3d12_error", message ? message : "unknown D3D12 message");
}

void ClientTestMode::FinalizeGraphics(const bool infoQueueAvailable, const std::uint32_t errorCount,
	const long deviceRemovedReason,
	const std::uint64_t completedFence, const std::uint64_t submittedFence)
{
	if (!_enabled)
	{
		return;
	}
	std::lock_guard lock(_mutex);
	_graphicsFinalized = true;
	_infoQueueAvailable = infoQueueAvailable;
	_d3dErrorCount = errorCount;
	_deviceRemovedReason = deviceRemovedReason;
	_completedFence = completedFence;
	_submittedFence = submittedFence;
	_graphicsPassed = infoQueueAvailable && errorCount == 0 && deviceRemovedReason == 0 &&
		completedFence >= submittedFence;
	LogLocked("graphics_final",
		"errors=" + std::to_string(errorCount) +
		" device_removed=" + std::to_string(deviceRemovedReason) +
		" completed_fence=" + std::to_string(completedFence) +
		" submitted_fence=" + std::to_string(submittedFence));
	if (!_graphicsPassed)
	{
		FailLocked("graphics validation failed");
	}
}

void ClientTestMode::OnCleanupSequenceCompleted()
{
	if (!_enabled)
	{
		return;
	}
	std::lock_guard lock(_mutex);
	_cleanupSequenceCompleted = true;
}

int ClientTestMode::FinalizeProcess()
{
	if (!_enabled)
	{
		return 0;
	}
	std::lock_guard lock(_mutex);
	if (_stage != Stage::CandidatePass)
	{
		FailLocked(_failureReason.empty() ? "client exited before completing the scenario" : _failureReason);
	}
	else if (!_graphicsFinalized || !_graphicsPassed)
	{
		FailLocked("graphics validation did not complete successfully");
	}
	else if (!_cleanupSequenceCompleted)
	{
		FailLocked("client cleanup sequence did not complete");
	}
	else if (!_gameStarted || _rosterCount != PLAYERNUM || !_ownSidPresent)
	{
		FailLocked("GAME_START roster validation is incomplete");
	}
	else if (_gameStartCount != 2 || !_secondMatchValidated)
	{
		FailLocked("the second match lifecycle was not validated");
	}
	else if (!_cameraValid)
	{
		FailLocked("camera lifecycle validation is incomplete");
	}
	else if (_ingamePresents < 60)
	{
		FailLocked("fewer than 60 in-game frames were presented");
	}
	else if (_presentResult < 0)
	{
		FailLocked("the final Present result is a failed HRESULT");
	}

	const bool passed = _stage == Stage::CandidatePass && _graphicsFinalized && _graphicsPassed &&
		_cleanupSequenceCompleted && _gameStarted && _gameStartCount == 2 && _secondMatchValidated &&
		_rosterCount == PLAYERNUM && _ownSidPresent && _cameraValid &&
		_ingamePresents >= 60 && _presentResult >= 0;
	_log << "{\"schema\":\"atb.dut.v1\",\"event\":\"summary\""
		<< ",\"status\":\"" << (passed ? "pass" : "fail") << "\""
		<< ",\"reason\":\"" << EscapeJson(passed ? "ok" : _failureReason) << "\""
		<< ",\"exit_code\":" << (passed ? 0 : 1)
		<< ",\"game_start\":" << (_gameStarted ? "true" : "false")
		<< ",\"game_start_count\":" << _gameStartCount
		<< ",\"second_match_validated\":" << (_secondMatchValidated ? "true" : "false")
		<< ",\"roster_count\":" << _rosterCount
		<< ",\"own_sid_present\":" << (_ownSidPresent ? "true" : "false")
		<< ",\"camera_valid\":" << (_cameraValid ? "true" : "false")
		<< ",\"camera_identity_stable\":" << (_cameraIdentity != 0 ? "true" : "false")
		<< ",\"camera_buffer_stable\":" << (_cameraBufferAddress != 0 ? "true" : "false")
		<< ",\"camera_buffer_create_count\":" << _cameraBufferCreateCount
		<< ",\"camera_mode_transitions\":" << _cameraModeTransitions
		<< ",\"ingame_presents\":" << _ingamePresents
		<< ",\"present_hr\":" << _presentResult
		<< ",\"device_removed_hr\":" << _deviceRemovedReason
		<< ",\"d3d12_info_queue_available\":" << (_infoQueueAvailable ? "true" : "false")
		<< ",\"d3d12_error_count\":" << _d3dErrorCount
		<< ",\"cleanup_sequence_completed\":" << (_cleanupSequenceCompleted ? "true" : "false")
		<< ",\"fence_submitted\":" << _submittedFence
		<< ",\"fence_completed\":" << _completedFence << "}\n";
	_log.flush();
	return passed ? 0 : 1;
}

void ClientTestMode::Fail(const std::string& reason)
{
	std::lock_guard lock(_mutex);
	FailLocked(reason);
}

void ClientTestMode::FailLocked(const std::string& reason)
{
	if (_stage == Stage::Failed)
	{
		return;
	}
	_stage = Stage::Failed;
	_failureReason = reason;
	LogLocked("failure", reason);
}

void ClientTestMode::LogLocked(const char* event, const std::string& detail)
{
	if (!_log)
	{
		return;
	}
	_log << "{\"seq\":" << ++_sequence
		<< ",\"ms\":" << (::GetTickCount64() - _startedAtMs)
		<< ",\"event\":\"" << EscapeJson(event ? event : "")
		<< "\",\"detail\":\"" << EscapeJson(detail) << "\"}\n";
	_log.flush();
}

std::string ClientTestMode::EscapeJson(const std::string& text)
{
	std::string escaped;
	escaped.reserve(text.size());
	for (const char value : text)
	{
		switch (value)
		{
		case '\\': escaped += "\\\\"; break;
		case '"': escaped += "\\\""; break;
		case '\r': escaped += "\\r"; break;
		case '\n': escaped += "\\n"; break;
		case '\t': escaped += "\\t"; break;
		default: escaped += value; break;
		}
	}
	return escaped;
}
