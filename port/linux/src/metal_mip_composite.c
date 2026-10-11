#if defined(HALO_MACOS_NATIVE_METAL)
#include "metal_mip_composite.h"

static int same_alias(const struct halo_metal_mip_composite_target *a,
                      const struct halo_metal_mip_composite_target *b) {
    return a->resource.id == b->resource.id &&
        a->resource.generation == b->resource.generation &&
        a->physical_data == b->physical_data && a->width == b->width &&
        a->height == b->height && a->storage_width == b->storage_width &&
        a->storage_height == b->storage_height && a->levels == b->levels &&
        a->type == b->type && a->format == b->format && a->usage == b->usage &&
        a->initialized == b->initialized && a->last_rendered == b->last_rendered &&
        a->content_version == b->content_version;
}

int halo_metal_mip_composite_plan(
    const struct halo_metal_mip_composite_request *request,
    const struct halo_metal_mip_composite_target *targets, uint32_t target_count,
    struct halo_metal_mip_composite_plan *plan) {
    if (!request || !plan || (target_count && !targets) ||
        target_count > HALO_METAL_MIP_COMPOSITE_MAX_TARGETS)
        return HALO_METAL_INVALID;
    const struct halo_metal_mip_composite_key *key = &request->key;
    if (!key->physical_data || !key->width || !key->height ||
        key->width > 32768u || key->height > 32768u ||
        (key->width & (key->width - 1u)) || (key->height & (key->height - 1u)) ||
        !key->levels || key->levels > HALO_METAL_MIP_COMPOSITE_MAX_LEVELS ||
        request->level_offsets[0])
        return HALO_METAL_INVALID;
    uint32_t largest = key->width > key->height ? key->width : key->height;
    uint32_t available_levels = 1u;
    while (largest > 1u) { largest >>= 1; available_levels++; }
    if (key->levels > available_levels) return HALO_METAL_INVALID;
    if (request->storage_format != HALO_METAL_RGBA8 &&
        request->storage_format != HALO_METAL_BGRA8)
        return HALO_METAL_UNSUPPORTED;
    for (uint32_t level = 0; level < key->levels; level++) {
        if (request->level_offsets[level + 1u] <= request->level_offsets[level])
            return HALO_METAL_INVALID;
    }
    /* End is exclusive. An endpoint of 2^32 is valid; no selected address may
     * wrap to zero. Both operands are widened before any addition. */
    if ((uint64_t)key->physical_data + request->level_offsets[key->levels] >
        UINT64_C(0x100000000)) return HALO_METAL_INVALID;

    struct halo_metal_mip_composite_plan result = {0};
    result.key = *key;
    result.storage_format = request->storage_format;
    result.copy_count = key->levels;
    for (uint32_t level = 0; level < key->levels; level++) {
        uint32_t physical = key->physical_data + request->level_offsets[level];
        const struct halo_metal_mip_composite_target *best = 0;
        int ambiguous = 0;
        for (uint32_t i = 0; i < target_count; i++) {
            const struct halo_metal_mip_composite_target *candidate = &targets[i];
            if (candidate->physical_data != physical ||
                !(candidate->usage & HALO_METAL_RENDER_TARGET) ||
                candidate->format == HALO_METAL_DEPTH32_STENCIL8) continue;
            if (!best || candidate->last_rendered > best->last_rendered) {
                best = candidate;
                ambiguous = 0;
            } else if (candidate->last_rendered == best->last_rendered &&
                       !same_alias(candidate, best)) ambiguous = 1;
        }
        if (!best) return HALO_METAL_UNSUPPORTED;
        if (ambiguous || !best->resource.id || !best->resource.generation ||
            best->initialized > 1u) return HALO_METAL_INVALID;
        uint32_t width = key->width >> level;
        uint32_t height = key->height >> level;
        if (!width) width = 1u;
        if (!height) height = 1u;
        if (best->width != width || best->height != height ||
            best->storage_width != width || best->storage_height != height ||
            best->levels != 1u || best->type != HALO_METAL_TEXTURE_2D ||
            best->format != request->storage_format ||
            (best->usage & ~(HALO_METAL_RENDER_TARGET | HALO_METAL_SHADER_READ)))
            return HALO_METAL_UNSUPPORTED;
        if (!best->last_rendered || !best->initialized || !best->content_version)
            return HALO_METAL_UNDEFINED_CONTENT;
        struct halo_metal_mip_composite_copy *copy = &result.copies[level];
        copy->source = best->resource;
        copy->source_content_version = best->content_version;
        copy->source_physical_data = physical;
        copy->destination_mip = level;
        copy->width = width;
        copy->height = height;
    }
    *plan = result;
    return HALO_METAL_OK;
}
#endif
