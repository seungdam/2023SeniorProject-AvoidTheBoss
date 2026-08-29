#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "../Shared/Protocol.h"
#include "../Shared/Types.h"

#include <WinSock2.h>
#include <WS2tcpip.h>

namespace
{
    using Clock = std::chrono::steady_clock;
    using namespace std::chrono_literals;

    constexpr std::size_t kConnectExInitialBytes = 255;
    constexpr uint8 kKeyForward = 0x01;
    constexpr int kBotCount = 3;
    constexpr int kRosterSize = 4;
    constexpr float kBossClientRayLimit = 5.0f;
    constexpr float kRescueClientDistance = 1.5f;
    constexpr int kHonestCooldownFrames = 35;
    constexpr auto kHonestCooldownTime = 2s;

    static_assert(sizeof(wchar_t) == 2, "The wire protocol requires MSVC's two-byte wchar_t.");
    static_assert(sizeof(C2S_LOGIN) == 42);
    static_assert(sizeof(C2S_ROOM_EVENT) == 2);
    static_assert(sizeof(C2S_ROOM_ENTER) == 6);
    static_assert(sizeof(C2S_KEY) == 12);
    static_assert(sizeof(C2S_ATTACK) == 8);
    static_assert(sizeof(SC_EVENTPACKET) == 3);
    static_assert(sizeof(S2C_LOGIN_OK) == 6);
    static_assert(sizeof(S2C_ROOM_INFO) == 14);
    static_assert(sizeof(S2C_GAMESTART) == 10);
    static_assert(sizeof(S2C_POS) == 12);
    static_assert(sizeof(S2C_FRAMEPACKET) == 6);

    struct Config
    {
        std::wstring host = L"127.0.0.1";
        uint16 port = PORTNUM;
        std::filesystem::path roomFile;
        std::filesystem::path telemetryFile;
        std::filesystem::path resultFile;
        int timeoutSeconds = 180;
    };

    std::string WideToUtf8(const std::wstring_view value)
    {
        if (value.empty()) return {};
        const int bytes = ::WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
            nullptr, 0, nullptr, nullptr);
        if (bytes <= 0) throw std::runtime_error("WideCharToMultiByte failed");
        std::string result(static_cast<std::size_t>(bytes), '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
            result.data(), bytes, nullptr, nullptr);
        return result;
    }

    std::string JsonEscape(const std::string_view value)
    {
        std::ostringstream out;
        for (const unsigned char ch : value)
        {
            switch (ch)
            {
            case '\\': out << "\\\\"; break;
            case '"': out << "\\\""; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (ch < 0x20)
                {
                    out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(ch)
                        << std::dec << std::setfill(' ');
                }
                else
                {
                    out << static_cast<char>(ch);
                }
                break;
            }
        }
        return out.str();
    }

    void EnsureParentDirectory(const std::filesystem::path& file)
    {
        const auto parent = file.parent_path();
        if (!parent.empty()) std::filesystem::create_directories(parent);
    }

    Config ParseArgs(const int argc, wchar_t** argv)
    {
        Config config;
        auto requireValue = [&](int& index, const std::wstring_view option) -> std::wstring
        {
            if (++index >= argc) throw std::runtime_error("Missing value for " + WideToUtf8(option));
            return argv[index];
        };

        for (int i = 1; i < argc; ++i)
        {
            const std::wstring_view arg(argv[i]);
            if (arg == L"--host")
            {
                config.host = requireValue(i, arg);
            }
            else if (arg == L"--port")
            {
                const auto value = std::stoul(requireValue(i, arg));
                if (value == 0 || value > std::numeric_limits<uint16>::max())
                    throw std::runtime_error("--port must be in 1..65535");
                config.port = static_cast<uint16>(value);
            }
            else if (arg == L"--room-file")
            {
                config.roomFile = requireValue(i, arg);
            }
            else if (arg == L"--telemetry")
            {
                config.telemetryFile = requireValue(i, arg);
            }
            else if (arg == L"--result")
            {
                config.resultFile = requireValue(i, arg);
            }
            else if (arg == L"--timeout-seconds")
            {
                config.timeoutSeconds = std::stoi(requireValue(i, arg));
                if (config.timeoutSeconds <= 0) throw std::runtime_error("--timeout-seconds must be positive");
            }
            else if (arg == L"--help" || arg == L"-h")
            {
                std::wcout
                    << L"ProtocolScenarioHarness --host <host> --port <port> --room-file <path> "
                    << L"--telemetry <jsonl-path> --result <json-path> --timeout-seconds <seconds>\n";
                std::exit(0);
            }
            else
            {
                throw std::runtime_error("Unknown option: " + WideToUtf8(arg));
            }
        }

        if (config.roomFile.empty()) throw std::runtime_error("--room-file is required");
        if (config.telemetryFile.empty()) throw std::runtime_error("--telemetry is required");
        if (config.resultFile.empty()) throw std::runtime_error("--result is required");
        return config;
    }

    class WinsockScope
    {
    public:
        WinsockScope()
        {
            WSADATA data{};
            if (::WSAStartup(MAKEWORD(2, 2), &data) != 0) throw std::runtime_error("WSAStartup failed");
        }

        ~WinsockScope()
        {
            ::WSACleanup();
        }

        WinsockScope(const WinsockScope&) = delete;
        WinsockScope& operator=(const WinsockScope&) = delete;
    };

    enum class QueueEventKind
    {
        Packet,
        Disconnected,
        SocketError,
        ProtocolError,
    };

    struct QueueEvent
    {
        QueueEventKind kind = QueueEventKind::Packet;
        int bot = -1;
        std::vector<uint8_t> packet;
        int error = 0;
        std::string message;
    };

    class EventQueue
    {
    public:
        void Push(QueueEvent event)
        {
            {
                std::lock_guard lock(mutex_);
                queue_.push_back(std::move(event));
            }
            cv_.notify_one();
        }

        std::optional<QueueEvent> WaitPop(const std::chrono::milliseconds timeout)
        {
            std::unique_lock lock(mutex_);
            cv_.wait_for(lock, timeout, [&] { return !queue_.empty(); });
            if (queue_.empty()) return std::nullopt;
            QueueEvent event = std::move(queue_.front());
            queue_.pop_front();
            return event;
        }

        std::optional<QueueEvent> TryPop()
        {
            std::lock_guard lock(mutex_);
            if (queue_.empty()) return std::nullopt;
            QueueEvent event = std::move(queue_.front());
            queue_.pop_front();
            return event;
        }

    private:
        std::mutex mutex_;
        std::condition_variable cv_;
        std::deque<QueueEvent> queue_;
    };

    bool SendAll(const SOCKET socket, const void* data, const std::size_t size)
    {
        const auto* bytes = static_cast<const char*>(data);
        std::size_t sent = 0;
        while (sent < size)
        {
            const auto remaining = size - sent;
            const int chunk = static_cast<int>(std::min<std::size_t>(remaining,
                static_cast<std::size_t>(std::numeric_limits<int>::max())));
            const int result = ::send(socket, bytes + sent, chunk, 0);
            if (result == SOCKET_ERROR || result == 0) return false;
            sent += static_cast<std::size_t>(result);
        }
        return true;
    }

    class BotSession
    {
    public:
        BotSession(const int index, EventQueue& events) : index_(index), events_(events) {}

        ~BotSession()
        {
            Stop();
        }

        BotSession(const BotSession&) = delete;
        BotSession& operator=(const BotSession&) = delete;

        bool ConnectOnce(const std::wstring& host, const uint16 port, std::string& error)
        {
            ADDRINFOW hints{};
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;

            PADDRINFOW addresses = nullptr;
            const std::wstring service = std::to_wstring(port);
            const int addressResult = ::GetAddrInfoW(host.c_str(), service.c_str(), &hints, &addresses);
            if (addressResult != 0)
            {
                error = "GetAddrInfoW failed: " + std::to_string(addressResult);
                return false;
            }

            SOCKET connected = INVALID_SOCKET;
            int lastError = 0;
            for (auto* address = addresses; address; address = address->ai_next)
            {
                SOCKET candidate = ::socket(address->ai_family, address->ai_socktype, address->ai_protocol);
                if (candidate == INVALID_SOCKET)
                {
                    lastError = ::WSAGetLastError();
                    continue;
                }
                if (::connect(candidate, address->ai_addr, static_cast<int>(address->ai_addrlen)) == 0)
                {
                    connected = candidate;
                    break;
                }
                lastError = ::WSAGetLastError();
                ::closesocket(candidate);
            }
            ::FreeAddrInfoW(addresses);

            if (connected == INVALID_SOCKET)
            {
                error = "connect failed: " + std::to_string(lastError);
                return false;
            }

            BOOL noDelay = TRUE;
            ::setsockopt(connected, IPPROTO_TCP, TCP_NODELAY,
                reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));

            // AcceptManager posts AcceptEx with 255 bytes of initial receive data. A plain TCP
            // connect does not complete that AcceptEx until these compatibility bytes arrive.
            std::array<uint8_t, kConnectExInitialBytes> initialData{};
            if (!SendAll(connected, initialData.data(), initialData.size()))
            {
                lastError = ::WSAGetLastError();
                ::closesocket(connected);
                error = "sending ConnectEx compatibility data failed: " + std::to_string(lastError);
                return false;
            }

            socket_ = connected;
            reader_ = std::jthread([this](const std::stop_token stop) { ReaderLoop(stop); });
            return true;
        }

        template <typename T>
        void SendPacket(const T& packet)
        {
            static_assert(sizeof(T) <= std::numeric_limits<uint8>::max());
            if (packet.size != sizeof(T)) throw std::runtime_error("packet.size does not match sizeof(packet)");
            std::lock_guard lock(sendMutex_);
            if (socket_ == INVALID_SOCKET || !SendAll(socket_, &packet, sizeof(T)))
                throw std::runtime_error("bot" + std::to_string(index_) + " send failed: "
                    + std::to_string(::WSAGetLastError()));
        }

        void Stop()
        {
            if (socket_ != INVALID_SOCKET)
            {
                ::shutdown(socket_, SD_BOTH);
                ::closesocket(socket_);
                socket_ = INVALID_SOCKET;
            }
            if (reader_.joinable())
            {
                reader_.request_stop();
                reader_.join();
            }
        }

    private:
        void ReaderLoop(const std::stop_token stop)
        {
            std::vector<uint8_t> buffered;
            buffered.reserve(2048);
            std::array<uint8_t, 2048> received{};

            while (!stop.stop_requested())
            {
                const int result = ::recv(socket_, reinterpret_cast<char*>(received.data()),
                    static_cast<int>(received.size()), 0);
                if (result == 0)
                {
                    if (!stop.stop_requested()) events_.Push({ QueueEventKind::Disconnected, index_ });
                    return;
                }
                if (result == SOCKET_ERROR)
                {
                    const int error = ::WSAGetLastError();
                    if (!stop.stop_requested())
                        events_.Push({ QueueEventKind::SocketError, index_, {}, error, "recv failed" });
                    return;
                }

                buffered.insert(buffered.end(), received.begin(), received.begin() + result);
                for (;;)
                {
                    if (buffered.size() < 2) break;
                    const std::size_t packetSize = buffered.front();
                    if (packetSize < 2)
                    {
                        events_.Push({ QueueEventKind::ProtocolError, index_, {}, 0,
                            "server packet has a size smaller than its header" });
                        return;
                    }
                    if (buffered.size() < packetSize) break;
                    std::vector<uint8_t> packet(buffered.begin(), buffered.begin() + packetSize);
                    buffered.erase(buffered.begin(), buffered.begin() + packetSize);
                    events_.Push({ QueueEventKind::Packet, index_, std::move(packet) });
                }
            }
        }

        int index_ = -1;
        EventQueue& events_;
        SOCKET socket_ = INVALID_SOCKET;
        std::mutex sendMutex_;
        std::jthread reader_;
    };

    struct FlatTelemetry
    {
        uint64_t lineSequence = 0;
        uint64_t lastThirdSequence = 0;
        uint64_t lastFirstSequence = 0;
        uint64_t lastResetFirstSequence = 0;
        uint64_t lastResultSequence = 0;
        uint64_t lastLobbySequence = 0;
		uint64_t lastSecondMatchSequence = 0;
		int gameStartCount = 0;
        int cameraMode = -1;
        int scene = -1;
        std::optional<int> hp;
        std::optional<int> behavior;
		std::optional<bool> hidden;
        std::optional<bool> invincible;
    };

    std::string LowerAscii(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch)
        {
            return static_cast<char>(std::tolower(ch));
        });
        return value;
    }

    std::optional<std::string> ExtractFlatValue(const std::string& lowerLine,
        const std::initializer_list<std::string_view> keys)
    {
        for (const auto key : keys)
        {
            std::size_t keyPos = lowerLine.find(std::string("\"") + std::string(key) + "\"");
            if (keyPos == std::string::npos) keyPos = lowerLine.find(std::string(key));
            if (keyPos == std::string::npos) continue;
            std::size_t separator = lowerLine.find_first_of(":=", keyPos + key.size());
            if (separator == std::string::npos) continue;
            std::size_t begin = lowerLine.find_first_not_of(" \t\"", separator + 1);
            if (begin == std::string::npos) continue;
            std::size_t end = begin;
            while (end < lowerLine.size())
            {
                const char ch = lowerLine[end];
                if (ch == '"' || ch == ',' || ch == '}' || std::isspace(static_cast<unsigned char>(ch))) break;
                ++end;
            }
            if (end > begin) return lowerLine.substr(begin, end - begin);
        }
        return std::nullopt;
    }

    std::optional<int> ParseInt(const std::optional<std::string>& value)
    {
        if (!value || value->empty()) return std::nullopt;
        int parsed = 0;
        const auto result = std::from_chars(value->data(), value->data() + value->size(), parsed);
        if (result.ec != std::errc{}) return std::nullopt;
        return parsed;
    }

    class TelemetryTail
    {
    public:
        explicit TelemetryTail(std::filesystem::path path) : path_(std::move(path))
        {
            std::error_code error;
            if (std::filesystem::exists(path_, error))
                offset_ = std::filesystem::file_size(path_, error);
        }

        void Poll()
        {
            std::error_code error;
            if (!std::filesystem::exists(path_, error)) return;
            const auto fileSize = std::filesystem::file_size(path_, error);
            if (error) return;
            if (fileSize < offset_)
            {
                offset_ = 0;
                pending_.clear();
            }
            if (fileSize == offset_) return;

            std::ifstream input(path_, std::ios::binary);
            if (!input) return;
            input.seekg(static_cast<std::streamoff>(offset_), std::ios::beg);
            std::string chunk(static_cast<std::size_t>(fileSize - offset_), '\0');
            input.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
            const auto read = input.gcount();
            if (read <= 0) return;
            chunk.resize(static_cast<std::size_t>(read));
            offset_ += static_cast<std::uintmax_t>(read);
            pending_ += chunk;

            std::size_t newline = 0;
            while ((newline = pending_.find('\n')) != std::string::npos)
            {
                std::string line = pending_.substr(0, newline);
                pending_.erase(0, newline + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (!line.empty()) Observe(line);
            }
        }

        const FlatTelemetry& State() const { return state_; }

    private:
        void Observe(const std::string& line)
        {
            ++state_.lineSequence;
            const std::string lower = LowerAscii(line);
            const bool semanticResultFirstReset = lower.find("result_first_reset") != std::string::npos;
            const bool lineMentionsLobby = lower.find("lobby") != std::string::npos;
			if (lower.find("\"event\":\"game_start\"") != std::string::npos) ++state_.gameStartCount;
			if (lower.find("\"event\":\"second_match_validated\"") != std::string::npos)
				state_.lastSecondMatchSequence = state_.lineSequence;

            const auto camera = ExtractFlatValue(lower, { "cameramode", "camera_mode", "camera", "mode" });
            int cameraMode = -1;
            if (camera)
            {
                if (*camera == "third" || *camera == "third_person" || *camera == "third-person") cameraMode = 3;
                else if (*camera == "first" || *camera == "first_person" || *camera == "first-person") cameraMode = 1;
                else if (const auto numeric = ParseInt(camera)) cameraMode = *numeric;
            }
            if (cameraMode < 0 && lower.find("camera") != std::string::npos)
            {
                if (lower.find("third") != std::string::npos) cameraMode = 3;
                else if (lower.find("first") != std::string::npos) cameraMode = 1;
            }
            // The debug DUT observer intentionally logs semantic lifecycle states as a flat
            // event/detail JSONL record rather than exposing internal pointers.
            if (cameraMode < 0)
            {
                if (lower.find("third_first_down") != std::string::npos
                    || lower.find("third_second_down") != std::string::npos) cameraMode = 3;
                else if (lower.find("first_initial") != std::string::npos
                    || lower.find("first_revived") != std::string::npos
                    || lower.find("result_first_reset") != std::string::npos) cameraMode = 1;
            }
            if (cameraMode == 1 || cameraMode == 3)
            {
                state_.cameraMode = cameraMode;
                if (cameraMode == 3) state_.lastThirdSequence = state_.lineSequence;
                else
                {
                    state_.lastFirstSequence = state_.lineSequence;
                    // A later lobby-present record can contain both scene=lobby and
                    // camera=first. Keep the result-scene reset distinct so that such a
                    // combined line cannot satisfy both lifecycle gates by itself.
                    if (semanticResultFirstReset
                        || (state_.lastResultSequence != 0 && state_.scene == 4 && !lineMentionsLobby))
                        state_.lastResetFirstSequence = state_.lineSequence;
                }
            }

            const auto sceneValue = ExtractFlatValue(lower, { "scene", "scenestate", "scene_state" });
            int scene = -1;
            if (sceneValue)
            {
                if (*sceneValue == "title") scene = 0;
                else if (*sceneValue == "lobby") scene = 1;
                else if (*sceneValue == "room") scene = 2;
                else if (*sceneValue == "ingame" || *sceneValue == "in_game") scene = 3;
                else if (*sceneValue == "result") scene = 4;
                else if (const auto numeric = ParseInt(sceneValue)) scene = *numeric;
            }
            if (scene < 0)
            {
                if (lower.find("result") != std::string::npos && lower.find("scene") != std::string::npos) scene = 4;
                else if (lower.find("lobby") != std::string::npos && lower.find("scene") != std::string::npos) scene = 1;
            }
            if (scene < 0 && lower.find("result_first_reset") != std::string::npos) scene = 4;
            if (scene < 0 && lower.find("\"detail\":\"lobby\"") != std::string::npos) scene = 1;
            if (scene >= 0)
            {
                state_.scene = scene;
                if (scene == 4)
                {
                    state_.lastResultSequence = state_.lineSequence;
                    if (cameraMode == 1) state_.lastResetFirstSequence = state_.lineSequence;
                }
                else if (scene == 1) state_.lastLobbySequence = state_.lineSequence;
            }

			if (const auto hp = ParseInt(ExtractFlatValue(lower, { "hp" }))) state_.hp = hp;
			if (const auto behavior = ParseInt(ExtractFlatValue(lower, { "behavior" })))
				state_.behavior = behavior;
			if (const auto value = ExtractFlatValue(lower, { "hidden" }))
			{
				if (*value == "true" || *value == "1") state_.hidden = true;
				else if (*value == "false" || *value == "0") state_.hidden = false;
			}
            if (const auto value = ExtractFlatValue(lower, { "invincible", "invincibility" }))
            {
                if (*value == "true" || *value == "1") state_.invincible = true;
                else if (*value == "false" || *value == "0") state_.invincible = false;
            }
        }

        std::filesystem::path path_;
        std::uintmax_t offset_ = 0;
        std::string pending_;
        FlatTelemetry state_;
    };

    struct Vec2
    {
        float x = 0.0f;
        float z = 0.0f;
    };

    Vec2 Subtract(const Vec2 a, const Vec2 b) { return { a.x - b.x, a.z - b.z }; }
    float Length(const Vec2 value) { return std::sqrt(value.x * value.x + value.z * value.z); }
    float Dot(const Vec2 a, const Vec2 b) { return a.x * b.x + a.z * b.z; }
    float CrossMagnitude(const Vec2 a, const Vec2 b) { return std::abs(a.x * b.z - a.z * b.x); }

    Vec2 Normalize(const Vec2 value)
    {
        const float length = Length(value);
        if (length <= 0.0001f) throw std::runtime_error("cannot normalize a zero-length direction");
        return { value.x / length, value.z / length };
    }

    template <typename T>
    T Decode(const std::vector<uint8_t>& bytes)
    {
        if (bytes.size() != sizeof(T)) throw std::runtime_error("packet size mismatch while decoding");
        T value{};
        std::memcpy(&value, bytes.data(), sizeof(T));
        return value;
    }

    std::size_t ExpectedServerPacketSize(const uint8 type)
    {
        switch (type)
        {
        case static_cast<uint8>(S_TITLE_PACKET_TYPE::REG_FAIL):
        case static_cast<uint8>(S_TITLE_PACKET_TYPE::REG_OK): return sizeof(S2C_REG);
        case static_cast<uint8>(S_TITLE_PACKET_TYPE::LOGIN_OK): return sizeof(S2C_LOGIN_OK);
        case static_cast<uint8>(S_TITLE_PACKET_TYPE::LOGIN_FAIL): return sizeof(S2C_LOGIN_FAIL);
        case static_cast<uint8>(S_ROOM_PACKET_TYPE::MK_RM_OK):
        case static_cast<uint8>(S_ROOM_PACKET_TYPE::MK_RM_FAIL):
        case static_cast<uint8>(S_ROOM_PACKET_TYPE::REP_EXIT_RM): return sizeof(S2C_ROOM_EVENT);
        case static_cast<uint8>(S_ROOM_PACKET_TYPE::REP_ENTER_FAIL):
        case static_cast<uint8>(S_ROOM_PACKET_TYPE::REP_ENTER_OK): return sizeof(S2C_ROOM_ENTER);
        case static_cast<uint8>(S_ROOM_PACKET_TYPE::UPDATE_LIST): return sizeof(S2C_ROOM_LIST);
        case static_cast<uint8>(S_ROOM_PACKET_TYPE::ROOM_INFO): return sizeof(S2C_ROOM_INFO);
        case static_cast<uint8>(S_ROOM_PACKET_TYPE::REP_READY):
        case static_cast<uint8>(S_ROOM_PACKET_TYPE::REP_READY_CANCEL): return sizeof(S2C_ROOM_READY);
        case static_cast<uint8>(S_ROOM_PACKET_TYPE::GAME_START): return sizeof(S2C_GAMESTART);
        case static_cast<uint8>(S_GAME_PACKET_TYPE::SCHAT): return sizeof(_CHAT);
        case static_cast<uint8>(S_GAME_PACKET_TYPE::SKEY): return sizeof(S2C_KEY);
        case static_cast<uint8>(S_GAME_PACKET_TYPE::SROT): return sizeof(S2C_ROTATE);
        case static_cast<uint8>(S_GAME_PACKET_TYPE::SPOS): return sizeof(S2C_POS);
        case static_cast<uint8>(S_GAME_PACKET_TYPE::ANIM): return sizeof(S2C_ANIMPACKET);
        case static_cast<uint8>(S_GAME_PACKET_TYPE::FRAME): return sizeof(S2C_FRAMEPACKET);
        case static_cast<uint8>(SC_GAME_PACKET_TYPE::GAMEEVENT): return sizeof(SC_EVENTPACKET);
        default: return 0;
        }
    }

    struct BotState
    {
        bool loginOk = false;
        int16 sid = -1;
        bool createOk = false;
		int createCount = 0;
        int16 enteredRoom = -1;
		int roomEntryCount = 0;
        bool roomInfoSeen = false;
        std::array<int16, kRosterSize> roomSids{ -1, -1, -1, -1 };
        bool gameStarted = false;
		int gameStartCount = 0;
		int roomListNumber = -1;
		int roomMembers = -1;
        std::array<int16, kRosterSize> roster{ -1, -1, -1, -1 };
        int32 latestFrame = -1;
        std::map<int16, Vec2> positions;
        std::array<int, kRosterSize> attackedEvents{};
        std::array<int, kRosterSize> aliveEvents{};
        int attackAnimationPackets = 0;
        int bossWinEvents = 0;
    };

    struct ResultState
    {
        int room = -1;
        int hostSid = -1;
        int dutSid = -1;
        std::array<int16, kRosterSize> roster{ -1, -1, -1, -1 };
        bool gameStarted = false;
        bool firstDownThird = false;
        bool revivedFirst = false;
        bool secondDownThird = false;
        bool bossWin = false;
        bool resultScene = false;
        bool resetFirst = false;
        bool lobby = false;
		bool secondMatchStarted = false;
		bool secondMatchFrame = false;
		bool secondMatchAttack = false;
        std::uint64_t packets = 0;
    };

    class ScenarioHarness
    {
    public:
        explicit ScenarioHarness(Config config)
            : config_(std::move(config)), telemetry_(config_.telemetryFile),
              startedAt_(Clock::now()), deadline_(startedAt_ + std::chrono::seconds(config_.timeoutSeconds))
        {
            for (int i = 0; i < kBotCount; ++i) bots_[i] = std::make_unique<BotSession>(i, events_);
        }

        void Run()
        {
            Log("connecting three protocol bots");
            for (int i = 0; i < kBotCount; ++i) ConnectWithRetry(i);

            for (int i = 0; i < kBotCount; ++i) SendLogin(i);
            PumpUntil([&]
            {
                return std::all_of(states_.begin(), states_.end(), [](const BotState& state) { return state.loginOk; });
            }, "LOGIN_OK for all bots", 20s);

            SendRoomEvent(0, C_ROOM_PACKET_TYPE::ACQ_MK_RM);
            PumpUntil([&] { return states_[0].createOk && states_[0].enteredRoom >= 0; },
                "bot0 room creation", 15s);
            result_.room = states_[0].enteredRoom;
            result_.hostSid = states_[0].sid;
            WriteRoomFile(result_.room);
            Log("room " + std::to_string(result_.room) + " created; waiting for GUI DUT in slot1");

            PumpUntil([&]
            {
                if (!states_[0].roomInfoSeen) return false;
                int occupied = 0;
                int16 otherSid = -1;
                for (const int16 sid : states_[0].roomSids)
                {
                    if (sid >= 0)
                    {
                        ++occupied;
                        if (sid != states_[0].sid) otherSid = sid;
                    }
                }
                if (occupied != 2 || states_[0].roomSids[0] != states_[0].sid || otherSid < 0) return false;
                result_.dutSid = otherSid;
                return true;
            }, "GUI DUT entering as slot1", 45s);

            SendRoomEnter(1, result_.room);
            PumpUntil([&] { return states_[1].enteredRoom == result_.room && OccupiedRoomSlots(0) >= 3; },
                "bot1 entering as slot2", 15s);
            SendRoomEnter(2, result_.room);
            PumpUntil([&] { return states_[2].enteredRoom == result_.room && OccupiedRoomSlots(0) == 4; },
                "bot2 entering as slot3", 15s);

            for (int i = 0; i < kBotCount; ++i) SendRoomEvent(i, C_ROOM_PACKET_TYPE::ACQ_READY);
            PumpUntil([&]
            {
                return std::all_of(states_.begin(), states_.end(), [](const BotState& state) { return state.gameStarted; });
            }, "GAME_START on all bots (DUT must also be ready)", 45s);
            ValidateRoster();
            result_.gameStarted = true;

            PumpUntil([&]
            {
                const auto& telemetry = telemetry_.State();
                return telemetry.lastFirstSequence != 0
                    && telemetry.scene == 3
                    && telemetry.cameraMode == 1
                    && telemetry.hp == 3;
            }, "DUT initial FIRST camera and full health", 15s);

            PumpUntil([&]
            {
                return LatestFrame() >= 1 && HasPosition(result_.roster[0]) && HasPosition(result_.roster[1])
                    && HasPosition(result_.roster[2]) && HasPosition(result_.roster[3]);
            }, "initial authoritative FRAME/SPOS", 15s);

            Log("positioning helper slot2 within legitimate rescue distance");
            MoveNear(1, result_.roster[2], result_.roster[1], 1.0f, "rescue helper positioning");

            Log("positioning boss and performing first honest three-hit DUT down");
            PositionBossFor(result_.roster[1]);
            const uint64_t firstThirdBefore = telemetry_.State().lastThirdSequence;
            AttackThree(1, "first DUT down");
            PumpUntil([&] { return telemetry_.State().lastThirdSequence > firstThirdBefore; },
                "DUT THIRD camera after first down", 15s);
            result_.firstDownThird = true;
            WaitHonestCooldown("DUT DOWN-to-CRAWL settling", 25, 1s);

            EnsureDistance(result_.roster[2], result_.roster[1], kRescueClientDistance,
                "rescue helper must remain within the real client range");
            const int aliveBefore = states_[0].aliveEvents[1];
            const uint64_t firstBeforeRescue = telemetry_.State().lastFirstSequence;
            SendInteraction(1, static_cast<uint8>(EVENT_TYPE::RESCUE_PLAYER_ONE) + 1);
            PumpUntil([&]
            {
                return states_[0].aliveEvents[1] > aliveBefore
                    && telemetry_.State().lastFirstSequence > firstBeforeRescue
                    && telemetry_.State().lastFirstSequence > telemetry_.State().lastThirdSequence;
            }, "DUT-generated ALIVE and FIRST camera after rescue", 25s);
            result_.revivedFirst = true;
            WaitHonestCooldown("post-rescue STAND/invincibility settling", kHonestCooldownFrames,
                kHonestCooldownTime);

            Log("performing second honest DUT down");
            PositionBossFor(result_.roster[1]);
            const uint64_t secondThirdBefore = telemetry_.State().lastThirdSequence;
            AttackThree(1, "second DUT down");
            PumpUntil([&] { return telemetry_.State().lastThirdSequence > secondThirdBefore; },
                "DUT THIRD camera after second down", 15s);
            result_.secondDownThird = true;
            const uint64_t secondDownSequence = telemetry_.State().lastThirdSequence;
            WaitHonestCooldown("second DUT down settling", 25, 1s);

            Log("downing helper slot2 through real attack packets");
            PositionBossFor(result_.roster[2]);
            AttackThree(2, "helper slot2 down");
            WaitHonestCooldown("helper slot2 down settling", 25, 1s);

            Log("downing helper slot3; server should reach normal BOSS_WIN");
            PositionBossFor(result_.roster[3]);
            AttackThree(3, "helper slot3 down");
            PumpUntil([&] { return states_[0].bossWinEvents > 0; }, "normal server BOSS_WIN", 15s);
            result_.bossWin = true;

			PumpUntil([&]
			{
				return states_[0].roomListNumber == result_.room && states_[0].roomMembers == 0;
			}, "server emptied the first match room", 10s);

			const int createCountBefore = states_[0].createCount;
			const int hostEntryCountBefore = states_[0].roomEntryCount;
			SendRoomEvent(0, C_ROOM_PACKET_TYPE::ACQ_MK_RM);
			PumpUntil([&]
			{
				return states_[0].createCount > createCountBefore &&
					states_[0].roomEntryCount > hostEntryCountBefore;
			}, "host recreated the room for match two", 10s);
			if (states_[0].enteredRoom != result_.room)
				throw std::runtime_error("second match room number differs from the isolated first room");

            PumpUntil([&]
            {
                const auto& telemetry = telemetry_.State();
                return telemetry.lastResultSequence > secondDownSequence
                    // Some DUT observers emit RESULT and reset FIRST atomically; the current
                    // observer emits two adjacent records. Both must precede the lobby record.
                    && telemetry.lastResetFirstSequence >= telemetry.lastResultSequence
                    && telemetry.lastLobbySequence > telemetry.lastResetFirstSequence;
            }, "DUT RESULT -> FIRST(reset) -> LOBBY telemetry order", 20s);
            result_.resultScene = true;
            result_.resetFirst = true;
            result_.lobby = true;

			PumpUntil([&]
			{
				return OccupiedRoomSlots(0) == 2 && states_[0].roomSids[0] == result_.hostSid &&
					states_[0].roomSids[1] == result_.dutSid;
			}, "same GUI DUT entered match two as slot1", 15s);

			const int bot1EntriesBefore = states_[1].roomEntryCount;
			const int bot2EntriesBefore = states_[2].roomEntryCount;
			SendRoomEnter(1, result_.room);
			PumpUntil([&]
			{
				return states_[1].roomEntryCount > bot1EntriesBefore && OccupiedRoomSlots(0) >= 3;
			}, "bot1 entered match two as slot2", 10s);
			SendRoomEnter(2, result_.room);
			PumpUntil([&]
			{
				return states_[2].roomEntryCount > bot2EntriesBefore && OccupiedRoomSlots(0) == 4;
			}, "bot2 entered match two as slot3", 10s);

			for (int i = 0; i < kBotCount; ++i) SendRoomEvent(i, C_ROOM_PACKET_TYPE::ACQ_READY);
			PumpUntil([&]
			{
				return std::all_of(states_.begin(), states_.end(),
					[](const BotState& state) { return state.gameStartCount >= 2; });
			}, "second GAME_START on all protocol bots", 15s);
			ValidateRoster();
			PumpUntil([&]
			{
				return LatestFrame() >= 1 && HasPosition(result_.roster[0]) && HasPosition(result_.roster[1]) &&
					HasPosition(result_.roster[2]) && HasPosition(result_.roster[3]);
			}, "match two authoritative FRAME/SPOS", 10s);
			result_.secondMatchFrame = true;

			PumpUntil([&]
			{
				const auto& telemetry = telemetry_.State();
				return telemetry.gameStartCount >= 2 && telemetry.lastSecondMatchSequence != 0 &&
					telemetry.scene == 3 && telemetry.cameraMode == 1 && telemetry.hp == 3 &&
					telemetry.behavior == 0 && telemetry.hidden == false;
			}, "same GUI DUT completed a reset match start", 15s);
			result_.secondMatchStarted = true;

			PositionBossFor(result_.roster[1]);
			const int secondMatchAckBefore = states_[0].attackedEvents[1];
			const int secondMatchAnimationBefore =
				states_[1].attackAnimationPackets + states_[2].attackAnimationPackets;
			SendAttack(1);
			PumpUntil([&]
			{
				return states_[0].attackedEvents[1] > secondMatchAckBefore &&
					states_[1].attackAnimationPackets + states_[2].attackAnimationPackets >
					secondMatchAnimationBefore;
			}, "match two authoritative attack ack/animation", 8s);
			result_.secondMatchAttack = true;
            Log("scenario passed");
        }

        const ResultState& Result() const { return result_; }
        long long ElapsedMilliseconds() const
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - startedAt_).count();
        }

    private:
        void Log(const std::string& message) const
        {
            std::cout << "[ProtocolScenarioHarness] " << message << std::endl;
        }

        void ConnectWithRetry(const int bot)
        {
            std::string lastError;
            int attempts = 0;
            while (Clock::now() < deadline_)
            {
                ++attempts;
                if (bots_[bot]->ConnectOnce(config_.host, config_.port, lastError))
                {
                    Log("bot" + std::to_string(bot) + " connected after " + std::to_string(attempts)
                        + " attempt(s), including 255 ConnectEx compatibility bytes");
                    return;
                }
                std::this_thread::sleep_for(250ms);
            }
            throw std::runtime_error("bot" + std::to_string(bot) + " connection timed out: " + lastError);
        }

        void SendLogin(const int bot)
        {
            C2S_LOGIN packet{};
            packet.size = sizeof(packet);
            packet.type = static_cast<uint8>(C_TITLE_PACKET_TYPE::ACQ_LOGIN);
            const std::wstring name = L"e2ebot" + std::to_wstring(bot);
            const std::wstring password = L"test" + std::to_wstring(bot);
            ::wcsncpy_s(packet.name, std::size(packet.name), name.c_str(), _TRUNCATE);
            ::wcsncpy_s(packet.pw, std::size(packet.pw), password.c_str(), _TRUNCATE);
            bots_[bot]->SendPacket(packet);
        }

        void SendRoomEvent(const int bot, const C_ROOM_PACKET_TYPE type)
        {
            C2S_ROOM_EVENT packet{};
            packet.size = sizeof(packet);
            packet.type = static_cast<uint8>(type);
            bots_[bot]->SendPacket(packet);
        }

        void SendRoomEnter(const int bot, const int room)
        {
            C2S_ROOM_ENTER packet{};
            packet.size = sizeof(packet);
            packet.type = static_cast<uint8>(C_ROOM_PACKET_TYPE::ACQ_ENTER_RM);
            packet.rmNum = room;
            bots_[bot]->SendPacket(packet);
        }

        void SendKey(const int bot, const int slot, const uint8 key, const Vec2 look)
        {
            C2S_KEY packet{};
            packet.size = sizeof(packet);
            packet.type = static_cast<uint8>(C_GAME_PACKET_TYPE::CKEY);
            packet.key = key;
            packet.x = look.x;
            packet.z = look.z;
            packet.idx = static_cast<int8>(slot);
            bots_[bot]->SendPacket(packet);
        }

        void SendAttack(const int targetSlot)
        {
            const int32 frame = LatestFrame();
            if (frame < 0) throw std::runtime_error("refusing to attack without a real server FRAME");

            SC_EVENTPACKET animation{};
            animation.size = sizeof(animation);
            animation.type = static_cast<uint8>(SC_GAME_PACKET_TYPE::GAMEEVENT);
            animation.eventId = static_cast<uint8>(EVENT_TYPE::ATTACK_ANIM);
            bots_[0]->SendPacket(animation);

            C2S_ATTACK attack{};
            attack.size = sizeof(attack);
            attack.type = static_cast<uint8>(C_GAME_PACKET_TYPE::CATTACK);
            attack.tidx = static_cast<int16>(targetSlot);
            attack.wf = frame;
            bots_[0]->SendPacket(attack);
        }

        void SendInteraction(const int bot, const uint8 eventId)
        {
            // This method is intentionally used only for RESCUE. The harness contains no path
            // that can synthesize ALIVE, generator activation, EXIT, EMP_WIN, or BOSS_WIN.
            const uint8 firstRescue = static_cast<uint8>(EVENT_TYPE::RESCUE_PLAYER_ONE);
            const uint8 lastRescue = static_cast<uint8>(EVENT_TYPE::RESCUE_PLAYER_FOUR);
            if (eventId < firstRescue || eventId > lastRescue)
                throw std::runtime_error("refusing to send a non-rescue synthetic interaction event");
            SC_EVENTPACKET packet{};
            packet.size = sizeof(packet);
            packet.type = static_cast<uint8>(SC_GAME_PACKET_TYPE::GAMEEVENT);
            packet.eventId = eventId;
            bots_[bot]->SendPacket(packet);
        }

        void WriteRoomFile(const int room) const
        {
            EnsureParentDirectory(config_.roomFile);
            std::ofstream output(config_.roomFile, std::ios::binary | std::ios::trunc);
            if (!output) throw std::runtime_error("failed to open --room-file");
            output << room << '\n';
            output.flush();
            if (!output) throw std::runtime_error("failed to write --room-file");
        }

        void ProcessQueueEvent(const QueueEvent& event)
        {
            if (event.kind != QueueEventKind::Packet)
            {
                std::string message = "bot" + std::to_string(event.bot) + " ";
                if (!event.message.empty()) message += event.message;
                else if (event.kind == QueueEventKind::Disconnected) message += "disconnected";
                else message += "network/protocol error";
                if (event.error != 0) message += " (" + std::to_string(event.error) + ")";
                throw std::runtime_error(message);
            }
            if (event.bot < 0 || event.bot >= kBotCount || event.packet.size() < 2)
                throw std::runtime_error("invalid queued packet event");

            ++result_.packets;
            const uint8 type = event.packet[1];
            const std::size_t expected = ExpectedServerPacketSize(type);
            if (expected == 0) throw std::runtime_error("unknown server packet type " + std::to_string(type));
            if (event.packet.size() != expected)
                throw std::runtime_error("server packet type " + std::to_string(type) + " has size "
                    + std::to_string(event.packet.size()) + ", expected " + std::to_string(expected));

            BotState& state = states_[event.bot];
            switch (type)
            {
            case static_cast<uint8>(S_TITLE_PACKET_TYPE::LOGIN_OK):
            {
                const auto packet = Decode<S2C_LOGIN_OK>(event.packet);
                state.loginOk = true;
                state.sid = packet.sid;
                break;
            }
            case static_cast<uint8>(S_TITLE_PACKET_TYPE::LOGIN_FAIL):
                throw std::runtime_error("bot" + std::to_string(event.bot) + " login failed");
            case static_cast<uint8>(S_ROOM_PACKET_TYPE::MK_RM_OK):
                state.createOk = true;
				++state.createCount;
                break;
            case static_cast<uint8>(S_ROOM_PACKET_TYPE::MK_RM_FAIL):
                throw std::runtime_error("room creation failed");
            case static_cast<uint8>(S_ROOM_PACKET_TYPE::REP_ENTER_OK):
            {
                const auto packet = Decode<S2C_ROOM_ENTER>(event.packet);
                state.enteredRoom = packet.rmNum;
				++state.roomEntryCount;
                break;
            }
            case static_cast<uint8>(S_ROOM_PACKET_TYPE::REP_ENTER_FAIL):
                throw std::runtime_error("bot" + std::to_string(event.bot) + " room entry failed");
            case static_cast<uint8>(S_ROOM_PACKET_TYPE::ROOM_INFO):
            {
                const auto packet = Decode<S2C_ROOM_INFO>(event.packet);
                std::copy(std::begin(packet.sids), std::end(packet.sids), state.roomSids.begin());
                state.roomInfoSeen = true;
                break;
            }
			case static_cast<uint8>(S_ROOM_PACKET_TYPE::UPDATE_LIST):
			{
				const auto packet = Decode<S2C_ROOM_LIST>(event.packet);
				state.roomListNumber = packet.rmNum;
				state.roomMembers = packet.member;
				break;
			}
            case static_cast<uint8>(S_ROOM_PACKET_TYPE::GAME_START):
            {
                const auto packet = Decode<S2C_GAMESTART>(event.packet);
                std::copy(std::begin(packet.sids), std::end(packet.sids), state.roster.begin());
                state.gameStarted = true;
				++state.gameStartCount;
				if (event.bot == 0 && state.gameStartCount == 2)
				{
					latestFrame_ = -1;
					positions_.clear();
				}
                break;
            }
            case static_cast<uint8>(S_GAME_PACKET_TYPE::SPOS):
            {
                const auto packet = Decode<S2C_POS>(event.packet);
                state.positions[packet.sid] = { packet.x, packet.z };
                positions_[packet.sid] = { packet.x, packet.z };
                break;
            }
            case static_cast<uint8>(S_GAME_PACKET_TYPE::FRAME):
            {
                const auto packet = Decode<S2C_FRAMEPACKET>(event.packet);
                state.latestFrame = std::max(state.latestFrame, packet.wf);
                latestFrame_ = std::max(latestFrame_, packet.wf);
                break;
            }
            case static_cast<uint8>(S_GAME_PACKET_TYPE::ANIM):
            {
                const auto packet = Decode<S2C_ANIMPACKET>(event.packet);
                if (packet.track == static_cast<uint8>(ANIMTRACK::ATTACK_ANIM)) ++state.attackAnimationPackets;
                break;
            }
            case static_cast<uint8>(SC_GAME_PACKET_TYPE::GAMEEVENT):
            {
                const auto packet = Decode<SC_EVENTPACKET>(event.packet);
                const int eventId = packet.eventId;
                const int firstAttacked = static_cast<int>(EVENT_TYPE::ATTACKED_PLAYER_ONE);
                const int lastAttacked = static_cast<int>(EVENT_TYPE::ATTACKED_PLAYER_FOUR);
                const int firstAlive = static_cast<int>(EVENT_TYPE::ALIVE_PLAYER_ONE);
                const int lastAlive = static_cast<int>(EVENT_TYPE::ALIVE_PLAYER_FOUR);
                if (eventId >= firstAttacked && eventId <= lastAttacked)
                    ++state.attackedEvents[eventId - firstAttacked];
                if (eventId >= firstAlive && eventId <= lastAlive)
                    ++state.aliveEvents[eventId - firstAlive];
                if (eventId == static_cast<int>(EVENT_TYPE::BOSS_WIN)) ++state.bossWinEvents;
                break;
            }
            default:
                // Valid but irrelevant packets (room lists, ready echoes, SKEY, SROT, chat) are
                // intentionally accepted so TCP ordering remains observable without overfitting.
                break;
            }
        }

        template <typename Predicate>
        void PumpUntil(Predicate predicate, const std::string& description,
            const std::chrono::steady_clock::duration stepTimeout)
        {
            const auto stepDeadline = std::min(deadline_, Clock::now() + stepTimeout);
            while (!predicate())
            {
                if (Clock::now() >= deadline_)
                    throw std::runtime_error("overall timeout while waiting for " + description);
                if (Clock::now() >= stepDeadline)
                    throw std::runtime_error("step timeout while waiting for " + description);

                telemetry_.Poll();
                if (const auto event = events_.WaitPop(25ms)) ProcessQueueEvent(*event);
                while (const auto event = events_.TryPop()) ProcessQueueEvent(*event);
            }
            telemetry_.Poll();
            Log("observed: " + description);
        }

        int OccupiedRoomSlots(const int bot) const
        {
            if (!states_[bot].roomInfoSeen) return 0;
            return static_cast<int>(std::count_if(states_[bot].roomSids.begin(), states_[bot].roomSids.end(),
                [](const int16 sid) { return sid >= 0; }));
        }

        void ValidateRoster()
        {
            const auto roster = states_[0].roster;
            for (int bot = 1; bot < kBotCount; ++bot)
                if (states_[bot].roster != roster) throw std::runtime_error("GAME_START rosters differ between bots");

            if (roster[0] != states_[0].sid) throw std::runtime_error("bot0 is not authoritative boss slot0");
            if (roster[1] != result_.dutSid) throw std::runtime_error("GUI DUT did not enter authoritative slot1");
            if (roster[2] != states_[1].sid || roster[3] != states_[2].sid)
                throw std::runtime_error("helper bot room slots do not match join order");
            std::array<int16, kRosterSize> sorted = roster;
            std::sort(sorted.begin(), sorted.end());
            if (sorted.front() < 0 || std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end())
                throw std::runtime_error("GAME_START roster contains invalid or duplicate sids");
            result_.roster = roster;
            Log("validated roster boss=" + std::to_string(roster[0]) + " dut=" + std::to_string(roster[1])
                + " helper2=" + std::to_string(roster[2]) + " helper3=" + std::to_string(roster[3]));
        }

        int32 LatestFrame() const { return latestFrame_; }
        bool HasPosition(const int16 sid) const { return positions_.contains(sid); }

        Vec2 Position(const int16 sid) const
        {
            const auto found = positions_.find(sid);
            if (found == positions_.end()) throw std::runtime_error("missing authoritative position for sid " + std::to_string(sid));
            return found->second;
        }

        void EnsureDistance(const int16 a, const int16 b, const float maximum, const std::string& message) const
        {
            const float distance = Length(Subtract(Position(a), Position(b)));
            if (distance > maximum)
                throw std::runtime_error(message + ": distance=" + std::to_string(distance)
                    + ", maximum=" + std::to_string(maximum));
        }

        void MoveNear(const int bot, const int16 actorSid, const int16 targetSid,
            const float desiredDistance, const std::string& description)
        {
            PumpUntil([&] { return HasPosition(actorSid) && HasPosition(targetSid) && LatestFrame() >= 0; },
                description + " initial pose", 10s);

            Vec2 delta = Subtract(Position(targetSid), Position(actorSid));
            Vec2 look = Normalize(delta);
            if (Length(delta) > desiredDistance)
            {
                SendKey(bot, RosterSlot(actorSid), kKeyForward, look);
                PumpUntil([&]
                {
                    return Length(Subtract(Position(targetSid), Position(actorSid))) <= desiredDistance;
                }, description + " movement", 25s);
            }

            delta = Subtract(Position(targetSid), Position(actorSid));
            look = Normalize(delta);
            SendKey(bot, RosterSlot(actorSid), 0, look);
            if (actorSid == result_.roster[0]) bossLook_ = look;

            const int32 stoppedAtFrame = LatestFrame();
            PumpUntil([&] { return LatestFrame() >= stoppedAtFrame + 2; }, description + " stable history pose", 5s);
            const float distance = Length(Subtract(Position(targetSid), Position(actorSid)));
            if (distance > desiredDistance + 0.75f)
                throw std::runtime_error(description + " drifted after stop: distance=" + std::to_string(distance));
        }

        int RosterSlot(const int16 sid) const
        {
            const auto found = std::find(result_.roster.begin(), result_.roster.end(), sid);
            if (found == result_.roster.end()) throw std::runtime_error("sid is not in GAME_START roster");
            return static_cast<int>(std::distance(result_.roster.begin(), found));
        }

        void PositionBossFor(const int16 targetSid)
        {
            MoveNear(0, result_.roster[0], targetSid, 3.5f, "boss positioning");
            const Vec2 toTarget = Subtract(Position(targetSid), Position(result_.roster[0]));
            bossLook_ = Normalize(toTarget);
            SendKey(0, 0, 0, bossLook_);
            const int32 directionFrame = LatestFrame();
            PumpUntil([&] { return LatestFrame() >= directionFrame + 1; }, "boss direction recorded in history", 5s);
            ValidateHonestAttackGeometry(targetSid);
        }

        void ValidateHonestAttackGeometry(const int16 targetSid) const
        {
            if (states_[0].roster[0] != states_[0].sid)
                throw std::runtime_error("refusing attack because sender is not GAME_START boss slot0");
            const Vec2 relative = Subtract(Position(targetSid), Position(result_.roster[0]));
            const float projection = Dot(relative, bossLook_);
            const float perpendicular = CrossMagnitude(relative, bossLook_);
            if (projection < 0.0f || projection > kBossClientRayLimit - 0.25f || perpendicular > 0.75f)
            {
                throw std::runtime_error("refusing attack outside real GUI ray: projection="
                    + std::to_string(projection) + ", perpendicular=" + std::to_string(perpendicular));
            }
        }

        void AttackThree(const int targetSlot, const std::string& description)
        {
            if (targetSlot <= 0 || targetSlot >= kRosterSize)
                throw std::runtime_error("refusing to attack a non-employee roster slot");
            const int16 targetSid = result_.roster[targetSlot];
            for (int hit = 1; hit <= 3; ++hit)
            {
                ValidateHonestAttackGeometry(targetSid);
                const int ackBefore = states_[0].attackedEvents[targetSlot];
                const int animationBefore = states_[1].attackAnimationPackets + states_[2].attackAnimationPackets;
                const int32 attackFrame = LatestFrame();
                if (attackFrame < 0) throw std::runtime_error("refusing stale/future attack without FRAME");
                SendAttack(targetSlot);
                PumpUntil([&]
                {
                    return states_[0].attackedEvents[targetSlot] > ackBefore
                        && states_[1].attackAnimationPackets + states_[2].attackAnimationPackets > animationBefore;
                }, description + " hit " + std::to_string(hit) + " authoritative ack/animation", 8s);

                // The current server merely logs packets older than five frames instead of rejecting
                // them. We sent the newest observed frame immediately and reject pathological lag here.
                if (LatestFrame() - attackFrame > 5)
                    throw std::runtime_error(description + " attack ack exceeded the honest five-frame rewind window");
                if (hit < 3) WaitHonestCooldown(description + " hit cooldown", kHonestCooldownFrames,
                    kHonestCooldownTime);
            }
        }

        void WaitHonestCooldown(const std::string& description, const int frames,
            const Clock::duration duration)
        {
            const int32 startFrame = LatestFrame();
            const auto startTime = Clock::now();
            const Clock::duration minimumTimeout = 10s;
            const Clock::duration requestedTimeout = duration + 8s;
            PumpUntil([&]
            {
                return LatestFrame() >= startFrame + frames && Clock::now() - startTime >= duration;
            }, description, std::max(minimumTimeout, requestedTimeout));
        }

        Config config_;
        EventQueue events_;
        std::array<std::unique_ptr<BotSession>, kBotCount> bots_;
        std::array<BotState, kBotCount> states_;
        std::map<int16, Vec2> positions_;
        int32 latestFrame_ = -1;
        Vec2 bossLook_{ 1.0f, 0.0f };
        TelemetryTail telemetry_;
        Clock::time_point startedAt_;
        Clock::time_point deadline_;
        ResultState result_;
    };

    void WriteResult(const std::filesystem::path& file, const std::string& status, const int exitCode,
        const std::string& reason, const ResultState& result, const long long elapsedMilliseconds)
    {
        EnsureParentDirectory(file);
        std::ofstream output(file, std::ios::binary | std::ios::trunc);
        if (!output) return;
        output << "{\n"
            << "  \"schema\": \"atb.protocol-scenario.v1\",\n"
            << "  \"status\": \"" << JsonEscape(status) << "\",\n"
            << "  \"exit_code\": " << exitCode << ",\n"
            << "  \"reason\": \"" << JsonEscape(reason) << "\",\n"
            << "  \"elapsed_ms\": " << elapsedMilliseconds << ",\n"
            << "  \"room\": " << result.room << ",\n"
            << "  \"host_sid\": " << result.hostSid << ",\n"
            << "  \"dut_sid\": " << result.dutSid << ",\n"
            << "  \"roster\": [" << result.roster[0] << ", " << result.roster[1] << ", "
            << result.roster[2] << ", " << result.roster[3] << "],\n"
            << "  \"game_start\": " << (result.gameStarted ? "true" : "false") << ",\n"
            << "  \"first_down_third\": " << (result.firstDownThird ? "true" : "false") << ",\n"
            << "  \"revived_first\": " << (result.revivedFirst ? "true" : "false") << ",\n"
            << "  \"second_down_third\": " << (result.secondDownThird ? "true" : "false") << ",\n"
            << "  \"boss_win\": " << (result.bossWin ? "true" : "false") << ",\n"
            << "  \"result_scene\": " << (result.resultScene ? "true" : "false") << ",\n"
            << "  \"reset_first\": " << (result.resetFirst ? "true" : "false") << ",\n"
            << "  \"lobby\": " << (result.lobby ? "true" : "false") << ",\n"
			<< "  \"second_match_started\": " << (result.secondMatchStarted ? "true" : "false") << ",\n"
			<< "  \"second_match_frame\": " << (result.secondMatchFrame ? "true" : "false") << ",\n"
			<< "  \"second_match_attack\": " << (result.secondMatchAttack ? "true" : "false") << ",\n"
            << "  \"packets\": " << result.packets << "\n"
            << "}\n";
    }
}

int wmain(const int argc, wchar_t** argv)
{
    Config config;
    ResultState result;
    const auto processStartedAt = Clock::now();
    try
    {
        config = ParseArgs(argc, argv);
        WinsockScope winsock;
        ScenarioHarness harness(config);
        try
        {
            harness.Run();
        }
        catch (...)
        {
            // Preserve partial progress in the failure artifact before stack unwinding closes the
            // sockets and tears down Winsock.
            result = harness.Result();
            throw;
        }
        result = harness.Result();
        WriteResult(config.resultFile, "passed", 0, "ok", result, harness.ElapsedMilliseconds());
        return 0;
    }
    catch (const std::exception& error)
    {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - processStartedAt).count();
        std::cerr << "[ProtocolScenarioHarness] FAILED: " << error.what() << std::endl;
        if (!config.resultFile.empty()) WriteResult(config.resultFile, "failed", 1, error.what(), result, elapsed);
        return 1;
    }
}
