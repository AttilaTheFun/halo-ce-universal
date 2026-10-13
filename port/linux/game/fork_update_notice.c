/*
FORK_UPDATE_NOTICE.C

The map download line is the terminal font across the top. This is that
font at half size, centered in the gap between the shield and the weapon.
It does not take a key or pause the game.
*/

#include "cseries.h"
#include "math/integer_math.h"
#include "math/real_math.h"
#include "interface/interface.h"
#include "rasterizer/rasterizer_text.h"
#include "render/render.h"
#include "text/draw_string.h"
#include "fork_update_notice.h"

#if defined(HALO_MACOS_NATIVE_METAL)
int halo_fork_update_notice(char *out, unsigned long capacity);
#endif

void fork_update_notice_render(void)
{
#if defined(HALO_MACOS_NATIVE_METAL)
	char text[160];
	long font_tag_index;
	rectangle2d bounds;
	short mid_x;
	short mid_y;

	if (!halo_fork_update_notice(text, sizeof(text)))
		return;
	font_tag_index = interface_get_tag_index(_interface_font_terminal);
	if (font_tag_index == NONE)
		return;
	bounds = render.camera.window_bounds;
	offset_rectangle2d(&bounds, -render.camera.viewport_bounds.x0,
		-render.camera.viewport_bounds.y0);
	/* the shield sits on the bottom left and the weapon on the bottom
	   right. half size keeps this line in the space between them. */
	if (bounds.y1 - bounds.y0 < 40)
		return;
	bounds.y0 = (short)(bounds.y1 - 36);
	bounds.y1 = (short)(bounds.y1 - 20);
	mid_x = (short)((bounds.x0 + bounds.x1) / 2);
	mid_y = (short)((bounds.y0 + bounds.y1) / 2);
	/* centered, as the map download line is */
	draw_string_set_draw_mode(font_tag_index, NONE, 2, 0, global_real_argb_white);
	rasterizer_draw_string_scaled(&bounds, NULL, text, 0.5f, (float)mid_x, (float)mid_y);
#endif
}
