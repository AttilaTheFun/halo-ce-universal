/* Complete-command capacity planning for the original native renderer.
 * No allocation, host call, or packet mutation occurs here. */
#ifndef HALO_METAL_PACKET_ROOM_H
#define HALO_METAL_PACKET_ROOM_H
#if defined(HALO_MACOS_NATIVE_METAL)
#include "../../macos/include/halo_metal_abi.h"

#define HALO_METAL_PACKET_ROOM_MAX_PAYLOADS 8u

/* cursor includes the 24-byte packet header and is aligned8. capacity follows
 * the transport's 32-byte minimum / 64MiB maximum. fixed_size is at least8.
 * Each nonzero payload is copied at aligned16 and padded to aligned8, exactly
 * as metal_guest_transport does. end is written only on success.
 * Command-count limits and optional soft batch targets belong to the caller.
 * Plan an empty packet as well before flushing: a single oversized command
 * must fail without submitting any previously queued commands. */
int halo_metal_packet_room(uint32_t cursor, uint32_t capacity, uint32_t fixed_size,
                           const uint32_t *payload_sizes, uint32_t payload_count,
                           uint32_t *end);
#endif
#endif
