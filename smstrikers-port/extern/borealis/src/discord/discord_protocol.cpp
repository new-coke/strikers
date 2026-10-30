#include "discord_protocol.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstddef>
#include <utility>

namespace borealis::discord::detail {
namespace {

using json = nlohmann::json;

constexpr uint32_t RpcVersion = 1;

void write_u32(uint8_t* out, uint32_t value) {
    out[0] = static_cast<uint8_t>(value & 0xff);
    out[1] = static_cast<uint8_t>((value >> 8) & 0xff);
    out[2] = static_cast<uint8_t>((value >> 16) & 0xff);
    out[3] = static_cast<uint8_t>((value >> 24) & 0xff);
}

uint32_t read_u32(const uint8_t* in) {
    return static_cast<uint32_t>(in[0]) | (static_cast<uint32_t>(in[1]) << 8) |
           (static_cast<uint32_t>(in[2]) << 16) | (static_cast<uint32_t>(in[3]) << 24);
}

json make_presence_activity(const Presence& presence) {
    json activity = json::object();
    if (!presence.state.empty()) {
        activity["state"] = presence.state;
    }
    if (!presence.details.empty()) {
        activity["details"] = presence.details;
    }
    // smstrikers-port: the end timestamp and the small image.
    if (presence.startTimestamp != 0 || presence.endTimestamp != 0) {
        json timestamps = json::object();
        if (presence.startTimestamp != 0) {
            timestamps["start"] = presence.startTimestamp;
        }
        if (presence.endTimestamp != 0) {
            timestamps["end"] = presence.endTimestamp;
        }
        activity["timestamps"] = std::move(timestamps);
    }
    if (!presence.largeImageKey.empty() || !presence.largeImageText.empty() ||
        !presence.smallImageKey.empty() || !presence.smallImageText.empty()) {
        json assets = json::object();
        if (!presence.largeImageKey.empty()) {
            assets["large_image"] = presence.largeImageKey;
        }
        if (!presence.largeImageText.empty()) {
            assets["large_text"] = presence.largeImageText;
        }
        if (!presence.smallImageKey.empty()) {
            assets["small_image"] = presence.smallImageKey;
        }
        if (!presence.smallImageText.empty()) {
            assets["small_text"] = presence.smallImageText;
        }
        activity["assets"] = std::move(assets);
    }
    // smstrikers-port: Join needs a party id, a size with room left and a join secret.
    if (!presence.partyId.empty()) {
        json party = {{"id", presence.partyId}};
        if (presence.partyMax > 0) {
            party["size"] = {presence.partySize, presence.partyMax};
        }
        activity["party"] = std::move(party);
    }
    if (!presence.joinSecret.empty()) {
        activity["secrets"] = {{"join", presence.joinSecret}};
    }
    return activity;
}

}  // namespace

std::vector<uint8_t> encode_frame(const Frame& frame) {
    if (frame.payload.size() > MaxFramePayloadSize) {
        return {};
    }
    std::vector<uint8_t> bytes(FrameHeaderSize + frame.payload.size());
    write_u32(bytes.data(), static_cast<uint32_t>(frame.opcode));
    write_u32(bytes.data() + sizeof(uint32_t), static_cast<uint32_t>(frame.payload.size()));
    std::copy(frame.payload.begin(), frame.payload.end(), bytes.begin() + FrameHeaderSize);
    return bytes;
}

DecodeStatus decode_frame(std::vector<uint8_t>& pending, Frame& frame) {
    if (pending.size() < FrameHeaderSize) {
        return DecodeStatus::None;
    }
    const uint32_t payloadLength = read_u32(pending.data() + sizeof(uint32_t));
    if (payloadLength > MaxFramePayloadSize) {
        return DecodeStatus::Corrupt;
    }
    const size_t frameLength = FrameHeaderSize + payloadLength;
    if (pending.size() < frameLength) {
        return DecodeStatus::None;
    }
    frame.opcode = static_cast<Opcode>(read_u32(pending.data()));
    frame.payload.assign(
        reinterpret_cast<const char*>(pending.data() + FrameHeaderSize), payloadLength);
    pending.erase(pending.begin(), pending.begin() + static_cast<std::ptrdiff_t>(frameLength));
    return DecodeStatus::Frame;
}

Frame make_handshake_frame(std::string_view applicationId) {
    return {
        Opcode::Handshake,
        json{{"v", RpcVersion}, {"client_id", std::string(applicationId)}}.dump(),
    };
}

Frame make_set_activity_frame(std::string nonce, int pid, std::optional<Presence> presence) {
    json args = {{"pid", pid}};
    args["activity"] = presence ? make_presence_activity(*presence) : json(nullptr);
    return {
        Opcode::Frame,
        json{
            {"cmd", "SET_ACTIVITY"},
            {"nonce", std::move(nonce)},
            {"args", std::move(args)},
        }
            .dump(),
    };
}

Frame make_subscribe_frame(std::string nonce, std::string_view event) {
    return {
        Opcode::Frame,
        json{
            {"cmd", "SUBSCRIBE"},
            {"evt", std::string(event)},
            {"nonce", std::move(nonce)},
            {"args", json::object()},
        }
            .dump(),
    };
}

bool PresenceState::update(Presence presence) {
    if (desiredPresence && *desiredPresence == presence) {
        return false;
    }
    desiredPresence = std::move(presence);
    dirty = true;
    clearDirty = false;
    return true;
}

bool PresenceState::clear() {
    if (!desiredPresence) {
        return false;
    }
    desiredPresence.reset();
    dirty = false;
    clearDirty = true;
    return true;
}

void PresenceState::mark_dirty() {
    if (desiredPresence) {
        dirty = true;
        clearDirty = false;
    } else {
        clearDirty = true;
    }
}

std::optional<Presence> PresenceState::take_dirty() {
    if (!dirty || !desiredPresence) {
        return std::nullopt;
    }
    dirty = false;
    return desiredPresence;
}

bool PresenceState::take_clear_dirty() {
    const bool result = clearDirty;
    clearDirty = false;
    return result;
}

}  // namespace borealis::discord::detail
