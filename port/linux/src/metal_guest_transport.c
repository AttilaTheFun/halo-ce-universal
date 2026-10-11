#if defined(HALO_MACOS_NATIVE_METAL)
#include "metal_guest_transport.h"

_Static_assert(sizeof(void *) == 4, "Native transport runs in the ILP32 guest");
#define ALL_CAPABILITIES (HALO_METAL_CAP_TARGETS | HALO_METAL_CAP_UPLOAD | HALO_METAL_CAP_CLEAR | \
    HALO_METAL_CAP_COPY | HALO_METAL_CAP_READBACK | HALO_METAL_CAP_PRESENT_EXACT | \
    HALO_METAL_CAP_DRAW | HALO_METAL_CAP_VISIBILITY | HALO_METAL_CAP_PRESENT_SCALED | \
    HALO_METAL_CAP_CLEAR_CHANNELS | HALO_METAL_CAP_BLACK_BORDER | HALO_METAL_CAP_ALPHA_BORDER | \
    HALO_METAL_CAP_COPY_SUBRESOURCE | HALO_METAL_CAP_VOLUME | HALO_METAL_CAP_VOLUME_BORDER | HALO_METAL_CAP_FXAA | \
    HALO_METAL_CAP_COMPACT_VERTICES)

static void zero_bytes(void *pointer, uint32_t size) {
    unsigned char *p = pointer;
    for (uint32_t i = 0; i < size; i++) p[i] = 0;
}
static void copy_bytes(void *destination, const void *source, uint32_t size) {
    unsigned char *d = destination; const unsigned char *s = source;
    for (uint32_t i = 0; i < size; i++) d[i] = s[i];
}
static uint32_t address(const void *pointer) { return (uint32_t)(uintptr_t)pointer; }
static int fail(struct halo_metal_guest_transport *t, int status, const char *error) {
    if (t) { t->status = status; t->error = error; }
    return status;
}
static int storage_valid(struct halo_metal_guest_transport *t) {
    return t && t->bytes && !(address(t->bytes) & 15u) &&
        t->capacity >= sizeof(struct halo_metal_packet) + sizeof(struct halo_metal_command) &&
        t->capacity <= HALO_METAL_MAX_PACKET;
}
static int overlaps_storage(struct halo_metal_guest_transport *t, const void *pointer, uint32_t size) {
    uint64_t begin = address(pointer), end = begin + size;
    uint64_t storage_begin = address(t->bytes), storage_end = storage_begin + t->capacity;
    return begin < storage_end && end > storage_begin;
}
static uint32_t command_capability(uint32_t opcode) {
    switch (opcode) {
        case HALO_METAL_CREATE_TEXTURE: case HALO_METAL_CREATE_TEXTURE_EX:
        case HALO_METAL_DELETE_TEXTURE: return HALO_METAL_CAP_TARGETS;
        case HALO_METAL_UPLOAD: case HALO_METAL_UPLOAD_EX: return HALO_METAL_CAP_UPLOAD;
        case HALO_METAL_CLEAR: return HALO_METAL_CAP_CLEAR;
        case HALO_METAL_COPY: return HALO_METAL_CAP_COPY;
        case HALO_METAL_COPY_SUBRESOURCE: return HALO_METAL_CAP_COPY_SUBRESOURCE;
        case HALO_METAL_FXAA: return HALO_METAL_CAP_FXAA;
        case HALO_METAL_PRESENT: return HALO_METAL_CAP_PRESENT_EXACT;
        case HALO_METAL_PRESENT_SCALED: case HALO_METAL_DISPLAY_SETTINGS: return HALO_METAL_CAP_PRESENT_SCALED;
        case HALO_METAL_CLEAR_CHANNELS: return HALO_METAL_CAP_CLEAR_CHANNELS;
        case HALO_METAL_CREATE_PROGRAM: case HALO_METAL_DELETE_PROGRAM:
        case HALO_METAL_DRAW: return HALO_METAL_CAP_DRAW;
        case HALO_METAL_DRAW_ALPHA_BORDER: return HALO_METAL_CAP_DRAW | HALO_METAL_CAP_ALPHA_BORDER;
        case HALO_METAL_DRAW_VOLUME_BORDER: return HALO_METAL_CAP_DRAW | HALO_METAL_CAP_VOLUME | HALO_METAL_CAP_VOLUME_BORDER;
        case HALO_METAL_DRAW_COMPACT: return HALO_METAL_CAP_DRAW | HALO_METAL_CAP_COMPACT_VERTICES;
        case HALO_METAL_CREATE_VISIBILITY: case HALO_METAL_DELETE_VISIBILITY:
        case HALO_METAL_BEGIN_VISIBILITY: case HALO_METAL_END_VISIBILITY: return HALO_METAL_CAP_VISIBILITY;
        default: return 0;
    }
}
static int recording_valid(struct halo_metal_guest_transport *t) {
    if (!storage_valid(t)) return fail(t, HALO_METAL_INVALID, "invalid native packet storage");
    if (!t->initialized) return fail(t, HALO_METAL_NOT_INITIALIZED, "native transport is not initialized");
    if (t->poisoned) return fail(t, HALO_METAL_GPU_ERROR, "native transport requires shutdown after an uncertain host result");
    if (t->status) return t->status;
    if (!t->recording || t->size < sizeof(struct halo_metal_packet) || t->size > t->capacity || (t->size & 7u))
        return fail(t, HALO_METAL_INVALID, "native packet is not recording");
    return HALO_METAL_OK;
}
static void synchronize_header(struct halo_metal_guest_transport *t) {
    struct halo_metal_packet *h = (void *)t->bytes;
    h->byte_size = t->size; h->command_count = t->command_count;
}
static int align_extent(struct halo_metal_guest_transport *t, uint32_t start,
                        uint32_t size, uint32_t alignment, uint32_t *begin, uint32_t *end) {
    uint32_t mask = alignment - 1u;
    if (start > UINT32_MAX - mask) return fail(t, HALO_METAL_MEMORY, "native packet alignment overflow");
    uint32_t aligned = (start + mask) & ~mask;
    if (aligned > t->capacity || size > t->capacity - aligned)
        return fail(t, HALO_METAL_MEMORY, "native command or payload exceeds packet capacity");
    uint32_t extent = aligned + size;
    if (extent > UINT32_MAX - 7u) return fail(t, HALO_METAL_MEMORY, "native packet extent overflow");
    extent = (extent + 7u) & ~7u;
    if (extent > t->capacity) return fail(t, HALO_METAL_MEMORY, "native packet padding exceeds capacity");
    *begin = aligned; *end = extent; return HALO_METAL_OK;
}
static int validate_reply(struct halo_metal_guest_transport *t, int returned,
                          uint64_t minimum_submitted, uint64_t minimum_completed,
                          uint32_t maximum_failed_command, uint32_t expected_bytes) {
    struct halo_metal_reply *r = &t->reply;
    if (r->abi_version != HALO_METAL_ABI_VERSION || r->reserved ||
        r->status > HALO_METAL_OK || r->status < HALO_METAL_UNDEFINED_CONTENT || r->status != returned ||
        (r->capabilities & ~ALL_CAPABILITIES) || r->completed_sequence > r->submitted_sequence ||
        r->submitted_sequence < minimum_submitted || r->completed_sequence < minimum_completed ||
        (r->failed_command != UINT32_MAX && r->failed_command >= maximum_failed_command) ||
        (returned == HALO_METAL_OK && (r->failed_command != UINT32_MAX || r->byte_size != expected_bytes)) ||
        (!expected_bytes && r->content_version) || (returned != HALO_METAL_OK && (r->byte_size || r->content_version))) {
        t->poisoned = 1;
        return fail(t, HALO_METAL_INVALID, "native host returned an invalid ABI, status, sequence or command reply");
    }
    if ((r->capabilities & t->required_capabilities) != t->required_capabilities) {
        t->poisoned = 1;
        return fail(t, HALO_METAL_UNSUPPORTED, "native host lacks required capabilities");
    }
    if (returned != HALO_METAL_OK) {
        if (returned == HALO_METAL_GPU_ERROR) t->poisoned = 1;
        return fail(t, returned, "native host rejected the operation; inspect reply.status and reply.failed_command");
    }
    t->status = HALO_METAL_OK; t->error = NULL; return HALO_METAL_OK;
}

int halo_metal_guest_setup(struct halo_metal_guest_transport *t, void *storage, uint32_t capacity) {
    if (!t) return HALO_METAL_INVALID;
    zero_bytes(t, sizeof(*t)); t->bytes = storage; t->capacity = capacity;
    return storage_valid(t) ? HALO_METAL_OK : fail(t, HALO_METAL_INVALID, "native storage must be aligned16 and between 32B and 64MiB");
}
int halo_metal_guest_initialize(struct halo_metal_guest_transport *t, uint32_t window,
                               uint32_t flags, uint32_t required) {
    if (!storage_valid(t)) return fail(t, HALO_METAL_INVALID, "invalid native packet storage");
    if (t->initialized) return fail(t, HALO_METAL_INVALID, "native transport already owns a host context");
    if ((flags & ~(HALO_METAL_OFFSCREEN | HALO_METAL_ENABLE_FXAA | HALO_METAL_ENABLE_COMPACT_VERTICES)) ||
        ((flags & HALO_METAL_OFFSCREEN) ? window != 0 : window == 0))
        return fail(t, HALO_METAL_INVALID, "native window handle and offscreen flag disagree");
    if (required & ~ALL_CAPABILITIES) return fail(t, HALO_METAL_UNSUPPORTED, "unknown required native capability");
    t->required_capabilities = required; t->poisoned = 0; zero_bytes(&t->reply, sizeof(t->reply));
    int returned = host_metal_initialize(window, flags, address(&t->reply), sizeof(t->reply));
    int result = validate_reply(t, returned, 0, 0, 0, 0);
    if (!result && (t->reply.submitted_sequence || t->reply.completed_sequence || t->reply.live_resources ||
        (!!(t->reply.capabilities & HALO_METAL_CAP_PRESENT_EXACT) != !(flags & HALO_METAL_OFFSCREEN))))
        result = fail(t, HALO_METAL_INVALID, "native initialization returned a nonempty context or wrong presentation capability");
    if (result) { if (returned == HALO_METAL_OK) host_metal_shutdown(); return result; }
    t->initialized = 1; t->recording = 0; return HALO_METAL_OK;
}
int halo_metal_guest_begin(struct halo_metal_guest_transport *t, uint64_t sequence) {
    if (!storage_valid(t)) return fail(t, HALO_METAL_INVALID, "invalid native packet storage");
    if (!t->initialized) return fail(t, HALO_METAL_NOT_INITIALIZED, "native transport is not initialized");
    if (t->poisoned) return fail(t, HALO_METAL_GPU_ERROR, "native transport requires shutdown after an uncertain host result");
    if (!sequence || sequence <= t->reply.submitted_sequence)
        return fail(t, HALO_METAL_INVALID, "native packet sequence must advance beyond host submission");
    struct halo_metal_packet h = { HALO_METAL_MAGIC, HALO_METAL_ABI_VERSION, sizeof(h), 0, sequence };
    copy_bytes(t->bytes, &h, sizeof(h)); t->size = sizeof(h); t->command_count = 0;
    t->command_offset = t->command_fixed_size = 0; t->sequence = sequence;
    t->recording = 1; t->status = HALO_METAL_OK; t->error = NULL; return HALO_METAL_OK;
}
int halo_metal_guest_append_command(struct halo_metal_guest_transport *t, const void *source,
                                   uint32_t fixed_size, uint32_t *offset) {
    int result = recording_valid(t); if (result) return result;
    if (!source || !offset || fixed_size < sizeof(struct halo_metal_command))
        return fail(t, HALO_METAL_INVALID, "invalid native fixed command metadata");
    struct halo_metal_command c; copy_bytes(&c, source, sizeof(c));
    if (c.opcode == HALO_METAL_DRAW_ALPHA_BORDER && fixed_size != sizeof(struct halo_metal_draw_alpha_border))
        return fail(t, HALO_METAL_INVALID, "native alpha-border draw requires exactly 424 bytes of fixed metadata");
    if (c.opcode == HALO_METAL_DRAW_VOLUME_BORDER && fixed_size != sizeof(struct halo_metal_draw_volume_border))
        return fail(t, HALO_METAL_INVALID, "native volume-border draw requires exactly 424 bytes of fixed metadata");
    uint32_t capability = command_capability(c.opcode);
    if (!capability || (t->reply.capabilities & capability) != capability)
        return fail(t, HALO_METAL_UNSUPPORTED, "native command is unknown or lacks a host capability");
    if (t->command_count >= 65536u) return fail(t, HALO_METAL_INVALID, "native command count exceeds 65536");
    uint32_t begin, end; result = align_extent(t, t->size, fixed_size, 8, &begin, &end); if (result) return result;
    if (overlaps_storage(t, source, fixed_size) || overlaps_storage(t, offset, sizeof(*offset)))
        return fail(t, HALO_METAL_INVALID, "native fixed metadata and offset output must not alias packet storage");
    zero_bytes(t->bytes + t->size, end - t->size); copy_bytes(t->bytes + begin, source, fixed_size);
    ((struct halo_metal_command *)(void *)(t->bytes + begin))->byte_size = end - begin;
    t->command_offset = begin; t->command_fixed_size = fixed_size; t->size = end; t->command_count++;
    synchronize_header(t); *offset = begin; return HALO_METAL_OK;
}
int halo_metal_guest_append_payload(struct halo_metal_guest_transport *t, const void *source,
                                   uint32_t size, uint32_t *offset) {
    int result = recording_valid(t); if (result) return result;
    if (!source || !size || !offset || !t->command_count || t->command_offset >= t->size ||
        t->command_fixed_size < sizeof(struct halo_metal_command) || t->command_fixed_size > t->size - t->command_offset)
        return fail(t, HALO_METAL_INVALID, "native payload requires current fixed command metadata");
    uint32_t begin, end; result = align_extent(t, t->size, size, 16, &begin, &end); if (result) return result;
    if (overlaps_storage(t, source, size) || overlaps_storage(t, offset, sizeof(*offset)))
        return fail(t, HALO_METAL_INVALID, "native payload and offset output must not alias packet storage");
    /* Only alignment bytes need initialization. Every payload byte is copied
       below, so clearing it first adds a complete redundant memory write. */
    zero_bytes(t->bytes + t->size, begin - t->size);
    copy_bytes(t->bytes + begin, source, size);
    zero_bytes(t->bytes + begin + size, end - begin - size);
    t->size = end;
    ((struct halo_metal_command *)(void *)(t->bytes + t->command_offset))->byte_size = end - t->command_offset;
    synchronize_header(t); *offset = begin; return HALO_METAL_OK;
}
int halo_metal_guest_patch_u32(struct halo_metal_guest_transport *t, uint32_t command_offset,
                              uint32_t field_offset, uint32_t value) {
    int result = recording_valid(t); if (result) return result;
    if (!t->command_count || command_offset != t->command_offset || (field_offset & 3u) || field_offset < 8u ||
        field_offset > t->command_fixed_size || t->command_fixed_size - field_offset < 4u ||
        command_offset > t->size || t->command_fixed_size > t->size - command_offset)
        return fail(t, HALO_METAL_INVALID, "native metadata patch is outside the current fixed command");
    copy_bytes(t->bytes + command_offset + field_offset, &value, sizeof(value)); return HALO_METAL_OK;
}
int halo_metal_guest_submit(struct halo_metal_guest_transport *t) {
    int result = recording_valid(t); if (result) return result;
    if (!t->command_count) return fail(t, HALO_METAL_INVALID, "native empty packet cannot be submitted");
    uint64_t submitted = t->reply.submitted_sequence, completed = t->reply.completed_sequence;
    synchronize_header(t); t->recording = 0; zero_bytes(&t->reply, sizeof(t->reply));
    int returned = host_metal_submit(address(t->bytes), t->size, address(&t->reply), sizeof(t->reply));
    result = validate_reply(t, returned, submitted, completed, t->command_count, 0);
    if (!result && t->reply.submitted_sequence != t->sequence) {
        t->poisoned = 1; return fail(t, HALO_METAL_INVALID, "native host acknowledged another packet sequence");
    }
    return result;
}
int halo_metal_guest_readback(struct halo_metal_guest_transport *t, struct halo_metal_ref ref,
                             uint32_t plane, void *destination, uint32_t size) {
    if (!storage_valid(t)) return fail(t, HALO_METAL_INVALID, "invalid native packet storage");
    if (!t->initialized) return fail(t, HALO_METAL_NOT_INITIALIZED, "native transport is not initialized");
    if (t->poisoned) return fail(t, HALO_METAL_GPU_ERROR, "native transport requires shutdown after an uncertain host result");
    /* Visibility polls a query from an earlier packet. Color and depth copies
       still need the open packet submitted first. */
    if (t->recording && plane != HALO_METAL_VISIBILITY)
        return fail(t, HALO_METAL_INVALID, "native readback requires a sealed packet");
    if (!(t->reply.capabilities & HALO_METAL_CAP_READBACK)) return fail(t, HALO_METAL_UNSUPPORTED, "native readback capability is absent");
    if (plane == HALO_METAL_VISIBILITY && !(t->reply.capabilities & HALO_METAL_CAP_VISIBILITY))
        return fail(t, HALO_METAL_UNSUPPORTED, "native visibility capability is absent");
    if (!ref.id || !ref.generation || !destination || !size ||
        (plane != HALO_METAL_COLOR && plane != HALO_METAL_DEPTH && plane != HALO_METAL_STENCIL && plane != HALO_METAL_VISIBILITY))
        return fail(t, HALO_METAL_INVALID, "invalid native readback resource, plane or destination");
    uint64_t submitted = t->reply.submitted_sequence, completed = t->reply.completed_sequence;
    zero_bytes(&t->reply, sizeof(t->reply));
    int returned = host_metal_readback(ref.id, ref.generation, plane, address(destination), size, address(&t->reply), sizeof(t->reply));
    int result = validate_reply(t, returned, submitted, completed, 0, size);
    if (!result && t->reply.submitted_sequence != submitted) {
        t->poisoned = 1; return fail(t, HALO_METAL_INVALID, "native readback changed the submission sequence");
    }
    return result;
}
void halo_metal_guest_shutdown(struct halo_metal_guest_transport *t) {
    if (!t) return;
    if (t->initialized) host_metal_shutdown();
    t->initialized = t->recording = t->poisoned = 0;
    t->size = t->command_count = t->command_offset = t->command_fixed_size = 0;
    t->sequence = 0; zero_bytes(&t->reply, sizeof(t->reply));
}
#endif
