#ifndef HALO_METAL_VERTEX_FETCH_H
#define HALO_METAL_VERTEX_FETCH_H

#if defined(HALO_MACOS_NATIVE_METAL) && HALO_MACOS_NATIVE_METAL
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum metal_vertex_status {
    METAL_VERTEX_OK = 0,
    METAL_VERTEX_INVALID = -1,
    METAL_VERTEX_UNSUPPORTED = -2,
    METAL_VERTEX_BOUNDS = -3
};

/* Xbox register/type numbers, not D3D9 declarations or host GPU formats. */
struct metal_vertex_element {
    uint32_t reg, stream, type, bytes, offset;
};
struct metal_vertex_declaration {
    uint32_t element_count, packed_mask;
    struct metal_vertex_element elements[16];
};
struct metal_vertex_stream {
    const void *pointer;
    size_t byte_count;
    uint32_t stride;
};

/* Bounded token parse; CONSTMEM payloads are skipped like the existing port.
 * Tessellator/vendor extension tokens fail explicitly. Requires END. */
int metal_vertex_declaration_parse(const uint32_t *tokens, size_t token_count,
    struct metal_vertex_declaration *declaration);

/* Caller-owned output is count * 256 bytes, sixteen float4 registers/vertex.
 * Every stream pointer denotes its vertex zero. Zero stride repeats one input.
 * Disabled registers retain fixed values; disabled packed registers are integer
 * zero. Packed x bits are never converted to float, including NaN bit patterns.
 * FLOAT2H intentionally matches the working port's three-float xyz upload.
 * Immediate End() data can use sixteen FLOAT4 elements, stride 256, mask zero.
 * All validation precedes output writes. No allocation or GL/Metal calls. */
int metal_vertex_fetch(const struct metal_vertex_declaration *declaration,
    const struct metal_vertex_stream streams[16], const float fixed[16][4],
    uint32_t first, uint32_t count, void *expanded, size_t expanded_bytes);

/* Compact original input for GPU fetch. The fixed-width 528-byte prefix is
 * defined in halo_metal_vertex_input.h. Only bound, non-NONE attributes are
 * active. Each used stream is copied once, from its first active attribute of
 * vertex first through its last active attribute of vertex first+count-1.
 * Descriptor offsets are prefix-relative and rebased to vertex first.
 * Packed fixed registers are sixteen zero bytes; other fixed values retain
 * their exact bits. No vertex conversion, allocation or GPU calls occur.
 * Outputs are unchanged on failure, including aliased/overflowing buffers. */
int metal_vertex_compact_size(const struct metal_vertex_declaration *declaration,
    const struct metal_vertex_stream streams[16], uint32_t first, uint32_t count,
    size_t *compact_bytes);
int metal_vertex_compact_pack(const struct metal_vertex_declaration *declaration,
    const struct metal_vertex_stream streams[16], const float fixed[16][4],
    uint32_t first, uint32_t count, void *compact, size_t compact_bytes);

/* Wire topology values: points 0, lines 1, line strip 2, triangles 3,
 * triangle strip 4. Xbox LINELOOP closes a line strip; QUADSTRIP preserves the
 * working port's triangle-strip ordering; POLYGON uses its triangle fan.
 * Source indices are little-endian uint16, never host-rebased before this API.
 * With no indices, first/count describes a nonindexed draw and base must be 0.
 * Plans/indices are output-only; errors leave them unchanged. */
struct metal_vertex_index_plan {
    uint32_t source_first, source_count, index_count, wire_primitive;
};
int metal_vertex_index_plan(uint32_t primitive, const uint16_t *indices,
    size_t index_bytes, uint32_t count, uint32_t first, uint32_t base,
    struct metal_vertex_index_plan *plan);
int metal_vertex_indices(uint32_t primitive, const uint16_t *indices,
    size_t index_bytes, uint32_t count, uint32_t first, uint32_t base,
    void *output, size_t output_bytes, struct metal_vertex_index_plan *plan);

#ifdef __cplusplus
}
#endif
#endif
#endif
