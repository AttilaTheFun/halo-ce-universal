/* CPU-only native backing-size and coordinate planning. The title's surface
 * metadata, shader constants and input coordinates stay in logical units. */
#ifndef HALO_METAL_RENDER_SCALE_H
#define HALO_METAL_RENDER_SCALE_H
#if defined(HALO_MACOS_NATIVE_METAL)
#include "../../macos/include/halo_metal_abi.h"

#define HALO_METAL_RENDER_LOGICAL_HEIGHT 480u
#define HALO_METAL_RENDER_MIN_SCREEN_WIDTH 640u
#define HALO_METAL_RENDER_MAX_SCREEN_WIDTH 1600u
/* The existing host's make_texture validation limits each storage axis. */
#define HALO_METAL_RENDER_MAX_DIMENSION 8192u
#define HALO_METAL_RENDER_SCALE_UNIFORM 0u
#define HALO_METAL_RENDER_SCALE_NATIVE_AXES 1u

struct halo_metal_render_dimensions {
    uint32_t logical_width, logical_height, storage_width, storage_height;
    uint32_t scale_mode;
};
struct halo_metal_render_edges { int64_t left, top, right, bottom; };
struct halo_metal_render_rectangle { uint32_t x, y, width, height; };
struct halo_metal_render_point { int32_t x, y; };

/* Screen width must be even640..1600. preset_height is480/720/1080/1440/2160.
 * Scale only a logical screen_width x480 target; all other target sizes remain
 * unchanged. This does not change authored texture dimensions or XDK headers.
 * Storage width rounds the logical right edge using preset_height/480, half up.
 * Every output in this API remains byte-for-byte unchanged on any error. */
int halo_metal_render_target_dimensions(uint32_t logical_width, uint32_t logical_height,
    uint32_t screen_width, uint32_t preset_height, struct halo_metal_render_dimensions *dimensions);

/* Native backing dimensions are measured from the synchronized SDL drawable.
 * Screen targets use those exact physical dimensions, with separate X/Y edge
 * scales like desktop OpenGL. The title still sees its logical screen_width
 * x480 surface. Other authored/offscreen sizes remain unchanged. Callers fit
 * an explicitly requested aspect before passing its native backing size.
 * Zero and dimensions above the host's8192-axis limit fail without mutation. */
int halo_metal_render_native_target_dimensions(uint32_t logical_width, uint32_t logical_height,
    uint32_t screen_width, uint32_t storage_width, uint32_t storage_height,
    struct halo_metal_render_dimensions *dimensions);

/* Clip ordered logical edges to the target, round each edge half up, then
 * subtract endpoints. Use the uniform storage_height/logical_height ratio for
 * BOTH axes in uniform mode, including when storage_width was rounded. Native
 * mode uses storage_width/logical_width for X and the height ratio for Y.
 * Adjacent logical edges
 * therefore share exactly one pixel edge. Empty/intersection-free rectangles
 * succeed with zero extent; callers skip them. Reversed edges fail INVALID.
 * UI offsets and viewport intersections are applied in logical units first. */
int halo_metal_render_scale_rectangle(const struct halo_metal_render_dimensions *dimensions,
    const struct halo_metal_render_edges *edges, struct halo_metal_render_rectangle *rectangle);

/* Match the host's integer aspect fit of the actual storage texture into a
 * drawable. The centered box describes physical drawable pixels. */
int halo_metal_render_presentation_box(const struct halo_metal_render_dimensions *dimensions,
    uint32_t drawable_width, uint32_t drawable_height, struct halo_metal_render_rectangle *rectangle);

/* Convert a window point through that exact physical presentation box, then
 * back to logical units and subtract the title's horizontal menu UI offset.
 * Floor matches the existing mouse conversion. Points in letterbox margins
 * remain outside the logical canvas. No clamping or render-preset-dependent
 * rescaling of UI coordinates occurs. Nonfinite/overflowing points fail. */
int halo_metal_render_window_point(const struct halo_metal_render_dimensions *dimensions,
    uint32_t window_width, uint32_t window_height, uint32_t drawable_width, uint32_t drawable_height,
    double window_x, double window_y, int32_t ui_offset, struct halo_metal_render_point *point);
#endif
#endif
