/* Compact original Xbox vertex input, copied into an owned draw payload.
 * Offsets are relative to this prefix, not the packet or guest address space.
 * Each active stream contributes one contiguous bounded original byte span;
 * descriptor offsets select its first vertex and stride advances from there.
 */
#ifndef HALO_METAL_VERTEX_INPUT_H
#define HALO_METAL_VERTEX_INPUT_H
#include <stdint.h>

#define HALO_METAL_COMPACT_VERTEX_VERSION 1u

struct halo_metal_compact_vertex_element {
    uint32_t reg, type, offset, stride;
};
struct halo_metal_compact_vertex_input {
    uint32_t version, element_count, vertex_count, packed_mask;
    struct halo_metal_compact_vertex_element elements[16];
    /* Disabled attributes retain the existing float4 defaults. Packed
       registers are sixteen zero bytes here, even when actively supplied. */
    float fixed[16][4];
};

#if defined(__cplusplus)
static_assert(sizeof(halo_metal_compact_vertex_input) == 528,
    "compact vertex input prefix");
#else
_Static_assert(sizeof(struct halo_metal_compact_vertex_input) == 528,
    "compact vertex input prefix");
#endif
#endif
