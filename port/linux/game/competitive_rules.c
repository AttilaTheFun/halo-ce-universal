/* Competitive rules that are not fields of the Xbox variant. Classic games
never set the anniversary bit, so none of this runs for them. The bytes ride
a data message an older build ignores. */
#include "cseries.h"
#include "math/real_math.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/game_globals.h"
#include "game/cheats.h"
#include "game/players.h"
#include "game/players_runtime.h"
#include "scenario/scenario_definitions.h"
#include "game/performance_variant.h"
#include "interface/hud_messaging.h"
#include "interface/ui_widget.h"
#include "items/weapons.h"
#include "items/weapon_definitions.h"
#include "items/equipment_definitions.h"
#include "objects/objects.h"
#include "physics/collisions.h"
#include "physics/physics.h"
#include "rasterizer/rasterizer.h"
#include "rasterizer/common/rasterizer_common.h"
#include "render/render.h"
#include "tag_files/tag_files.h"
#include "tag_files/tag_groups.h"
#include "text/draw_string.h"
#include "units/biped_definitions.h"
#include "units/bipeds.h"
#include "units/units.h"
#include "halo_port_limits.h"
#include "performance_options.h"
#include "competitive_rules.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define COMPETITIVE_RULES_VERSION 2
#define COMPETITIVE_RULES_MESSAGE_SIZE 34
#define COMPETITIVE_SKIP_SLOTS HALO_PORT_MAXIMUM_OBJECTS_PER_MAP

#ifdef HALO_PORT_MAXIMUM_NETWORK_PLAYERS
#define COMPETITIVE_PLAYER_SLOTS HALO_PORT_MAXIMUM_NETWORK_PLAYERS
#else
#define COMPETITIVE_PLAYER_SLOTS 16
#endif

boolean network_game_distributed_client(void);

static struct competitive_rules competitive_edit;
static struct competitive_rules competitive_live;
static unsigned char competitive_skip_armed[COMPETITIVE_SKIP_SLOTS];
/* bit 0: jump was already down. bit 1: this time in the air already jumped off a wall. */
static unsigned char competitive_wall_state[COMPETITIVE_SKIP_SLOTS];
static long competitive_shot_press[COMPETITIVE_SKIP_SLOTS];
static long competitive_shot_seen[COMPETITIVE_SKIP_SLOTS];
static int competitive_random_percent;
static int competitive_cheats_written;

static int competitive_anniversary(void)
{
	struct game_variant const *variant = game_engine_get_variant();

	return variant && TEST_FLAG(variant->universal_variant.flags, _game_variant_anniversary_rules_bit);
}

void competitive_rules_defaults(struct competitive_rules *rules)
{
	csmemset(rules, 0, sizeof(*rules));
	rules->hill_move_s = 60;
	rules->movement_sounds = 1;
}

struct competitive_rules *competitive_rules_editing(void)
{
	return &competitive_edit;
}

void competitive_rules_reset_editing(void)
{
	competitive_rules_defaults(&competitive_edit);
}

void competitive_rules_commit(void)
{
	competitive_live = competitive_edit;
}

boolean competitive_rules_active(void)
{
	return competitive_anniversary() ? TRUE : FALSE;
}

const struct competitive_rules *competitive_rules_live(void)
{
	return &competitive_live;
}

static void competitive_apply_camo(void)
{
	int amount;

	if (!global_rasterizer_data || competitive_live.camo == 0)
		return;
	/* MCC hardcore. Custom uses the same numbers. Xbox leaves the tag. */
	amount = 9;
	global_rasterizer_data->active_camouflage_refraction_amount = (real)amount;
	global_rasterizer_data->active_camouflage_distance_falloff = (real)amount;
	global_rasterizer_data->active_camouflage_tint_color.red = 1.0f;
	global_rasterizer_data->active_camouflage_tint_color.green = 1.0f;
	global_rasterizer_data->active_camouflage_tint_color.blue = 1.0f;
	global_rasterizer_data->active_camouflage_hyper_stealth_refraction_amount = (real)amount;
	global_rasterizer_data->active_camouflage_hyper_stealth_distance_falloff = (real)amount;
	global_rasterizer_data->active_camouflage_hyper_stealth_tint_color.red = 1.0f;
	global_rasterizer_data->active_camouflage_hyper_stealth_tint_color.green = 1.0f;
	global_rasterizer_data->active_camouflage_hyper_stealth_tint_color.blue = 1.0f;
}

static void competitive_apply_presentation(void)
{
	unsigned long flags = performance_options_get_flags();

	if (competitive_live.spawns_visible)
		flags |= _performance_option_spawn_markers;
	if (!competitive_live.movement_sounds)
		flags |= _performance_option_silent_movement;
	performance_options_apply_host_flags(flags);
	competitive_apply_camo();
}

void competitive_rules_begin_map(void)
{
	csmemset(competitive_skip_armed, 0, sizeof(competitive_skip_armed));
	csmemset(competitive_wall_state, 0, sizeof(competitive_wall_state));
	csmemset(competitive_shot_press, 0, sizeof(competitive_shot_press));
	csmemset(competitive_shot_seen, 0, sizeof(competitive_shot_seen));
	competitive_random_percent = -1;
	if (competitive_cheats_written)
	{
		cheat.bottomless_clip = FALSE;
		cheat.infinite_ammo = FALSE;
		competitive_cheats_written = 0;
	}
	if (!competitive_rules_active())
		return;
	competitive_apply_presentation();
}

void competitive_rules_tick(void)
{
	if (!competitive_rules_active())
	{
		if (competitive_cheats_written)
		{
			cheat.bottomless_clip = FALSE;
			cheat.infinite_ammo = FALSE;
			competitive_cheats_written = 0;
		}
		return;
	}
	cheat.bottomless_clip = competitive_live.infinite_ammo == 1;
	cheat.infinite_ammo = competitive_live.infinite_ammo == 2;
	competitive_cheats_written = 1;
}

boolean competitive_rules_manual_pickup(long weapon_index)
{
	struct weapon_datum *weapon;
	struct weapon_definition *definition;

	/* The flag and the ball are weapons that do not take an inventory slot.
	A sniper is readied too, and still picks up on its own. */
	if (!competitive_rules_active() || !game_engine)
		return FALSE;
	weapon = weapon_try_and_get(weapon_index);
	if (!weapon)
		return FALSE;
	definition = weapon_definition_get(weapon->definition_index);
	if (!definition || !TEST_FLAG(definition->weapon.flags, _weapon_doesnt_count_toward_maximum_bit))
		return FALSE;
	if (game_engine->type == game_engine_ctf)
		return competitive_live.flag_pickup == 0;
	if (game_engine->type == game_engine_oddball)
		return competitive_live.ball_pickup == 0;
	return FALSE;
}

boolean competitive_rules_hill_ordered(void)
{
	return competitive_rules_active() && competitive_live.hill_order == 0;
}

long competitive_rules_hill_move_ticks(void)
{
	int seconds = competitive_live.hill_move_s;

	if (seconds < 5)
		seconds = 60;
	return (long)seconds * TICKS_PER_SECOND;
}

real competitive_rules_spawn_rating(long player_index, real_point3d const *position, real rating)
{
	struct player_datum *player;
	struct game_variant const *variant;
	long mates[COMPETITIVE_PLAYER_SLOTS];
	short count, index, mine, partner;

	if (!competitive_rules_active() || !competitive_live.fireteams || rating <= 0.0f || !position)
		return rating;
	variant = game_engine_get_variant();
	player = player_try_and_get(player_index);
	if (!variant || !variant->universal_variant.teams || !player || player->team_index < 0)
		return rating;
	count = 0;
	for (index = 0; index < COMPETITIVE_PLAYER_SLOTS && count < COMPETITIVE_PLAYER_SLOTS; index++)
	{
		struct player_datum *other = player_try_and_get(index);

		if (other && other->team_index == player->team_index)
			mates[count++] = index;
	}
	if (count <= 2)
		return rating;
	mine = 0;
	for (index = 0; index < count; index++)
		if (mates[index] == player_index)
			mine = index;
	partner = (mine ^ 1) < count ? (short)mates[mine ^ 1] : (short)player_index;
	for (index = 0; index < count; index++)
	{
		struct player_datum *other;
		struct object_datum *unit;
		real dx, dy, dz;

		if (mates[index] == player_index || mates[index] == partner)
			continue;
		other = player_try_and_get(mates[index]);
		if (!other || other->unit_index == NONE)
			continue;
		unit = object_try_and_get(other->unit_index);
		if (!unit)
			continue;
		dx = unit->object.position.x - position->x;
		dy = unit->object.position.y - position->y;
		dz = unit->object.position.z - position->z;
		if (dx * dx + dy * dy + dz * dz < 36.0f)
			return rating * 0.15f;
	}
	return rating;
}

boolean competitive_rules_shot_blocked(long weapon_index)
{
	int delay;
	long now;
	unsigned slot;

	delay = (int)competitive_live.aim_delay + (int)competitive_live.trig_delay;
	if (!competitive_rules_active() || delay <= 0 || weapon_index == NONE)
		return FALSE;
	now = game_time_get();
	slot = (unsigned)DATUM_INDEX_TO_ABSOLUTE_INDEX(weapon_index);
	if (slot >= COMPETITIVE_SKIP_SLOTS)
		return FALSE;
	if (competitive_shot_seen[slot] != now - 1)
		competitive_shot_press[slot] = now;
	competitive_shot_seen[slot] = now;
	return now - competitive_shot_press[slot] < delay;
}

static real competitive_scale(unsigned char which, real tag_value, real const *table, int count)
{
	if (!competitive_rules_active() || which == 0 || which >= count)
		return tag_value;
	return tag_value * table[which];
}

real competitive_rules_friction(real tag_value)
{
	static real const table[] = { 1.0f, 0.0f, 0.5f, 1.0f, 1.5f };

	return competitive_scale(competitive_live.friction, tag_value, table, 5);
}

real competitive_rules_adhesion(real tag_value)
{
	static real const table[] = { 1.0f, 0.0f, 0.5f, 1.0f, 1.5f };

	return competitive_scale(competitive_live.adhesion, tag_value, table, 5);
}

real competitive_rules_spread(long weapon_definition_index, real error)
{
	char const *name;
	unsigned char which;
	static real const table[] = { 1.0f, 0.5f, 1.5f, 2.0f };

	if (!competitive_rules_active() || weapon_definition_index == NONE)
		return error;
	name = tag_get_name(weapon_definition_index);
	if (!name)
		return error;
	which = 0;
	if (strstr(name, "pistol"))
		which = competitive_live.pistol_spread;
	else if (strstr(name, "shotgun"))
		which = competitive_live.shotgun_spread;
	return competitive_scale(which, error, table, 4);
}

void competitive_rules_skip_arm(long unit_index)
{
	unsigned slot = (unsigned)DATUM_INDEX_TO_ABSOLUTE_INDEX(unit_index);

	if (unit_index == NONE || slot >= COMPETITIVE_SKIP_SLOTS)
		return;
	competitive_skip_armed[slot] = 1;
}

boolean competitive_rules_skip_consume(long unit_index)
{
	unsigned slot = (unsigned)DATUM_INDEX_TO_ABSOLUTE_INDEX(unit_index);

	if (unit_index == NONE || slot >= COMPETITIVE_SKIP_SLOTS || !competitive_skip_armed[slot])
		return FALSE;
	competitive_skip_armed[slot] = 0;
	return competitive_rules_active() && competitive_live.skip_draw;
}

static void competitive_drop_powerup(long unit_index, char const *name)
{
	long definition;
	struct object_datum *unit;
	struct object_placement_data placement;
	long object_index;

	definition = tag_loaded(EQUIPMENT_DEFINITION_TAG, name);
	unit = object_try_and_get(unit_index);
	if (definition == NONE || !unit)
		return;
	object_placement_data_new(&placement, definition, NONE);
	placement.position = unit->object.position;
	object_index = object_new(&placement);
	if (object_index != NONE)
		object_set_garbage(object_index, FALSE);
}

void competitive_rules_on_kill(long killing_player_index, long dead_player_index)
{
	struct player_datum *killer;
	struct player_datum *dead;
	struct unit_datum *killer_unit;
	struct unit_datum *dead_unit;

	if (!competitive_rules_active())
		return;
	dead = player_try_and_get(dead_player_index);
	dead_unit = dead && dead->unit_index != NONE ? unit_try_and_get(dead->unit_index) : NULL;
	if (competitive_live.kill_restores && killing_player_index != NONE &&
		killing_player_index != dead_player_index)
	{
		killer = player_try_and_get(killing_player_index);
		killer_unit = killer && killer->unit_index != NONE ? unit_try_and_get(killer->unit_index) : NULL;
		/* An overshield is the same number past full. Restoring would delete it. */
		if (killer_unit && killer_unit->object.shield_vitality <= 1.0f)
		{
			killer_unit->object.shield_vitality = 1.0f;
			killer_unit->object.body_vitality = 1.0f;
		}
	}
	if (!dead || !dead_unit || network_game_distributed_client())
		return;
	if (competitive_live.camo_drop && dead_unit->unit.active_camouflage > 0.0f)
		competitive_drop_powerup(dead->unit_index, "powerups\\active camouflage");
	if (competitive_live.os_kill && dead_unit->object.shield_vitality > 1.0f)
		competitive_drop_powerup(dead->unit_index, "powerups\\over shield");
}

void competitive_rules_plasma_contact(long projectile_definition_index, long owner_object_index,
	real_point3d const *point)
{
	/* Quake 3 plasma splash, scaled from its 56-unit box onto the biped. */
	real const push_seconds = (75.0f * 10.0f / 800.0f) / 7.5f * 1.30f;
	struct biped_datum *biped;
	struct biped_definition *definition;
	char const *name;
	real_point3d base;
	real pill_height, pill_radius, floor_z, top_z, scale, radius;
	real dx, dy, rho, dr, dz, distance, falloff;
	real_vector3d push;
	real push_length, push_per_tick;

	if (!competitive_rules_active() || !competitive_live.plasma_climb || !point || owner_object_index == NONE)
		return;
	name = tag_get_name(projectile_definition_index);
	if (!name || !strstr(name, "plasma rifle") || !strstr(name, "bolt"))
		return;
	biped = biped_try_and_get(owner_object_index);
	if (!biped || TEST_FLAG(biped->object.damage_flags, _object_dead_bit) ||
		biped->unit.player_index == NONE || biped->object.parent_object_index != NONE)
		return;
	definition = biped_definition_get(biped->definition_index);
	biped_get_physics_pill(owner_object_index, &base, &pill_height, &pill_radius);
	floor_z = base.z - pill_radius;
	top_z = base.z + pill_height + pill_radius;
	scale = definition->biped.collision_height_standing / 56.0f;
	if (!(scale > 0.0f))
		scale = (top_z - floor_z) / 56.0f;
	radius = 20.0f * scale;
	dx = point->x - base.x;
	dy = point->y - base.y;
	rho = (real)sqrt(dx * dx + dy * dy);
	dr = rho > pill_radius ? rho - pill_radius : 0.0f;
	dz = point->z < floor_z ? floor_z - point->z : point->z > top_z ? point->z - top_z : 0.0f;
	distance = (real)sqrt(dr * dr + dz * dz);
	if (distance >= radius)
		return;
	falloff = 1.0f - distance / radius;
	push.i = base.x - point->x;
	push.j = base.y - point->y;
	push.k = (floor_z + 48.0f * scale) - point->z;
	push_length = (real)sqrt(push.i * push.i + push.j * push.j + push.k * push.k);
	if (push_length < 0.000001f)
		return;
	push_per_tick = push_seconds * global_gravity * (real)TICKS_PER_SECOND * falloff / push_length;
	biped->object.translational_velocity.i += push.i * push_per_tick;
	biped->object.translational_velocity.j += push.j * push_per_tick;
	biped->object.translational_velocity.k += push.k * push_per_tick;
	SET_FLAG(biped->object.flags, _object_at_rest_bit, FALSE);
	biped->biped.flags |= FLAG(_biped_airborne_bit) | FLAG(_biped_slipping_bit);
}

void competitive_rules_wall_jump(long biped_index)
{
	struct biped_datum *biped;
	struct biped_definition *definition;
	struct collision_result hit;
	real_point3d origin;
	real_point3d base;
	real_vector3d ray;
	real pill_height, pill_radius;
	real fx, fy, flen, reach, best, jump;
	real nx, ny, nlen;
	int slot, direction, jump_down, pressed;
	unsigned long flags;

	slot = DATUM_INDEX_TO_ABSOLUTE_INDEX(biped_index);
	if (slot < 0 || slot >= COMPETITIVE_SKIP_SLOTS)
		return;
	if (!competitive_rules_active() || !competitive_live.wall_jump)
	{
		competitive_wall_state[slot] = 0;
		return;
	}
	biped = biped_try_and_get(biped_index);
	if (!biped || biped->unit.player_index == NONE || biped->object.parent_object_index != NONE ||
		TEST_FLAG(biped->object.damage_flags, _object_dead_bit))
		return;
	jump_down = TEST_FLAG(biped->unit.control_flags, _unit_control_jump_bit) != 0;
	pressed = jump_down && (competitive_wall_state[slot] & 1) == 0;
	if (jump_down)
		competitive_wall_state[slot] |= 1;
	else
		competitive_wall_state[slot] &= (unsigned char)~1;
	if (!TEST_FLAG(biped->biped.flags, _biped_airborne_bit))
	{
		competitive_wall_state[slot] &= (unsigned char)~2;
		return;
	}
	if (!pressed || (competitive_wall_state[slot] & 2))
		return;
	biped_get_physics_pill(biped_index, &base, &pill_height, &pill_radius);
	fx = biped->object.forward.i;
	fy = biped->object.forward.j;
	flen = (real)sqrt(fx * fx + fy * fy);
	if (flen < 0.001f)
	{
		fx = 1.0f;
		fy = 0.0f;
		flen = 1.0f;
	}
	fx /= flen;
	fy /= flen;
	origin.x = base.x;
	origin.y = base.y;
	origin.z = base.z + pill_height * 0.5f;
	reach = pill_radius + 0.2f;
	best = 2.0f;
	nx = 0.0f;
	ny = 0.0f;
	flags = FLAG(_collision_test_front_facing_surfaces_bit) | FLAG(_collision_test_structure_bit) |
		FLAG(_collision_test_objects_bit) | FLAG(_collision_test_objects_scenery_bit) |
		FLAG(_collision_test_objects_machines_bit);
	for (direction = 0; direction < 8; direction++)
	{
		real angle = (real)direction * (3.14159265f * 0.25f);
		real dx = fx * (real)cos(angle) - fy * (real)sin(angle);
		real dy = fx * (real)sin(angle) + fy * (real)cos(angle);

		ray.i = dx * reach;
		ray.j = dy * reach;
		ray.k = 0.0f;
		if (!collision_test_vector(flags, &origin, &ray, biped_index, &hit))
			continue;
		if (hit.t >= best || hit.plane.n.k > 0.5f || hit.plane.n.k < -0.5f)
			continue;
		nlen = (real)sqrt(hit.plane.n.i * hit.plane.n.i + hit.plane.n.j * hit.plane.n.j);
		if (nlen < 0.001f)
			continue;
		best = hit.t;
		nx = hit.plane.n.i / nlen;
		ny = hit.plane.n.j / nlen;
	}
	if (best > 1.0f)
		return;
	definition = biped_definition_get(biped->definition_index);
	jump = definition->biped.jump_velocity;
	biped->object.translational_velocity.i += nx * jump;
	biped->object.translational_velocity.j += ny * jump;
	if (biped->object.translational_velocity.k < jump)
		biped->object.translational_velocity.k = jump;
	SET_FLAG(biped->object.flags, _object_at_rest_bit, FALSE);
	competitive_wall_state[slot] |= 2;
}

boolean competitive_rules_omit_spawn(long now, long respawn_seconds)
{
	if (!competitive_rules_active() || now != 0)
		return FALSE;
	if (competitive_live.omit_first == 1)
		return TRUE;
	return competitive_live.omit_first == 2 && respawn_seconds >= 30;
}

long competitive_rules_item_period(long period_ticks, long authored_seconds)
{
	(void)authored_seconds;
	if (!competitive_rules_active() || competitive_live.item_spawn_s == 0)
		return period_ticks;
	return (long)competitive_live.item_spawn_s * TICKS_PER_SECOND;
}

static void competitive_random_update(void)
{
	struct player_datum *local;
	struct unit_datum *body;
	struct game_variant const *variant;
	real near_weight, total_weight;
	short index, count;
	long player_index;

	competitive_random_percent = -1;
	variant = game_engine_get_variant();
	if (!competitive_live.random_pct || !variant || !variant->universal_variant.teams ||
		!players_globals || players_globals->local_player_count <= 0)
		return;
	player_index = players_globals->local_players[0];
	local = player_try_and_get(player_index);
	if (!local || local->unit_index == NONE)
		return;
	body = unit_try_and_get(local->unit_index);
	if (!body)
		return;
	near_weight = 0.0f;
	total_weight = 0.0f;
	count = player_get_starting_location_count();
	for (index = 0; index < count; index++)
	{
		struct player_starting_location const *start = player_get_starting_location(index);
		real rating, dx, dy, dz;

		if (!start)
			continue;
		rating = game_engine_get_starting_location_rating(player_index, start);
		if (rating <= 0.0f)
			continue;
		rating = rating * rating;
		total_weight += rating;
		dx = start->position.x - body->object.position.x;
		dy = start->position.y - body->object.position.y;
		dz = start->position.z - body->object.position.z;
		if (dx * dx + dy * dy + dz * dz < 36.0f)
			near_weight += rating;
	}
	if (total_weight <= 0.0f)
		return;
	competitive_random_percent = (int)(100.0f * (1.0f - near_weight / total_weight) + 0.5f);
	if (competitive_random_percent < 0)
		competitive_random_percent = 0;
	if (competitive_random_percent > 100)
		competitive_random_percent = 100;
}

static void competitive_marker_line(real_point3d const *position, real_argb_color const *color)
{
	real_point3d base, top;

	base = *position;
	top = base;
	top.z += 0.8f;
	rasterizer_debug_line(&base, &top, color);
}

static void competitive_markers(void)
{
	struct player_datum *local;
	real_argb_color const outline = { 1.0f, 0.2f, 0.85f, 1.0f };
	real_argb_color const shield = { 1.0f, 1.0f, 0.85f, 0.2f };
	long index;

	if (ui_widgets_active() || !players_globals || players_globals->local_player_count <= 0)
		return;
	if (!competitive_live.outlines && !competitive_live.os_visible)
		return;
	local = player_try_and_get(players_globals->local_players[0]);
	for (index = 0; index < COMPETITIVE_PLAYER_SLOTS; index++)
	{
		struct player_datum *player = player_try_and_get(index);
		struct unit_datum *unit;

		if (!player || player->unit_index == NONE)
			continue;
		unit = unit_try_and_get(player->unit_index);
		if (!unit || TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
			continue;
		if (competitive_live.outlines && local && player != local &&
			local->team_index >= 0 && player->team_index == local->team_index)
			competitive_marker_line(&unit->object.position, &outline);
		if (competitive_live.os_visible && unit->object.shield_vitality > 1.0f)
			competitive_marker_line(&unit->object.position, &shield);
	}
}

void competitive_rules_render(void)
{
	long font;
	real_argb_color color;
	rectangle2d bounds;
	char text[48];

	if (!competitive_rules_active() || !game_in_progress())
		return;
	competitive_markers();
	competitive_random_update();
	if (competitive_random_percent < 0 || ui_widgets_active() || (font = hud_get_font_index()) == NONE)
		return;
	snprintf(text, sizeof(text), "RANDOM %d%%", competitive_random_percent);
	bounds = render.camera.window_bounds;
	bounds.x0 += 16;
	bounds.y0 = bounds.y1 - 36;
	if (bounds.x0 >= bounds.x1 || bounds.y0 >= bounds.y1)
		return;
	hud_get_text_color(&color);
	draw_string_set_draw_mode(font, NONE, 0, 0, &color);
	rasterizer_draw_string(&bounds, NULL, NULL, 0, text);
}

boolean competitive_rules_write_message(void *buffer, long buffer_size)
{
	unsigned char *bytes = buffer;
	unsigned long header;

	if (!buffer || buffer_size < COMPETITIVE_RULES_MESSAGE_SIZE)
		return FALSE;
	csmemset(buffer, 0, COMPETITIVE_RULES_MESSAGE_SIZE);
	/* Host order until the connection write swaps the header word. */
	header = ((unsigned long)COMPETITIVE_RULES_MESSAGE_SIZE << 4) | (2u << 2);
	bytes[0] = (unsigned char)header;
	bytes[1] = (unsigned char)(header >> 8);
	bytes[2] = 'C';
	bytes[3] = 'R';
	bytes[4] = 'U';
	bytes[5] = 'L';
	bytes[6] = COMPETITIVE_RULES_VERSION;
	csmemcpy(bytes + 7, &competitive_live, sizeof(competitive_live));
	return TRUE;
}

boolean competitive_rules_take_message(void const *message, long size)
{
	unsigned char const *bytes = message;

	if (!message || size != COMPETITIVE_RULES_MESSAGE_SIZE)
		return FALSE;
	if (bytes[2] != 'C' || bytes[3] != 'R' || bytes[4] != 'U' || bytes[5] != 'L' ||
		bytes[6] != COMPETITIVE_RULES_VERSION)
		return FALSE;
	if (bytes[0] != (unsigned char)(((unsigned)COMPETITIVE_RULES_MESSAGE_SIZE << 4) | (2u << 2)) ||
		bytes[1] != (unsigned char)((((unsigned)COMPETITIVE_RULES_MESSAGE_SIZE << 4) | (2u << 2)) >> 8))
		return FALSE;
	csmemcpy(&competitive_live, bytes + 7, sizeof(competitive_live));
	if (game_in_progress())
		competitive_apply_presentation();
	return TRUE;
}
