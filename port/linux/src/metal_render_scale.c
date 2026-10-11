#if defined(HALO_MACOS_NATIVE_METAL)
#include "metal_render_scale.h"
#include <math.h>
#include <limits.h>

static int valid_preset(uint32_t height) {
    return height == 480u || height == 720u || height == 1080u ||
        height == 1440u || height == 2160u;
}
static int valid_screen_width(uint32_t width) {
    return width >= HALO_METAL_RENDER_MIN_SCREEN_WIDTH &&
        width <= HALO_METAL_RENDER_MAX_SCREEN_WIDTH && !(width & 1u);
}
static uint32_t rounded_edge(uint32_t edge, uint32_t numerator, uint32_t denominator) {
    /* Inputs are bounded by8192; widening also makes the operation safe on
     * the original ILP32 guest. Integer half-up matches GL target_pixel. */
    return (uint32_t)(((uint64_t)edge * numerator + denominator / 2u) / denominator);
}
static int valid_dimensions(const struct halo_metal_render_dimensions *d) {
    if (!d || !d->logical_width || !d->logical_height || !d->storage_width || !d->storage_height ||
        d->logical_width > HALO_METAL_RENDER_MAX_DIMENSION ||
        d->logical_height > HALO_METAL_RENDER_MAX_DIMENSION ||
        d->storage_width > HALO_METAL_RENDER_MAX_DIMENSION ||
        d->storage_height > HALO_METAL_RENDER_MAX_DIMENSION) return 0;
    if (d->scale_mode == HALO_METAL_RENDER_SCALE_NATIVE_AXES)
        return valid_screen_width(d->logical_width) &&
            d->logical_height == HALO_METAL_RENDER_LOGICAL_HEIGHT;
    if (d->scale_mode != HALO_METAL_RENDER_SCALE_UNIFORM) return 0;
    if (d->storage_width == d->logical_width && d->storage_height == d->logical_height) return 1;
    return valid_screen_width(d->logical_width) && d->logical_height == HALO_METAL_RENDER_LOGICAL_HEIGHT &&
        valid_preset(d->storage_height) &&
        d->storage_width == rounded_edge(d->logical_width, d->storage_height, d->logical_height);
}
int halo_metal_render_target_dimensions(uint32_t width, uint32_t height, uint32_t screen_width,
    uint32_t preset_height, struct halo_metal_render_dimensions *dimensions) {
    if (!dimensions || !width || !height || width > HALO_METAL_RENDER_MAX_DIMENSION ||
        height > HALO_METAL_RENDER_MAX_DIMENSION || !valid_screen_width(screen_width) ||
        !valid_preset(preset_height)) return HALO_METAL_INVALID;
    struct halo_metal_render_dimensions result = {width, height, width, height,
        HALO_METAL_RENDER_SCALE_UNIFORM};
    if (width == screen_width && height == HALO_METAL_RENDER_LOGICAL_HEIGHT) {
        result.storage_width = rounded_edge(width, preset_height, height);
        result.storage_height = preset_height;
    }
    if (!valid_dimensions(&result)) return HALO_METAL_INVALID;
    *dimensions = result;
    return HALO_METAL_OK;
}
int halo_metal_render_native_target_dimensions(uint32_t width, uint32_t height, uint32_t screen_width,
    uint32_t storage_width, uint32_t storage_height, struct halo_metal_render_dimensions *dimensions) {
    if (!dimensions || !width || !height || width > HALO_METAL_RENDER_MAX_DIMENSION ||
        height > HALO_METAL_RENDER_MAX_DIMENSION || !valid_screen_width(screen_width) ||
        !storage_width || !storage_height || storage_width > HALO_METAL_RENDER_MAX_DIMENSION ||
        storage_height > HALO_METAL_RENDER_MAX_DIMENSION) return HALO_METAL_INVALID;
    struct halo_metal_render_dimensions result = {width, height, width, height,
        HALO_METAL_RENDER_SCALE_UNIFORM};
    if (width == screen_width && height == HALO_METAL_RENDER_LOGICAL_HEIGHT) {
        result.storage_width = storage_width;
        result.storage_height = storage_height;
        result.scale_mode = HALO_METAL_RENDER_SCALE_NATIVE_AXES;
    }
    if (!valid_dimensions(&result)) return HALO_METAL_INVALID;
    *dimensions = result;
    return HALO_METAL_OK;
}
static uint32_t clipped_edge(int64_t edge, uint32_t extent) {
    if (edge <= 0) return 0;
    if (edge >= (int64_t)extent) return extent;
    return (uint32_t)edge;
}
int halo_metal_render_scale_rectangle(const struct halo_metal_render_dimensions *d,
    const struct halo_metal_render_edges *edges, struct halo_metal_render_rectangle *rectangle) {
    if (!valid_dimensions(d) || !edges || !rectangle ||
        edges->left > edges->right || edges->top > edges->bottom) return HALO_METAL_INVALID;
    uint32_t x_numerator = d->scale_mode == HALO_METAL_RENDER_SCALE_NATIVE_AXES ?
        d->storage_width : d->storage_height;
    uint32_t x_denominator = d->scale_mode == HALO_METAL_RENDER_SCALE_NATIVE_AXES ?
        d->logical_width : d->logical_height;
    uint32_t left = rounded_edge(clipped_edge(edges->left, d->logical_width), x_numerator, x_denominator);
    uint32_t right = rounded_edge(clipped_edge(edges->right, d->logical_width), x_numerator, x_denominator);
    uint32_t top = rounded_edge(clipped_edge(edges->top, d->logical_height), d->storage_height, d->logical_height);
    uint32_t bottom = rounded_edge(clipped_edge(edges->bottom, d->logical_height), d->storage_height, d->logical_height);
    struct halo_metal_render_rectangle result = {left, top, right - left, bottom - top};
    *rectangle = result;
    return HALO_METAL_OK;
}
int halo_metal_render_presentation_box(const struct halo_metal_render_dimensions *d,
    uint32_t drawable_width, uint32_t drawable_height, struct halo_metal_render_rectangle *rectangle) {
    if (!valid_dimensions(d) || !rectangle || !drawable_width || !drawable_height) return HALO_METAL_INVALID;
    uint64_t width = drawable_width;
    uint64_t height = (uint64_t)drawable_width * d->storage_height / d->storage_width;
    if (height > drawable_height) {
        height = drawable_height;
        width = (uint64_t)drawable_height * d->storage_width / d->storage_height;
    }
    if (!width || !height) return HALO_METAL_INVALID;
    struct halo_metal_render_rectangle result = {
        (drawable_width - (uint32_t)width) / 2u, (drawable_height - (uint32_t)height) / 2u,
        (uint32_t)width, (uint32_t)height};
    *rectangle = result;
    return HALO_METAL_OK;
}
int halo_metal_render_window_point(const struct halo_metal_render_dimensions *d,
    uint32_t window_width, uint32_t window_height, uint32_t drawable_width, uint32_t drawable_height,
    double window_x, double window_y, int32_t ui_offset, struct halo_metal_render_point *point) {
    if (!point || !window_width || !window_height || !isfinite(window_x) || !isfinite(window_y))
        return HALO_METAL_INVALID;
    struct halo_metal_render_rectangle box;
    int status = halo_metal_render_presentation_box(d, drawable_width, drawable_height, &box);
    if (status) return status;
    double x = floor((window_x * drawable_width / window_width - box.x) * d->logical_width / box.width - ui_offset);
    double y = floor((window_y * drawable_height / window_height - box.y) * d->logical_height / box.height);
    if (!isfinite(x) || !isfinite(y) || x < INT32_MIN || x > INT32_MAX || y < INT32_MIN || y > INT32_MAX)
        return HALO_METAL_INVALID;
    struct halo_metal_render_point result = {(int32_t)x, (int32_t)y};
    *point = result;
    return HALO_METAL_OK;
}
#endif
