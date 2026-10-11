#if defined(HALO_MACOS_NATIVE_METAL)
#include "metal_packet_room.h"

static uint64_t aligned(uint64_t value, uint64_t alignment) {
    return (value + alignment - 1u) & ~(alignment - 1u);
}

int halo_metal_packet_room(uint32_t cursor, uint32_t capacity, uint32_t fixed_size,
                           const uint32_t *payload_sizes, uint32_t payload_count,
                           uint32_t *end) {
    if (!end || capacity < sizeof(struct halo_metal_packet) + sizeof(struct halo_metal_command) ||
        capacity > HALO_METAL_MAX_PACKET || cursor < sizeof(struct halo_metal_packet) ||
        cursor > capacity || (cursor & 7u) || fixed_size < sizeof(struct halo_metal_command) ||
        payload_count > HALO_METAL_PACKET_ROOM_MAX_PAYLOADS || (payload_count && !payload_sizes))
        return HALO_METAL_INVALID;
    /* Validate every payload before capacity failure, so malformed requests
     * never depend on current packet occupancy. Eight uint32 extents plus
     * alignment fit uint64 even when every supplied size is UINT32_MAX. */
    for (uint32_t i = 0; i < payload_count; i++)
        if (!payload_sizes[i]) return HALO_METAL_INVALID;
    uint64_t position = aligned(aligned(cursor, 8u) + fixed_size, 8u);
    for (uint32_t i = 0; i < payload_count; i++)
        position = aligned(aligned(position, 16u) + payload_sizes[i], 8u);
    if (position > capacity) return HALO_METAL_MEMORY;
    *end = (uint32_t)position;
    return HALO_METAL_OK;
}
#endif
