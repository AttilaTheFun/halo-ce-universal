/* Bounded CPU state for the original loading-screen vertex pipeline.
 * This is the checked-in game's unlit SetVertexShader(0) immediate route,
 * not a general Windows FVF or Xbox fixed-function implementation. */
#ifndef HALO_METAL_FIXED_FUNCTION_H
#define HALO_METAL_FIXED_FUNCTION_H
#include "metal_draw_state.h"

struct metal_fixed_function_input {
    const uint32_t *render_state;        /* original [144] */
    const uint32_t (*texture_state)[32]; /* original [4][32] */
    const float *world, *view, *projection; /* original row-major [16] each */
    const struct metal_draw_vertex_uniforms *base;
    uint32_t immediate;
};

/* Atomic: output is unchanged on failure. The original device constant bank
 * is never modified. Only this fixed route's private uniform copy uses
 * c[0..3]=World, c[4..7]=View and c[8..11]=Projection. The remaining original
 * bank bytes and the existing viewport/scalar ABI are retained verbatim.
 * Supports unlit immediate input, direct T0..3 and disabled texture matrices.
 * Table fog, streams/FVF, lighting and generated texture coordinates remain
 * unsupported. Enabled fog with a pixel combiner reading F0 is unsupported.
 * The original loading pixel programs do not consume fog or
 * specular, so this route's fog output is 1 and specular is the raw v4 color.
 */
int metal_fixed_function_pack_unlit_immediate(
    const struct metal_fixed_function_input *input,
    struct metal_draw_vertex_uniforms *output,
    struct metal_draw_state_error *error);

/* Allocated direct Metal source; caller frees it. Uses the existing expanded
 * 16-register/VS3120 ABI, original v0/v3/v4/v9..12 semantics and row-vector
 * transforms. Includes the same pixel-center bridge as the native NV2A path;
 * projection Z is already normalized D3D clip depth and is not divided by an
 * original raw depth range. Safe vertex compiler contract is required. */
char *metal_fixed_function_vertex_to_msl(void);
#endif
