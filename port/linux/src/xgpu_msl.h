/* Native Metal shader ABI. These declarations are emitted directly from the
 * Xbox instruction/state translators; they do not require a GL runtime. */
#ifndef XGPU_MSL_H
#define XGPU_MSL_H

#define XGPU_MSL_TYPES \
    "#include <metal_stdlib>\nusing namespace metal;\n" \
    "typedef float2 vec2; typedef float3 vec3; typedef float4 vec4;\n"

#define XGPU_MSL_INTERPOLANTS \
    "float4 xD0 [[user(xD0)]], xD1 [[user(xD1)]];\n" \
    "float4 xB0 [[user(xB0)]], xB1 [[user(xB1)]];\n" \
    "float4 xT0 [[user(xT0)]], xT1 [[user(xT1)]];\n" \
    "float4 xT2 [[user(xT2)]], xT3 [[user(xT3)]];\n" \
    "float xFog [[user(xFog)]];\n"

#define XGPU_MSL_VERTEX_VARYINGS \
    "struct XgpuVaryings { float4 position [[position, invariant]]; float point_size [[point_size]];\n" \
    XGPU_MSL_INTERPOLANTS "};\n"
#define XGPU_MSL_FRAGMENT_VARYINGS \
    "struct XgpuVaryings { float4 position [[position]];\n" \
    XGPU_MSL_INTERPOLANTS "};\n"
#define XGPU_MSL_VARYINGS XGPU_MSL_FRAGMENT_VARYINGS

#define XGPU_MSL_VERTEX_UNIFORMS \
    "struct XgpuVertexUniforms { float4 c[192]; float4 viewport_scale, viewport_offset;\n" \
    "float point_size, screen_offset; float padding[2]; };\n"
#define XGPU_MSL_PIXEL_UNIFORMS \
    "struct XgpuPixelUniforms { float4 ps_c0[8], ps_c1[8];\n" \
    "float4 ps_final_c0, ps_final_c1, fog_color, fog_parameters;\n" \
    "float alpha_reference, depth_scale, depth_clip_min, depth_clip_max;\n" \
    "float4 bump_matrix[4], bump_luminance[4], texture_scale[4], texture_border_color[4];\n" \
    "float4 texture_lod_bias; };\n"

/* DOT_ZW uses the Xbox target's raw depth coordinates. The ordinary entrypoint
 * cannot infer that target from a pixel key and continues to reject this mode.
 * The extended entrypoint is an explicit opt-in for a captured D24 contract;
 * F16/F24 and missing/unknown contracts must not be treated as D24. */
enum xgpu_msl_depth_contract { XGPU_MSL_DEPTH_NONE = 0, XGPU_MSL_DEPTH_RAW_D24 = 1 };
struct nv2a_pixel_shader_msl_options {
    unsigned int version;
    unsigned int depth_contract;
    /* Version2 and later. Each set bit uses the same normalized texture2D twice:
     * transparent-black sampler at stage and an otherwise identical
     * opaque-black sampler at4+stage. Reconstruct only the authored black
     * border's alpha before COLOR_SIGN, alpha kill and combiner execution.
     * The caller validates original border RGB0 and the sampled footprint.
     * Version1 retains its original two-word ABI and never reads this word. */
    unsigned int native_alpha_border_mask;
    /* Version3 only. Each set bit uses the same normalized texture3D with
     * transparent-black and otherwise identical opaque-white samplers at
     * stage and4+stage. Reconstruct the authored RGBA border before signed
     * conversion, alpha kill and combiners, using their implicit footprints.
     * This mask and native_alpha_border_mask must be disjoint. The caller
     * validates original color/texture/sampler contracts. Version1 and2
     * retain their two/three-word ABIs and never read this fourth word. */
    unsigned int native_volume_border_mask;
};
struct nv2a_pixel_shader_key;
char *nv2a_pixel_shader_to_msl_with_options(const struct nv2a_pixel_shader_key *,
    const struct nv2a_pixel_shader_msl_options *);

#endif
