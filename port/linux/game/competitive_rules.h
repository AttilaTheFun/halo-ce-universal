/* Competitive match rules that are not fields of the Xbox variant.
The variant the network already sends is unchanged. These bytes ride a
data message old builds ignore, and they apply only while that variant's
anniversary bit is set. */
#ifndef __COMPETITIVE_RULES_H
#define __COMPETITIVE_RULES_H
#pragma once

#include "cseries.h"
#include "math/real_math.h"

struct competitive_rules
{
	unsigned char flag_pickup;      /* 0 manual, 1 automatic */
	unsigned char ball_pickup;
	unsigned char hill_order;       /* 0 ordered, 1 random after the first */
	unsigned char hill_move_s;      /* seconds the hill stays; 60 is the old period */
	unsigned char fireteams;
	unsigned char infinite_ammo;    /* 0 no, 1 bottomless clip, 2 magazines */
	unsigned char kill_restores;
	unsigned char skip_draw;
	unsigned char movement_sounds;  /* 1 on */
	unsigned char spawns_visible;
	unsigned char random_pct;
	unsigned char omit_first;       /* 0 no, 1 yes, 2 power items */
	unsigned char item_spawn_s;     /* 0 off, else seconds for every item */
	unsigned char camo;             /* 0 xbox, 1 mcc, 2 custom (mcc numbers) */
	unsigned char camo_drop;        /* 0 off, 1 pause, 2 continue */
	unsigned char outlines;
	unsigned char os_visible;
	unsigned char os_type;          /* 0 constant, 1 fading */
	unsigned char os_kill;          /* 0 nothing, 1 full, 2 remaining */
	unsigned char plasma_climb;
	unsigned char friction;         /* 0 the tag, then none, half, full, strong */
	unsigned char adhesion;
	unsigned char aim_delay;        /* ticks */
	unsigned char trig_delay;
	unsigned char pistol_spread;    /* 0 the tag, 1 tight, 2 wide, 3 double */
	unsigned char shotgun_spread;
	unsigned char wall_jump;        /* a second jump off a wall while airborne */
};

typedef char competitive_rules_size_assert[sizeof(struct competitive_rules) == 27 ? 1 : -1];

void competitive_rules_defaults(struct competitive_rules *rules);
struct competitive_rules *competitive_rules_editing(void);
void competitive_rules_reset_editing(void);
void competitive_rules_commit(void);

boolean competitive_rules_active(void);
const struct competitive_rules *competitive_rules_live(void);

void competitive_rules_begin_map(void);
void competitive_rules_tick(void);
void competitive_rules_render(void);

boolean competitive_rules_manual_pickup(long weapon_index);
boolean competitive_rules_hill_ordered(void);
long competitive_rules_hill_move_ticks(void);
real competitive_rules_spawn_rating(long player_index, real_point3d const *position, real rating);
boolean competitive_rules_shot_blocked(long weapon_index);
real competitive_rules_friction(real tag_value);
real competitive_rules_adhesion(real tag_value);
real competitive_rules_spread(long weapon_definition_index, real error);
void competitive_rules_skip_arm(long unit_index);
boolean competitive_rules_skip_consume(long unit_index);
void competitive_rules_on_kill(long killing_player_index, long dead_player_index);
void competitive_rules_plasma_contact(long projectile_definition_index, long owner_object_index,
	real_point3d const *point);
void competitive_rules_wall_jump(long biped_index);
boolean competitive_rules_omit_spawn(long game_time, long respawn_seconds);
long competitive_rules_item_period(long period_ticks, long authored_seconds);

boolean competitive_rules_write_message(void *buffer, long buffer_size);
boolean competitive_rules_take_message(void const *message, long size);

#endif
