#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

class ClientSession;

struct ClientFrameSnapshot
{
	int _scene = -1;
	int _health = -1;
	int _behavior = -1;
	int _cameraMode = -1;
	bool _hidden = false;
	bool _rescuing = false;
	bool _renderCameraStateValid = false;
	bool _localPlayerMatchesDut = false;
	bool _cameraViewerMatchesLocal = false;
	bool _cameraResourcesValid = false;
	std::uintptr_t _cameraIdentity = 0;
	std::uint64_t _cameraBufferAddress = 0;
	std::uint32_t _cameraBufferCreateCount = 0;
	std::uint64_t _submittedFence = 0;
	std::uint64_t _completedFence = 0;
};

class ClientTestMode final
{
public:
	bool Configure();
	bool Enabled() const { return _enabled; }

	void OnConnected(ClientSession& session);
	void OnLoginOk(ClientSession& session);
	void OnRoomEntered(ClientSession& session, int roomNumber);
	void OnProtocolFailure(const char* reason);
	bool ValidateGameStart(const std::int16_t* sids, std::int32_t ownSid);
	void OnGameStarted();

	int DutPlayerIndex() const;
	bool Pump(const ClientFrameSnapshot& snapshot);
	void OnPresent(long result);
	void OnD3DMessage(const char* message);
	void FinalizeGraphics(bool infoQueueAvailable, std::uint32_t errorCount, long deviceRemovedReason,
		std::uint64_t completedFence, std::uint64_t submittedFence);
	void OnCleanupSequenceCompleted();
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
		AwaitSecondRoom,
		AwaitSecondGame,
		AwaitSecondInitialFirst,
		AwaitSecondPresent,
		CandidatePass,
		Failed
	};

	void Fail(const std::string& reason);
	void FailLocked(const std::string& reason);
	void LogLocked(const char* event, const std::string& detail);
	static std::string EscapeJson(const std::string& text);

	mutable std::mutex _mutex;
	std::ofstream _log;
	std::filesystem::path _telemetryPath;
	Stage _stage = Stage::AwaitGame;
	bool _enabled = false;
	bool _roomRequested = false;
	bool _readySent = false;
	bool _secondReadySent = false;
	bool _graphicsFinalized = false;
	bool _graphicsPassed = false;
	bool _cleanupSequenceCompleted = false;
	bool _gameStarted = false;
	bool _secondMatchValidated = false;
	bool _ownSidPresent = false;
	bool _cameraValid = false;
	bool _infoQueueAvailable = false;
	int _roomNumber = -1;
	int _dutPlayerIndex = -1;
	int _gameStartCount = 0;
	int _rosterCount = 0;
	int _lastScene = -1;
	long _presentResult = 0;
	long _deviceRemovedReason = 0;
	std::uint32_t _d3dErrorCount = 0;
	std::uint64_t _ingamePresents = 0;
	std::uint64_t _completedFence = 0;
	std::uint64_t _submittedFence = 0;
	std::uintptr_t _cameraIdentity = 0;
	std::uint64_t _cameraBufferAddress = 0;
	std::uint32_t _cameraBufferCreateCount = 0;
	std::uint32_t _cameraModeTransitions = 0;
	int _lastCameraMode = -1;
	std::uint64_t _startedAtMs = 0;
	std::uint64_t _timeoutMs = 180'000;
	std::uint64_t _sequence = 0;
	std::string _failureReason;
	ClientSession* _session = nullptr; // Observer; the IOCP core owns the connected session.
};

extern ClientTestMode g_clientTestMode;
