#pragma once

#include "borealis/discord.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace borealis::discord::detail {

constexpr size_t FrameHeaderSize = sizeof(uint32_t) * 2;
constexpr size_t MaxFramePayloadSize = 64 * 1024;

enum class Opcode : uint32_t {
    Handshake = 0,
    Frame = 1,
    Close = 2,
    Ping = 3,
    Pong = 4,
};

struct Frame {
    Opcode opcode = Opcode::Frame;
    std::string payload;
};

enum class DecodeStatus {
    None,
    Frame,
    Corrupt,
};

std::vector<uint8_t> encode_frame(const Frame& frame);
DecodeStatus decode_frame(std::vector<uint8_t>& pending, Frame& frame);
Frame make_handshake_frame(std::string_view applicationId);
Frame make_set_activity_frame(std::string nonce, int pid, std::optional<Presence> presence);
// smstrikers-port: asks for DISPATCH frames of `event`.
Frame make_subscribe_frame(std::string nonce, std::string_view event);

// Suppresses unchanged presence updates.
class PresenceState {
public:
    bool update(Presence presence);
    bool clear();
    void mark_dirty();
    std::optional<Presence> take_dirty();
    bool take_clear_dirty();

private:
    std::optional<Presence> desiredPresence;
    bool dirty = false;
    bool clearDirty = false;
};

}  // namespace borealis::discord::detail
