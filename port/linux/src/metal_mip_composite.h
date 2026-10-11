/* CPU planning for the original renderer's independently rendered mip levels.
 * This helper neither allocates resources nor copies or generates GPU texels. */
#ifndef HALO_METAL_MIP_COMPOSITE_H
#define HALO_METAL_MIP_COMPOSITE_H
#if defined(HALO_MACOS_NATIVE_METAL)
#include "../../macos/include/halo_metal_abi.h"

#define HALO_METAL_MIP_COMPOSITE_MAX_LEVELS 16u
#define HALO_METAL_MIP_COMPOSITE_MAX_TARGETS 65536u

/* Exactly the GL composite cache identity. A different requested level count
 * identifies a different destination allocation, even at the same address. */
struct halo_metal_mip_composite_key {
    uint32_t physical_data, width, height, levels;
};
struct halo_metal_mip_composite_request {
    struct halo_metal_mip_composite_key key;
    uint32_t storage_format;
    /* Fill 0..levels using xgpu_texture_level_offset(original, level), including
     * the one-past requested chain. Do not infer physical pitch from BGRA8 GPU
     * storage: the original surface's physical format can have another pitch. */
    uint32_t level_offsets[HALO_METAL_MIP_COMPOSITE_MAX_LEVELS + 1u];
};
struct halo_metal_mip_composite_target {
    struct halo_metal_ref resource;
    uint32_t physical_data, width, height, storage_width, storage_height;
    uint32_t levels, type, format, usage, initialized;
    uint64_t last_rendered, content_version;
};
struct halo_metal_mip_composite_copy {
    struct halo_metal_ref source;
    uint64_t source_content_version;
    uint32_t source_physical_data, source_mip, source_slice;
    uint32_t destination_mip, destination_slice, width, height;
};
struct halo_metal_mip_composite_plan {
    struct halo_metal_mip_composite_key key;
    uint32_t storage_format, copy_count;
    struct halo_metal_mip_composite_copy copies[HALO_METAL_MIP_COMPOSITE_MAX_LEVELS];
};

/* Select the latest nondepth render-target alias at each exact physical mip
 * address, then validate its one-level, unscaled 2D color storage. A newer
 * incompatible alias never falls back to older data. Conflicting equal-time
 * aliases fail INVALID instead of depending on list order. Missing levels,
 * including a suffix, fail UNSUPPORTED; this interface does not generate mips.
 * A selected unrendered/uninitialized source fails UNDEFINED_CONTENT.
 * All requested levels are copied in order on success. Source content versions
 * describe the caller's snapshot; the caller must preserve ordering/freshness
 * when appending copy commands. *plan remains byte-for-byte unchanged on any
 * failure. No host/guest pointers are included in the result. */
int halo_metal_mip_composite_plan(
    const struct halo_metal_mip_composite_request *request,
    const struct halo_metal_mip_composite_target *targets, uint32_t target_count,
    struct halo_metal_mip_composite_plan *plan);
#endif
#endif
