/* Native guest packet builder. Storage and resource policy belong to the
 * original Direct3D frontend; this layer owns bounds and host handshakes. */
#ifndef HALO_METAL_GUEST_TRANSPORT_H
#define HALO_METAL_GUEST_TRANSPORT_H
#if defined(HALO_MACOS_NATIVE_METAL)
#include "../../macos/include/halo_metal_abi.h"

struct halo_metal_guest_transport {
    unsigned char *bytes;
    uint32_t capacity, size, command_count, command_offset, command_fixed_size;
    uint32_t required_capabilities;
    uint64_t sequence;
    struct halo_metal_reply reply;
    int initialized, recording, poisoned, status;
    const char *error;
};

/* Storage is caller-owned and 16-byte aligned. setup is for a fresh object or
 * one already shut down, never for replacing a live host owner. */
int halo_metal_guest_setup(struct halo_metal_guest_transport *, void *storage, uint32_t capacity);
int halo_metal_guest_initialize(struct halo_metal_guest_transport *, uint32_t window,
                               uint32_t flags, uint32_t required_capabilities);
int halo_metal_guest_begin(struct halo_metal_guest_transport *, uint64_t sequence);
/* Fixed command metadata is copied; its opcode is retained and byte_size is
 * derived by the builder. Appending another command seals the previous one. */
int halo_metal_guest_append_command(struct halo_metal_guest_transport *, const void *command,
                                   uint32_t fixed_size, uint32_t *command_offset);
/* Copies immutable payload into the current command's extent, aligned16.
 * All offsets are packet-relative. Padding is zeroed; source may be reused. */
int halo_metal_guest_append_payload(struct halo_metal_guest_transport *, const void *payload,
                                   uint32_t size, uint32_t *payload_offset);
/* Update only a current fixed uint32 metadata field, not opcode/extent or payload. */
int halo_metal_guest_patch_u32(struct halo_metal_guest_transport *, uint32_t command_offset,
                              uint32_t field_offset, uint32_t value);
int halo_metal_guest_submit(struct halo_metal_guest_transport *);
int halo_metal_guest_readback(struct halo_metal_guest_transport *, struct halo_metal_ref,
                             uint32_t plane, void *destination, uint32_t size);
void halo_metal_guest_shutdown(struct halo_metal_guest_transport *);
#endif
#endif
