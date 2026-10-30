#pragma once

#include "borealis/app_info.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace borealis::discord {

struct User {
    std::string id;
    std::string username;
    std::string discriminator;
    std::string avatar;
};

struct Presence {
    std::string state;
    std::string details;
    int64_t startTimestamp = 0;
    std::string largeImageKey;
    std::string largeImageText;
    // smstrikers-port: a countdown to this time, and the small image in the large one's corner.
    int64_t endTimestamp = 0;
    std::string smallImageKey;
    std::string smallImageText;
    // smstrikers-port: the party, and the secret a friend's game receives when they press Join.
    std::string partyId;
    int partySize = 0;
    int partyMax = 0;
    std::string joinSecret;

    bool operator==(const Presence&) const = default;
};

struct EventHandlers {
    std::function<void(const User&)> ready;
    std::function<void(int, std::string_view)> disconnected;
    std::function<void(int, std::string_view)> error;
    // smstrikers-port: this user pressed Join on a friend's presence; the argument is its secret.
    std::function<void(std::string_view secret)> activityJoin;
};

/** Returns whether the desktop Discord IPC backend is available. */
bool available();

/** Starts the IPC worker. Returns false if unsupported or the application ID is empty. */
bool initialize(const AppInfo& info, EventHandlers handlers = {});
void run_callbacks();

/** Queues a presence if it differs from the current value. */
bool update_presence(Presence presence);
bool clear_presence();
void shutdown();

}  // namespace borealis::discord
