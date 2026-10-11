#include "metal_vertex_fetch.h"

#if defined(HALO_MACOS_NATIVE_METAL) && HALO_MACOS_NATIVE_METAL
#include "../../macos/include/halo_metal_vertex_input.h"
#include <string.h>

/* Mirrors d3d8_gl.c:vertex_type_bytes/attribute_format/macOS upload. */
static int element_bytes(uint32_t type, uint32_t *bytes)
{
    switch (type) {
    case 0x02: *bytes = 0; break;
    case 0x12: *bytes = 4; break;
    case 0x22: *bytes = 8; break;
    case 0x32: case 0x72: *bytes = 12; break;
    case 0x42: *bytes = 16; break;
    case 0x40: case 0x16: *bytes = 4; break;
    case 0x11: case 0x15: *bytes = 2; break;
    case 0x21: case 0x25: *bytes = 4; break;
    case 0x31: case 0x35: *bytes = 6; break;
    case 0x41: case 0x45: *bytes = 8; break;
    case 0x14: *bytes = 1; break;
    case 0x24: *bytes = 2; break;
    case 0x34: *bytes = 3; break;
    case 0x44: *bytes = 4; break;
    default: return METAL_VERTEX_UNSUPPORTED;
    }
    return METAL_VERTEX_OK;
}

int metal_vertex_declaration_parse(const uint32_t *tokens, size_t token_count,
    struct metal_vertex_declaration *declaration)
{
    struct metal_vertex_declaration result = {0};
    uint32_t offsets[16] = {0}, stream = 0, seen = 0;
    size_t i;
    if (!tokens || !declaration) return METAL_VERTEX_INVALID;
    if (!token_count || token_count > SIZE_MAX / sizeof(*tokens) ||
        token_count * sizeof(*tokens) > UINTPTR_MAX - (uintptr_t)tokens)
        return METAL_VERTEX_BOUNDS;
    for (i = 0; i < token_count; i++) {
        uint32_t token;
        memcpy(&token, (const unsigned char *)tokens + i * 4, 4);
        if (token == UINT32_MAX) {
            *declaration = result;
            return METAL_VERTEX_OK;
        }
        switch (token >> 29) {
        case 0:
            if (token) return METAL_VERTEX_UNSUPPORTED;
            break;
        case 1:
            if (token & ~UINT32_C(0x2000000f)) return METAL_VERTEX_UNSUPPORTED;
            stream = token & 15;
            break;
        case 2:
            if (token & UINT32_C(0x10000000)) {
                uint32_t skip = (token >> 16) & 15;
                if (token & ~UINT32_C(0x580f0000)) return METAL_VERTEX_INVALID;
                if (!(token & UINT32_C(0x08000000))) skip *= 4;
                if (offsets[stream] > UINT32_MAX - skip) return METAL_VERTEX_BOUNDS;
                offsets[stream] += skip;
            } else {
                struct metal_vertex_element *element;
                uint32_t reg = token & 31, type = (token >> 16) & 255, bytes;
                int status;
                if (token & ~UINT32_C(0x40ff001f)) return METAL_VERTEX_INVALID;
                if (reg >= 16 || result.element_count >= 16 || (seen & (1u << reg)))
                    return METAL_VERTEX_INVALID;
                status = element_bytes(type, &bytes);
                if (status) return status;
                if (offsets[stream] > UINT32_MAX - bytes) return METAL_VERTEX_BOUNDS;
                element = &result.elements[result.element_count++];
                element->reg = reg; element->stream = stream; element->type = type;
                element->bytes = bytes; element->offset = offsets[stream];
                offsets[stream] += bytes;
                seen |= 1u << reg;
                if (type == 0x16) result.packed_mask |= 1u << reg;
            }
            break;
        case 4: {
            size_t words = ((token >> 25) & 15) * 4;
            if (words > token_count - i - 1) return METAL_VERTEX_BOUNDS;
            i += words;
            break;
        }
        default:
            return METAL_VERTEX_UNSUPPORTED;
        }
    }
    return METAL_VERTEX_BOUNDS;
}

static int overlaps(const void *a, size_t an, const void *b, size_t bn)
{
    uintptr_t aa = (uintptr_t)a, bb = (uintptr_t)b;
    /* Pointer arithmetic overflow is rejected rather than wrapped. */
    if (an > UINTPTR_MAX - aa || bn > UINTPTR_MAX - bb) return 1;
    return an && bn && aa < bb + bn && bb < aa + an;
}

static int validate_declaration(const struct metal_vertex_declaration *d)
{
    uint32_t i, seen = 0, packed = 0;
    if (!d || d->element_count > 16 || d->packed_mask & ~UINT32_C(0xffff))
        return METAL_VERTEX_INVALID;
    for (i = 0; i < d->element_count; i++) {
        const struct metal_vertex_element *e = &d->elements[i];
        uint32_t bytes;
        int status;
        if (e->reg >= 16 || e->stream >= 16 || (seen & (1u << e->reg)))
            return METAL_VERTEX_INVALID;
        status = element_bytes(e->type, &bytes);
        if (status) return status;
        if (e->bytes != bytes) return METAL_VERTEX_INVALID;
        seen |= 1u << e->reg;
        if (e->type == 0x16) packed |= 1u << e->reg;
    }
    return packed == d->packed_mask ? METAL_VERTEX_OK : METAL_VERTEX_INVALID;
}

/* Expanded storage may be a caller-owned byte array, not a float object. Keep
 * direct aligned stores valid even when this helper uses strict aliasing. */
typedef float metal_vertex_float __attribute__((__may_alias__));

static void fetch_attribute(metal_vertex_float out[4], const unsigned char *in, uint32_t type)
{
    uint32_t component, components = type == 0x72 ? 3 : type >> 4;
    out[0] = out[1] = out[2] = 0;
    out[3] = 1;
    if (type == 0x16) { memcpy(out, in, 4); return; }
    for (component = 0; component < components; component++) {
        if ((type & 15) == 2) memcpy(out + component, in + component * 4, 4);
        else if ((type & 15) == 1 || (type & 15) == 5) {
            int16_t value;
            memcpy(&value, in + component * 2, 2);
            out[component] = (type & 15) == 1 ?
                (value == INT16_MIN ? -1.0f : value / 32767.0f) : (float)value;
        } else {
            uint32_t channel = type == 0x40 && (component == 0 || component == 2) ?
                2 - component : component;
            out[component] = in[channel] / 255.0f;
        }
    }
}

int metal_vertex_fetch(const struct metal_vertex_declaration *declaration,
    const struct metal_vertex_stream streams[16], const float fixed[16][4],
    uint32_t first, uint32_t count, void *expanded, size_t expanded_bytes)
{
    uint32_t i, vertex;
    size_t needed;
    int aligned;
    int status = validate_declaration(declaration);
    if (status) return status;
    if (!streams || !fixed) return METAL_VERTEX_INVALID;
    if (!count) return METAL_VERTEX_OK;
    if (first > UINT32_MAX - (count - 1))
        return METAL_VERTEX_BOUNDS;
#if SIZE_MAX <= UINT32_MAX
    if (count > SIZE_MAX / 256) return METAL_VERTEX_BOUNDS;
#endif
    needed = (size_t)count * 256;
    if (!expanded || expanded_bytes < needed) return METAL_VERTEX_BOUNDS;
    if (overlaps(expanded, needed, fixed, 256) ||
        overlaps(expanded, needed, declaration, sizeof(*declaration)) ||
        overlaps(expanded, needed, streams, 16 * sizeof(*streams))) return METAL_VERTEX_INVALID;
    for (i = 0; i < declaration->element_count; i++) {
        const struct metal_vertex_element *e = &declaration->elements[i];
        const struct metal_vertex_stream *s = &streams[e->stream];
        size_t remaining;
        uint32_t last = first + count - 1;
        if (!e->bytes || !s->pointer) continue;
        if (e->offset > s->byte_count || e->bytes > s->byte_count - e->offset)
            return METAL_VERTEX_BOUNDS;
        remaining = s->byte_count - e->offset - e->bytes;
        if (s->stride && last > remaining / s->stride) return METAL_VERTEX_BOUNDS;
        if (overlaps(expanded, needed, s->pointer, s->byte_count)) return METAL_VERTEX_INVALID;
    }
    aligned = (uintptr_t)expanded % _Alignof(metal_vertex_float) == 0;
    for (vertex = 0; vertex < count; vertex++) {
        metal_vertex_float fallback[16][4];
        metal_vertex_float (*values)[4] = aligned ?
            (metal_vertex_float (*)[4])((unsigned char *)expanded + (size_t)vertex * 256) : fallback;
        memcpy(values, fixed, 256);
        for (i = 0; i < 16; i++) {
            if (declaration->packed_mask & (1u << i)) memset(values[i], 0, 16);
        }
        for (i = 0; i < declaration->element_count; i++) {
            const struct metal_vertex_element *e = &declaration->elements[i];
            const struct metal_vertex_stream *s = &streams[e->stream];
            if (e->bytes && s->pointer) fetch_attribute(values[e->reg],
                (const unsigned char *)s->pointer + e->offset + (size_t)(first + vertex) * s->stride,
                e->type);
        }
        if (!aligned) memcpy((unsigned char *)expanded + (size_t)vertex * 256, values, 256);
    }
    return METAL_VERTEX_OK;
}

struct compact_vertex_plan {
    struct halo_metal_compact_vertex_input input;
    size_t stream_first[16], stream_bytes[16];
    uint32_t stream_output[16];
    size_t bytes;
};

static int compact_vertex_plan(const struct metal_vertex_declaration *declaration,
    const struct metal_vertex_stream streams[16], uint32_t first, uint32_t count,
    struct compact_vertex_plan *plan)
{
    struct compact_vertex_plan result = {0};
    size_t minimum[16], maximum[16] = {0};
    uint32_t i;
    int status;
    if (!declaration || !streams) return METAL_VERTEX_INVALID;
    if (sizeof(*declaration) > UINTPTR_MAX - (uintptr_t)declaration ||
        16 * sizeof(*streams) > UINTPTR_MAX - (uintptr_t)streams)
        return METAL_VERTEX_BOUNDS;
    status = validate_declaration(declaration);
    if (status) return status;
    result.input.version = HALO_METAL_COMPACT_VERTEX_VERSION;
    result.input.vertex_count = count;
    result.input.packed_mask = declaration->packed_mask;
    result.bytes = sizeof(result.input);
    if (!count) { *plan = result; return METAL_VERTEX_OK; }
    if (first > UINT32_MAX - (count - 1)) return METAL_VERTEX_BOUNDS;
    for (i = 0; i < 16; i++) minimum[i] = SIZE_MAX;
    for (i = 0; i < declaration->element_count; i++) {
        const struct metal_vertex_element *e = &declaration->elements[i];
        const struct metal_vertex_stream *s = &streams[e->stream];
        size_t end, remaining;
        if (!e->bytes || !s->pointer) continue;
        if (s->byte_count > UINTPTR_MAX - (uintptr_t)s->pointer ||
            e->offset > s->byte_count || e->bytes > s->byte_count - e->offset)
            return METAL_VERTEX_BOUNDS;
        end = (size_t)e->offset + e->bytes;
        remaining = s->byte_count - end;
        if (s->stride && first + count - 1 > remaining / s->stride)
            return METAL_VERTEX_BOUNDS;
        if (e->offset < minimum[e->stream]) minimum[e->stream] = e->offset;
        if (end > maximum[e->stream]) maximum[e->stream] = end;
    }
    for (i = 0; i < 16; i++) {
        const struct metal_vertex_stream *s = &streams[i];
        size_t span, start;
        if (minimum[i] == SIZE_MAX) continue;
        /* Attribute bounds above establish both products and additions fit
           size_t, including zero stride and unaligned input records. */
        start = (size_t)first * s->stride + minimum[i];
        span = (size_t)(count - 1) * s->stride + maximum[i] - minimum[i];
        if (span > UINT32_MAX - result.bytes) return METAL_VERTEX_BOUNDS;
        result.stream_first[i] = start;
        result.stream_bytes[i] = span;
        result.stream_output[i] = (uint32_t)result.bytes;
        result.bytes += span;
    }
    for (i = 0; i < declaration->element_count; i++) {
        const struct metal_vertex_element *e = &declaration->elements[i];
        const struct metal_vertex_stream *s = &streams[e->stream];
        struct halo_metal_compact_vertex_element *out;
        if (!e->bytes || !s->pointer) continue;
        out = &result.input.elements[result.input.element_count++];
        out->reg = e->reg; out->type = e->type; out->stride = s->stride;
        out->offset = result.stream_output[e->stream] +
            ((size_t)e->offset - minimum[e->stream]);
    }
    *plan = result;
    return METAL_VERTEX_OK;
}

static int compact_vertex_output_overlaps(const void *output, size_t bytes,
    const struct metal_vertex_declaration *declaration,
    const struct metal_vertex_stream streams[16], const struct compact_vertex_plan *plan)
{
    uint32_t i;
    if (overlaps(output, bytes, declaration, sizeof(*declaration)) ||
        overlaps(output, bytes, streams, 16 * sizeof(*streams)))
        return 1;
    for (i = 0; i < 16; i++) {
        if (plan->stream_bytes[i] &&
            overlaps(output, bytes, streams[i].pointer, streams[i].byte_count))
            return 1;
    }
    return 0;
}

int metal_vertex_compact_size(const struct metal_vertex_declaration *declaration,
    const struct metal_vertex_stream streams[16], uint32_t first, uint32_t count,
    size_t *compact_bytes)
{
    struct compact_vertex_plan plan;
    int status;
    if (!compact_bytes) return METAL_VERTEX_INVALID;
    if (sizeof(*compact_bytes) > UINTPTR_MAX - (uintptr_t)compact_bytes)
        return METAL_VERTEX_BOUNDS;
    status = compact_vertex_plan(declaration, streams, first, count, &plan);
    if (status) return status;
    if (compact_vertex_output_overlaps(compact_bytes, sizeof(*compact_bytes),
        declaration, streams, &plan)) return METAL_VERTEX_INVALID;
    *compact_bytes = plan.bytes;
    return METAL_VERTEX_OK;
}

int metal_vertex_compact_pack(const struct metal_vertex_declaration *declaration,
    const struct metal_vertex_stream streams[16], const float fixed[16][4],
    uint32_t first, uint32_t count, void *compact, size_t compact_bytes)
{
    struct compact_vertex_plan plan;
    uint32_t i;
    int status;
    if (!fixed) return METAL_VERTEX_INVALID;
    if (256 > UINTPTR_MAX - (uintptr_t)fixed) return METAL_VERTEX_BOUNDS;
    status = compact_vertex_plan(declaration, streams, first, count, &plan);
    if (status) return status;
    if (!compact || compact_bytes < plan.bytes ||
        plan.bytes > UINTPTR_MAX - (uintptr_t)compact) return METAL_VERTEX_BOUNDS;
    if (overlaps(compact, plan.bytes, fixed, 256) ||
        compact_vertex_output_overlaps(compact, plan.bytes, declaration, streams, &plan))
        return METAL_VERTEX_INVALID;
    memcpy(plan.input.fixed, fixed, 256);
    for (i = 0; i < 16; i++) {
        if (declaration->packed_mask & (1u << i)) memset(plan.input.fixed[i], 0, 16);
    }
    memcpy(compact, &plan.input, sizeof(plan.input));
    for (i = 0; i < 16; i++) {
        if (plan.stream_bytes[i]) memcpy((unsigned char *)compact + plan.stream_output[i],
            (const unsigned char *)streams[i].pointer + plan.stream_first[i], plan.stream_bytes[i]);
    }
    return METAL_VERTEX_OK;
}

int metal_vertex_index_plan(uint32_t primitive, const uint16_t *indices,
    size_t index_bytes, uint32_t count, uint32_t first, uint32_t base,
    struct metal_vertex_index_plan *plan)
{
    struct metal_vertex_index_plan result = {0};
    uint32_t i, minimum = UINT32_MAX, maximum = 0;
    if (!plan) return METAL_VERTEX_INVALID;
    switch (primitive) {
    case 1: result.wire_primitive = 0; break;
    case 2: result.wire_primitive = 1; if (count % 2) return METAL_VERTEX_INVALID; break;
    case 3: case 4: result.wire_primitive = 2; if (count && count < 2) return METAL_VERTEX_INVALID; break;
    case 5: result.wire_primitive = 3; if (count % 3) return METAL_VERTEX_INVALID; break;
    case 6: case 9:
        result.wire_primitive = 4;
        if (count && (count < (primitive == 9 ? 4u : 3u) || (primitive == 9 && count % 2)))
            return METAL_VERTEX_INVALID;
        break;
    case 7: case 10: result.wire_primitive = 3; if (count && count < 3) return METAL_VERTEX_INVALID; break;
    case 8: result.wire_primitive = 3; if (count % 4) return METAL_VERTEX_INVALID; break;
    default: return METAL_VERTEX_UNSUPPORTED;
    }
    if ((!indices && (index_bytes || base)) || (indices && first)) return METAL_VERTEX_INVALID;
    if (indices && count > index_bytes / 2) return METAL_VERTEX_BOUNDS;
    if (indices && index_bytes > UINTPTR_MAX - (uintptr_t)indices) return METAL_VERTEX_BOUNDS;
    if (!count) { *plan = result; return METAL_VERTEX_OK; }
    if (!indices) {
        if (first > UINT32_MAX - (count - 1)) return METAL_VERTEX_BOUNDS;
        minimum = first; maximum = first + count - 1;
    } else {
        for (i = 0; i < count; i++) {
            uint16_t value;
            uint32_t source;
            memcpy(&value, (const unsigned char *)indices + (size_t)i * 2, 2);
            if (base > UINT32_MAX - value) return METAL_VERTEX_BOUNDS;
            source = base + value;
            if (source < minimum) minimum = source;
            if (source > maximum) maximum = source;
        }
    }
    if (maximum - minimum == UINT32_MAX) return METAL_VERTEX_BOUNDS;
    result.source_first = minimum; result.source_count = maximum - minimum + 1;
    if (primitive == 3) {
        if (count == UINT32_MAX) return METAL_VERTEX_BOUNDS;
        result.index_count = count + 1;
    } else if (primitive == 7 || primitive == 10) {
        if (count - 2 > UINT32_MAX / 3) return METAL_VERTEX_BOUNDS;
        result.index_count = (count - 2) * 3;
    } else if (primitive == 8) {
        if (count / 4 > UINT32_MAX / 6) return METAL_VERTEX_BOUNDS;
        result.index_count = (count / 4) * 6;
    } else result.index_count = count;
#if SIZE_MAX <= UINT32_MAX
    if (result.index_count > SIZE_MAX / 4) return METAL_VERTEX_BOUNDS;
#endif
    *plan = result;
    return METAL_VERTEX_OK;
}

static uint32_t source_index(const uint16_t *indices, uint32_t n,
    uint32_t first, uint32_t base, uint32_t minimum)
{
    uint16_t value;
    if (!indices) return first + n - minimum;
    memcpy(&value, (const unsigned char *)indices + (size_t)n * 2, 2);
    return base + value - minimum;
}

int metal_vertex_indices(uint32_t primitive, const uint16_t *indices,
    size_t index_bytes, uint32_t count, uint32_t first, uint32_t base,
    void *output, size_t output_bytes, struct metal_vertex_index_plan *plan)
{
    struct metal_vertex_index_plan result;
    uint32_t i, write = 0;
    size_t needed;
    int status;
    if (!plan) return METAL_VERTEX_INVALID;
    status = metal_vertex_index_plan(primitive, indices, index_bytes, count, first, base, &result);
    if (status) return status;
    needed = (size_t)result.index_count * 4;
    if (needed && (!output || output_bytes < needed)) return METAL_VERTEX_BOUNDS;
    if (overlaps(output, needed, indices, index_bytes) || overlaps(output, needed, plan, sizeof(*plan)))
        return METAL_VERTEX_INVALID;
    if (primitive == 7 || primitive == 10) {
        for (i = 1; i + 1 < count; i++) {
            uint32_t triangle[3] = {0, source_index(indices, i, first, base, result.source_first),
                source_index(indices, i + 1, first, base, result.source_first)};
            triangle[0] = source_index(indices, 0, first, base, result.source_first);
            memcpy((unsigned char *)output + (size_t)write * 4, triangle, 12); write += 3;
        }
    } else if (primitive == 8) {
        static const uint32_t order[6] = {0, 1, 2, 0, 2, 3};
        for (i = 0; i < count; i += 4) {
            uint32_t j;
            for (j = 0; j < 6; j++) {
                uint32_t value = source_index(indices, i + order[j], first, base, result.source_first);
                memcpy((unsigned char *)output + (size_t)write++ * 4, &value, 4);
            }
        }
    } else {
        for (i = 0; i < result.index_count; i++) {
            uint32_t value = source_index(indices, i == count ? 0 : i, first, base, result.source_first);
            memcpy((unsigned char *)output + (size_t)i * 4, &value, 4);
        }
    }
    *plan = result;
    return METAL_VERTEX_OK;
}
#endif
