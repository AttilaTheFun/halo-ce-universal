/*
SHIELD_COLOR.C

The shield is drawn in its own color. A chosen armor color is not offered.
The helpers stay so a later choice can replace the two shader colors and
keep their brightness, without new assets.

Only the drawing would change; the shield itself is the game's.
*/

#include "cseries.h"
#include "real_math.h"
#include "objects/objects.h"
#include "units/units.h"
#include "game/players.h"
#include "saved games/player_profile.h"

static short shield_color_index = NONE;

static void shield_color_update(
	void)
{
	shield_color_index = NONE;
}

/* the colors a unit's shield is drawn with, given the unit's own: when the
unit is a local player's and a color is chosen, a copy of them that
shield_color_override knows again; otherwise the unit's own */
static real_rgb_color shield_color_marker[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS][4];

real_rgb_color const *shield_color_colors(
	long unit_index,
	real_rgb_color const *colors)
{
	short local_player_index;

	shield_color_update();
	if (shield_color_index == NONE || !colors)
		return colors;
	for (local_player_index = 0; local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; local_player_index++)
	{
		long player_index = local_player_get_player_index(local_player_index);

		if (player_index != NONE && player_get(player_index)->unit_index == unit_index)
		{
			csmemcpy(shield_color_marker[local_player_index], colors, sizeof(shield_color_marker[local_player_index]));
			return shield_color_marker[local_player_index];
		}
	}
	return colors;
}

/* the chosen color, when a plasma shader is drawn with colors from
shield_color_colors (FALSE: drawn as it is) */
boolean shield_color_override(
	real_rgb_color const *colors,
	real_rgb_color *chosen)
{
	short local_player_index;

	if (!colors || shield_color_index == NONE)
		return FALSE;
	for (local_player_index = 0; local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; local_player_index++)
	{
		if (colors == shield_color_marker[local_player_index])
		{
			*chosen = player_profile_get_rgb_color(shield_color_index);
			return TRUE;
		}
	}
	return FALSE;
}

/* a shield color in place of one of the shader's own: the chosen hue at the
original's brightness (its brightest channel) */
void shield_color_apply(
	real_rgb_color const *chosen,
	real_rgb_color const *original,
	real_rgb_color *result)
{
	real brightness = MAX(original->red, MAX(original->green, original->blue));
	real peak = MAX(chosen->red, MAX(chosen->green, chosen->blue));

	if (peak <= 0.f)
	{
		/* (black: the flare is added to what is behind it, so black would
		not show at all; a dark smoke-grey instead) */
		result->red = result->green = result->blue = brightness * 0.12f;
		return;
	}
	result->red = chosen->red / peak * brightness;
	result->green = chosen->green / peak * brightness;
	result->blue = chosen->blue / peak * brightness;
}
