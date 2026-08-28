#include "pch.h"
#include "ClientTestMode.h"

#include "ClientSession.h"
#include "GameFramework.h"

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
	if (!arguments) return false;

	for (int i = 1; i < argumentCount; ++i)
	{
		const std::wstring argument = arguments[i];
		if (argument == L"--e2e-camera") m_enabled = true;
		else if (argument.starts_with(L"--room="))
		{
			long room = -1;
			if (!ParseInteger(argument.substr(7), room) || room < 0 || room > INT_MAX)
			{
				::LocalFree(arguments);
				return false;
			}
			m_roomNumber = static_cast<int>(room);
		}
		else if (argument.starts_with(L"--telemetry=")) m_telemetryPath = argument.substr(12);
		else if (argument.starts_with(L"--timeout-seconds="))
		{
			long seconds = 0;
			if (!ParseInteger(argument.substr(18), seconds) || seconds < 1 || seconds > 3'600)
			{
				::LocalFree(arguments);
				return false;
			}
			m_timeoutMs = static_cast<std::uint64_t>(seconds) * 1'000;
		}
	}
	::LocalFree(arguments);

	if (!m_enabled) return true;
	if (m_roomNumber < 0 || m_telemetryPath.empty()) return false;

	std::error_code error;
	if (const auto parent = m_telemetryPath.parent_path(); !parent.empty())
		std::filesystem::create_directories(parent, error);
	if (error) return false;

	m_log.open(m_telemetryPath, std::ios::out | std::ios::trunc);
	if (!m_log) return false;

	m_startedAtMs = ::GetTickCount64();
	std::lock_guard lock(m_mutex);
	LogLocked("configured", "room=" + std::to_string(m_roomNumber));
	return true;
#endif
}

void ClientTestMode::OnConnected(CSession& session)
{
	if (!m_enabled) return;

	C2S_LOGIN packet{};
	packet.size = sizeof(packet);
	packet.type = static_cast<uint8>(C_TITLE_PACKET_TYPE::ACQ_LOGIN);
	wcscpy_s(packet.name, L"e2e_dut");
	wcscpy_s(packet.pw, L"e2e_dut");
	if (!session.DoSend(&packet)) Fail("failed to send login");
}

void ClientTestMode::OnLoginOk(CSession& session)
{
	if (!m_enabled) return;
	{
		std::lock_guard lock(m_mutex);
		if (m_roomRequested) return;
		m_roomRequested = true;
		LogLocked("login_ok", "requesting room=" + std::to_string(m_roomNumber));
	}

	C2S_ROOM_ENTER packet{};
	packet.size = sizeof(packet);
	packet.type = static_cast<uint8>(C_ROOM_PACKET_TYPE::ACQ_ENTER_RM);
	packet.rmNum = m_roomNumber;
	if (!session.DoSend(&packet)) Fail("failed to send room enter");
}

void ClientTestMode::OnRoomEntered(CSession& session, const int roomNumber)
{
	if (!m_enabled) return;
	{
		std::lock_guard lock(m_mutex);
		if (roomNumber != m_roomNumber)
		{
			FailLocked("entered unexpected room=" + std::to_string(roomNumber));
			return;
		}
		if (m_readySent) return;
		m_readySent = true;
		LogLocked("room_entered", "room=" + std::to_string(roomNumber));
	}

	C2S_ROOM_EVENT packet{};
	packet.size = sizeof(packet);
	packet.type = static_cast<uint8>(C_ROOM_PACKET_TYPE::ACQ_READY);
	if (!session.DoSend(&packet)) Fail("failed to send ready");
}

void ClientTestMode::OnProtocolFailure(const char* reason)
{
	if (m_enabled) Fail(reason);
}

bool ClientTestMode::ValidateGameStart(const std::int16_t* sids, const std::int32_t ownSid)
{
	if (!m_enabled) return true;
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
		if (sids[i] == ownSid) ownIndex = i;
	}

	if (ownIndex != 1)
	{
		Fail("DUT did not receive employee slot 1");
		return false;
	}

	std::lock_guard lock(m_mutex);
	m_dutPlayerIndex = ownIndex;
	m_rosterCount = PLAYERNUM;
	m_ownSidPresent = true;
	return true;
}

void ClientTestMode::OnGameStarted()
{
	if (!m_enabled) return;
	std::lock_guard lock(m_mutex);
	if (m_stage != Stage::AwaitGame)
	{
		FailLocked("duplicate or out-of-order GAME_START");
		return;
	}
	m_stage = Stage::AwaitInitialFirst;
	m_gameStarted = true;
	LogLocked("game_start", "dut_slot=1");
}

int ClientTestMode::DutPlayerIndex() const
{
	std::lock_guard lock(m_mutex);
	return m_dutPlayerIndex;
}

bool ClientTestMode::Pump(const ClientFrameSnapshot& snapshot)
{
	if (!m_enabled) return false;
	std::lock_guard lock(m_mutex);
	m_lastScene = snapshot.scene;

	if (m_stage != Stage::CandidatePass && m_stage != Stage::Failed &&
		::GetTickCount64() - m_startedAtMs > m_timeoutMs)
		FailLocked("client scenario timeout");

	const int firstPerson = static_cast<int>(FIRST_PERSON_CAMERA);
	const int thirdPerson = static_cast<int>(THIRD_PERSON_CAMERA);
	const int inGame = static_cast<int>(CGameFramework::SCENESTATE::INGAME);
	const int result = static_cast<int>(CGameFramework::SCENESTATE::RESULT);
	const int lobby = static_cast<int>(CGameFramework::SCENESTATE::LOBBY);
	if (m_stage >= Stage::AwaitInitialFirst && m_stage <= Stage::AwaitResult && snapshot.cameraMode >= 0)
	{
		if (snapshot.cameraIdentity == 0)
			FailLocked("DUT camera identity is unavailable");
		else if (m_cameraIdentity != 0 && snapshot.cameraIdentity != m_cameraIdentity)
			FailLocked("DUT camera object changed during a mode transition");
		else if (!snapshot.cameraResourcesValid)
			FailLocked("DUT camera shader variables are unavailable");
		else if (snapshot.scene == inGame &&
			(!snapshot.sceneCameraMatches || !snapshot.scenePlayerIndexMatches))
			FailLocked("in-game scene camera is not bound to the DUT player camera");
		else if (snapshot.scene == result && !snapshot.sceneCameraMatches)
			FailLocked("ResetGame left an active in-game render camera");

		if (m_cameraIdentity == 0) m_cameraIdentity = snapshot.cameraIdentity;
	}

	switch (m_stage)
	{
	case Stage::AwaitInitialFirst:
		if (snapshot.scene == inGame && snapshot.hp == 3 && snapshot.cameraMode == firstPerson)
		{
			m_stage = Stage::AwaitFirstDown;
			LogLocked("state", "camera=first scene=ingame hp=3");
		}
		break;
	case Stage::AwaitFirstDown:
		if (snapshot.hp == 0 && snapshot.cameraMode == thirdPerson)
		{
			m_stage = Stage::AwaitRevive;
			LogLocked("state", "camera=third scene=ingame hp=0");
		}
		break;
	case Stage::AwaitRevive:
		if (snapshot.hp == 3 && snapshot.cameraMode == firstPerson &&
			snapshot.behavior == static_cast<int>(PLAYER_BEHAVIOR::IDLE))
		{
			m_stage = Stage::AwaitSecondDown;
			LogLocked("state", "camera=first scene=ingame hp=3 behavior=0");
		}
		break;
	case Stage::AwaitSecondDown:
		if (snapshot.hp == 0 && snapshot.cameraMode == thirdPerson)
		{
			m_stage = Stage::AwaitResult;
			LogLocked("state", "camera=third scene=ingame hp=0");
		}
		break;
	case Stage::AwaitResult:
		if (snapshot.scene == result && snapshot.cameraMode == firstPerson)
		{
			m_stage = Stage::AwaitLobby;
			LogLocked("scene", "scene=result");
			LogLocked("state", "camera=first hp=3");
		}
		break;
	case Stage::AwaitLobby:
		if (snapshot.scene == lobby)
		{
			m_stage = Stage::AwaitLobbyPresent;
		}
		break;
	default:
		break;
	}

	// The supervisor closes a successful DUT after the harness records its result.
	// Only failures self-terminate, always from this main-thread pump.
	return m_stage == Stage::Failed;
}

void ClientTestMode::OnPresent(const long result)
{
	if (!m_enabled) return;
	std::lock_guard lock(m_mutex);
	if (m_lastScene == static_cast<int>(CGameFramework::SCENESTATE::INGAME)) ++m_ingamePresents;
	m_presentResult = result;
	if (result < 0)
	{
		FailLocked("swap-chain Present failed");
		return;
	}
	if (m_stage == Stage::AwaitLobbyPresent)
	{
		m_cameraValid = true;
		m_stage = Stage::CandidatePass;
		LogLocked("scene", "scene=lobby");
	}
}

void ClientTestMode::OnD3DMessage(const char* message)
{
	if (!m_enabled) return;
	std::lock_guard lock(m_mutex);
	LogLocked("d3d12_error", message ? message : "unknown D3D12 message");
}

void ClientTestMode::FinalizeGraphics(const bool infoQueueAvailable, const std::uint32_t errorCount,
	const long deviceRemovedReason,
	const std::uint64_t completedFence, const std::uint64_t submittedFence)
{
	if (!m_enabled) return;
	std::lock_guard lock(m_mutex);
	m_graphicsFinalized = true;
	m_infoQueueAvailable = infoQueueAvailable;
	m_d3dErrorCount = errorCount;
	m_deviceRemovedReason = deviceRemovedReason;
	m_completedFence = completedFence;
	m_submittedFence = submittedFence;
	m_graphicsPassed = infoQueueAvailable && errorCount == 0 && deviceRemovedReason == 0 &&
		completedFence >= submittedFence;
	LogLocked("graphics_final",
		"errors=" + std::to_string(errorCount) +
		" device_removed=" + std::to_string(deviceRemovedReason) +
		" completed_fence=" + std::to_string(completedFence) +
		" submitted_fence=" + std::to_string(submittedFence));
	if (!m_graphicsPassed) FailLocked("graphics validation failed");
}

int ClientTestMode::FinalizeProcess()
{
	if (!m_enabled) return 0;
	std::lock_guard lock(m_mutex);
	if (m_stage != Stage::CandidatePass)
		FailLocked(m_failureReason.empty() ? "client exited before completing the scenario" : m_failureReason);
	else if (!m_graphicsFinalized || !m_graphicsPassed)
		FailLocked("graphics validation did not complete successfully");
	else if (!m_gameStarted || m_rosterCount != PLAYERNUM || !m_ownSidPresent)
		FailLocked("GAME_START roster validation is incomplete");
	else if (!m_cameraValid)
		FailLocked("camera lifecycle validation is incomplete");
	else if (m_ingamePresents < 60)
		FailLocked("fewer than 60 in-game frames were presented");
	else if (m_presentResult < 0)
		FailLocked("the final Present result is a failed HRESULT");

	const bool passed = m_stage == Stage::CandidatePass && m_graphicsFinalized && m_graphicsPassed &&
		m_gameStarted && m_rosterCount == PLAYERNUM && m_ownSidPresent && m_cameraValid &&
		m_ingamePresents >= 60 && m_presentResult >= 0;
	m_log << "{\"schema\":\"atb.dut.v1\",\"event\":\"summary\""
		<< ",\"status\":\"" << (passed ? "pass" : "fail") << "\""
		<< ",\"reason\":\"" << EscapeJson(passed ? "ok" : m_failureReason) << "\""
		<< ",\"exit_code\":" << (passed ? 0 : 1)
		<< ",\"game_start\":" << (m_gameStarted ? "true" : "false")
		<< ",\"roster_count\":" << m_rosterCount
		<< ",\"own_sid_present\":" << (m_ownSidPresent ? "true" : "false")
		<< ",\"camera_valid\":" << (m_cameraValid ? "true" : "false")
		<< ",\"ingame_presents\":" << m_ingamePresents
		<< ",\"present_hr\":" << m_presentResult
		<< ",\"device_removed_hr\":" << m_deviceRemovedReason
		<< ",\"d3d12_info_queue_available\":" << (m_infoQueueAvailable ? "true" : "false")
		<< ",\"d3d12_error_count\":" << m_d3dErrorCount
		<< ",\"cleanup_completed\":true"
		<< ",\"fence_submitted\":" << m_submittedFence
		<< ",\"fence_completed\":" << m_completedFence << "}\n";
	m_log.flush();
	return passed ? 0 : 1;
}

void ClientTestMode::Fail(const std::string& reason)
{
	std::lock_guard lock(m_mutex);
	FailLocked(reason);
}

void ClientTestMode::FailLocked(const std::string& reason)
{
	if (m_stage == Stage::Failed) return;
	m_stage = Stage::Failed;
	m_failureReason = reason;
	LogLocked("failure", reason);
}

void ClientTestMode::LogLocked(const char* event, const std::string& detail)
{
	if (!m_log) return;
	m_log << "{\"seq\":" << ++m_sequence
		<< ",\"ms\":" << (::GetTickCount64() - m_startedAtMs)
		<< ",\"event\":\"" << EscapeJson(event ? event : "")
		<< "\",\"detail\":\"" << EscapeJson(detail) << "\"}\n";
	m_log.flush();
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
