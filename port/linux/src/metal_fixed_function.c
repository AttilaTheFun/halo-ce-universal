#include "xgpu.h"
#include "metal_fixed_function.h"
#include "xgpu_msl.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int fixed_fail(struct metal_draw_state_error *error, int status,
    uint32_t stage, uint32_t state, uint32_t value, const char *message)
{
    if (error) *error=(struct metal_draw_state_error){stage,state,value,message};
    return status;
}

static int fixed_reads_fog(uint32_t inputs, unsigned bytes)
{
    for (unsigned i=0;i<bytes;i++) if (((inputs>>(24-8*i))&15)==3) return 1;
    return 0;
}

int metal_fixed_function_pack_unlit_immediate(
    const struct metal_fixed_function_input *input,
    struct metal_draw_vertex_uniforms *output,
    struct metal_draw_state_error *error)
{
    if (!input || !output || !input->render_state || !input->texture_state ||
        !input->world || !input->view || !input->projection || !input->base)
        return fixed_fail(error,HALO_METAL_INVALID,UINT32_MAX,0,0,
            "missing fixed-function state");
    if (input->immediate>1)
        return fixed_fail(error,HALO_METAL_INVALID,UINT32_MAX,0,input->immediate,
            "malformed fixed-function immediate flag");
    if (!input->immediate)
        return fixed_fail(error,HALO_METAL_UNSUPPORTED,UINT32_MAX,0,0,
            "fixed-function stream/FVF input is unsupported");
    if (input->render_state[D3DRS_LIGHTING])
        return fixed_fail(error,HALO_METAL_UNSUPPORTED,UINT32_MAX,D3DRS_LIGHTING,
            input->render_state[D3DRS_LIGHTING],"fixed-function lighting is unsupported");
    if (input->render_state[D3DRS_FOGTABLEMODE]!=D3DFOG_NONE)
        return fixed_fail(error,HALO_METAL_UNSUPPORTED,UINT32_MAX,D3DRS_FOGTABLEMODE,
            input->render_state[D3DRS_FOGTABLEMODE],"fixed-function table fog is unsupported");
    if (input->render_state[D3DRS_FOGENABLE]) {
        unsigned count=input->render_state[D3DRS_PSCOMBINERCOUNT]&255;
        if (count>8) count=8; /* the pixel emitter separately validates count */
        for (unsigned stage=0;stage<count;stage++)
            if (fixed_reads_fog(input->render_state[D3DRS_PSALPHAINPUTS0+stage],4) ||
                fixed_reads_fog(input->render_state[D3DRS_PSRGBINPUTS0+stage],4))
                return fixed_fail(error,HALO_METAL_UNSUPPORTED,stage,D3DRS_FOGENABLE,1,
                    "fixed-function pixel fog input is unsupported");
        if (fixed_reads_fog(input->render_state[D3DRS_PSFINALCOMBINERINPUTSABCD],4) ||
            fixed_reads_fog(input->render_state[D3DRS_PSFINALCOMBINERINPUTSEFG],3))
            return fixed_fail(error,HALO_METAL_UNSUPPORTED,UINT32_MAX,D3DRS_FOGENABLE,1,
                "fixed-function pixel fog input is unsupported");
    }
    for (uint32_t stage=0;stage<4;stage++) {
        if (input->texture_state[stage][D3DTSS_TEXCOORDINDEX]!=stage)
            return fixed_fail(error,HALO_METAL_UNSUPPORTED,stage,D3DTSS_TEXCOORDINDEX,
                input->texture_state[stage][D3DTSS_TEXCOORDINDEX],
                "fixed-function generated or remapped texture coordinates are unsupported");
        if (input->texture_state[stage][D3DTSS_TEXTURETRANSFORMFLAGS])
            return fixed_fail(error,HALO_METAL_UNSUPPORTED,stage,D3DTSS_TEXTURETRANSFORMFLAGS,
                input->texture_state[stage][D3DTSS_TEXTURETRANSFORMFLAGS],
                "fixed-function texture matrices are unsupported");
    }
    const float *matrices[]={input->world,input->view,input->projection};
    const uint32_t states[]={D3DTS_WORLD,D3DTS_VIEW,D3DTS_PROJECTION};
    for (unsigned matrix=0;matrix<3;matrix++) for (unsigned word=0;word<16;word++)
        if (!isfinite(matrices[matrix][word])) {
            uint32_t bits;memcpy(&bits,matrices[matrix]+word,sizeof(bits));
            return fixed_fail(error,HALO_METAL_INVALID,UINT32_MAX,states[matrix],bits,
                "nonfinite fixed-function transform");
        }
    /* Retain valid original unused bank padding, including raw NaN bits. */
    float tail[12];memcpy(tail,input->base->viewport_scale,sizeof(tail));
    for (unsigned word=0;word<12;word++) if (!isfinite(tail[word])) {
        uint32_t bits;memcpy(&bits,tail+word,sizeof(bits));
        return fixed_fail(error,HALO_METAL_INVALID,UINT32_MAX,0,bits,
            "nonfinite fixed-function viewport or scalar");
    }
    if (!(input->base->viewport_scale[0]>0) || !(input->base->viewport_scale[1]<0))
        return fixed_fail(error,HALO_METAL_INVALID,UINT32_MAX,0,0,
            "fixed-function viewport requires positive X and negative Y scale");
    struct metal_draw_vertex_uniforms result;
    memcpy(&result,input->base,sizeof(result));
    for (unsigned matrix=0;matrix<3;matrix++)
        memcpy(result.c+matrix*4,matrices[matrix],16*sizeof(float));
    memcpy(output,&result,sizeof(result));
    if (error) *error=(struct metal_draw_state_error){UINT32_MAX,0,0,NULL};
    return HALO_METAL_OK;
}

char *metal_fixed_function_vertex_to_msl(void)
{
    static const char source[]=XGPU_MSL_TYPES XGPU_MSL_VERTEX_VARYINGS
        XGPU_MSL_VERTEX_UNIFORMS
        "struct XgpuFixedInput {\n"
        "float4 position [[attribute(0)]], diffuse [[attribute(3)]], specular [[attribute(4)]];\n"
        "float4 tex0 [[attribute(9)]], tex1 [[attribute(10)]], tex2 [[attribute(11)]], tex3 [[attribute(12)]];\n"
        "};\n"
        "float4 fixed_transform(float4 v, constant float4 *rows)\n"
        "{\n"
        "float4 result = v.x * rows[0];\n"
        "result = result + v.y * rows[1];\n"
        "result = result + v.z * rows[2];\n"
        "return result + v.w * rows[3];\n"
        "}\n"
        "vertex XgpuVaryings xgpu_vertex(XgpuFixedInput input [[stage_in]],\n"
        "constant XgpuVertexUniforms &uniforms [[buffer(0)]])\n"
        "{\n"
        "float4 world_position = fixed_transform(input.position, uniforms.c);\n"
        "float4 view_position = fixed_transform(world_position, uniforms.c + 4);\n"
        "float4 clip_position = fixed_transform(view_position, uniforms.c + 8);\n"
        "float2 pixel_offset = float2(0.5 + uniforms.screen_offset, 0.5);\n"
        "clip_position.xy = clip_position.xy + (pixel_offset * clip_position.w) / uniforms.viewport_scale.xy;\n"
        "XgpuVaryings output;\n"
        "output.position = clip_position;\n"
        "output.point_size = uniforms.point_size;\n"
        "output.xD0 = clamp(input.diffuse, 0.0, 1.0);\n"
        "output.xD1 = clamp(input.specular, 0.0, 1.0);\n"
        "output.xB0 = output.xD0; output.xB1 = output.xD1;\n"
        "output.xT0 = input.tex0; output.xT1 = input.tex1;\n"
        "output.xT2 = input.tex2; output.xT3 = input.tex3;\n"
        "output.xFog = 1.0;\n"
        "return output;\n"
        "}\n";
    char *result=malloc(sizeof(source));
    if (result) memcpy(result,source,sizeof(source));
    return result;
}
