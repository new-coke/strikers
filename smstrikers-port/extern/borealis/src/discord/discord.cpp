#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "borealis/discord.hpp"
#include "discord_protocol.hpp"

#include "borealis/log.hpp"
#include "nlohmann/json.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMCX
#define NOSERVICE
#define NOIME
#include <windows.h>
#else
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace borealis::discord {
namespace {

using json = nlohmann::json;
using detail::Frame;
using detail::Opcode;

constexpr Log Log{"borealis::discord"};

constexpr auto kIoWait = std::chrono::milliseconds(500);
constexpr auto kShutdownClearTimeout = std::chrono::milliseconds(200);
constexpr auto kInitialReconnectDelay = std::chrono::milliseconds(500);
constexpr auto kMaxReconnectDelay = std::chrono::milliseconds(60 * 1000);

enum class ConnectionState {
    Disconnected,
    SentHandshake,
    Connected,
};

enum class DisconnectCode : int {
    PipeClosed = 1,
    ReadCorrupt = 2,
    BadFrame = 3,
};

struct QueuedEvent {
    enum class Type {
        Ready,
        Disconnected,
        Error,
        ActivityJoin,  // smstrikers-port: `message` holds the join secret.
    };

    Type type = Type::Ready;
    User user;
    int code = 0;
    std::string message;
};

class Backoff {
public:
    std::chrono::milliseconds next_delay() {
        const auto delay = currentDelay;
        currentDelay = std::min(currentDelay * 2, kMaxReconnectDelay);
        return delay;
    }

    void reset() { currentDelay = kInitialReconnectDelay; }

private:
    std::chrono::milliseconds currentDelay = kInitialReconnectDelay;
};

class IpcConnection {
public:
    IpcConnection() = default;
    ~IpcConnection() { close(); }

    IpcConnection(const IpcConnection&) = delete;
    IpcConnection& operator=(const IpcConnection&) = delete;

    bool open() {
#ifdef _WIN32
        wchar_t pipeName[] = L"\\\\?\\pipe\\discord-ipc-0";
        constexpr size_t kPipeDigit = sizeof(pipeName) / sizeof(wchar_t) - 2;

        for (wchar_t pipeNumber = L'0'; pipeNumber <= L'9'; ++pipeNumber) {
            pipeName[kPipeDigit] = pipeNumber;
            pipe = CreateFileW(
                pipeName, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
            if (pipe != INVALID_HANDLE_VALUE) {
                return true;
            }

            if (GetLastError() == ERROR_PIPE_BUSY && WaitNamedPipeW(pipeName, 10000)) {
                --pipeNumber;
            }
        }
        return false;
#else
        // smstrikers-port: sandboxed clients' folders, as the discord-rich-presence crate has them.
        static constexpr const char* kClientFolders[] = {"", "/app/com.discordapp.Discord",
            "/app/dev.vencord.Vesktop", "/.flatpak/com.discordapp.Discord/xdg-run",
            "/.flatpak/dev.vencord.Vesktop/xdg-run", "/snap.discord-canary", "/snap.discord"};
        for (const std::string& tempPath : get_temp_paths()) {
            for (const char* clientFolder : kClientFolders) {
                for (int pipeNumber = 0; pipeNumber < 10; ++pipeNumber) {
                    if (connect_to(tempPath + clientFolder + "/discord-ipc-" +
                                   std::to_string(pipeNumber))) {
                        return true;
                    }
                }
            }
        }
        return false;
#endif
    }

    void close() {
#ifdef _WIN32
        if (pipe != INVALID_HANDLE_VALUE) {
            CloseHandle(pipe);
            pipe = INVALID_HANDLE_VALUE;
        }
#else
        if (socketFd != -1) {
            ::close(socketFd);
            socketFd = -1;
        }
#endif
        pendingRead.clear();
    }

    bool is_open() const {
#ifdef _WIN32
        return pipe != INVALID_HANDLE_VALUE;
#else
        return socketFd != -1;
#endif
    }

    bool write_frame(const Frame& frame) {
        const std::vector<uint8_t> bytes = detail::encode_frame(frame);
        return !bytes.empty() && write_all(bytes.data(), bytes.size());
    }

    enum class ReadStatus {
        None,
        Frame,
        Closed,
        Corrupt,
    };

    ReadStatus read_frame(Frame& frame) {
        read_available();
        switch (detail::decode_frame(pendingRead, frame)) {
        case detail::DecodeStatus::Frame:
            return ReadStatus::Frame;
        case detail::DecodeStatus::Corrupt:
            return ReadStatus::Corrupt;
        case detail::DecodeStatus::None:
        default:
            return is_open() ? ReadStatus::None : ReadStatus::Closed;
        }
    }

private:
#ifndef _WIN32
    static std::vector<std::string> get_temp_paths() {
        std::vector<std::string> paths;
        for (const char* name : {"XDG_RUNTIME_DIR", "TMPDIR", "TMP", "TEMP"}) {
            if (const char* value = std::getenv(name); value && value[0] != '\0') {
                if (std::find(paths.begin(), paths.end(), value) == paths.end()) {
                    paths.emplace_back(value);
                }
            }
        }
        if (std::find(paths.begin(), paths.end(), "/tmp") == paths.end()) {
            paths.emplace_back("/tmp");
        }
        return paths;
    }

    bool connect_to(const std::string& socketPath) {
        sockaddr_un pipeAddress{};
        pipeAddress.sun_family = AF_UNIX;
        if (socketPath.size() >= sizeof(pipeAddress.sun_path)) {
            return false;
        }
        socketFd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (socketFd == -1) {
            return false;
        }
        std::strncpy(pipeAddress.sun_path, socketPath.c_str(), sizeof(pipeAddress.sun_path) - 1);
        if (connect(socketFd, reinterpret_cast<const sockaddr*>(&pipeAddress),
                sizeof(pipeAddress)) == 0)
        {
            fcntl(socketFd, F_SETFL, O_NONBLOCK);
#ifdef SO_NOSIGPIPE
            int optval = 1;
            setsockopt(socketFd, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval));
#endif
            return true;
        }
        close();
        return false;
    }
#endif

    bool write_all(const uint8_t* data, size_t length) {
        size_t written = 0;
        while (written < length) {
#ifdef _WIN32
            DWORD bytesWritten = 0;
            if (WriteFile(pipe, data + written, static_cast<DWORD>(length - written), &bytesWritten,
                    nullptr) == FALSE ||
                bytesWritten == 0)
            {
                close();
                return false;
            }
            written += bytesWritten;
#else
#ifdef MSG_NOSIGNAL
            constexpr int kMsgFlags = MSG_NOSIGNAL;
#else
            constexpr int kMsgFlags = 0;
#endif
            const ssize_t bytesWritten =
                send(socketFd, data + written, length - written, kMsgFlags);
            if (bytesWritten < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }
                close();
                return false;
            }
            if (bytesWritten == 0) {
                close();
                return false;
            }
            written += static_cast<size_t>(bytesWritten);
#endif
        }
        return true;
    }

    bool read_available() {
        std::array<uint8_t, 4096> buffer{};
        bool readAny = false;

        for (;;) {
#ifdef _WIN32
            DWORD bytesAvailable = 0;
            if (PeekNamedPipe(pipe, nullptr, 0, nullptr, &bytesAvailable, nullptr) == FALSE) {
                close();
                return readAny;
            }
            if (bytesAvailable == 0) {
                return readAny;
            }

            const DWORD bytesToRead =
                std::min<DWORD>(bytesAvailable, static_cast<DWORD>(buffer.size()));
            DWORD bytesRead = 0;
            if (ReadFile(pipe, buffer.data(), bytesToRead, &bytesRead, nullptr) == FALSE) {
                close();
                return readAny;
            }
            if (bytesRead == 0) {
                close();
                return readAny;
            }
            pendingRead.insert(pendingRead.end(), buffer.begin(), buffer.begin() + bytesRead);
            readAny = true;
#else
#ifdef MSG_NOSIGNAL
            constexpr int kMsgFlags = MSG_NOSIGNAL;
#else
            constexpr int kMsgFlags = 0;
#endif
            const ssize_t bytesRead = recv(socketFd, buffer.data(), buffer.size(), kMsgFlags);
            if (bytesRead < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return readAny;
                }
                close();
                return readAny;
            }
            if (bytesRead == 0) {
                close();
                return readAny;
            }
            pendingRead.insert(pendingRead.end(), buffer.begin(), buffer.begin() + bytesRead);
            readAny = true;
#endif
        }
    }

#ifdef _WIN32
    HANDLE pipe = INVALID_HANDLE_VALUE;
#else
    int socketFd = -1;
#endif
    std::vector<uint8_t> pendingRead;
};

int current_process_id() {
#ifdef _WIN32
    return static_cast<int>(GetCurrentProcessId());
#else
    return static_cast<int>(getpid());
#endif
}

std::string next_nonce() {
    static std::atomic_uint64_t sNonce{1};
    return std::to_string(sNonce.fetch_add(1));
}

const json* find_member(const json& object, const char* key) {
    if (!object.is_object()) {
        return nullptr;
    }

    const auto member = object.find(key);
    if (member == object.end()) {
        return nullptr;
    }
    return &*member;
}

std::string json_string_member(const json& object, const char* key) {
    const json* member = find_member(object, key);
    if (!member || !member->is_string()) {
        return {};
    }
    return member->get<std::string>();
}

int json_int_member(const json& object, const char* key, int defaultValue) {
    const json* member = find_member(object, key);
    if (!member || !member->is_number_integer()) {
        return defaultValue;
    }

    try {
        return member->get<int>();
    } catch (const json::exception&) {
        return defaultValue;
    }
}

class Client {
public:
    ~Client() { shutdown(); }

    void initialize(std::string applicationId, EventHandlers handlers) {
        shutdown();

        {
            std::lock_guard lock(mutex);
            this->applicationId = std::move(applicationId);
            this->handlers = std::move(handlers);
            shouldRun = true;
            presenceState = {};
            sentInitialConnectLog = false;
        }

        ioThread = std::thread([this] { io_loop(); });
    }

    void run_callbacks() {
        std::deque<QueuedEvent> events;
        EventHandlers localHandlers;
        {
            std::lock_guard lock(mutex);
            events.swap(queuedEvents);
            localHandlers = handlers;
        }

        for (const QueuedEvent& event : events) {
            switch (event.type) {
            case QueuedEvent::Type::Ready:
                if (localHandlers.ready) {
                    localHandlers.ready(event.user);
                }
                break;
            case QueuedEvent::Type::Disconnected:
                if (localHandlers.disconnected) {
                    localHandlers.disconnected(event.code, event.message);
                }
                break;
            case QueuedEvent::Type::Error:
                if (localHandlers.error) {
                    localHandlers.error(event.code, event.message);
                }
                break;
            case QueuedEvent::Type::ActivityJoin:
                if (localHandlers.activityJoin) {
                    localHandlers.activityJoin(event.message);
                }
                break;
            }
        }
    }

    bool update_presence(Presence presence) {
        bool changed = false;
        {
            std::lock_guard lock(mutex);
            if (!shouldRun) {
                return false;
            }
            changed = presenceState.update(std::move(presence));
        }
        if (changed) {
            cv.notify_all();
        }
        return changed;
    }

    bool clear_presence() {
        bool changed = false;
        {
            std::lock_guard lock(mutex);
            if (!shouldRun) {
                return false;
            }
            changed = presenceState.clear();
        }
        if (changed) {
            cv.notify_all();
        }
        return changed;
    }

    void shutdown() {
        {
            std::lock_guard lock(mutex);
            if (!shouldRun && !ioThread.joinable()) {
                return;
            }
            shouldRun = false;
        }
        cv.notify_all();

        if (ioThread.joinable()) {
            ioThread.join();
        }

        std::lock_guard lock(mutex);
        presenceState = {};
        queuedEvents.clear();
        handlers = {};
        applicationId.clear();
    }

private:
    void io_loop() {
        IpcConnection connection;
        ConnectionState state = ConnectionState::Disconnected;
        Backoff reconnectBackoff;
        auto nextConnect = std::chrono::steady_clock::now();
        const int pid = current_process_id();
        std::string localApplicationId;

        for (;;) {
            {
                std::unique_lock lock(mutex);
                if (!shouldRun) {
                    break;
                }
                localApplicationId = applicationId;
            }

            const auto now = std::chrono::steady_clock::now();
            if (state == ConnectionState::Disconnected && now >= nextConnect) {
                if (connection.open()) {
                    if (connection.write_frame(detail::make_handshake_frame(localApplicationId))) {
                        state = ConnectionState::SentHandshake;
                    } else {
                        connection.close();
                    }
                }

                if (state == ConnectionState::Disconnected) {
                    log_waiting_for_discord_once();
                    nextConnect = now + reconnectBackoff.next_delay();
                }
            }

            if (state != ConnectionState::Disconnected) {
                process_reads(connection, state, reconnectBackoff, nextConnect);
            }

            if (state == ConnectionState::Connected) {
                flush_pending_presence(connection, pid);
            }

            std::unique_lock lock(mutex);
            if (!shouldRun) {
                break;
            }
            cv.wait_for(lock, kIoWait);
        }

        flush_shutdown_clear(connection, state, pid);
        connection.close();
    }

    void process_reads(IpcConnection& connection, ConnectionState& state, Backoff& reconnectBackoff,
        std::chrono::steady_clock::time_point& nextConnect) {
        for (;;) {
            Frame frame;
            const auto status = connection.read_frame(frame);
            if (status == IpcConnection::ReadStatus::None) {
                return;
            }
            if (status == IpcConnection::ReadStatus::Closed) {
                handle_disconnect(connection, state, reconnectBackoff, nextConnect,
                    static_cast<int>(DisconnectCode::PipeClosed), "Pipe closed");
                return;
            }
            if (status == IpcConnection::ReadStatus::Corrupt) {
                handle_disconnect(connection, state, reconnectBackoff, nextConnect,
                    static_cast<int>(DisconnectCode::ReadCorrupt), "Oversized Discord IPC frame");
                return;
            }

            switch (frame.opcode) {
            case Opcode::Frame:
                process_json_frame(frame.payload, connection, state, reconnectBackoff);
                break;
            case Opcode::Close:
                process_close_frame(
                    frame.payload, connection, state, reconnectBackoff, nextConnect);
                return;
            case Opcode::Ping:
                connection.write_frame({Opcode::Pong, frame.payload});
                break;
            case Opcode::Pong:
                break;
            case Opcode::Handshake:
            default:
                handle_disconnect(connection, state, reconnectBackoff, nextConnect,
                    static_cast<int>(DisconnectCode::BadFrame), "Bad Discord IPC frame");
                return;
            }
        }
    }

    void process_json_frame(const std::string& payload, IpcConnection& connection,
        ConnectionState& state, Backoff& reconnectBackoff) {
        json message;
        try {
            message = json::parse(payload);
        } catch (const json::parse_error&) {
            enqueue_error(
                static_cast<int>(DisconnectCode::ReadCorrupt), "Invalid Discord IPC JSON");
            return;
        }

        const std::string cmd = json_string_member(message, "cmd");
        const std::string evt = json_string_member(message, "evt");

        if (state == ConnectionState::SentHandshake && cmd == "DISPATCH" && evt == "READY") {
            state = ConnectionState::Connected;
            reconnectBackoff.reset();
            {
                std::lock_guard lock(mutex);
                presenceState.mark_dirty();
            }
            // smstrikers-port: subscriptions last one connection, so every READY asks again.
            connection.write_frame(detail::make_subscribe_frame(next_nonce(), "ACTIVITY_JOIN"));
            enqueue_ready(message);
            return;
        }

        if (cmd == "DISPATCH" && evt == "ACTIVITY_JOIN") {
            const json* data = find_member(message, "data");
            std::lock_guard lock(mutex);
            QueuedEvent event;
            event.type = QueuedEvent::Type::ActivityJoin;
            event.message = data ? json_string_member(*data, "secret") : std::string{};
            queuedEvents.push_back(std::move(event));
            return;
        }

        if (evt == "ERROR") {
            const json* data = find_member(message, "data");
            enqueue_error(data ? json_int_member(*data, "code", 0) : 0,
                data ? json_string_member(*data, "message") : std::string{});
        }
    }

    void process_close_frame(const std::string& payload, IpcConnection& connection,
        ConnectionState& state, Backoff& reconnectBackoff,
        std::chrono::steady_clock::time_point& nextConnect) {
        int code = static_cast<int>(DisconnectCode::PipeClosed);
        std::string message = "Discord closed IPC connection";

        try {
            const json closePayload = json::parse(payload);
            code = json_int_member(closePayload, "code", code);
            const std::string closeMessage = json_string_member(closePayload, "message");
            if (!closeMessage.empty()) {
                message = closeMessage;
            }
        } catch (const json::exception&) {
        }

        handle_disconnect(connection, state, reconnectBackoff, nextConnect, code, message);
    }

    void handle_disconnect(IpcConnection& connection, ConnectionState& state,
        Backoff& reconnectBackoff, std::chrono::steady_clock::time_point& nextConnect, int code,
        std::string_view message) {
        const bool wasConnected =
            state == ConnectionState::Connected || state == ConnectionState::SentHandshake;
        connection.close();
        state = ConnectionState::Disconnected;
        nextConnect = std::chrono::steady_clock::now() + reconnectBackoff.next_delay();
        if (wasConnected) {
            enqueue_disconnected(code, message);
        }
    }

    void flush_pending_presence(IpcConnection& connection, int pid) {
        std::optional<Presence> presence;
        bool shouldClear = false;
        {
            std::lock_guard lock(mutex);
            presence = presenceState.take_dirty();
            shouldClear = !presence && presenceState.take_clear_dirty();
        }

        if (presence) {
            if (!connection.write_frame(
                    detail::make_set_activity_frame(next_nonce(), pid, std::move(presence))))
            {
                requeue_presence();
            }
        } else if (shouldClear) {
            if (!connection.write_frame(
                    detail::make_set_activity_frame(next_nonce(), pid, std::nullopt)))
            {
                requeue_presence();
            }
        }
    }

    void flush_shutdown_clear(IpcConnection& connection, ConnectionState state, int pid) {
        if (state != ConnectionState::Connected || !connection.is_open()) {
            return;
        }

        connection.write_frame(detail::make_set_activity_frame(next_nonce(), pid, std::nullopt));
        const auto deadline = std::chrono::steady_clock::now() + kShutdownClearTimeout;
        while (std::chrono::steady_clock::now() < deadline) {
            Frame frame;
            const auto status = connection.read_frame(frame);
            if (status == IpcConnection::ReadStatus::None) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            if (status != IpcConnection::ReadStatus::Frame) {
                break;
            }
            if (frame.opcode == Opcode::Ping) {
                connection.write_frame({Opcode::Pong, frame.payload});
            }
        }
    }

    void requeue_presence() {
        std::lock_guard lock(mutex);
        presenceState.mark_dirty();
    }

    void enqueue_ready(const json& readyMessage) {
        User user;
        const auto data = readyMessage.find("data");
        if (data != readyMessage.end() && data->is_object()) {
            const auto userIt = data->find("user");
            if (userIt != data->end() && userIt->is_object()) {
                user.id = json_string_member(*userIt, "id");
                user.username = json_string_member(*userIt, "username");
                user.discriminator = json_string_member(*userIt, "discriminator");
                user.avatar = json_string_member(*userIt, "avatar");
            }
        }

        std::lock_guard lock(mutex);
        queuedEvents.push_back({QueuedEvent::Type::Ready, std::move(user)});
    }

    void enqueue_disconnected(int code, std::string_view message) {
        std::lock_guard lock(mutex);
        QueuedEvent event;
        event.type = QueuedEvent::Type::Disconnected;
        event.code = code;
        event.message = message;
        queuedEvents.push_back(std::move(event));
    }

    void enqueue_error(int code, std::string_view message) {
        std::lock_guard lock(mutex);
        QueuedEvent event;
        event.type = QueuedEvent::Type::Error;
        event.code = code;
        event.message = message;
        queuedEvents.push_back(std::move(event));
    }

    void log_waiting_for_discord_once() {
        bool shouldLog = false;
        {
            std::lock_guard lock(mutex);
            if (!sentInitialConnectLog) {
                sentInitialConnectLog = true;
                shouldLog = true;
            }
        }
        if (shouldLog) {
            Log.info("Waiting for local Discord IPC");
        }
    }

    std::mutex mutex;
    std::condition_variable cv;
    std::thread ioThread;
    std::string applicationId;
    EventHandlers handlers;
    std::deque<QueuedEvent> queuedEvents;
    detail::PresenceState presenceState;
    bool shouldRun = false;
    bool sentInitialConnectLog = false;
};

Client& client() {
    static Client sClient;
    return sClient;
}

}  // namespace

bool available() {
    return true;
}

bool initialize(const AppInfo& info, EventHandlers handlers) {
    if (info.discordApplicationId.empty()) {
        Log.warn("Discord Rich Presence requires a non-empty application id");
        return false;
    }
    client().initialize(std::string(info.discordApplicationId), std::move(handlers));
    return true;
}

void run_callbacks() {
    client().run_callbacks();
}

bool update_presence(Presence presence) {
    return client().update_presence(std::move(presence));
}

bool clear_presence() {
    return client().clear_presence();
}

void shutdown() {
    client().shutdown();
}

}  // namespace borealis::discord
