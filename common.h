#ifndef PARTICLEPAC_COMMON_H
#define PARTICLEPAC_COMMON_H

#include "jstudio.h"
#include "romassets.h"

#define PAC_SCREEN_W 320
#define PAC_SCREEN_H 224
#define PAC_CLUT4_PITCH (PAC_SCREEN_W / 2)
#define PAC_CLUT4_BYTES (PAC_CLUT4_PITCH * PAC_SCREEN_H)
#define PAC_ACTOR_W 16
#define PAC_ACTOR_H 16
#define PAC_ACTOR_FRAME_BYTES ((PAC_ACTOR_W / 2) * PAC_ACTOR_H)
#define PAC_ACTOR_FRAMES 59
#define PAC_WAKE_W 32
#define PAC_WAKE_H 32
#define PAC_WAKE_FRAME_BYTES ((PAC_WAKE_W / 2) * PAC_WAKE_H)
#define PAC_WAKE_FRAMES 56
#define PAC_PICKUP_FRAME_BYTES ((16 / 2) * 16)
#define PAC_PICKUP_FRAMES 4

enum
{
    PAC_OBJ_CANVAS = 0,
    PAC_OBJ_GPU_PARTICLES = 1,
    PAC_OBJ_PLAYER_WAKE = 2,
    PAC_OBJ_GHOST_WAKE_0 = 3,
    PAC_OBJ_GHOST_WAKE_7 = 10,
    PAC_OBJ_PICKUP_GLINT = 11,
    PAC_OBJ_PLAYER = 12,
    PAC_OBJ_GHOST_0 = 13,
    PAC_OBJ_GHOST_1 = 14,
    PAC_OBJ_GHOST_2 = 15,
    PAC_OBJ_GHOST_3 = 16,
    PAC_OBJ_GHOST_4 = 17,
    PAC_OBJ_GHOST_5 = 18,
    PAC_OBJ_GHOST_6 = 19,
    PAC_OBJ_GHOST_7 = 20,
    PAC_OBJ_FRUIT = 21,
    PAC_OBJ_SCORE_POPUP = 22,
    PAC_OBJ_SENTINEL = 23
};

extern int PAC_canvas_a;
extern int PAC_canvas_b;
extern int PAC_credits_gfx;
extern int PAC_title_gfx;
extern int PAC_scores_gfx;
extern int PAC_actor_gfx;
extern int PAC_wake_gfx;
extern int PAC_pickup_gfx;
extern int PAC_popup_gfx;
extern int PAC_sentinel_gfx;
extern int PAC_particle_table;
extern int raptor_maxparts asm("raptor_maxparts");

#endif
