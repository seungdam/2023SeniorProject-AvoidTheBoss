#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

class CSession;

struct ClientFrameSnapshot
{
	int scene = -1;
	int hp = -1;
	int behavior = -1;
	int cameraMode = -1;
	bool rescuing = false;
	bool sceneCameraMatches = false;
	bool scenePlayerIndexMatches = false;
	bool cameraResourcesValid = false;
	std::uintptr_t cameraIdentity = 0;
	std::uint64_t submittedFence = 0;
	std::uint64_t completedFence = 0;
};

class ClientTestMode final
{
public:
	bool Configure();
	bool Enabled() const { return m_enabled; }

	void OnConnected(CSession& session);
	void OnLoginOk(CSession& session);
	void OnRoomEntered(CSession& session, int roomNumber);
	void OnProtocolFailure(const char* reason);
	bool ValidateGameStart(const std::int16_t* sids, std::int32_t ownSid);
	void OnGameStarted();

	int DutPlayerIndex() const;
	bool Pump(const ClientFrameSnapshot& snapshot);
	void OnPresent(long result);
	void OnD3DMessage(const char* message);
	void FinalizeGraphics(bool infoQueueAvailable, std::uint32_t errorCount, long deviceRemovedReason,
		std::uint64_t completedFence, std::uint64_t submittedFence);
	int FinalizeProcess();

private:
	enum class Stage
	{
		AwaitGame,
		AwaitInitialFirst,
		AwaitFirstDown,
		AwaitRevive,
		AwaitSecondDown,
		AwaitResult,
		AwaitLobby,
		AwaitLobbyPresent,
		CandidatePass,
		Failed
	};

	void Fail(const std::string& reason);
	void FailLocked(const std::string& reason);
	void LogLocked(const char* event, const std::string& detail);
	static std::string EscapeJson(const std::string& text);

	mutable std::mutex m_mutex;
	std::ofstream m_log;
	std::filesystem::path m_telemetryPath;
	Stage m_stage = Stage::AwaitGame;
	bool m_enabled = false;
	bool m_roomRequested = false;
	bool m_readySent = false;
	bool m_graphicsFinalized = false;
	bool m_graphicsPassed = false;
	bool m_gameStarted = false;
	bool m_ownSidPresent = false;
	bool m_cameraValid = false;
	bool m_infoQueueAvailable = false;
	int m_roomNumber = -1;
	int m_dutPlayerIndex = -1;
	int m_rosterCount = 0;
	int m_lastScene = -1;
	long m_presentResult = 0;
	long m_deviceRemovedReason = 0;
	std::uint32_t m_d3dErrorCount = 0;
	std::uint64_t m_ingamePresents = 0;
	std::uint64_t m_completedFence = 0;
	std::uint64_t m_submittedFence = 0;
	std::uintptr_t m_cameraIdentity = 0;
	std::uint64_t m_startedAtMs = 0;
	std::uint64_t m_timeoutMs = 180'000;
	std::uint64_t m_sequence = 0;
	std::string m_failureReason;
};

extern ClientTestMode g_clientTestMode;
