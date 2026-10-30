// STUN Binding (RFC 5389): the request a host sends and the answer naming its address outside.

#ifndef PORT_ONLINE_STUN_H
#define PORT_ONLINE_STUN_H

#include <cstddef>
#include <cstdint>

#include "pipe.h"

namespace online
{
const size_t kStunRequestSize = 20;
const size_t kStunTxidSize = 12;

size_t EncodeStunRequest(const uint8_t txid[kStunTxidSize], uint8_t out[kStunRequestSize]);

// A Binding Success response to `txid` with an IPv4 XOR-MAPPED-ADDRESS, else MAPPED-ADDRESS.
bool DecodeStunResponse(const uint8_t* p, size_t n, const uint8_t txid[kStunTxidSize],
                        NetAddr* out);
}   // namespace online

#endif   // PORT_ONLINE_STUN_H
