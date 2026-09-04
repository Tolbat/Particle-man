#include "common.h"
#include "credit_art.h"

/* Particle-Man campaign build. Persistent imagery and actors remain Object
   Processor work. The 68000 owns compact game state and event publication;
   RAPTOR's GPU simulates and plots the bounded particle pool. */

#define PAC_MAP_W 24
#define PAC_MAP_H 23
#define PAC_TILE_X 13
#define PAC_TILE_Y 8
#define PAC_TILE_CENTER_X 6
#define PAC_TILE_CENTER_Y 4
#define PAC_MAP_LEFT 4 /* 24 * 13 = 312: centred in the real 320px display. */
#define PAC_MAP_TOP 20
#define PAC_FX_MAX_INJECT 16
#define PAC_FX_FRAME_BUDGET 10
#define PAC_PARTICLE_SOFT_LIMIT 32
#define PAC_TUNNEL_Y 10
#define PAC_HOUSE_LEFT 10
#define PAC_HOUSE_RIGHT 13
#define PAC_HOUSE_DOOR_Y 8
#define PAC_PLAYER_TILE_X 11
#define PAC_PLAYER_TILE_Y 17
#define PAC_FRUIT_TILE_X 11
#define PAC_FRUIT_TILE_Y 13
#define PAC_WARP_A_X 1
#define PAC_WARP_A_Y 3
#define PAC_WARP_B_X 22
#define PAC_WARP_B_Y 21
#define PAC_FRAME_GHOST_BASE 8
#define PAC_FRAME_POWER_BASE 16
#define PAC_FRAME_FRUIT 24
#define PAC_FRAME_ECHO_BASE 25
#define PAC_FRAME_CENTIPEDE_BASE 57
#define PAC_WAKE_MODE_NORMAL 0
#define PAC_WAKE_MODE_POWER 1
#define PAC_WAKE_MODE_DASH 2
#define PAC_WAKE_MODE_SHIELD 3
#define PAC_WAKE_MODE_PULSE 4
#define PAC_WAKE_MODE_MAGNET 5
#define PAC_WAKE_MODE_COUNT 6
#define PAC_WAKE_GHOST_BASE (PAC_WAKE_MODE_COUNT * 8)
#define PAC_CENTIPEDE_HISTORY 32
#define PAC_CENTIPEDE_SPACING 7
#define PAC_LEVEL_MAX 50
#define PAC_CHAPTERS 10
#define PAC_GHOSTS 8
#define PAC_CLASSIC_GHOSTS 4
#define PAC_LEVEL_SPARK 6
#define PAC_LEVEL_FRUIT 11
#define PAC_LEVEL_WARP 16
#define PAC_LEVEL_ALARM 21
#define PAC_LEVEL_DASH 26
#define PAC_LEVEL_PULSE 31
#define PAC_LEVEL_SHIELD 36
#define PAC_LEVEL_CENTIPEDE 41
#define PAC_LEVEL_STORM 46
#define PAC_SCORE_COUNT 5
#define PAC_EXTRA_LIFE_FIRST 15000
#define PAC_EXTRA_LIFE_SECOND 50000
#define PAC_MAX_LIVES 5
#define PAC_EEPROM_MAGIC 0x504d414eUL
#define PAC_EEPROM_PROGRESS_MAGIC 0x4c564c32UL
#define PAC_DASH_RECHARGE_NTSC 75
#define PAC_DASH_RECHARGE_PAL 63
#define PAC_FRIGHTENED_WARNING_NTSC 120
#define PAC_FRIGHTENED_WARNING_PAL 100
#define PAC_SOUND_DIVIDER (46168 / 11025)
#define PAC_SOUND_CHANNELS 4
#define PAC_ENABLE_LEVEL_SECRET 1
#define PAC_ENABLE_UNLIMITED_SECRET 1
#define PAC_PARTICLE_CAP_DEFAULT 64
#define PAC_PARTICLE_CAP_ALLOCATED 96
#define PAC_PARTICLE_EXPIRY_SLOTS 128
#define PAC_PARTICLE_RECORD_BYTES 44
#define PAC_START_MASK (JAGPAD_A | JAGPAD_B | JAGPAD_C | JAGPAD_PAUSE)
#define PAC_DIRECTION_MASK \
    (JAGPAD_UP | JAGPAD_LEFT | JAGPAD_DOWN | JAGPAD_RIGHT)
#define PAC_READY_CONTINUE_MASK \
    (PAC_DIRECTION_MASK | PAC_START_MASK | JAGPAD_OPTION)
#define PAC_TURN_GRACE 64 /* One quarter tile before/after a junction. */
/* Unsigned conversion preserves negative wrap-lane coordinates as a defined
   16.16 bit pattern without adding a function call to actor publication. */
#define PAC_SCREEN_FIXED(coordinate) (((unsigned int)(coordinate)) << 16)

#define PAC_RGB16(r,g,b) \
    (unsigned short)((((unsigned int)(r) & 31U) << 11) | \
                     (((unsigned int)(b) & 31U) << 6) | \
                     ((unsigned int)(g) & 63U))

enum
{
    PAC_PHASE_CREDITS = 0,
    PAC_PHASE_TITLE,
    PAC_PHASE_SCORES,
    PAC_PHASE_READY,
    PAC_PHASE_PLAY,
    PAC_PHASE_PAUSED,
    PAC_PHASE_LEVEL_CLEAR,
    PAC_PHASE_END
};

enum
{
    PAC_CODE_NONE = 0,
    PAC_CODE_LEVEL,
    PAC_CODE_UNLIMITED
};

enum
{
    PAC_DIR_NONE = 0,
    PAC_DIR_UP,
    PAC_DIR_LEFT,
    PAC_DIR_DOWN,
    PAC_DIR_RIGHT
};

typedef struct
{
    int tile_x;
    int tile_y;
    int direction;
    int offset;
    int speed;
    int released;
} PAC_Actor;

/* Exact 44-byte RAPTOR particle database record from raptor.h.  The GPU still
   owns motion, lifetime, colour decay and plotting; C only seeds a bounded
   inactive slot after VBL, avoiding a second GPU program launch in gameplay. */
typedef struct
{
    volatile int active;
    volatile int x;
    volatile int y;
    volatile int angle;
    volatile int speed;
    volatile int angular_speed;
    volatile int colour;
    volatile int colour_decay;
    volatile int current_decay;
    volatile int life;
    volatile int address;
} PAC_RaptorParticle;

typedef struct
{
    PAC_Actor player;
    PAC_Actor ghosts[PAC_GHOSTS];
    int active_ghosts;
    int queued_direction;
    int player_drive;
    int phase;
    int ready_ticks;
    int ready_limit;
    int ready_requires_input;
    int frightened_ticks;
    int frightened_limit;
    int score;
    int lives;
    int extra_life_stage;
    int level;
    int pellets_remaining;
    int initial_pellets;
    int fruit_active;
    int fruit_ticks;
    int fruit_limit;
    int fruit_stage;
    int fruit_required;
    int fruit_collected;
    int fruit_respawn_ticks;
    int fruit_trigger_one;
    int fruit_trigger_two;
    int fruit_trigger_three;
    unsigned int play_ticks;
    unsigned int ghost_release_at[PAC_GHOSTS];
    int ghost_reentry_delay;
    int high_score;
    int hud_score_drawn;
    int hud_high_score_drawn;
    int hud_lives_drawn;
    int hud_combo_drawn;
    int siren_ticks;
    int ghost_cycle_ticks;
    int ghost_scatter_window;
    int ghost_accuracy;
    int flipper_ticks;
    int flipper_surge_latched;
    int crawler_spawn_ticks;
    int storm_ticks;
    int storm_notice_ticks;
    int prism_pressure;
    int prism_pressure_ticks;
    int prism_pressure_limit;
    int otto_notice_ticks;
    int wall_scheme;
    int chapter;
    int presentation_ticks;
    int presentation_limit;
    int level_clear_ticks;
    int level_clear_limit;
    int level_bonus_total;
    int level_bonus_remaining;
    int level_bonus_step;
    int combo;
    int combo_ticks;
    int combo_limit;
    int magnet_ticks;
    int warp_cooldown;
    int dash_charges;
    int dash_ticks;
    int dash_recharge_ticks;
    int dash_recharge_limit;
    int dash_recharge_step;
    int dash_recharge_next;
    int dash_recharge_segment;
    int dash_overdrive;
    int pulse_charges;
    int ghost_stun_ticks;
    int shield_charges;
    int shield_ticks;
    int energizer_flash_ticks;
    int dash_flash_ticks;
    int pulse_flash_ticks;
    int shield_flash_ticks;
    int ghost_queue[PAC_GHOSTS];
    int ghost_queue_count;
    int next_ghost_release;
    unsigned int ghost_recovered_mask;
    int hazard_x;
    int hazard_y;
    int hazard_ticks;
    int score_popup_value;
    int score_popup_ticks;
    int score_submitted;
    int score_eligible;
    int cheat_used;
    int status_drawn;
    int pellet_fx_pending;
    int pellet_fx_x;
    int pellet_fx_y;
    int sound_enabled;
    int frightened_chain;
    int vector_lane_latched;
    int centipede_announced;
    int centipede_history_head;
    int centipede_history_count;
    int frightened_audio_ticks;
    int frightened_warning_limit;
    PAC_Actor centipede_history[PAC_CENTIPEDE_HISTORY];
    int cheat_step;
    int cheat_level;
    unsigned int random;
} PAC_Game;

static int pacActorX(PAC_Actor *actor);
static int pacActorY(PAC_Actor *actor);
static void pacDrawBootProgress(unsigned char *buffer, int step);
static void pacRefreshScores(unsigned char *buffer);
static void pacDrawScoresStatus(unsigned char *buffer);

static const int pac_dir_x[5] = {0,0,-1,0,1};
static const int pac_dir_y[5] = {0,-1,0,1,0};
static const int pac_trail_angle[5] = {0,128,0,384,256};
static const int pac_decimal_divisors[7] =
    {1000000,100000,10000,1000,100,10,1};
static const unsigned short pac_squares[64] =
{
    0,1,4,9,16,25,36,49,64,81,100,121,144,169,196,225,
    256,289,324,361,400,441,484,529,576,625,676,729,784,841,900,961,
    1024,1089,1156,1225,1296,1369,1444,1521,1600,1681,1764,1849,
    1936,2025,2116,2209,2304,2401,2500,2601,2704,2809,2916,3025,
    3136,3249,3364,3481,3600,3721,3844,3969
};
static int pac_top_score;
static int pac_scores[PAC_SCORE_COUNT];
static int pac_active_map;
static int pac_sound_alternate;
static int pac_siren_alternate;
static int pac_sound_enabled = 1;
static int pac_unlimited_specials;
static int pac_highest_unlocked = 1;
static int pac_selected_level = 1;
static unsigned char pac_title_colour_step[12];
static unsigned char pac_title_animate_index;
static const unsigned char pac_unlimited_code[4] = {2, 6, 0, 0};

typedef struct
{
    unsigned char x;
    unsigned char y;
    unsigned char width;
} PAC_Carve;

/* Each chapter opens a different set of deliberate cross-corridors in the
   validated base graph. Opening walls cannot disconnect the graph; the static
   validator additionally checks every resulting topology. */
static const PAC_Carve pac_map_carves[PAC_CHAPTERS][5] =
{
    {{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}},
    {{3,2,2},{19,2,2},{11,4,2},{0,0,0},{0,0,0}},
    {{5,4,3},{16,4,3},{10,12,4},{0,0,0},{0,0,0}},
    {{1,6,4},{19,6,4},{8,8,2},{14,8,2},{0,0,0}},
    {{2,16,3},{19,16,3},{9,18,6},{0,0,0},{0,0,0}},
    {{4,2,4},{16,2,4},{5,12,3},{16,12,3},{0,0,0}},
    {{2,18,3},{19,18,3},{8,20,3},{14,20,3},{0,0,0}},
    {{1,4,4},{19,4,4},{5,14,3},{16,14,3},{10,18,4}},
    {{3,6,3},{18,6,3},{2,16,4},{18,16,4},{9,20,6}},
    {{4,2,4},{16,2,4},{1,12,4},{19,12,4},{9,18,6}}
};

static const char pac_map_base[PAC_MAP_H][PAC_MAP_W + 1] =
{
    "########################",
    "#..........##..........#",
    "#o###.####.##.####.###o#",
    "#......................#",
    "#.###.##.######.##.###.#",
    "#.....##...##...##.....#",
    "#####.####.##.####.#####",
    "#####.##........##.#####",
    "#####.##.##  ##.##.#####",
    "#####.##.#    #.##.#####",
    ".........#    #.........",
    "#####.##.#    #.##.#####",
    "#####.##.######.##.#####",
    "#####.##... ....##.#####",
    "#####.##.######.##.#####",
    "#..........##..........#",
    "#.###.####.##.####.###.#",
    "#o..#..............#..o#",
    "###.#.##.######.##.#.###",
    "#.....##...##...##.....#",
    "#.########.##.########.#",
    "#......................#",
    "########################"
};

static unsigned char pac_pellets[PAC_MAP_H][PAC_MAP_W];
static int pac_fx[PAC_FX_MAX_INJECT][6];
static int pac_fx_frame_remaining;
static int pac_fx_capacity_remaining;
static const int pac_particle_pool_cap = PAC_PARTICLE_CAP_DEFAULT;
static unsigned char pac_particle_expiry[PAC_PARTICLE_EXPIRY_SLOTS];
static unsigned int pac_particle_slot_ready[PAC_PARTICLE_CAP_ALLOCATED];
static int pac_particle_expiry_cursor;
static int pac_particle_active_estimate;
static int pac_particle_write_cursor;
static unsigned int pac_particle_clock;
static int pac_palette_flash_stage = -1;
static int pac_actor_flash_stage = -1;

static unsigned short pac_palette[16] =
{
    0,
    PAC_RGB16(0, 5, 14), PAC_RGB16(0, 18, 31),
    PAC_RGB16(4, 48, 31), PAC_RGB16(31, 63, 31),
    PAC_RGB16(31, 63, 0), PAC_RGB16(31, 35, 0),
    PAC_RGB16(31, 4, 0), PAC_RGB16(31, 34, 0),
    PAC_RGB16(25, 5, 31), PAC_RGB16(14, 5, 31),
    PAC_RGB16(0, 48, 6), PAC_RGB16(10, 63, 5),
    PAC_RGB16(0, 56, 31), PAC_RGB16(0, 10, 15),
    PAC_RGB16(0, 40, 31)
};

static const unsigned short pac_pulse[8] =
{
    PAC_RGB16(0, 12, 20), PAC_RGB16(0, 30, 31),
    PAC_RGB16(5, 55, 31), PAC_RGB16(31, 63, 31),
    PAC_RGB16(31, 63, 0), PAC_RGB16(31, 30, 0),
    PAC_RGB16(31, 10, 20), PAC_RGB16(12, 10, 31)
};

static const unsigned short pac_particle_palette[16] =
{
    0,
    PAC_RGB16(0, 2, 8), PAC_RGB16(0, 10, 24),
    PAC_RGB16(0, 40, 31), PAC_RGB16(0, 63, 31),
    PAC_RGB16(8, 63, 24), PAC_RGB16(20, 63, 8),
    PAC_RGB16(31, 63, 0), PAC_RGB16(31, 34, 0),
    PAC_RGB16(31, 4, 0), PAC_RGB16(31, 6, 18),
    PAC_RGB16(25, 8, 31), PAC_RGB16(8, 18, 31),
    PAC_RGB16(0, 52, 31), PAC_RGB16(20, 63, 31),
    PAC_RGB16(31, 63, 31)
};

static const unsigned short pac_maze_glow[PAC_CHAPTERS][3] =
{
    {PAC_RGB16(0, 3, 12), PAC_RGB16(0, 25, 31), PAC_RGB16(12, 61, 31)},
    {PAC_RGB16(7, 0, 10), PAC_RGB16(25, 2, 29), PAC_RGB16(31, 24, 31)},
    {PAC_RGB16(0, 5, 3),  PAC_RGB16(0, 31, 11), PAC_RGB16(10, 63, 27)},
    {PAC_RGB16(9, 2, 0),  PAC_RGB16(28, 10, 0), PAC_RGB16(31, 51, 8)},
    {PAC_RGB16(8, 0, 13), PAC_RGB16(23, 5, 30), PAC_RGB16(31, 42, 31)},
    {PAC_RGB16(0, 7, 5),  PAC_RGB16(0, 34, 18), PAC_RGB16(18, 63, 31)},
    {PAC_RGB16(9, 6, 0),  PAC_RGB16(28, 29, 0), PAC_RGB16(31, 63, 14)},
    {PAC_RGB16(2, 2, 12), PAC_RGB16(7, 18, 31), PAC_RGB16(24, 55, 31)},
    {PAC_RGB16(11, 0, 4), PAC_RGB16(30, 2, 16), PAC_RGB16(31, 32, 31)},
    {PAC_RGB16(5, 0, 11), PAC_RGB16(18, 4, 30), PAC_RGB16(31, 58, 31)}
};

/* Prepared CLUT states make the energizer hit screen-wide and cheap: the
   maze pixels stay untouched while three wall entries change together. */
static const unsigned short pac_energizer_glow[3][3] =
{
    {PAC_RGB16(10, 20, 31), PAC_RGB16(20, 50, 31),
     PAC_RGB16(31, 63, 31)},
    {PAC_RGB16(0, 10, 20), PAC_RGB16(0, 42, 31),
     PAC_RGB16(20, 63, 31)},
    {PAC_RGB16(12, 4, 0), PAC_RGB16(31, 24, 0),
     PAC_RGB16(31, 63, 10)}
};

static const int pac_title_colours[12] =
    {5,13,9,12,7,15,8,10,4,11,6,3};

static const char *pac_chapter_names[PAC_CHAPTERS] =
{
    "CLASSIC NEON", "VECTOR WRAITH", "FRUIT FEVER", "CRAWLER SHADE",
    "GHOST ALARM", "OTTO ECHO", "PRISM PULSE", "FLIPPER PHANTOM",
    "GHOST CENTIPEDE", "PARTICLE STORM"
};

static const char *pac_stage_names[5] =
{
    "NEW CIRCUIT", "FASTER RELEASE", "HUNTER SHIFT",
    "SHORTER POWER", "OVERCLOCK"
};

static const char *pac_chapter_rules[PAC_CHAPTERS] =
{
    "RED CHASE PINK AHEAD CYAN FLANK GOLD SHY",
    "VECTOR HUNTS THE WRAP LANE",
    "FRUIT PULLS NEARBY PELLETS",
    "CRAWLER MARKS A LIVE HAZARD",
    "ALARMS WARN MODE CHANGES",
    "OTTO IGNORES POWER PELLETS",
    "PULSE CLEARS RISING PRESSURE",
    "FLIPPER FLASH WARNS ITS SURGE",
    "RED HEAD PULLS THREE LINKS",
    "STORM WARNING THEN MASS REVERSAL"
};

/* Boot-generated signed PCM. Long effects use their own buffers so the DSP
   can play them without any per-frame synthesis work. */
static unsigned long pac_snd_dot_words[192];
static unsigned long pac_snd_power_words[384];
static unsigned long pac_snd_power_active_words[256];
static unsigned long pac_snd_power_warning_words[384];
static unsigned long pac_snd_fruit_words[512];
static unsigned long pac_snd_ghost_words[512];
static unsigned long pac_snd_death_words[2048];
static unsigned long pac_snd_clear_words[1536];
static unsigned long pac_snd_ready_words[1024];
static unsigned long pac_snd_dash_words[256];
static unsigned long pac_snd_pulse_words[640];
static unsigned long pac_snd_shield_words[512];
static unsigned long pac_snd_warp_words[384];
static unsigned long pac_snd_siren_words[512];
static unsigned long pac_snd_vector_words[384];
static unsigned long pac_snd_crawler_words[576];
static unsigned long pac_snd_otto_words[384];
static unsigned long pac_snd_flipper_words[320];
static unsigned long pac_snd_centipede_words[640];
static unsigned long pac_snd_storm_words[768];
static unsigned long pac_snd_alarm_words[384];
static unsigned long pac_snd_extra_life_words[768];
static unsigned long pac_snd_game_over_words[2048];
static unsigned long pac_snd_finale_words[2048];

static unsigned short pac_actor_palettes[PAC_GHOSTS + 1][16];
static const unsigned short pac_ghost_colours[PAC_GHOSTS] =
{
    PAC_RGB16(31, 4, 0),
    PAC_RGB16(31, 20, 22),
    PAC_RGB16(0, 58, 31),
    PAC_RGB16(31, 30, 0),
    PAC_RGB16(7, 28, 31),
    PAC_RGB16(25, 7, 31),
    PAC_RGB16(31, 24, 0),
    PAC_RGB16(12, 63, 22)
};

static const unsigned short pac_ghost_highlights[PAC_GHOSTS] =
{
    PAC_RGB16(31, 34, 18),
    PAC_RGB16(31, 48, 31),
    PAC_RGB16(14, 63, 31),
    PAC_RGB16(31, 52, 12),
    PAC_RGB16(18, 55, 31),
    PAC_RGB16(31, 34, 31),
    PAC_RGB16(31, 52, 10),
    PAC_RGB16(22, 63, 31)
};

static const unsigned short pac_ghost_glows[PAC_GHOSTS] =
{
    PAC_RGB16(9, 1, 0),
    PAC_RGB16(9, 3, 6),
    PAC_RGB16(0, 14, 8),
    PAC_RGB16(10, 7, 0),
    PAC_RGB16(1, 7, 9),
    PAC_RGB16(7, 1, 9),
    PAC_RGB16(10, 5, 0),
    PAC_RGB16(2, 14, 5)
};

static const char pac_glyph_order[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const unsigned char pac_glyphs[36][7] =
{
    {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14},
    {14,17,1,2,4,8,31}, {30,1,1,14,1,1,30},
    {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8},
    {14,17,17,14,17,17,14}, {14,17,17,15,1,1,14},
    {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30},
    {14,17,16,16,16,17,14}, {30,17,17,17,17,17,30},
    {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
    {14,17,16,23,17,17,14}, {17,17,17,31,17,17,17},
    {14,4,4,4,4,4,14}, {7,2,2,2,18,18,12},
    {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17}, {17,25,25,21,19,19,17},
    {14,17,17,17,17,17,14}, {30,17,17,30,16,16,16},
    {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4},
    {17,17,17,17,17,17,14}, {17,17,17,17,17,10,4},
    {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31}
};

/* Dedicated 8x11 title masks keep the ordinary 5x7 HUD font compact. */
static const char pac_title_glyph_order[] = "PARTICLE-MN";
static const unsigned char pac_title_glyphs[11][11] =
{
    {252,130,130,130,252,128,128,128,128,128,128},
    {56,68,130,130,130,254,130,130,130,130,130},
    {252,130,130,130,252,144,136,132,130,130,130},
    {254,16,16,16,16,16,16,16,16,16,16},
    {124,16,16,16,16,16,16,16,16,16,124},
    {60,66,128,128,128,128,128,128,128,66,60},
    {128,128,128,128,128,128,128,128,128,128,254},
    {254,128,128,128,252,128,128,128,128,128,254},
    {0,0,0,0,124,124,0,0,0,0,0},
    {130,198,170,146,130,130,130,130,130,130,130},
    {130,194,162,146,138,134,130,130,130,130,130}
};

static void pacZero(void *address, int bytes)
{
    unsigned long *destination = (unsigned long *)address;
    int count = bytes >> 2;
    while (count-- > 0) *destination++ = 0;
}

/* JagStudio 1.11 declares used/free particle counters in jstudio.h, but the
   bundled RAPTOR v2.0.31 object does not export them. Keep a conservative,
   constant-time expiry wheel plus per-slot reuse timestamps. No live RAPTOR
   table scan is performed, and over-estimation can only drop decoration. */
static void pacParticleLedgerReset(void)
{
    pacZero(pac_particle_expiry, PAC_PARTICLE_EXPIRY_SLOTS);
    pacZero(pac_particle_slot_ready,
            PAC_PARTICLE_CAP_ALLOCATED * sizeof(unsigned int));
    pac_particle_expiry_cursor = 0;
    pac_particle_active_estimate = 0;
    pac_particle_write_cursor = 0;
    pac_particle_clock = 0;
}

static void pacParticleLedgerAdvance(void)
{
    int expired;
    ++pac_particle_clock;
    pac_particle_expiry_cursor =
        (pac_particle_expiry_cursor + 1) &
        (PAC_PARTICLE_EXPIRY_SLOTS - 1);
    expired = pac_particle_expiry[pac_particle_expiry_cursor];
    pac_particle_expiry[pac_particle_expiry_cursor] = 0;
    pac_particle_active_estimate -= expired;
    if (pac_particle_active_estimate < 0)
        pac_particle_active_estimate = 0;
}

static void pacParticleLedgerAdmit(int life)
{
    int slot;
    /* The extra VBL is deliberately conservative around RAPTOR's terminal
       lifetime update, preventing reuse before the engine can free a record. */
    if (life < 1) life = 1;
    if (life > PAC_PARTICLE_EXPIRY_SLOTS - 2)
        life = PAC_PARTICLE_EXPIRY_SLOTS - 2;
    slot = (pac_particle_expiry_cursor + life + 1) &
        (PAC_PARTICLE_EXPIRY_SLOTS - 1);
    ++pac_particle_expiry[slot];
    ++pac_particle_active_estimate;
}

static int pacParticleLedgerFree(void)
{
    int free_particles = pac_particle_pool_cap -
        pac_particle_active_estimate;
    if (free_particles < 0) free_particles = 0;
    return free_particles;
}

static int pacParticleAcquireSlot(void)
{
    int tries;
    int slot;
    for (tries = 0; tries < pac_particle_pool_cap; ++tries)
    {
        slot = pac_particle_write_cursor++;
        if (pac_particle_write_cursor >= pac_particle_pool_cap)
            pac_particle_write_cursor = 0;
        if (pac_particle_slot_ready[slot] == 0U ||
            (int)(pac_particle_clock - pac_particle_slot_ready[slot]) >= 0)
            return slot;
    }
    return -1;
}

static void pacParticleSeedSlot(int slot, int x, int y,
                                int *definition)
{
    volatile PAC_RaptorParticle *table =
        (volatile PAC_RaptorParticle *)(void *)&PAC_particle_table;
    volatile PAC_RaptorParticle *particle = &table[slot];
    int life = definition[5];
    if (life < 1) life = 1;
    if (life > PAC_PARTICLE_EXPIRY_SLOTS - 2)
        life = PAC_PARTICLE_EXPIRY_SLOTS - 2;

    /* Active is the publication flag and is written last. RAPTOR's VBI GPU
       updater sees either the old inactive record or the complete new one. */
    particle->active = 0;
    particle->x = (int)PAC_SCREEN_FIXED(x);
    particle->y = (int)PAC_SCREEN_FIXED(y);
    particle->angle = definition[0] & 511;
    particle->speed = definition[1];
    particle->angular_speed = definition[2];
    particle->colour = definition[3] & 15;
    particle->colour_decay = definition[4];
    particle->current_decay = 0;
    particle->life = life;
    particle->address = 0;
    pac_particle_slot_ready[slot] = pac_particle_clock +
        (unsigned int)life + 1U;
    particle->active = 1;
    pacParticleLedgerAdmit(life);
}

enum
{
    PAC_SND_DOT = 0,
    PAC_SND_POWER,
    PAC_SND_POWER_ACTIVE,
    PAC_SND_POWER_WARNING,
    PAC_SND_FRUIT,
    PAC_SND_GHOST,
    PAC_SND_DEATH,
    PAC_SND_CLEAR,
    PAC_SND_READY,
    PAC_SND_DASH,
    PAC_SND_PULSE,
    PAC_SND_SHIELD,
    PAC_SND_SIREN,
    PAC_SND_WARP,
    PAC_SND_VECTOR,
    PAC_SND_CRAWLER,
    PAC_SND_OTTO,
    PAC_SND_FLIPPER,
    PAC_SND_CENTIPEDE,
    PAC_SND_STORM,
    PAC_SND_COMBO,
    PAC_SND_ALARM,
    PAC_SND_EXTRA_LIFE,
    PAC_SND_GAME_OVER,
    PAC_SND_FINALE
};

static void pacSynth(unsigned long *words, int samples, int period_start,
                     int period_end, int bass_period, int noise_amount)
{
    unsigned char *output = (unsigned char *)words;
    unsigned int noise = 0x1d872b41U;
    unsigned long tone_phase = 0;
    unsigned long bass_phase = 0;
    long step = ((long)(65536 / period_start)) << 16;
    long step_end = ((long)(65536 / period_end)) << 16;
    long step_delta = (step_end - step) / samples;
    unsigned long bass_step = ((unsigned long)(65536 / bass_period)) << 16;
    long envelope = 255L << 16;
    long envelope_step = envelope / samples;
    int index;
    int triangle;
    int bass;
    int grit;
    int wave;
    int sample;
    for (index = 0; index < samples; ++index)
    {
        tone_phase += (unsigned long)step;
        bass_phase += bass_step;
        wave = (int)((tone_phase >> 24) & 255U);
        if (wave > 127) wave = 255 - wave;
        triangle = wave - 64;
        bass = (bass_phase & 0x80000000UL) ? 28 : -28;
        noise = noise * 1664525U + 1013904223U;
        grit = (((int)((noise >> 24) & 255U) - 128) *
                noise_amount) >> 7;
        sample = ((triangle + bass + grit) *
                  (int)(envelope >> 16)) >> 8;
        /* Use the available signed eight-bit range without clipping the
           oscillator after the four-channel DSP mix. */
        sample = (sample * 3) >> 1;
        if (sample > 127) sample = 127;
        if (sample < -127) sample = -127;
        output[index] = (unsigned char)(sample & 255);
        step += step_delta;
        envelope -= envelope_step;
    }
}

static void pacGenerateSounds(unsigned char *boot_canvas)
{
    pacSynth(pac_snd_dot_words, 768, 20, 11, 43, 4);
    pacDrawBootProgress(boot_canvas, 1);
    jsfVsync(0);
    pacSynth(pac_snd_power_words, 1536, 42, 14, 73, 18);
    pacDrawBootProgress(boot_canvas, 2);
    jsfVsync(0);
    pacSynth(pac_snd_power_active_words, 1024, 34, 27, 59, 2);
    pacDrawBootProgress(boot_canvas, 3);
    jsfVsync(0);
    pacSynth(pac_snd_power_warning_words, 1536, 18, 46, 83, 20);
    pacDrawBootProgress(boot_canvas, 4);
    jsfVsync(0);
    pacSynth(pac_snd_fruit_words, 2048, 30, 7, 51, 7);
    pacDrawBootProgress(boot_canvas, 5);
    jsfVsync(0);
    pacSynth(pac_snd_ghost_words, 2048, 9, 35, 47, 15);
    pacDrawBootProgress(boot_canvas, 6);
    jsfVsync(0);
    pacSynth(pac_snd_death_words, 8192, 8, 68, 81, 12);
    pacDrawBootProgress(boot_canvas, 7);
    jsfVsync(0);
    pacSynth(pac_snd_clear_words, 6144, 32, 5, 57, 5);
    pacDrawBootProgress(boot_canvas, 8);
    jsfVsync(0);
    pacSynth(pac_snd_ready_words, 4096, 24, 7, 61, 6);
    pacDrawBootProgress(boot_canvas, 9);
    jsfVsync(0);
    /* Abilities and specialist warnings have separate generated voices. The
       buffers remain boot-built signed PCM, but no longer rely on pitch-shifted
       copies of one generic action or siren sample. */
    pacSynth(pac_snd_dash_words, 1024, 24, 3, 31, 5);
    pacDrawBootProgress(boot_canvas, 10);
    jsfVsync(0);
    pacSynth(pac_snd_pulse_words, 2560, 58, 9, 43, 3);
    pacDrawBootProgress(boot_canvas, 11);
    jsfVsync(0);
    pacSynth(pac_snd_shield_words, 2048, 13, 38, 91, 20);
    pacDrawBootProgress(boot_canvas, 12);
    jsfVsync(0);
    pacSynth(pac_snd_warp_words, 1536, 6, 52, 71, 12);
    pacDrawBootProgress(boot_canvas, 13);
    jsfVsync(0);
    pacSynth(pac_snd_siren_words, 2048, 52, 39, 83, 2);
    pacDrawBootProgress(boot_canvas, 14);
    jsfVsync(0);
    pacSynth(pac_snd_vector_words, 1536, 40, 5, 29, 3);
    pacDrawBootProgress(boot_canvas, 15);
    jsfVsync(0);
    pacSynth(pac_snd_crawler_words, 2304, 72, 25, 101, 30);
    pacDrawBootProgress(boot_canvas, 16);
    jsfVsync(0);
    pacSynth(pac_snd_otto_words, 1536, 11, 14, 23, 34);
    pacDrawBootProgress(boot_canvas, 17);
    jsfVsync(0);
    pacSynth(pac_snd_flipper_words, 1280, 7, 31, 47, 10);
    pacDrawBootProgress(boot_canvas, 18);
    jsfVsync(0);
    pacSynth(pac_snd_centipede_words, 2560, 18, 67, 37, 8);
    pacDrawBootProgress(boot_canvas, 19);
    jsfVsync(0);
    pacSynth(pac_snd_storm_words, 3072, 85, 8, 109, 42);
    pacDrawBootProgress(boot_canvas, 20);
    jsfVsync(0);
    pacSynth(pac_snd_alarm_words, 1536, 9, 9, 67, 2);
    pacDrawBootProgress(boot_canvas, 21);
    jsfVsync(0);
    pacSynth(pac_snd_extra_life_words, 3072, 36, 4, 41, 3);
    pacDrawBootProgress(boot_canvas, 22);
    jsfVsync(0);
    pacSynth(pac_snd_game_over_words, 8192, 10, 92, 113, 30);
    pacDrawBootProgress(boot_canvas, 23);
    jsfVsync(0);
    pacSynth(pac_snd_finale_words, 8192, 48, 3, 35, 4);
    pacDrawBootProgress(boot_canvas, 24);
    jsfVsync(0);
}

static void pacSilence(void)
{
    int channel;
    for (channel = 1; channel <= PAC_SOUND_CHANNELS; ++channel)
        zeroPlaySample(channel, (void *)0, 0, PAC_SOUND_DIVIDER, 0);
}

static void pacPlaySound(PAC_Game *game, int sound)
{
    void *address = (void *)pac_snd_dash_words;
    int length = 1024;
    int channel = 2;
    int mirror_channel = 0;
    int divider = PAC_SOUND_DIVIDER;
    if (!game->sound_enabled) return;
    if (sound == PAC_SND_DOT)
    {
        address = (void *)pac_snd_dot_words;
        length = 768;
        channel = 1;
        divider += pac_sound_alternate;
        pac_sound_alternate ^= 1;
    }
    else if (sound == PAC_SND_POWER)
    {
        address = (void *)pac_snd_power_words;
        length = 1536;
        mirror_channel = 3;
    }
    else if (sound == PAC_SND_POWER_ACTIVE)
    {
        address = (void *)pac_snd_power_active_words;
        length = 1024;
        channel = 2;
    }
    else if (sound == PAC_SND_POWER_WARNING)
    {
        address = (void *)pac_snd_power_warning_words;
        length = 1536;
        channel = 2;
        mirror_channel = 3;
    }
    else if (sound == PAC_SND_FRUIT)
    {
        address = (void *)pac_snd_fruit_words;
        length = 2048;
        mirror_channel = 3;
    }
    else if (sound == PAC_SND_GHOST)
    {
        address = (void *)pac_snd_ghost_words;
        length = 2048;
        channel = 3;
        mirror_channel = 2;
    }
    else if (sound == PAC_SND_DEATH)
    {
        address = (void *)pac_snd_death_words;
        length = 8192;
        channel = 4;
        mirror_channel = 3;
    }
    else if (sound == PAC_SND_CLEAR)
    {
        address = (void *)pac_snd_clear_words;
        length = 6144;
        channel = 4;
        mirror_channel = 3;
    }
    else if (sound == PAC_SND_READY)
    {
        address = (void *)pac_snd_ready_words;
        length = 4096;
        channel = 4;
        mirror_channel = 3;
    }
    else if (sound == PAC_SND_DASH)
    {
        address = (void *)pac_snd_dash_words;
        length = 1024;
        channel = 2;
    }
    else if (sound == PAC_SND_PULSE)
    {
        address = (void *)pac_snd_pulse_words;
        length = 2560;
        channel = 2;
    }
    else if (sound == PAC_SND_SHIELD)
    {
        address = (void *)pac_snd_shield_words;
        length = 2048;
        channel = 2;
    }
    else if (sound == PAC_SND_SIREN)
    {
        address = (void *)pac_snd_siren_words;
        length = 2048;
        channel = 2;
        /* Two low pitches make the sparse no-music soundscape breathe like
           an arcade danger circuit instead of repeating one flat sample. */
        divider = PAC_SOUND_DIVIDER + 1 + pac_siren_alternate;
        pac_siren_alternate ^= 1;
    }
    else if (sound == PAC_SND_WARP)
    {
        address = (void *)pac_snd_warp_words;
        length = 1536;
        channel = 3;
    }
    else if (sound == PAC_SND_VECTOR)
    {
        address = (void *)pac_snd_vector_words;
        length = 1536;
        channel = 3;
    }
    else if (sound == PAC_SND_CRAWLER)
    {
        address = (void *)pac_snd_crawler_words;
        length = 2304;
        channel = 3;
    }
    else if (sound == PAC_SND_OTTO)
    {
        address = (void *)pac_snd_otto_words;
        length = 1536;
        channel = 3;
    }
    else if (sound == PAC_SND_FLIPPER)
    {
        address = (void *)pac_snd_flipper_words;
        length = 1280;
        channel = 3;
    }
    else if (sound == PAC_SND_CENTIPEDE)
    {
        address = (void *)pac_snd_centipede_words;
        length = 2560;
        channel = 3;
    }
    else if (sound == PAC_SND_STORM)
    {
        address = (void *)pac_snd_storm_words;
        length = 3072;
        channel = 3;
    }
    else if (sound == PAC_SND_COMBO)
    {
        address = (void *)pac_snd_dot_words;
        length = 768;
        divider = PAC_SOUND_DIVIDER - 1;
        channel = 1;
    }
    else if (sound == PAC_SND_ALARM)
    {
        address = (void *)pac_snd_alarm_words;
        length = 1536;
        channel = 3;
    }
    else if (sound == PAC_SND_EXTRA_LIFE)
    {
        /* A short rising voice on the otherwise event-only fourth channel;
           it can layer with the scoring sound without creating a queue. */
        address = (void *)pac_snd_extra_life_words;
        length = 3072;
        channel = 4;
    }
    else if (sound == PAC_SND_GAME_OVER)
    {
        address = (void *)pac_snd_game_over_words;
        length = 8192;
        channel = 4;
        mirror_channel = 3;
    }
    else if (sound == PAC_SND_FINALE)
    {
        address = (void *)pac_snd_finale_words;
        length = 8192;
        channel = 4;
        mirror_channel = 3;
    }
    zeroPlaySample(channel, address, length, divider,
                   Zero_Audio_8bit_Signed);
    if (mirror_channel != 0)
        zeroPlaySample(mirror_channel, address, length, divider,
                       Zero_Audio_8bit_Signed);
}

static void pacLoadScores(void)
{
    int index;
    int valid = 1;
    for (index = 0; index < PAC_SCORE_COUNT; ++index) pac_scores[index] = 0;
    jsfEEPROMUserDataRead();
    if ((unsigned int)rapUserSaveData[0] != PAC_EEPROM_MAGIC) valid = 0;
    for (index = 0; index < PAC_SCORE_COUNT; ++index)
        if (rapUserSaveData[index + 1] < 0 ||
            rapUserSaveData[index + 1] > 9999999) valid = 0;
    for (index = 1; index < PAC_SCORE_COUNT; ++index)
        if (rapUserSaveData[index + 1] > rapUserSaveData[index]) valid = 0;
    if (valid)
    {
        for (index = 0; index < PAC_SCORE_COUNT; ++index)
            pac_scores[index] = rapUserSaveData[index + 1];
        if ((unsigned int)rapUserSaveData[7] == PAC_EEPROM_PROGRESS_MAGIC &&
            rapUserSaveData[6] >= 1 &&
            rapUserSaveData[6] <= PAC_LEVEL_MAX)
            pac_highest_unlocked = rapUserSaveData[6];
        else
            pac_highest_unlocked = 1;
    }
    else
    {
        for (index = 0; index < 128; ++index) rapUserSaveData[index] = 0;
        pac_highest_unlocked = 1;
    }
    pac_selected_level = pac_highest_unlocked;
    pac_top_score = pac_scores[0];
}

static void pacSaveScores(void)
{
    int index;
    rapUserSaveData[0] = (int)PAC_EEPROM_MAGIC;
    for (index = 0; index < PAC_SCORE_COUNT; ++index)
        rapUserSaveData[index + 1] = pac_scores[index];
    rapUserSaveData[6] = pac_highest_unlocked;
    rapUserSaveData[7] = (int)PAC_EEPROM_PROGRESS_MAGIC;
    jsfEEPROMUserDataWrite();
}

static void pacSubmitScore(PAC_Game *game)
{
    int index;
    int insert = PAC_SCORE_COUNT;
    int score;
    if (game->score_submitted) return;
    game->score_submitted = 1;
    if (!game->score_eligible || game->cheat_used) return;
    score = game->score;
    if (score <= 0) return;
    for (index = 0; index < PAC_SCORE_COUNT; ++index)
        if (score > pac_scores[index])
        {
            insert = index;
            break;
        }
    if (insert >= PAC_SCORE_COUNT) return;
    for (index = PAC_SCORE_COUNT - 1; index > insert; --index)
        pac_scores[index] = pac_scores[index - 1];
    pac_scores[insert] = score;
    pac_top_score = pac_scores[0];
    pacSaveScores();
    /* The Hall is a prebuilt attract page. Only its five numeric rows change
       after a qualifying run; refresh them while gameplay is leaving. */
    pacRefreshScores((unsigned char *)&PAC_scores_gfx);
}

static void pacRecordLevelClear(PAC_Game *game)
{
    int unlocked;
    if (game->cheat_used) return;
    unlocked = game->level < PAC_LEVEL_MAX ? game->level + 1 : PAC_LEVEL_MAX;
    if (unlocked <= pac_highest_unlocked) return;
    pac_highest_unlocked = unlocked;
    pac_selected_level = unlocked;
    pacSaveScores();
}

static int pacAbs(int value)
{
    return value < 0 ? -value : value;
}

static int pacDistanceSquared(int dx, int dy)
{
    dx = pacAbs(dx);
    dy = pacAbs(dy);
    if (dx >= 64 || dy >= 64) return 0x7fff;
    return (int)pac_squares[dx] + (int)pac_squares[dy];
}

static void pacPixel(unsigned char *buffer, int x, int y, int colour)
{
    unsigned char *pixel;
    if ((unsigned int)x >= PAC_SCREEN_W ||
        (unsigned int)y >= PAC_SCREEN_H) return;
    pixel = buffer + y * PAC_CLUT4_PITCH + (x >> 1);
    if (x & 1)
        *pixel = (unsigned char)((*pixel & 0xf0) | (colour & 15));
    else
        *pixel = (unsigned char)((*pixel & 0x0f) | ((colour & 15) << 4));
}

static void pacFill(unsigned char *buffer, int x, int y,
                    int width, int height, int colour)
{
    int px;
    int py;
    if (width <= 0 || height <= 0) return;
    for (py = y; py < y + height; ++py)
        for (px = x; px < x + width; ++px)
            pacPixel(buffer, px, py, colour);
}

static int pacGlyphIndex(char value)
{
    int index = 0;
    while (pac_glyph_order[index] != 0)
    {
        if (pac_glyph_order[index] == value) return index;
        ++index;
    }
    return -1;
}

static int pacTextWidth(const char *text, int scale)
{
    int count = 0;
    while (*text++ != 0) ++count;
    return count == 0 ? 0 : count * 6 * scale - scale;
}

static void pacText(unsigned char *buffer, int x, int y,
                    const char *text, int scale, int colour)
{
    int glyph;
    int row;
    int column;
    int sx;
    int sy;
    while (*text != 0)
    {
        glyph = pacGlyphIndex(*text);
        if (glyph >= 0)
        {
            for (row = 0; row < 7; ++row)
                for (column = 0; column < 5; ++column)
                    if (pac_glyphs[glyph][row] & (16 >> column))
                        for (sy = 0; sy < scale; ++sy)
                            for (sx = 0; sx < scale; ++sx)
                                pacPixel(buffer, x + column * scale + sx,
                                         y + row * scale + sy, colour);
        }
        else if (*text == '-')
        {
            for (column = 0; column < 5 * scale; ++column)
                for (sy = 0; sy < scale; ++sy)
                    pacPixel(buffer, x + column, y + 3 * scale + sy, colour);
        }
        else if (*text == '+')
        {
            pacFill(buffer, x + 2 * scale, y + scale,
                    scale, 5 * scale, colour);
            pacFill(buffer, x, y + 3 * scale,
                    5 * scale, scale, colour);
        }
        else if (*text == ':')
        {
            pacFill(buffer, x + 2 * scale, y + 2 * scale,
                    scale, scale, colour);
            pacFill(buffer, x + 2 * scale, y + 5 * scale,
                    scale, scale, colour);
        }
        else if (*text == '.')
        {
            pacFill(buffer, x + 2 * scale, y + 6 * scale,
                    scale, scale, colour);
        }
        else if (*text == '(' || *text == ')')
        {
            int side = *text == '(' ? 2 : 3;
            pacFill(buffer, x + side * scale, y + scale,
                    scale, 5 * scale, colour);
            pacFill(buffer, x + (side + (*text == '(' ? 1 : -1)) * scale,
                    y, scale, scale, colour);
            pacFill(buffer, x + (side + (*text == '(' ? 1 : -1)) * scale,
                    y + 6 * scale, scale, scale, colour);
        }
        x += 6 * scale;
        ++text;
    }
}

static void pacCenteredText(unsigned char *buffer, int y,
                            const char *text, int scale, int colour)
{
    pacText(buffer, (PAC_SCREEN_W - pacTextWidth(text, scale)) >> 1,
            y, text, scale, colour);
}

static void pacFormatNumber(int value, int digits, char *text)
{
    int divisor_index;
    int digit;
    int index;
    if (value < 0) value = 0;
    if (value > 9999999) value = 9999999;
    if (digits < 1) digits = 1;
    if (digits > 7) digits = 7;
    divisor_index = 7 - digits;
    for (index = 0; index < digits; ++index)
    {
        digit = 0;
        while (value >= pac_decimal_divisors[divisor_index + index])
        {
            value -= pac_decimal_divisors[divisor_index + index];
            ++digit;
        }
        text[index] = (char)('0' + digit);
    }
    text[digits] = 0;
}

static void pacNumber(unsigned char *buffer, int x, int y,
                      int value, int digits, int colour)
{
    char text[8];
    pacFormatNumber(value, digits, text);
    pacFill(buffer, x, y, digits * 6, 7, 0);
    pacText(buffer, x, y, text, 1, colour);
}

static void pacNumberChanged(unsigned char *buffer, int x, int y,
                             int value, int old_value, int digits,
                             int colour)
{
    char text[8];
    char old_text[8];
    char glyph[2];
    int index;
    pacFormatNumber(value, digits, text);
    if (old_value < 0)
    {
        pacFill(buffer, x, y, digits * 6, 7, 0);
        pacText(buffer, x, y, text, 1, colour);
        return;
    }
    pacFormatNumber(old_value, digits, old_text);
    glyph[1] = 0;
    for (index = 0; index < digits; ++index)
    {
        if (text[index] == old_text[index]) continue;
        glyph[0] = text[index];
        pacFill(buffer, x + index * 6, y, 6, 7, 0);
        pacText(buffer, x + index * 6, y, glyph, 1, colour);
    }
}

static void pacBeginFxFrame(void)
{
    int free_particles;
    int soft_free;
    pacParticleLedgerAdvance();
    pac_fx_frame_remaining = PAC_FX_FRAME_BUDGET;
    free_particles = pacParticleLedgerFree();
    /* Keep cosmetic overlap below the cadence-safe live-particle ceiling while
       preserving the 64-record physical safety cap. */
    soft_free = PAC_PARTICLE_SOFT_LIMIT - pac_particle_active_estimate;
    if (soft_free < 0) soft_free = 0;
    if (free_particles > soft_free) free_particles = soft_free;
    pac_fx_capacity_remaining = free_particles;
}

static void pacInitParticlePool(void)
{
    /* RAPTOR_particle_init is a startup routine. At this post-VBL point the
       GPU update is complete, so clear the fixed database and bitmap directly
       and publish the hardware-approved shipping scan bound once. */
    pacZero((void *)&PAC_particle_table,
            PAC_PARTICLE_CAP_ALLOCATED * PAC_PARTICLE_RECORD_BYTES);
    rapParticleClear();
    raptor_maxparts = PAC_PARTICLE_CAP_DEFAULT;
    pacParticleLedgerReset();
}

static void pacTubeBox(unsigned char *buffer, int x, int y,
                       int width, int height)
{
    pacFill(buffer, x, y, width, 3, 2);
    pacFill(buffer, x, y + height - 3, width, 3, 2);
    pacFill(buffer, x, y, 3, height, 2);
    pacFill(buffer, x + width - 3, y, 3, height, 2);
    pacFill(buffer, x + 1, y + 1, width - 2, 1, 3);
    pacFill(buffer, x + 1, y + height - 2, width - 2, 1, 3);
    pacFill(buffer, x + 1, y + 1, 1, height - 2, 3);
    pacFill(buffer, x + width - 2, y + 1, 1, height - 2, 3);
}

static int pacTitlePacInside(int dx, int dy)
{
    if (dx * dx + dy * dy > 30 * 30) return 0;
    if (dx > -1 && pacAbs(dy) * 4 < dx * 3) return 0;
    return 1;
}

static void pacTitleHero(unsigned char *buffer, int center_x, int center_y)
{
    int dx;
    int dy;
    int inside;
    int edge;
    int near_edge;
    for (dy = -33; dy <= 33; ++dy)
    {
        for (dx = -33; dx <= 33; ++dx)
        {
            inside = pacTitlePacInside(dx, dy);
            if (!inside) continue;
            edge = !pacTitlePacInside(dx - 1, dy) ||
                   !pacTitlePacInside(dx + 1, dy) ||
                   !pacTitlePacInside(dx, dy - 1) ||
                   !pacTitlePacInside(dx, dy + 1);
            near_edge = edge || !pacTitlePacInside(dx - 3, dy) ||
                        !pacTitlePacInside(dx + 3, dy) ||
                        !pacTitlePacInside(dx, dy - 3) ||
                        !pacTitlePacInside(dx, dy + 3);
            if (edge)
                pacPixel(buffer, center_x + dx, center_y + dy, 15);
            else if (near_edge)
                pacPixel(buffer, center_x + dx, center_y + dy, 5);
        }
    }
}

static void pacBlitCreditRle(unsigned char *buffer, int x, int y,
                             int width, int height,
                             const unsigned char *rle, int rle_bytes)
{
    int source = 0;
    int pixel = 0;
    int run;
    int colour;
    int count;
    while (source + 1 < rle_bytes && pixel < width * height)
    {
        run = rle[source++];
        colour = rle[source++];
        for (count = 0; count < run && pixel < width * height; ++count)
        {
            if (colour != 0)
                pacPixel(buffer, x + pixel % width, y + pixel / width,
                         colour);
            ++pixel;
        }
    }
}

static void pacDrawPresentationGridClipped(unsigned char *buffer,
                                           int covered_left,
                                           int covered_right)
{
    int x;
    int y;
    int px;
    int py;
    pacTubeBox(buffer, 4, 4, PAC_SCREEN_W - 8, PAC_SCREEN_H - 8);
    for (x = 14; x < PAC_SCREEN_W - 14; x += 18)
    {
        pacPixel(buffer, x, 12, 3);
        pacPixel(buffer, PAC_SCREEN_W - 1 - x, PAC_SCREEN_H - 14, 10);
    }
    for (y = 16; y < PAC_SCREEN_H - 16; y += 14)
    {
        pacPixel(buffer, 10, y, 15);
        pacPixel(buffer, PAC_SCREEN_W - 11, y + 4, 9);
    }
    /* Actual level-one topology, kept faint so it reads as an attract-mode
       frightened-maze decoration behind the credit/title/score panels. */
    for (y = 0; y < PAC_MAP_H; ++y)
    {
        for (x = 0; x < PAC_MAP_W; ++x)
        {
            px = PAC_MAP_LEFT + x * PAC_TILE_X;
            py = PAC_MAP_TOP + y * PAC_TILE_Y;
            /* Attract panels are opaque. Do not spend startup time drawing
               maze detail that the following panel immediately erases. */
            if (covered_left < covered_right && px >= covered_left &&
                px + PAC_TILE_X <= covered_right)
                continue;
            if (pac_map_base[y][x] == '#')
            {
                int open_left = x > 0 && pac_map_base[y][x - 1] != '#';
                int open_right = x + 1 < PAC_MAP_W &&
                    pac_map_base[y][x + 1] != '#';
                int open_up = y > 0 && pac_map_base[y - 1][x] != '#';
                int open_down = y + 1 < PAC_MAP_H &&
                    pac_map_base[y + 1][x] != '#';
                if (open_left)
                    pacFill(buffer, px, py + 1, 1, PAC_TILE_Y - 2, 1);
                if (open_right)
                    pacFill(buffer, px + PAC_TILE_X - 1, py + 1,
                            1, PAC_TILE_Y - 2, 1);
                if (open_up)
                    pacFill(buffer, px + 1, py, PAC_TILE_X - 2, 1, 1);
                if (open_down)
                    pacFill(buffer, px + 1, py + PAC_TILE_Y - 1,
                            PAC_TILE_X - 2, 1, 1);
                if (((x * 3 + y * 5) & 15) == 0 &&
                    (open_left || open_right || open_up || open_down))
                    pacPixel(buffer, px + PAC_TILE_CENTER_X,
                             py + PAC_TILE_CENTER_Y, 2);
            }
            else if (pac_map_base[y][x] == 'o')
            {
                pacPixel(buffer, px + PAC_TILE_CENTER_X,
                         py + PAC_TILE_CENTER_Y, 15);
            }
        }
    }
}

static void pacDrawPresentationGrid(unsigned char *buffer)
{
    pacDrawPresentationGridClipped(buffer, 0, 0);
}

static int pacTitleGlyphIndex(char value)
{
    int index = 0;
    while (pac_title_glyph_order[index] != 0)
    {
        if (pac_title_glyph_order[index] == value) return index;
        ++index;
    }
    return -1;
}

static void pacDrawTitleGlyph(unsigned char *buffer, int x, int y,
                              char value, int colour)
{
    int glyph = pacTitleGlyphIndex(value);
    int row;
    int column;
    if (glyph < 0) return;
    for (row = 0; row < 11; ++row)
        for (column = 0; column < 8; ++column)
            if (pac_title_glyphs[glyph][row] & (128 >> column))
                pacFill(buffer, x + column * 2 - 1, y + row * 2 - 1,
                        4, 4, 1);
    for (row = 0; row < 11; ++row)
        for (column = 0; column < 8; ++column)
            if (pac_title_glyphs[glyph][row] & (128 >> column))
            {
                pacFill(buffer, x + column * 2, y + row * 2,
                        2, 2, colour);
                if (((row * 3 + column * 5) & 7) == 0)
                    pacPixel(buffer, x + column * 2, y + row * 2, 15);
            }
}

static void pacDrawColourTitle(unsigned char *buffer, int y,
                               unsigned int frame)
{
    static const char title[] = "PARTICLE-MAN";
    int index;
    int colour_step;
    int x = (PAC_SCREEN_W - 236) >> 1;
    pacFill(buffer, x - 6, y - 3, 248, 28, 0);
    for (index = 0; title[index] != 0; ++index)
    {
        colour_step = (index * 5 + (int)(frame >> 4)) % 12;
        pac_title_colour_step[index] = (unsigned char)colour_step;
        pacDrawTitleGlyph(buffer, x + index * 20, y, title[index],
            pac_title_colours[colour_step]);
    }
}

static void pacAnimateColourTitle(unsigned char *buffer, int y,
                                  unsigned int frame)
{
    static const char title[] = "PARTICLE-MAN";
    int index;
    int colour_step;
    int x = (PAC_SCREEN_W - 236) >> 1;
    if ((frame & 3U) != 0U) return;
    index = pac_title_animate_index;
    ++pac_title_animate_index;
    if (pac_title_animate_index >= 12) pac_title_animate_index = 0;
    colour_step = (int)pac_title_colour_step[index] +
        ((index & 1) ? 5 : 7);
    if (colour_step >= 12) colour_step -= 12;
    pac_title_colour_step[index] = (unsigned char)colour_step;
    pacDrawTitleGlyph(buffer, x + index * 20, y, title[index],
        pac_title_colours[colour_step]);
}

static void pacBuildBoot(unsigned char *buffer)
{
    pacZero(buffer, PAC_CLUT4_BYTES);
    pacTubeBox(buffer, 28, 50, PAC_SCREEN_W - 56, 112);
    pacDrawColourTitle(buffer, 70, 0);
    pacCenteredText(buffer, 109, "POWERING PARTICLE-MAN", 1, 13);
    pacCenteredText(buffer, 131, "TOLBAT GAMES", 1, 8);
    pacDrawBootProgress(buffer, 0);
}

static void pacDrawBootProgress(unsigned char *buffer, int step)
{
    int width;
    if (step < 0) step = 0;
    if (step > 31) step = 31;
    width = (step * 160) / 31;
    pacFill(buffer, 80, 147, 160, 2, 1);
    if (width > 0)
    {
        pacFill(buffer, 80, 147, width, 2, step < 15 ? 3 : 5);
        pacPixel(buffer, 79 + width, 147, 15);
        pacPixel(buffer, 79 + width, 148, 15);
    }
}

static void pacBuildCredits(unsigned char *buffer)
{
    pacZero(buffer, PAC_CLUT4_BYTES);
    pacDrawPresentationGridClipped(buffer, 20, 300);
    /* Keep the publication hierarchy quiet, centered and television-readable. */
    pacFill(buffer, 28, 8, 264, 198, 0);
    pacTubeBox(buffer, 28, 8, 264, 198);
    pacCenteredText(buffer, 13, "PUBLISHED BY", 1, 5);
    pacBlitCreditRle(buffer, 72, 25,
        PAC_CREDIT_TOLBAT_W, PAC_CREDIT_TOLBAT_H,
        pac_credit_tolbat_rle, PAC_CREDIT_TOLBAT_RLE_BYTES);
    pacCenteredText(buffer, 117, "MADE BY CHATGPT 5.6 SOL", 1, 13);
    pacFill(buffer, 46, 130, 228, 1, 1);
    pacCenteredText(buffer, 135, "MADE WITH", 1, 4);
    pacBlitCreditRle(buffer, 42, 151,
        PAC_CREDIT_JAGSTUDIO_W, PAC_CREDIT_JAGSTUDIO_H,
        pac_credit_jagstudio_rle, PAC_CREDIT_JAGSTUDIO_RLE_BYTES);
    pacBlitCreditRle(buffer, 242, 140,
        PAC_CREDIT_RAPTOR_W, PAC_CREDIT_RAPTOR_H,
        pac_credit_raptor_rle, PAC_CREDIT_RAPTOR_RLE_BYTES);
}

static void pacDrawStartLevel(unsigned char *buffer)
{
    pacFill(buffer, 52, 184, 216, 9, 0);
    pacText(buffer, 61, 185, "START", 1, 12);
    pacNumber(buffer, 97, 185, pac_selected_level, 3, 5);
    pacText(buffer, 127, 185, pac_selected_level == 1 ?
            "FULL SCORE RUN" : "CONTINUE NO HIGH SCORE", 1,
            pac_selected_level == 1 ? 13 : 8);
}

static void pacDrawTitleFooter(unsigned char *buffer)
{
    pacFill(buffer, 10, 199, 300, 14, 0);
    pacCenteredText(buffer, 202,
        "C B A PAUSE START  OPTION PAUSE  8 SOUND", 1, 5);
}

static void pacBuildTitle(unsigned char *buffer, unsigned int frame)
{
    pacZero(buffer, PAC_CLUT4_BYTES);
    pacDrawPresentationGridClipped(buffer, 6, 314);
    pacFill(buffer, 6, 8, 308, 207, 0);
    pacTubeBox(buffer, 6, 8, 308, 207);
    pacCenteredText(buffer, 14, "JAGUAR PARTICLE ARCADE", 1, 13);
    pacDrawColourTitle(buffer, 27, frame);
    pacTitleHero(buffer, PAC_SCREEN_W >> 1, 86);
    pacCenteredText(buffer, 124, "C SHIELD   B PULSE   A DASH", 1, 15);
    pacCenteredText(buffer, 145, "POWER PELLET REFILLS SHIELD DASH", 1, 4);
    pacCenteredText(buffer, 160, "FRUIT ADDS ONE PULSE", 1, 5);
    pacCenteredText(buffer, 174, "LEFT RIGHT CHOOSE START LEVEL", 1, 12);
    pacDrawStartLevel(buffer);
    pacDrawTitleFooter(buffer);
}

static void pacBuildScores(unsigned char *buffer)
{
    pacZero(buffer, PAC_CLUT4_BYTES);
    pacDrawPresentationGridClipped(buffer, 27, 293);
    pacFill(buffer, 27, 8, 266, 207, 0);
    pacTubeBox(buffer, 27, 8, 266, 207);
    pacCenteredText(buffer, 14, "HALL OF SPARKS", 2, 5);
    pacCenteredText(buffer, 39, "RANK   SCORE", 1, 13);
    pacRefreshScores(buffer);
    pacDrawScoresStatus(buffer);
    pacCenteredText(buffer, 179, "TOP SCORES SAVED IN CARTRIDGE", 1, 12);
    pacCenteredText(buffer, 196, "C B A OR PAUSE TO TITLE", 1, 4);
}

static void pacRefreshScores(unsigned char *buffer)
{
    int index;
    for (index = 0; index < PAC_SCORE_COUNT; ++index)
    {
        pacFill(buffer, 75, 53 + index * 23, 170, 14, 0);
        pacFill(buffer, 75, 53 + index * 23, 170, 1, 1);
        pacNumber(buffer, 92, 58 + index * 23, index + 1, 1,
                  pac_title_colours[index * 2]);
        pacNumber(buffer, 151, 58 + index * 23, pac_scores[index], 7,
                  pac_title_colours[index * 2 + 1]);
    }
}

static void pacDrawScoresStatus(unsigned char *buffer)
{
    pacFill(buffer, 54, 164, 212, 11, 0);
    pacCenteredText(buffer, 166, pac_unlimited_specials ?
        "SPECIALS UNLIMITED" : "SPECIALS LIMITED", 1,
        pac_unlimited_specials ? 5 : 14);
}

static char pacMapTile(int x, int y)
{
    int index;
    const PAC_Carve *carve;
    if ((unsigned int)x >= PAC_MAP_W || (unsigned int)y >= PAC_MAP_H)
        return '#';
    /* Close the unused lower row so the visible and logical house agree. */
    if (y == PAC_TUNNEL_Y + 1 && x >= PAC_HOUSE_LEFT &&
        x <= PAC_HOUSE_RIGHT) return '#';
    for (index = 0; index < 5; ++index)
    {
        carve = &pac_map_carves[pac_active_map][index];
        if (carve->width == 0) break;
        if (y == carve->y && x >= carve->x &&
            x < carve->x + carve->width) return '.';
    }
    return pac_map_base[y][x];
}

static int pacMapOpen(int x, int y)
{
    if (y == PAC_TUNNEL_Y && (x < 0 || x >= PAC_MAP_W)) return 1;
    if (x < 0 || x >= PAC_MAP_W || y < 0 || y >= PAC_MAP_H) return 0;
    return pacMapTile(x, y) != '#';
}

static void pacResetPellets(PAC_Game *game)
{
    int x;
    int y;
    int pellet_index = 0;
    int divisor;
    char tile;
    game->pellets_remaining = 0;
    for (y = 0; y < PAC_MAP_H; ++y)
    {
        for (x = 0; x < PAC_MAP_W; ++x)
        {
            tile = pacMapTile(x, y);
            pac_pellets[y][x] = tile == 'o' ? 2 : (tile == '.' ? 1 : 0);
            if (pac_pellets[y][x] == 1)
            {
                /* Early chapters deliberately use classic arcade density
                   instead of filling every walkable graph cell. */
                divisor = game->level <= 5 ? 2 :
                    (game->level <= 15 ? 3 :
                     (game->level <= 30 ? 4 : 5));
                if (((pellet_index + game->level * 3 + pac_active_map) %
                     divisor) == 0)
                    pac_pellets[y][x] = 0;
                ++pellet_index;
                if ((x == PAC_PLAYER_TILE_X && y == PAC_PLAYER_TILE_Y) ||
                    (x == PAC_FRUIT_TILE_X && y == PAC_FRUIT_TILE_Y))
                    pac_pellets[y][x] = 0;
            }
            if (game->level >= PAC_LEVEL_WARP &&
                ((x == PAC_WARP_A_X && y == PAC_WARP_A_Y) ||
                 (x == PAC_WARP_B_X && y == PAC_WARP_B_Y)))
                pac_pellets[y][x] = 0;
            if (pac_pellets[y][x] != 0) ++game->pellets_remaining;
        }
    }
}

static void pacDrawPellet(unsigned char *buffer, int center_x, int center_y,
                          int power)
{
    if (!power)
    {
        /* A filled two-by-two phosphor bead survives composite capture as a
           round light at television distance. */
        pacFill(buffer, center_x - 1, center_y - 1, 2, 2, 5);
        pacPixel(buffer, center_x, center_y - 1, 15);
        return;
    }

    /* A solid seven-by-seven plasma orb with a hot three-step interior. It is
       deliberately an order of magnitude more luminous than a normal bead. */
    pacFill(buffer, center_x - 1, center_y - 3, 3, 7, 8);
    pacFill(buffer, center_x - 2, center_y - 2, 5, 5, 8);
    pacFill(buffer, center_x - 3, center_y - 1, 7, 3, 8);
    pacFill(buffer, center_x - 2, center_y - 1, 5, 3, 5);
    pacFill(buffer, center_x - 1, center_y - 1, 3, 3, 4);
    pacPixel(buffer, center_x, center_y, 15);
}

static void pacSetFruitVisible(int visible)
{
    sprite[PAC_OBJ_FRUIT].active = visible ? R_is_active : R_is_inactive;
}

static void pacDrawMiniFruit(unsigned char *buffer, int x, int lit,
                            int fruit_index)
{
    int body = lit ? (fruit_index == 0 ? 7 :
        (fruit_index == 1 ? 5 : 11)) : 1;
    int shine = lit ? 15 : 2;
    pacPixel(buffer, x + 3, 14, lit ? 11 : 1);
    pacFill(buffer, x + 1, 16, 5, 3, body);
    pacFill(buffer, x + 2, 15, 3, 4, body);
    pacPixel(buffer, x + 2, 16, shine);
}

static void pacDrawFruitTracker(unsigned char *buffer, PAC_Game *game)
{
    int index;
    int width = 4 + game->fruit_required * 8;
    int left = 264 - width;
    pacFill(buffer, 228, 13, 37, 7, 0);
    pacFill(buffer, left, 13, width, 7, 14);
    pacFill(buffer, left + 1, 14, width - 2, 5, 0);
    for (index = 0; index < game->fruit_required; ++index)
        pacDrawMiniFruit(buffer, left + 2 + index * 8,
                         index < game->fruit_collected, index);
}

static void pacDrawAbilityBadge(unsigned char *buffer, int x, const char *label,
                                int charges, int active, int flash, int colour,
                                int unlimited)
{
    int available = unlimited || charges > 0;
    int edge = active || flash > 0 ? 15 : (available ? colour : 1);
    int text_colour = available || active ? colour : 14;
    pacFill(buffer, x, 1, 17, 13, 0);
    pacFill(buffer, x, 1, 17, 1, edge);
    pacFill(buffer, x, 13, 17, 1, edge);
    pacFill(buffer, x, 1, 1, 13, edge);
    pacFill(buffer, x + 16, 1, 1, 13, edge);
    if (active)
    {
        pacFill(buffer, x + 2, 10, 13, 2, colour);
        if (flash & 4) pacFill(buffer, x + 2, 2, 13, 1, 15);
    }
    pacText(buffer, x + 2, 4, label, 1, text_colour);
    if (unlimited)
        pacText(buffer, x + 9, 4, "*", 1, active ? 15 : text_colour);
    else
        pacNumber(buffer, x + 9, 4, charges, 1,
                  active ? 15 : text_colour);
}

static void pacDrawDashBadge(unsigned char *buffer, PAC_Game *game)
{
    int unlimited = pac_unlimited_specials || game->dash_overdrive;
    int available = unlimited || game->dash_charges > 0;
    int edge = game->dash_ticks > 0 || game->dash_flash_ticks > 0 ?
        15 : (available ? 5 : 1);
    int bar = unlimited ? 12 : game->dash_recharge_segment * 2;
    pacFill(buffer, 301, 1, 19, 13, 0);
    pacFill(buffer, 301, 1, 19, 1, edge);
    pacFill(buffer, 301, 13, 19, 1, edge);
    pacFill(buffer, 301, 1, 1, 13, edge);
    pacFill(buffer, 319, 1, 1, 13, edge);
    pacText(buffer, 303, 4, "A", 1, available ? 5 : 14);
    if (unlimited)
        pacText(buffer, 310, 4, "*", 1, game->dash_ticks > 0 ? 15 : 5);
    else
    {
        pacFill(buffer, 303, 10, 12, 2, 1);
        if (bar > 0) pacFill(buffer, 303, 10, bar, 2, 5);
    }
}

static void pacDrawAbilityStatus(unsigned char *buffer, PAC_Game *game)
{
    pacFill(buffer, 265, 0, 55, 15, 0);
    pacDrawAbilityBadge(buffer, 265, "C", game->shield_charges,
                        game->shield_ticks > 0,
                        game->shield_flash_ticks, 9,
                        pac_unlimited_specials);
    pacDrawAbilityBadge(buffer, 283, "B", game->pulse_charges,
                        game->ghost_stun_ticks > 0,
                        game->pulse_flash_ticks, 13,
                        pac_unlimited_specials);
    pacDrawDashBadge(buffer, game);
}

static int pacStatusCode(PAC_Game *game)
{
    int code = 0;
    if (game->magnet_ticks > 0) code |= 1;
    if (game->level >= PAC_LEVEL_PULSE)
        code |= 32 | (game->prism_pressure << 1);
    if (game->level >= PAC_LEVEL_STORM &&
        ((game->storm_ticks > 0 && game->storm_ticks <= 60) ||
         game->storm_notice_ticks > 0)) code |= 8;
    if (game->otto_notice_ticks > 0) code |= 16;
    code |= game->fruit_collected << 6;
    if (game->dash_overdrive) code |= 512;
    return code;
}

static void pacDrawStatusLine(unsigned char *buffer, PAC_Game *game)
{
    int code = pacStatusCode(game);
    pacFill(buffer, 0, 13, 265, 7, 0);
    if (game->magnet_ticks > 0)
        pacText(buffer, 2, 13, "MAGNET", 1, 11);
    if (game->level >= PAC_LEVEL_PULSE)
    {
        pacText(buffer, 48, 13, "PRISM", 1,
                game->prism_pressure >= 3 ? 7 : 13);
        pacNumber(buffer, 80, 13, game->prism_pressure, 1,
                  game->prism_pressure >= 3 ? 7 : 15);
    }
    if (game->level >= PAC_LEVEL_STORM &&
        ((game->storm_ticks > 0 && game->storm_ticks <= 60) ||
         game->storm_notice_ticks > 0))
        pacText(buffer, 98, 13, "STORM", 1, 9);
    if (game->otto_notice_ticks > 0)
        pacText(buffer, 150, 13, "OTTO IMMUNE", 1, 8);
    pacDrawFruitTracker(buffer, game);
    game->status_drawn = code;
}

static void pacUpdateStatusLine(unsigned char *buffer, PAC_Game *game)
{
    if (pacStatusCode(game) != game->status_drawn)
        pacDrawStatusLine(buffer, game);
}

static void pacDrawWarpGate(unsigned char *buffer, int center_x, int center_y)
{
    /* A filled cyan portal rim and black aperture remain distinct from pellets
       and wall highlights. */
    pacFill(buffer, center_x - 3, center_y - 3, 7, 7, 11);
    pacFill(buffer, center_x - 4, center_y - 2, 9, 5, 11);
    pacFill(buffer, center_x - 3, center_y - 2, 7, 5, 13);
    pacFill(buffer, center_x - 2, center_y - 1, 5, 3, 0);
    pacPixel(buffer, center_x - 3, center_y, 15);
    pacPixel(buffer, center_x + 3, center_y, 15);
}

static void pacDrawCrawlerHazard(unsigned char *buffer, PAC_Game *game,
                                 unsigned int frame)
{
    int x;
    int center_x;
    int center_y;
    int hot = (frame & 8U) ? 15 : 11;
    int edge = (frame & 8U) ? 9 : 13;
    if (game->hazard_ticks <= 0) return;
    center_x = PAC_MAP_LEFT + game->hazard_x * PAC_TILE_X + PAC_TILE_CENTER_X;
    center_y = PAC_MAP_TOP + game->hazard_y * PAC_TILE_Y + PAC_TILE_CENTER_Y;
    /* A fixed OP-free local field remains on the live canvas for every lethal
       tick. Its open centre preserves any collectible under the warning. */
    for (x = -4; x <= 4; x += 2)
    {
        pacPixel(buffer, center_x + x, center_y - 2, edge);
        pacPixel(buffer, center_x + x, center_y + 2, edge);
    }
    pacPixel(buffer, center_x - 4, center_y, hot);
    pacPixel(buffer, center_x + 4, center_y, hot);
    pacPixel(buffer, center_x - 2, center_y - 1, hot);
    pacPixel(buffer, center_x + 2, center_y + 1, hot);
}

static void pacClearCrawlerHazard(unsigned char *buffer, PAC_Game *game)
{
    int center_x = PAC_MAP_LEFT + game->hazard_x * PAC_TILE_X +
        PAC_TILE_CENTER_X;
    int center_y = PAC_MAP_TOP + game->hazard_y * PAC_TILE_Y +
        PAC_TILE_CENTER_Y;
    pacFill(buffer, center_x - 4, center_y - 2, 9, 5, 0);
    if ((unsigned int)game->hazard_x < PAC_MAP_W &&
        (unsigned int)game->hazard_y < PAC_MAP_H &&
        pac_pellets[game->hazard_y][game->hazard_x] != 0)
        pacDrawPellet(buffer, center_x, center_y,
                      pac_pellets[game->hazard_y][game->hazard_x] == 2);
    if (game->level >= PAC_LEVEL_WARP &&
        ((game->hazard_x == PAC_WARP_A_X && game->hazard_y == PAC_WARP_A_Y) ||
         (game->hazard_x == PAC_WARP_B_X && game->hazard_y == PAC_WARP_B_Y)))
        pacDrawWarpGate(buffer, center_x, center_y);
}

static void pacDrawCombo(unsigned char *buffer, PAC_Game *game)
{
    pacFill(buffer, 76, 4, 26, 9, 0);
    if (game->level >= PAC_LEVEL_SPARK)
    {
        pacText(buffer, 78, 5, "X", 1, 8);
        pacNumber(buffer, 90, 5, game->combo, 1,
                  game->combo > 1 ? 5 : 14);
    }
}

static void pacBuildPlayfield(unsigned char *buffer, PAC_Game *game)
{
    int x;
    int y;
    int px;
    int py;
    int open_left;
    int open_right;
    int open_up;
    int open_down;
    pacZero(buffer, PAC_CLUT4_BYTES);

    pacText(buffer, 2, 5, "SCORE", 1, 4);
    pacNumber(buffer, 32, 5, game->score, 7, 4);
    pacText(buffer, 105, 5, "TOP", 1, 13);
    pacNumber(buffer, 125, 5, game->high_score, 7, 13);
    pacText(buffer, 171, 5, "L", 1, 13);
    pacNumber(buffer, 183, 5, game->lives, 1, 13);
    pacText(buffer, 198, 5, "LV", 1, 4);
    pacNumber(buffer, 214, 5, game->level, 3, 4);
    pacDrawCombo(buffer, game);
    pacDrawAbilityStatus(buffer, game);
    pacDrawStatusLine(buffer, game);
    game->hud_score_drawn = game->score;
    game->hud_high_score_drawn = game->high_score;
    game->hud_lives_drawn = game->lives;
    game->hud_combo_drawn = game->combo;

    for (y = 0; y < PAC_MAP_H; ++y)
    {
        for (x = 0; x < PAC_MAP_W; ++x)
        {
            px = PAC_MAP_LEFT + x * PAC_TILE_X;
            py = PAC_MAP_TOP + y * PAC_TILE_Y;
            if (pacMapTile(x, y) == '#')
            {
                open_left = pacMapOpen(x - 1, y);
                open_right = pacMapOpen(x + 1, y);
                open_up = pacMapOpen(x, y - 1);
                open_down = pacMapOpen(x, y + 1);
                /* Four-pixel tubes put visible luminous mass around the path:
                   dark bloom, saturated band and a one-pixel hot surface.
                   Wall interiors remain true black for depth. */
                if (open_left)
                {
                    pacFill(buffer, px - 1, py, 1, PAC_TILE_Y, 1);
                    pacFill(buffer, px, py, 4, PAC_TILE_Y, 1);
                    pacFill(buffer, px, py, 3, PAC_TILE_Y, 2);
                    pacFill(buffer, px, py, 1, PAC_TILE_Y, 3);
                }
                if (open_right)
                {
                    pacFill(buffer, px + PAC_TILE_X, py, 1, PAC_TILE_Y, 1);
                    pacFill(buffer, px + PAC_TILE_X - 4, py, 4,
                            PAC_TILE_Y, 1);
                    pacFill(buffer, px + PAC_TILE_X - 3, py, 3,
                            PAC_TILE_Y, 2);
                    pacFill(buffer, px + PAC_TILE_X - 1, py, 1,
                            PAC_TILE_Y, 3);
                }
                if (open_up)
                {
                    pacFill(buffer, px, py - 1, PAC_TILE_X, 1, 1);
                    pacFill(buffer, px, py, PAC_TILE_X, 4, 1);
                    pacFill(buffer, px, py, PAC_TILE_X, 3, 2);
                    pacFill(buffer, px, py, PAC_TILE_X, 1, 3);
                }
                if (open_down)
                {
                    pacFill(buffer, px, py + PAC_TILE_Y, PAC_TILE_X, 1, 1);
                    pacFill(buffer, px, py + PAC_TILE_Y - 4,
                            PAC_TILE_X, 4, 1);
                    pacFill(buffer, px, py + PAC_TILE_Y - 3,
                            PAC_TILE_X, 3, 2);
                    pacFill(buffer, px, py + PAC_TILE_Y - 1,
                            PAC_TILE_X, 1, 3);
                }
            }
            else if (pac_pellets[y][x] == 1)
            {
                pacDrawPellet(buffer, px + PAC_TILE_CENTER_X,
                              py + PAC_TILE_CENTER_Y, 0);
            }
            else if (pac_pellets[y][x] == 2)
            {
                pacDrawPellet(buffer, px + PAC_TILE_CENTER_X,
                              py + PAC_TILE_CENTER_Y, 1);
            }
        }
    }
    if (game->level >= PAC_LEVEL_WARP)
    {
        px = PAC_MAP_LEFT + PAC_WARP_A_X * PAC_TILE_X + PAC_TILE_CENTER_X;
        py = PAC_MAP_TOP + PAC_WARP_A_Y * PAC_TILE_Y + PAC_TILE_CENTER_Y;
        pacDrawWarpGate(buffer, px, py);
        px = PAC_MAP_LEFT + PAC_WARP_B_X * PAC_TILE_X + PAC_TILE_CENTER_X;
        py = PAC_MAP_TOP + PAC_WARP_B_Y * PAC_TILE_Y + PAC_TILE_CENTER_Y;
        pacDrawWarpGate(buffer, px, py);
    }
    pacDrawCrawlerHazard(buffer, game, 0);
    if (game->phase == PAC_PHASE_READY)
    {
        if (game->level == 1)
        {
            pacFill(buffer, 10, 65, 300, 108, 0);
            pacTubeBox(buffer, 10, 65, 300, 108);
            pacCenteredText(buffer, 70, "READY", 2, 6);
            pacCenteredText(buffer, 94,
                "C1 SHIELD  B2 PULSE  A DASH RECHARGES", 1, 15);
            pacCenteredText(buffer, 114,
                "POWER PELLET REFILLS SHIELD DASH", 1, 4);
            pacCenteredText(buffer, 132, "FRUIT ADDS ONE PULSE", 1, 5);
            pacCenteredText(buffer, 144,
                "RED CHASE PINK AHEAD CYAN FLANK GOLD SHY", 1, 8);
            pacCenteredText(buffer, 160,
                "PRESS C B A OPTION OR MOVE", 1, 13);
        }
        else if (game->level == PAC_LEVEL_DASH)
        {
            pacFill(buffer, 10, 68, 300, 112, 0);
            pacTubeBox(buffer, 10, 68, 300, 112);
            pacCenteredText(buffer, 75, "BEWARE OTTO ECHO", 2, 8);
            pacCenteredText(buffer, 107,
                "POWER PELLETS CANNOT STOP HIM", 1, 15);
            pacCenteredText(buffer, 126,
                "USE PULSE SHIELD OR EVADE", 1, 13);
            pacCenteredText(buffer, 146,
                "COLLECT 3 FRUIT FOR DASH OVERDRIVE", 1, 5);
            pacCenteredText(buffer, 162, "PRESS C B A OPTION OR MOVE", 1, 6);
        }
        else
        {
            pacFill(buffer, 10, 78, 300, 94, 0);
            pacTubeBox(buffer, 10, 78, 300, 94);
            pacCenteredText(buffer, 84, "READY", 2, 6);
            pacCenteredText(buffer, 107, pac_chapter_names[game->chapter], 1, 6);
            pacCenteredText(buffer, 122,
                pac_stage_names[(game->level - 1) % 5], 1, 13);
            pacCenteredText(buffer, 140,
                pac_chapter_rules[game->chapter], 1, 15);
            pacCenteredText(buffer, 154,
                game->fruit_required == 2 ?
                "COLLECT 2 FRUIT FOR DASH OVERDRIVE" :
                "COLLECT 3 FRUIT FOR DASH OVERDRIVE", 1, 5);
        }
    }
}

static void pacActorPixel(int frame, int x, int y, int colour)
{
    unsigned char *base = (unsigned char *)&PAC_actor_gfx;
    unsigned char *pixel;
    if ((unsigned int)frame >= PAC_ACTOR_FRAMES ||
        (unsigned int)x >= PAC_ACTOR_W ||
        (unsigned int)y >= PAC_ACTOR_H) return;
    pixel = base + frame * PAC_ACTOR_FRAME_BYTES + y * (PAC_ACTOR_W / 2) + (x >> 1);
    if (x & 1)
        *pixel = (unsigned char)((*pixel & 0xf0) | (colour & 15));
    else
        *pixel = (unsigned char)((*pixel & 0x0f) | ((colour & 15) << 4));
}

static void pacActorHLine(int frame, int x, int y, int width, int colour)
{
    int index;
    for (index = 0; index < width; ++index)
        pacActorPixel(frame, x + index, y, colour);
}

static void pacWakePixel(int frame, int x, int y, int colour)
{
    unsigned char *base = (unsigned char *)&PAC_wake_gfx;
    unsigned char *pixel;
    if ((unsigned int)frame >= PAC_WAKE_FRAMES ||
        (unsigned int)x >= PAC_WAKE_W || (unsigned int)y >= PAC_WAKE_H) return;
    pixel = base + frame * PAC_WAKE_FRAME_BYTES +
        y * (PAC_WAKE_W / 2) + (x >> 1);
    if (x & 1)
        *pixel = (unsigned char)((*pixel & 0xf0) | (colour & 15));
    else
        *pixel = (unsigned char)((*pixel & 0x0f) | ((colour & 15) << 4));
}

static void pacWakeCross(int frame, int x, int y, int colour, int radius)
{
    pacWakePixel(frame, x, y, colour);
    if (radius <= 0) return;
    pacWakePixel(frame, x - radius, y, colour);
    pacWakePixel(frame, x + radius, y, colour);
    pacWakePixel(frame, x, y - radius, colour);
    pacWakePixel(frame, x, y + radius, colour);
}

static void pacPickupPixel(int frame, int x, int y, int colour)
{
    unsigned char *base = (unsigned char *)&PAC_pickup_gfx;
    unsigned char *pixel;
    if ((unsigned int)frame >= PAC_PICKUP_FRAMES ||
        (unsigned int)x >= 16U || (unsigned int)y >= 16U) return;
    pixel = base + frame * PAC_PICKUP_FRAME_BYTES + y * 8 + (x >> 1);
    if (x & 1)
        *pixel = (unsigned char)((*pixel & 0xf0) | (colour & 15));
    else
        *pixel = (unsigned char)((*pixel & 0x0f) | ((colour & 15) << 4));
}

static void pacGenerateWakes(void)
{
    static const int orbit_x[8] = {0,7,10,7,0,-7,-10,-7};
    static const int orbit_y[8] = {-10,-7,0,7,10,7,0,-7};
    unsigned char *wake = (unsigned char *)&PAC_wake_gfx;
    unsigned char *pickup = (unsigned char *)&PAC_pickup_gfx;
    int mode;
    int direction;
    int phase;
    int frame;
    int index;
    int distance;
    int lateral;
    int dx;
    int dy;
    int side_x;
    int side_y;
    int x;
    int y;
    int colour;

    pacZero(wake, PAC_WAKE_FRAMES * PAC_WAKE_FRAME_BYTES);
    pacZero(pickup, PAC_PICKUP_FRAMES * PAC_PICKUP_FRAME_BYTES);

    /* These point clouds are generated once.  During play the Object Processor
       only moves/selects them; no shared GPU particle record is touched by the
       continuous comet wakes. */
    for (mode = 0; mode < PAC_WAKE_MODE_COUNT; ++mode)
    {
        for (direction = PAC_DIR_UP; direction <= PAC_DIR_RIGHT; ++direction)
        {
            dx = pac_dir_x[direction];
            dy = pac_dir_y[direction];
            side_x = -dy;
            side_y = dx;
            for (phase = 0; phase < 2; ++phase)
            {
                frame = mode * 8 + (direction - 1) * 2 + phase;
                for (index = 0; index < 6; ++index)
                {
                    distance = 6 + index * 2;
                    lateral = ((index + phase) & 1 ? 1 : -1) *
                        (1 + ((index + mode) & 1));
                    x = 16 - dx * distance + side_x * lateral;
                    y = 16 - dy * distance + side_y * lateral;
                    colour = index == 0 ? 4 :
                        (index < 3 ? 6 : (index == 3 ? 5 : 3));
                    pacWakeCross(frame, x, y, colour,
                        (mode == PAC_WAKE_MODE_DASH && index < 3) ||
                        (mode == PAC_WAKE_MODE_POWER && index == phase) ? 1 : 0);
                }

                if (mode == PAC_WAKE_MODE_POWER || mode == PAC_WAKE_MODE_SHIELD ||
                    mode == PAC_WAKE_MODE_PULSE || mode == PAC_WAKE_MODE_MAGNET)
                {
                    for (index = 0; index < 8; ++index)
                    {
                        if (mode == PAC_WAKE_MODE_POWER && ((index + phase) & 1))
                            continue;
                        if (mode == PAC_WAKE_MODE_MAGNET && index > 4)
                            continue;
                        colour = mode == PAC_WAKE_MODE_PULSE ?
                            ((index + phase) & 1 ? 4 : 6) :
                            (index & 1 ? 5 : 6);
                        pacWakeCross(frame,
                            16 + orbit_x[(index + phase) & 7],
                            16 + orbit_y[(index + phase) & 7],
                            colour,
                            mode == PAC_WAKE_MODE_SHIELD && !(index & 1));
                    }
                }
                if (mode == PAC_WAKE_MODE_DASH)
                {
                    for (index = 0; index < 4; ++index)
                        pacWakeCross(frame,
                            16 - dx * (10 + index * 2) + side_x * (index - 2),
                            16 - dy * (10 + index * 2) + side_y * (index - 2),
                            index < 2 ? 4 : 6, 1);
                }
            }
        }
    }

    for (direction = PAC_DIR_UP; direction <= PAC_DIR_RIGHT; ++direction)
    {
        dx = pac_dir_x[direction];
        dy = pac_dir_y[direction];
        side_x = -dy;
        side_y = dx;
        for (phase = 0; phase < 2; ++phase)
        {
            frame = PAC_WAKE_GHOST_BASE + (direction - 1) * 2 + phase;
            for (index = 0; index < 5; ++index)
            {
                distance = 7 + index * 2;
                lateral = ((index + phase) & 1 ? 1 : -1) *
                    (1 + (index & 1));
                colour = index == 0 ? 4 :
                    (index < 3 ? 6 : (index == 3 ? 5 : 3));
                pacWakeCross(frame,
                    16 - dx * distance + side_x * lateral,
                    16 - dy * distance + side_y * lateral,
                    colour, index == phase + 1);
            }
        }
    }

    /* The four-frame pellet starburst is an independent transparent OP object. */
    for (phase = 0; phase < PAC_PICKUP_FRAMES; ++phase)
    {
        int radius = 1 + phase * 2;
        int arm;
        for (arm = -radius; arm <= radius; ++arm)
        {
            colour = arm == 0 ? 15 : (pacAbs(arm) == radius ? 8 : 5);
            pacPickupPixel(phase, 7 + arm, 7, colour);
            pacPickupPixel(phase, 7, 7 + arm, colour);
        }
        pacPickupPixel(phase, 7 - phase, 7 - phase, 13);
        pacPickupPixel(phase, 7 + phase, 7 - phase, 13);
        pacPickupPixel(phase, 7 - phase, 7 + phase, 13);
        pacPickupPixel(phase, 7 + phase, 7 + phase, 13);
    }
}

static int pacMouthCut(int direction, int dx, int dy)
{
    if (direction == PAC_DIR_UP) return dy < 0 && pacAbs(dx) <= -dy * 2;
    if (direction == PAC_DIR_LEFT) return dx < 0 && pacAbs(dy) <= -dx * 2;
    if (direction == PAC_DIR_DOWN) return dy > 0 && pacAbs(dx) <= dy * 2;
    return dx > 0 && pacAbs(dy) <= dx * 2;
}

static int pacNormalMouthCut(int direction, int dx, int dy, int phase)
{
    if (phase) return pacMouthCut(direction, dx, dy);
    if (direction == PAC_DIR_UP) return dy < 0 && pacAbs(dx) * 2 < -dy;
    if (direction == PAC_DIR_LEFT) return dx < 0 && pacAbs(dy) * 2 < -dx;
    if (direction == PAC_DIR_DOWN) return dy > 0 && pacAbs(dx) * 2 < dy;
    return dx > 0 && pacAbs(dy) * 2 < dx;
}

static int pacPoweredMouthCut(int direction, int dx, int dy, int phase)
{
    int side_scale = phase ? 1 : 2;
    int forward_scale = phase ? 2 : 1;
    if (direction == PAC_DIR_UP)
        return dy < 0 && pacAbs(dx) * side_scale < -dy * forward_scale;
    if (direction == PAC_DIR_LEFT)
        return dx < 0 && pacAbs(dy) * side_scale < -dx * forward_scale;
    if (direction == PAC_DIR_DOWN)
        return dy > 0 && pacAbs(dx) * side_scale < dy * forward_scale;
    return dx > 0 && pacAbs(dy) * side_scale < dx * forward_scale;
}

static int pacPoweredInside(int direction, int dx, int dy, int phase)
{
    if (dx * dx + dy * dy > 31) return 0;
    return !pacPoweredMouthCut(direction, dx, dy, phase);
}

static void pacGenerateFruitFrame(int variant)
{
    unsigned char *base = (unsigned char *)&PAC_actor_gfx;
    int y;
    pacZero(base + PAC_FRAME_FRUIT * PAC_ACTOR_FRAME_BYTES,
            PAC_ACTOR_FRAME_BYTES);
    if (variant == 0)
    {
        /* Twin cherry nodes. */
        pacActorPixel(PAC_FRAME_FRUIT, 8, 2, 11);
        pacActorHLine(PAC_FRAME_FRUIT, 6, 3, 4, 11);
        pacActorPixel(PAC_FRAME_FRUIT, 5, 4, 11);
        pacActorPixel(PAC_FRAME_FRUIT, 10, 4, 11);
        for (y = 7; y <= 11; ++y)
        {
            pacActorHLine(PAC_FRAME_FRUIT, 2, y, 5, 7);
            pacActorHLine(PAC_FRAME_FRUIT, 9, y, 5, 7);
        }
        pacActorHLine(PAC_FRAME_FRUIT, 3, 12, 3, 7);
        pacActorHLine(PAC_FRAME_FRUIT, 10, 12, 3, 7);
        pacActorPixel(PAC_FRAME_FRUIT, 3, 8, 15);
        pacActorPixel(PAC_FRAME_FRUIT, 10, 8, 15);
    }
    else if (variant == 1)
    {
        /* Strawberry wedge. */
        pacActorHLine(PAC_FRAME_FRUIT, 4, 3, 8, 11);
        pacActorHLine(PAC_FRAME_FRUIT, 3, 5, 10, 7);
        for (y = 6; y <= 10; ++y)
            pacActorHLine(PAC_FRAME_FRUIT, 3 + (y >> 2), y,
                          10 - (y >> 1), 7);
        pacActorHLine(PAC_FRAME_FRUIT, 6, 11, 4, 7);
        pacActorPixel(PAC_FRAME_FRUIT, 5, 6, 15);
        pacActorPixel(PAC_FRAME_FRUIT, 9, 8, 5);
        pacActorPixel(PAC_FRAME_FRUIT, 7, 10, 15);
    }
    else if (variant == 2)
    {
        /* Orange orb with a leaf. */
        pacActorPixel(PAC_FRAME_FRUIT, 8, 2, 11);
        pacActorHLine(PAC_FRAME_FRUIT, 9, 2, 3, 11);
        pacActorHLine(PAC_FRAME_FRUIT, 5, 4, 6, 8);
        pacActorHLine(PAC_FRAME_FRUIT, 3, 5, 10, 8);
        for (y = 6; y <= 11; ++y)
            pacActorHLine(PAC_FRAME_FRUIT, 2, y, 12, 8);
        pacActorHLine(PAC_FRAME_FRUIT, 4, 12, 8, 8);
        pacActorPixel(PAC_FRAME_FRUIT, 4, 6, 15);
    }
    else if (variant == 3)
    {
        /* Striped neon melon. */
        for (y = 4; y <= 12; ++y)
            pacActorHLine(PAC_FRAME_FRUIT, 2 + ((y == 4 || y == 12) ? 2 : 0),
                          y, (y == 4 || y == 12) ? 8 : 12, 11);
        for (y = 5; y <= 11; ++y)
        {
            pacActorPixel(PAC_FRAME_FRUIT, 6, y, 12);
            pacActorPixel(PAC_FRAME_FRUIT, 10, y, 12);
        }
        pacActorPixel(PAC_FRAME_FRUIT, 4, 5, 15);
    }
    else if (variant == 4)
    {
        /* Five-point starfruit. */
        pacActorHLine(PAC_FRAME_FRUIT, 7, 2, 2, 5);
        pacActorHLine(PAC_FRAME_FRUIT, 6, 4, 4, 5);
        pacActorHLine(PAC_FRAME_FRUIT, 2, 6, 12, 5);
        pacActorHLine(PAC_FRAME_FRUIT, 4, 8, 8, 5);
        pacActorHLine(PAC_FRAME_FRUIT, 5, 10, 6, 5);
        pacActorHLine(PAC_FRAME_FRUIT, 4, 12, 3, 5);
        pacActorHLine(PAC_FRAME_FRUIT, 9, 12, 3, 5);
        pacActorPixel(PAC_FRAME_FRUIT, 7, 6, 15);
    }
    else if (variant == 5)
    {
        /* Pear with a narrow neck. */
        pacActorPixel(PAC_FRAME_FRUIT, 9, 2, 11);
        pacActorHLine(PAC_FRAME_FRUIT, 7, 3, 3, 11);
        pacActorHLine(PAC_FRAME_FRUIT, 6, 5, 5, 5);
        pacActorHLine(PAC_FRAME_FRUIT, 4, 7, 9, 5);
        for (y = 8; y <= 11; ++y)
            pacActorHLine(PAC_FRAME_FRUIT, 3, y, 11, 5);
        pacActorHLine(PAC_FRAME_FRUIT, 5, 12, 7, 5);
        pacActorPixel(PAC_FRAME_FRUIT, 5, 8, 15);
    }
    else if (variant == 6)
    {
        /* Faceted prism berry. */
        pacActorHLine(PAC_FRAME_FRUIT, 7, 2, 2, 13);
        pacActorHLine(PAC_FRAME_FRUIT, 5, 4, 6, 13);
        pacActorHLine(PAC_FRAME_FRUIT, 3, 6, 10, 9);
        pacActorHLine(PAC_FRAME_FRUIT, 2, 8, 12, 9);
        pacActorHLine(PAC_FRAME_FRUIT, 4, 10, 8, 12);
        pacActorHLine(PAC_FRAME_FRUIT, 6, 12, 4, 12);
        pacActorPixel(PAC_FRAME_FRUIT, 7, 6, 15);
        pacActorPixel(PAC_FRAME_FRUIT, 5, 8, 13);
    }
    else if (variant == 7)
    {
        /* Grape cluster. */
        pacActorHLine(PAC_FRAME_FRUIT, 7, 2, 4, 11);
        pacActorHLine(PAC_FRAME_FRUIT, 4, 5, 8, 9);
        pacActorHLine(PAC_FRAME_FRUIT, 3, 7, 11, 9);
        pacActorHLine(PAC_FRAME_FRUIT, 4, 9, 9, 9);
        pacActorHLine(PAC_FRAME_FRUIT, 6, 11, 5, 9);
        pacActorHLine(PAC_FRAME_FRUIT, 7, 13, 3, 9);
        pacActorPixel(PAC_FRAME_FRUIT, 5, 6, 15);
        pacActorPixel(PAC_FRAME_FRUIT, 10, 9, 13);
    }
    else if (variant == 8)
    {
        /* Curved banana crescent. */
        pacActorHLine(PAC_FRAME_FRUIT, 3, 4, 3, 5);
        pacActorHLine(PAC_FRAME_FRUIT, 2, 6, 4, 5);
        pacActorHLine(PAC_FRAME_FRUIT, 3, 8, 4, 5);
        pacActorHLine(PAC_FRAME_FRUIT, 5, 10, 5, 5);
        pacActorHLine(PAC_FRAME_FRUIT, 8, 11, 5, 5);
        pacActorHLine(PAC_FRAME_FRUIT, 11, 9, 3, 5);
        pacActorPixel(PAC_FRAME_FRUIT, 3, 4, 15);
        pacActorPixel(PAC_FRAME_FRUIT, 13, 9, 8);
    }
    else
    {
        /* Finale nova fruit. */
        pacActorHLine(PAC_FRAME_FRUIT, 7, 2, 2, 15);
        pacActorHLine(PAC_FRAME_FRUIT, 5, 4, 6, 13);
        pacActorHLine(PAC_FRAME_FRUIT, 3, 6, 10, 12);
        pacActorHLine(PAC_FRAME_FRUIT, 2, 8, 12, 9);
        pacActorHLine(PAC_FRAME_FRUIT, 4, 10, 8, 7);
        pacActorHLine(PAC_FRAME_FRUIT, 6, 12, 4, 5);
        pacActorPixel(PAC_FRAME_FRUIT, 7, 7, 15);
        pacActorPixel(PAC_FRAME_FRUIT, 8, 8, 15);
    }
}

static void pacGenerateActors(void)
{
    unsigned char *base = (unsigned char *)&PAC_actor_gfx;
    int direction;
    int phase;
    int frame;
    int x;
    int y;
    int dx;
    int dy;
    int distance;
    int body;
    int interior;
    int edge;
    int eye;
    int ex;
    int ey;
    int eye_x;
    int eye_y;
    int pupil_x;
    int pupil_y;
    int variant;

    pacZero(base, PAC_ACTOR_FRAMES * PAC_ACTOR_FRAME_BYTES);
    for (direction = PAC_DIR_UP; direction <= PAC_DIR_RIGHT; ++direction)
    {
        for (phase = 0; phase < 2; ++phase)
        {
            frame = (direction - 1) * 2 + phase;
            for (y = 0; y < 16; ++y)
            {
                for (x = 0; x < 16; ++x)
                {
                    dx = x - 7;
                    dy = y - 7;
                    distance = dx * dx + dy * dy;
                    if (distance > 42) continue;
                    if (pacNormalMouthCut(direction, dx, dy, phase)) continue;
                    if (distance > 30)
                        pacActorPixel(frame, x, y, 3);
                    else
                        pacActorPixel(frame, x, y, distance > 20 ? 6 : 5);
                }
            }
        }
    }

    for (direction = PAC_DIR_UP; direction <= PAC_DIR_RIGHT; ++direction)
    {
        for (phase = 0; phase < 2; ++phase)
        {
            frame = PAC_FRAME_GHOST_BASE + (direction - 1) * 2 + phase;
            for (y = 1; y < 15; ++y)
            {
                for (x = 1; x < 15; ++x)
                {
                    dx = x - 7;
                    body = 0;
                    if (y <= 8 && dx * dx + (y - 7) * (y - 7) <= 30) body = 1;
                    if (y >= 7 && y <= 11 && x >= 3 && x <= 12) body = 1;
                    if (y == 12 && x >= 3 && x <= 12 &&
                        ((x + 1 + phase * 2) & 3) != 0) body = 1;
                    if (!body)
                    {
                        interior = 0;
                        if (y <= 9 && dx * dx + (y - 7) * (y - 7) <= 42)
                            interior = 1;
                        if (y >= 6 && y <= 13 && x >= 2 && x <= 13)
                            interior = 1;
                        if (interior) pacActorPixel(frame, x, y, 3);
                    }
                    else
                    {
                        interior = (y <= 8 && dx * dx + (y - 7) * (y - 7) <= 20) ||
                                   (y >= 7 && y <= 10 && x >= 4 && x <= 11);
                        if (!interior)
                            pacActorPixel(frame, x, y,
                                ((x + y + phase) & 3) == 0 ? 6 : 5);
                    }
                }
            }
            for (eye = 0; eye < 2; ++eye)
            {
                eye_x = 5 + eye * 5;
                eye_y = 6;
                for (ey = -1; ey <= 1; ++ey)
                    for (ex = -1; ex <= 1; ++ex)
                        pacActorPixel(frame, eye_x + ex, eye_y + ey, 4);
                pupil_x = eye_x + pac_dir_x[direction];
                pupil_y = eye_y + pac_dir_y[direction];
                pacActorPixel(frame, pupil_x, pupil_y, 2);
                if (direction == PAC_DIR_UP || direction == PAC_DIR_DOWN)
                    pacActorPixel(frame, pupil_x + 1, pupil_y, 2);
                else
                    pacActorPixel(frame, pupil_x, pupil_y + 1, 2);
            }
        }
    }

    for (direction = PAC_DIR_UP; direction <= PAC_DIR_RIGHT; ++direction)
    {
        for (phase = 0; phase < 2; ++phase)
        {
            frame = PAC_FRAME_POWER_BASE + (direction - 1) * 2 + phase;
            for (y = 0; y < 16; ++y)
            {
                for (x = 0; x < 16; ++x)
                {
                    dx = x - 7;
                    dy = y - 7;
                    distance = dx * dx + dy * dy;
                    if (!pacPoweredInside(direction, dx, dy, phase))
                    {
                        if (distance <= 42 &&
                            !pacPoweredMouthCut(direction, dx, dy, phase))
                            pacActorPixel(frame, x, y, 3);
                        continue;
                    }
                    edge = !pacPoweredInside(direction, dx - 1, dy, phase) ||
                           !pacPoweredInside(direction, dx + 1, dy, phase) ||
                           !pacPoweredInside(direction, dx, dy - 1, phase) ||
                           !pacPoweredInside(direction, dx, dy + 1, phase);
                    if (!edge)
                        edge = !pacPoweredInside(direction, dx - 2, dy, phase) ||
                               !pacPoweredInside(direction, dx + 2, dy, phase) ||
                               !pacPoweredInside(direction, dx, dy - 2, phase) ||
                               !pacPoweredInside(direction, dx, dy + 2, phase);
                    if (!edge) continue;
                    if (distance > 26 || pacPoweredMouthCut(direction, dx, dy - 1, phase) ||
                        pacPoweredMouthCut(direction, dx, dy + 1, phase) ||
                        pacPoweredMouthCut(direction, dx - 1, dy, phase) ||
                        pacPoweredMouthCut(direction, dx + 1, dy, phase))
                        pacActorPixel(frame, x, y, 4);
                    else
                        pacActorPixel(frame, x, y,
                            ((x + y + phase) & 3) == 0 ? 6 : 5);
                }
            }
        }
    }

    pacGenerateFruitFrame(0);

    /* Four original arcade-echo silhouettes.  They remain generated point
       clouds, share the classic ghost animation language, and are visually
       distinct enough to advertise their different rules at a glance. */
    for (variant = 0; variant < 4; ++variant)
    {
        for (direction = PAC_DIR_UP; direction <= PAC_DIR_RIGHT; ++direction)
        {
            for (phase = 0; phase < 2; ++phase)
            {
                frame = PAC_FRAME_ECHO_BASE + variant * 8 +
                    (direction - 1) * 2 + phase;
                for (y = 1; y < 15; ++y)
                {
                    for (x = 1; x < 15; ++x)
                    {
                        dx = x - 7;
                        dy = y - 7;
                        body = 0;
                        if (variant == 0)
                        {
                            if (y <= 8 && dx * dx + dy * dy <= 31) body = 1;
                            if (y >= 7 && y <= 10 && x >= 2 && x <= 13) body = 1;
                            if (y == 11 && ((x + phase) & 2) && x >= 3 && x <= 12)
                                body = 1;
                        }
                        else if (variant == 1)
                        {
                            if (y >= 3 && y <= 11 && x >= 3 && x <= 12 &&
                                (pacAbs(dx) + pacAbs(dy) <= 8)) body = 1;
                            if ((x == 2 || x == 13) && y >= 6 && y <= 10)
                                body = 1;
                            if (y == 12 && ((x + phase) & 3) != 1 &&
                                x >= 3 && x <= 12) body = 1;
                        }
                        else if (variant == 2)
                        {
                            if (dx * dx + dy * dy <= 33) body = 1;
                            if (y >= 9 && y <= 12 && x >= 3 && x <= 12) body = 1;
                            if (y == 13 && ((x + phase * 2) & 3) != 0 &&
                                x >= 4 && x <= 11) body = 1;
                        }
                        else
                        {
                            if (pacAbs(dx) + pacAbs(dy) <= 7) body = 1;
                            if (y >= 7 && y <= 11 && x >= 2 && x <= 13 &&
                                pacAbs(dx) <= 8 - pacAbs(y - 9)) body = 1;
                            if (y == 12 && ((x + phase + 1) & 2) &&
                                x >= 3 && x <= 12) body = 1;
                        }
                        if (!body)
                        {
                            if (dx * dx + dy * dy <= 45 ||
                                (y >= 6 && y <= 13 && x >= 1 && x <= 14))
                                pacActorPixel(frame, x, y, 3);
                            continue;
                        }
                        interior = 0;
                        if (x > 1 && x < 14 && y > 1 && y < 14)
                        {
                            if (variant == 2)
                                interior = dx * dx + dy * dy <= 20 && y < 10;
                            else
                                interior = x >= 4 && x <= 11 && y >= 5 && y <= 9;
                        }
                        if (!interior)
                            pacActorPixel(frame, x, y,
                                ((x + y + phase + variant) & 3) ? 5 : 6);
                    }
                }
                for (eye = 0; eye < 2; ++eye)
                {
                    eye_x = 5 + eye * 5;
                    eye_y = variant == 0 ? 6 : 7;
                    for (ey = -1; ey <= 1; ++ey)
                        for (ex = -1; ex <= 1; ++ex)
                            pacActorPixel(frame, eye_x + ex, eye_y + ey, 4);
                    pupil_x = eye_x + pac_dir_x[direction];
                    pupil_y = eye_y + pac_dir_y[direction];
                    pacActorPixel(frame, pupil_x, pupil_y, 2);
                }
                if (variant == 2)
                {
                    pacActorPixel(frame, 5, 10, 4);
                    pacActorPixel(frame, 7, 11 + phase, 4);
                    pacActorPixel(frame, 9, 10, 4);
                }
            }
        }
    }

    /* Two animated neon body links for the late-game ghost centipede.  The
       red classic ghost remains the expressive head; these articulated rings
       replace the other three classic bodies while they follow its history. */
    for (phase = 0; phase < 2; ++phase)
    {
        frame = PAC_FRAME_CENTIPEDE_BASE + phase;
        for (y = 1; y < 15; ++y)
        {
            for (x = 1; x < 15; ++x)
            {
                dx = x - 7;
                dy = y - 7;
                distance = dx * dx + dy * dy;
                if (distance < 15 || distance > 34) continue;
                if (((x + y + phase) & 3) == 0)
                    pacActorPixel(frame, x, y, 6);
                else
                    pacActorPixel(frame, x, y, 5);
            }
        }
        pacActorPixel(frame, 7, 7, 4);
        pacActorPixel(frame, 6 - phase, 7, 4);
        pacActorPixel(frame, 8 + phase, 7, 4);
    }
}

static void pacInitActorPalettes(void)
{
    int bank;
    int entry;
    for (bank = 0; bank <= PAC_GHOSTS; ++bank)
        for (entry = 0; entry < 16; ++entry)
            pac_actor_palettes[bank][entry] = 0;
    for (bank = 0; bank <= PAC_GHOSTS; ++bank)
    {
        pac_actor_palettes[bank][2] = PAC_RGB16(0, 8, 31);
        pac_actor_palettes[bank][4] = PAC_RGB16(31, 63, 31);
    }
    pac_actor_palettes[0][5] = PAC_RGB16(31, 63, 0);
    pac_actor_palettes[0][6] = PAC_RGB16(31, 63, 10);
    pac_actor_palettes[0][3] = PAC_RGB16(10, 18, 0);
    for (bank = 1; bank <= PAC_GHOSTS; ++bank)
    {
        pac_actor_palettes[bank][3] = pac_ghost_glows[bank - 1];
        pac_actor_palettes[bank][5] = pac_ghost_colours[bank - 1];
        pac_actor_palettes[bank][6] = pac_ghost_highlights[bank - 1];
    }
}

static int pacGhostIsQueued(PAC_Game *game, int ghost_index)
{
    int index;
    for (index = 0; index < game->ghost_queue_count; ++index)
        if (game->ghost_queue[index] == ghost_index) return 1;
    return 0;
}

static int pacGhostIsFrightened(PAC_Game *game, int ghost_index)
{
    if (game->frightened_ticks <= 0 || ghost_index == 6) return 0;
    return (game->ghost_recovered_mask & (1U << ghost_index)) == 0U;
}

static void pacLoadActorPalettes(PAC_Game *game, int frightened)
{
    int bank;
    int weak;
    for (bank = 1; bank <= PAC_GHOSTS; ++bank)
    {
        /* An eaten ghost reforms in its own colour immediately.  It remains
           boxed and harmless, but never respawns as another edible blue ghost. */
        weak = frightened && pacGhostIsFrightened(game, bank - 1) &&
            !pacGhostIsQueued(game, bank - 1);
        pac_actor_palettes[bank][3] = weak ?
            PAC_RGB16(0, 3, 10) : pac_ghost_glows[bank - 1];
        pac_actor_palettes[bank][5] = weak ?
            PAC_RGB16(0, 12, 31) : pac_ghost_colours[bank - 1];
        pac_actor_palettes[bank][6] = weak ?
            PAC_RGB16(31, 63, 31) : pac_ghost_highlights[bank - 1];
    }
    for (bank = 0; bank <= PAC_GHOSTS; ++bank)
        jsfLoadClut(pac_actor_palettes[bank], (short)(bank + 1), 16);
}

static int pacOpposite(int direction)
{
    if (direction == PAC_DIR_UP) return PAC_DIR_DOWN;
    if (direction == PAC_DIR_LEFT) return PAC_DIR_RIGHT;
    if (direction == PAC_DIR_DOWN) return PAC_DIR_UP;
    if (direction == PAC_DIR_RIGHT) return PAC_DIR_LEFT;
    return PAC_DIR_NONE;
}

static int pacCanMove(PAC_Actor *actor, int direction)
{
    int x = actor->tile_x + pac_dir_x[direction];
    int y = actor->tile_y + pac_dir_y[direction];
    return direction != PAC_DIR_NONE && pacMapOpen(x, y);
}

static int pacHouseCell(int x, int y)
{
    if (y >= PAC_HOUSE_DOOR_Y + 1 && y <= PAC_TUNNEL_Y &&
        x >= PAC_HOUSE_LEFT && x <= PAC_HOUSE_RIGHT) return 1;
    return y == PAC_HOUSE_DOOR_Y &&
        (x == (PAC_MAP_W >> 1) - 1 || x == (PAC_MAP_W >> 1));
}

static int pacPlayerCanMove(PAC_Actor *actor, int direction)
{
    int x = actor->tile_x + pac_dir_x[direction];
    int y = actor->tile_y + pac_dir_y[direction];
    return pacCanMove(actor, direction) && !pacHouseCell(x, y);
}

static int pacGhostCanMove(PAC_Game *game, int ghost_index, int direction)
{
    PAC_Actor *ghost = &game->ghosts[ghost_index];
    int x = ghost->tile_x + pac_dir_x[direction];
    int y = ghost->tile_y + pac_dir_y[direction];
    if (!pacCanMove(ghost, direction)) return 0;
    if (ghost->released && pacHouseCell(x, y)) return 0;
    return 1;
}

static void pacWrap(PAC_Actor *actor)
{
    if (actor->tile_y == PAC_TUNNEL_Y)
    {
        if (actor->tile_x < 0) actor->tile_x = PAC_MAP_W - 1;
        else if (actor->tile_x >= PAC_MAP_W) actor->tile_x = 0;
    }
}

static void pacResetActors(PAC_Game *game, int pal_mode)
{
    int index;
    int release_step = (pal_mode ? 150 : 180) - game->level * 2;
    int ghost_speed = (pal_mode ? 31 : 27) + game->level / 12;
    if (((game->level - 1) % 5) == 4) ++ghost_speed;
    if (release_step < (pal_mode ? 58 : 70))
        release_step = pal_mode ? 58 : 70;
    if (ghost_speed > (pal_mode ? 36 : 31))
        ghost_speed = pal_mode ? 36 : 31;
    game->player.tile_x = PAC_PLAYER_TILE_X;
    game->player.tile_y = PAC_PLAYER_TILE_Y;
    /* Start every life and level at rest. The first direction must come from
       the Jaguar D-pad; no synthetic opening move is queued. */
    game->player.direction = PAC_DIR_NONE;
    game->player.offset = 0;
    game->player.speed = pal_mode ? 38 : 32;
    game->player.released = 1;
    game->queued_direction = PAC_DIR_NONE;
    game->player_drive = 0;
    game->play_ticks = 0;
    game->siren_ticks = 1;
    game->ghost_cycle_ticks = 0;
    game->flipper_ticks = 144;
    game->flipper_surge_latched = 0;
    game->crawler_spawn_ticks = 330 - game->level * 3;
    if (game->crawler_spawn_ticks < 150)
        game->crawler_spawn_ticks = 150;
    game->storm_ticks = 480;
    game->storm_notice_ticks = 0;
    game->otto_notice_ticks = 0;
    game->prism_pressure = 0;
    game->prism_pressure_ticks = game->prism_pressure_limit;
    game->ghost_reentry_delay = (pal_mode ? 85 : 102) - game->level / 2;
    if (game->ghost_reentry_delay < (pal_mode ? 35 : 42))
        game->ghost_reentry_delay = pal_mode ? 35 : 42;
    game->ghost_queue_count = 0;
    game->next_ghost_release = 0;
    game->ghost_recovered_mask = 0U;
    game->hazard_ticks = 0;
    /* A lost life ends every temporary field and scoring chain. Inventory is
       retained, but no active effect may resume after the READY pause. */
    game->frightened_ticks = 0;
    game->frightened_audio_ticks = 0;
    game->frightened_chain = 0;
    game->combo = 1;
    game->combo_ticks = 0;
    game->magnet_ticks = 0;
    game->warp_cooldown = 0;
    game->dash_ticks = 0;
    game->dash_charges = 1;
    game->dash_recharge_ticks = game->dash_recharge_limit;
    game->dash_recharge_next = game->dash_recharge_limit;
    game->dash_recharge_segment = 6;
    game->ghost_stun_ticks = 0;
    game->shield_ticks = 0;
    game->energizer_flash_ticks = 0;
    game->dash_flash_ticks = 0;
    game->pulse_flash_ticks = 0;
    game->shield_flash_ticks = 0;
    game->score_popup_ticks = 0;
    game->pellet_fx_pending = 0;
    game->vector_lane_latched = 0;
    game->centipede_announced = 0;
    game->centipede_history_head = 0;
    game->centipede_history_count = 0;
    pacZero(game->centipede_history,
            sizeof(PAC_Actor) * PAC_CENTIPEDE_HISTORY);
    for (index = 0; index < PAC_GHOSTS; ++index)
    {
        game->ghosts[index].tile_x = PAC_HOUSE_LEFT + (index & 3);
        game->ghosts[index].tile_y = PAC_TUNNEL_Y - (index >> 2);
        game->ghosts[index].direction = (index & 1) ? PAC_DIR_RIGHT : PAC_DIR_LEFT;
        game->ghosts[index].offset = 0;
        game->ghosts[index].speed = index == 6 ? ghost_speed - 1 :
            ghost_speed + (index == 0 || index == 4 ? 1 : 0);
        game->ghosts[index].released = 0;
        game->ghost_release_at[index] =
            (unsigned int)(index * release_step);
    }
}

static void pacConfigureLevel(PAC_Game *game, int pal_mode)
{
    game->phase = PAC_PHASE_READY;
    game->ready_ticks = 0;
    game->ready_limit = pal_mode ? 50 : 60;
    game->ready_requires_input = game->level == PAC_LEVEL_DASH;
    game->frightened_ticks = 0;
    game->frightened_limit = (pal_mode ? 440 : 528) - game->level * 6;
    if (game->frightened_limit < (pal_mode ? 150 : 180))
        game->frightened_limit = pal_mode ? 150 : 180;
    game->chapter = (game->level - 1) / 5;
    if (game->chapter < 0) game->chapter = 0;
    if (game->chapter >= PAC_CHAPTERS) game->chapter = PAC_CHAPTERS - 1;
    pacGenerateFruitFrame(game->chapter);
    game->wall_scheme = game->level - 1;
    while (game->wall_scheme >= PAC_CHAPTERS)
        game->wall_scheme -= PAC_CHAPTERS;
    game->ghost_scatter_window = 420 - game->level * 4;
    if (game->ghost_scatter_window < 180)
        game->ghost_scatter_window = 180;
    game->ghost_accuracy = 72 + (game->level * 2) / 5;
    /* A new topology arrives every five levels; colour, pursuit pressure and
       timings still change on every individual level. */
    pac_active_map = game->chapter;
    game->active_ghosts = 4;
    if (game->level >= PAC_LEVEL_SPARK) ++game->active_ghosts;
    if (game->level >= PAC_LEVEL_WARP) ++game->active_ghosts;
    if (game->level >= PAC_LEVEL_DASH) ++game->active_ghosts;
    if (game->level >= PAC_LEVEL_SHIELD) ++game->active_ghosts;
    game->fruit_active = 0;
    game->fruit_ticks = 0;
    /* The landscape maze needs a generous, region-normalized pickup window.
       Each required fruit remains visible for about thirteen to fifteen
       seconds, then returns after a short cooldown if it was missed. */
    game->fruit_limit = (pal_mode ? 750 : 900) - game->level * 2;
    game->fruit_stage = 0;
    game->fruit_required = game->level <= 25 ? 2 : 3;
    game->fruit_collected = 0;
    game->fruit_respawn_ticks = 0;
    game->combo = 1;
    game->combo_ticks = 0;
    game->combo_limit = pal_mode ? 55 : 66;
    game->magnet_ticks = 0;
    game->prism_pressure = 0;
    game->prism_pressure_limit = pal_mode ? 200 : 240;
    game->prism_pressure_ticks = game->prism_pressure_limit;
    game->storm_notice_ticks = 0;
    game->otto_notice_ticks = 0;
    game->status_drawn = -1;
    game->warp_cooldown = 0;
    game->dash_ticks = 0;
    game->dash_recharge_limit = pal_mode ?
        PAC_DASH_RECHARGE_PAL : PAC_DASH_RECHARGE_NTSC;
    game->dash_recharge_step = game->dash_recharge_limit / 6;
    game->dash_recharge_ticks = game->dash_recharge_limit;
    game->dash_recharge_next = game->dash_recharge_limit;
    game->dash_recharge_segment = 6;
    game->dash_overdrive = 0;
    game->ghost_stun_ticks = 0;
    game->shield_ticks = 0;
    game->energizer_flash_ticks = 0;
    game->dash_flash_ticks = 0;
    game->pulse_flash_ticks = 0;
    game->shield_flash_ticks = 0;
    game->hazard_ticks = 0;
    game->pellet_fx_pending = 0;
    game->level_bonus_total = 0;
    game->level_bonus_remaining = 0;
    game->level_bonus_step = 0;
    game->frightened_chain = 0;
    game->frightened_audio_ticks = 0;
    game->frightened_warning_limit = pal_mode ?
        PAC_FRIGHTENED_WARNING_PAL : PAC_FRIGHTENED_WARNING_NTSC;
    pacResetPellets(game);
    game->initial_pellets = game->pellets_remaining;
    if (game->fruit_required == 2)
    {
        game->fruit_trigger_one = game->initial_pellets / 3;
        game->fruit_trigger_two = (game->initial_pellets * 2) / 3;
        game->fruit_trigger_three = game->initial_pellets;
    }
    else
    {
        game->fruit_trigger_one = game->initial_pellets / 4;
        game->fruit_trigger_two = game->initial_pellets / 2;
        game->fruit_trigger_three = (game->initial_pellets * 3) / 4;
    }
    pacResetActors(game, pal_mode);
}

static void pacGameReset(PAC_Game *game, int pal_mode)
{
    pacZero(game, sizeof(PAC_Game));
    game->score = 0;
    game->lives = 3;
    game->level = 1;
    game->high_score = pac_top_score;
    game->sound_enabled = pac_sound_enabled;
    game->random = 0x43a91d27U;
    game->score_eligible = 1;
    pacConfigureLevel(game, pal_mode);
    game->ready_requires_input = 1;
    /* Normal play starts with C1/B2 and one ready rechargeable Dash.
       Power pellets refill Dash/Shield and fruit replenishes Pulse. */
    game->dash_charges = 1;
    game->pulse_charges = 2;
    game->shield_charges = 1;
}

static void pacGameResetAtLevel(PAC_Game *game, int pal_mode, int level)
{
    pacZero(game, sizeof(PAC_Game));
    game->score = 0;
    game->lives = 3;
    game->level = level;
    game->high_score = pac_top_score;
    game->sound_enabled = pac_sound_enabled;
    game->random = 0x43a91d27U;
    game->score_eligible = level == 1;
    pacConfigureLevel(game, pal_mode);
    game->dash_charges = 1;
    game->pulse_charges = 2;
    game->shield_charges = 1;
}

static int pacPressedDigit(int pressed)
{
    if (pressed & JAGPAD_0) return 0;
    if (pressed & JAGPAD_1) return 1;
    if (pressed & JAGPAD_2) return 2;
    if (pressed & JAGPAD_3) return 3;
    if (pressed & JAGPAD_4) return 4;
    if (pressed & JAGPAD_5) return 5;
    if (pressed & JAGPAD_6) return 6;
    if (pressed & JAGPAD_7) return 7;
    if (pressed & JAGPAD_8) return 8;
    if (pressed & JAGPAD_9) return 9;
    return -1;
}

static int pacHandleCheatCode(PAC_Game *game, int pressed, int pal_mode)
{
    int digit;
    int secret_index;
    int secret_phase;
    digit = pacPressedDigit(pressed);
    if (digit < 0) return PAC_CODE_NONE;
#if PAC_ENABLE_UNLIMITED_SECRET
    /* The keypad is reserved for codes. C, B and A are the only gameplay
       action controls, so arcade codes remain valid during active play. */
    secret_phase = game->phase == PAC_PHASE_CREDITS ||
        game->phase == PAC_PHASE_TITLE || game->phase == PAC_PHASE_SCORES ||
        game->phase == PAC_PHASE_READY || game->phase == PAC_PHASE_PLAY ||
        game->phase == PAC_PHASE_PAUSED;
    if (!secret_phase && game->cheat_step >= 11)
        game->cheat_step = 0;
    if (secret_phase && game->cheat_step >= 11 &&
        game->cheat_step < 14)
    {
        secret_index = game->cheat_step - 10;
        if (digit == pac_unlimited_code[secret_index])
        {
            ++game->cheat_step;
            if (game->cheat_step < 14) return PAC_CODE_NONE;
            game->cheat_step = 0;
            pac_unlimited_specials = !pac_unlimited_specials;
            game->cheat_used = 1;
            game->score_eligible = 0;
            game->dash_charges = 1;
            game->dash_recharge_ticks = game->dash_recharge_limit;
            game->dash_recharge_next = game->dash_recharge_limit;
            game->dash_recharge_segment = 6;
            game->pulse_charges = 2;
            game->shield_charges = 1;
            game->dash_flash_ticks = 90;
            game->pulse_flash_ticks = 90;
            game->shield_flash_ticks = 90;
            return PAC_CODE_UNLIMITED;
        }
        game->cheat_step = digit == 2 ? 11 : (digit == 9 ? 1 : 0);
        return PAC_CODE_NONE;
    }
    if (game->cheat_step == 0 && digit == 2 && secret_phase)
    {
        game->cheat_step = 11;
        return PAC_CODE_NONE;
    }
#endif
#if PAC_ENABLE_LEVEL_SECRET
    if (game->cheat_step == 0)
    {
        game->cheat_step = digit == 9 ? 1 : 0;
        return PAC_CODE_NONE;
    }
    if (game->cheat_step == 1)
    {
        game->cheat_step = digit == 9 ? 2 : 0;
        game->cheat_level = 0;
        return PAC_CODE_NONE;
    }
    game->cheat_level = game->cheat_level * 10 + digit;
    ++game->cheat_step;
    if (game->cheat_step < 5) return PAC_CODE_NONE;
    game->cheat_step = 0;
    if (game->cheat_level < 1 || game->cheat_level > PAC_LEVEL_MAX)
        return PAC_CODE_NONE;
    digit = game->cheat_level;
    /* The fruit silhouette shares one writable actor frame. Latch the fruit
       object inactive before level configuration regenerates that frame. */
    pacSetFruitVisible(0);
    jsfVsync(0);
    pacGameResetAtLevel(game, pal_mode, digit);
    game->cheat_used = 1;
    game->score_eligible = 0;
    game->dash_flash_ticks = 90;
    game->pulse_flash_ticks = 90;
    game->shield_flash_ticks = 90;
    return PAC_CODE_LEVEL;
#else
    game->cheat_step = 0;
    return PAC_CODE_NONE;
#endif
}

static int pacDirectionFromBits(int bits)
{
    if (bits & JAGPAD_UP) return PAC_DIR_UP;
    if (bits & JAGPAD_LEFT) return PAC_DIR_LEFT;
    if (bits & JAGPAD_DOWN) return PAC_DIR_DOWN;
    if (bits & JAGPAD_RIGHT) return PAC_DIR_RIGHT;
    return PAC_DIR_NONE;
}

static int pacSingleHeldDirection(int held)
{
    int direction_bits = held &
        (JAGPAD_UP | JAGPAD_LEFT | JAGPAD_DOWN | JAGPAD_RIGHT);
    if (direction_bits == JAGPAD_UP) return PAC_DIR_UP;
    if (direction_bits == JAGPAD_LEFT) return PAC_DIR_LEFT;
    if (direction_bits == JAGPAD_DOWN) return PAC_DIR_DOWN;
    if (direction_bits == JAGPAD_RIGHT) return PAC_DIR_RIGHT;
    return PAC_DIR_NONE;
}

static void pacQueueDirection(PAC_Game *game, int held, int pressed)
{
    int direction = pacDirectionFromBits(pressed);
    game->player_drive = (held & PAC_DIRECTION_MASK) != 0;
    /* Some real-pad rolls change cardinal state without yielding a useful
       neutral/new-edge frame. A single held cardinal is therefore refreshed
       during play. With two cardinals held, only the newest edge may replace
       the queue, so an older direction cannot steal the intended turn. */
    if (direction == PAC_DIR_NONE)
        direction = pacSingleHeldDirection(held);
    /* READY also accepts a diagonal held before Start. */
    if (direction == PAC_DIR_NONE && game->phase == PAC_PHASE_READY)
        direction = pacDirectionFromBits(held);
    if (direction != PAC_DIR_NONE) game->queued_direction = direction;
}

static void pacApplyImmediateReverse(PAC_Game *game)
{
    PAC_Actor *player = &game->player;
    if (player->direction == PAC_DIR_NONE || player->offset == 0) return;
    if (game->queued_direction != pacOpposite(player->direction)) return;

    /* Rebase the same physical point onto the opposite end of the segment. */
    player->tile_x += pac_dir_x[player->direction];
    player->tile_y += pac_dir_y[player->direction];
    pacWrap(player);
    player->direction = game->queued_direction;
    player->offset = 256 - player->offset;
}

static void pacFxPublish(int x, int y, int count)
{
    int index;
    int admitted = 0;
    int slot;
    /* Seed already-free records directly. RAPTOR's VBI GPU remains the sole
       particle simulation and plotting owner during gameplay. */
    if (count > pac_fx_frame_remaining) count = pac_fx_frame_remaining;
    if (count > pac_fx_capacity_remaining)
        count = pac_fx_capacity_remaining;
    if (count < 0) count = 0;
    for (index = 0; index < count; ++index)
    {
        slot = pacParticleAcquireSlot();
        if (slot < 0) break;
        pacParticleSeedSlot(slot, x, y, pac_fx[index]);
        ++admitted;
    }
    pac_fx_frame_remaining -= admitted;
    pac_fx_capacity_remaining -= admitted;
}

static void pacFxBurst(int x, int y, int count, int base_angle,
                       int angle_step, int speed, int curve,
                       int colour, int decay, int life)
{
    int index;
    int particle_colour;
    if (x < 0 || x >= PAC_SCREEN_W || y < 0 || y >= PAC_SCREEN_H) return;
    if (count < 1) return;
    if (count > PAC_FX_MAX_INJECT) count = PAC_FX_MAX_INJECT;

    for (index = 0; index < count; ++index)
    {
        particle_colour = colour - (index & 3);
        if (particle_colour < 4) particle_colour = 4;
        pac_fx[index][0] = (base_angle + index * angle_step) & 511;
        pac_fx[index][1] = speed + (index & 1);
        pac_fx[index][2] = (index & 1) ? curve : -curve;
        pac_fx[index][3] = particle_colour;
        pac_fx[index][4] = decay + (index & 3) * 12;
        pac_fx[index][5] = life - (index & 3) * 2;
    }
    pacFxPublish(x, y, count);
}

static void pacDrawScore(unsigned char *canvas, PAC_Game *game)
{
    if (game->score_eligible && !game->cheat_used &&
        game->score > game->high_score)
    {
        game->high_score = game->score;
    }
    if (game->score != game->hud_score_drawn)
    {
        pacNumberChanged(canvas, 32, 5, game->score,
                         game->hud_score_drawn, 7, 4);
        game->hud_score_drawn = game->score;
    }
    if (game->high_score != game->hud_high_score_drawn)
    {
        pacNumberChanged(canvas, 125, 5, game->high_score,
                         game->hud_high_score_drawn, 7, 13);
        game->hud_high_score_drawn = game->high_score;
    }
    if (game->lives != game->hud_lives_drawn)
    {
        pacNumberChanged(canvas, 183, 5, game->lives,
                         game->hud_lives_drawn, 1, 13);
        game->hud_lives_drawn = game->lives;
    }
    if (game->combo != game->hud_combo_drawn)
    {
        pacDrawCombo(canvas, game);
        game->hud_combo_drawn = game->combo;
    }
}

static void pacPopupPixel(int x, int y, int colour)
{
    unsigned char *base = (unsigned char *)&PAC_popup_gfx;
    unsigned char *pixel;
    if ((unsigned int)x >= 64U || (unsigned int)y >= 16U) return;
    pixel = base + y * 32 + (x >> 1);
    if (x & 1)
        *pixel = (unsigned char)((*pixel & 0xf0) | (colour & 15));
    else
        *pixel = (unsigned char)((*pixel & 0x0f) | ((colour & 15) << 4));
}

static void pacPopupGlyph(int x, int glyph, int colour)
{
    int row;
    int column;
    int sx;
    int sy;
    if (glyph < 0 || glyph >= 36) return;
    for (row = 0; row < 7; ++row)
        for (column = 0; column < 5; ++column)
            if (pac_glyphs[glyph][row] & (16 >> column))
                for (sy = 0; sy < 2; ++sy)
                    for (sx = 0; sx < 2; ++sx)
                        pacPopupPixel(x + column * 2 + sx,
                                      1 + row * 2 + sy, colour);
}

static void pacShowScoreBonus(PAC_Game *game, int x, int y,
                              int value, int colour)
{
    int divisor;
    int digit;
    int index;
    if (x < 0) x = 0;
    if (x > PAC_SCREEN_W - 64) x = PAC_SCREEN_W - 64;
    if (y < 14) y = 14;
    if (y > PAC_SCREEN_H - 16) y = PAC_SCREEN_H - 16;
    game->score_popup_value = value;
    game->score_popup_ticks = 30;
    pacZero((void *)&PAC_popup_gfx, 512);
    for (index = 0; index < 10; ++index)
    {
        pacPopupPixel(6, 3 + index, colour);
        pacPopupPixel(7, 3 + index, colour);
        pacPopupPixel(2 + index, 7, colour);
        pacPopupPixel(2 + index, 8, colour);
    }
    divisor = 1000;
    for (index = 0; index < 4; ++index)
    {
        digit = (value / divisor) % 10;
        pacPopupGlyph(14 + index * 12, digit, colour);
        divisor /= 10;
    }
    sprite[PAC_OBJ_SCORE_POPUP].gfxbase =
        (unsigned int)(unsigned long)&PAC_popup_gfx;
    sprite[PAC_OBJ_SCORE_POPUP].CLUT = 0;
    sprite[PAC_OBJ_SCORE_POPUP].x = PAC_SCREEN_FIXED(x);
    sprite[PAC_OBJ_SCORE_POPUP].y = PAC_SCREEN_FIXED(y);
    sprite[PAC_OBJ_SCORE_POPUP].active = R_is_active;
}

static int pacCheckExtraLife(PAC_Game *game)
{
    int threshold;
    if (game->extra_life_stage == 0)
        threshold = PAC_EXTRA_LIFE_FIRST;
    else if (game->extra_life_stage == 1)
        threshold = PAC_EXTRA_LIFE_SECOND;
    else return 0;

    if (game->score < threshold) return 0;
    ++game->extra_life_stage;
    /* A milestone crossed at the five-life cap is consumed, not deferred
       until an unrelated scoring event after a later death. */
    if (game->lives >= PAC_MAX_LIVES) return 0;
    ++game->lives;
    return 1;
}

static void pacShowExtraLifePopup(PAC_Game *game, int x, int y)
{
    int index;
    if (x < 0) x = 0;
    if (x > PAC_SCREEN_W - 64) x = PAC_SCREEN_W - 64;
    if (y < 14) y = 14;
    if (y > PAC_SCREEN_H - 16) y = PAC_SCREEN_H - 16;
    game->score_popup_value = -1;
    game->score_popup_ticks = 75;
    pacZero((void *)&PAC_popup_gfx, 512);
    for (index = 0; index < 8; ++index)
    {
        pacPopupPixel(6, 4 + index, 15);
        pacPopupPixel(7, 4 + index, 15);
        pacPopupPixel(3 + index, 7, 15);
        pacPopupPixel(3 + index, 8, 15);
    }
    pacPopupGlyph(16, pacGlyphIndex('1'), 15);
    pacPopupGlyph(30, pacGlyphIndex('U'), 5);
    pacPopupGlyph(44, pacGlyphIndex('P'), 7);
    sprite[PAC_OBJ_SCORE_POPUP].gfxbase =
        (unsigned int)(unsigned long)&PAC_popup_gfx;
    sprite[PAC_OBJ_SCORE_POPUP].CLUT = 0;
    sprite[PAC_OBJ_SCORE_POPUP].x = PAC_SCREEN_FIXED(x);
    sprite[PAC_OBJ_SCORE_POPUP].y = PAC_SCREEN_FIXED(y);
    sprite[PAC_OBJ_SCORE_POPUP].active = R_is_active;
}

static void pacCelebrateExtraLife(PAC_Game *game, unsigned char *canvas,
                                  int x, int y)
{
    pacDrawScore(canvas, game);
    pacShowExtraLifePopup(game, x - 32, y - 8);
    pacFxBurst(x, y, 16, 0, 32, 5, 9, 15, 64, 54);
    pacPlaySound(game, PAC_SND_EXTRA_LIFE);
}

static void pacCollectFruit(PAC_Game *game, unsigned char *canvas)
{
    int center_x = PAC_MAP_LEFT + PAC_FRUIT_TILE_X * PAC_TILE_X +
        PAC_TILE_CENTER_X;
    int center_y = PAC_MAP_TOP + PAC_FRUIT_TILE_Y * PAC_TILE_Y +
        PAC_TILE_CENTER_Y;
    game->fruit_active = 0;
    game->fruit_ticks = 0;
    game->score += game->fruit_stage * 1000;
    ++game->fruit_collected;
    /* A late first pickup may occur after the next pellet milestone.  Keep
       each objective fruit as a distinct pickup instead of immediately
       spawning and auto-collecting the next one under Particle-Man. */
    if (game->fruit_collected < game->fruit_required)
        game->fruit_respawn_ticks = game->dash_recharge_limit;
    if (game->level >= PAC_LEVEL_FRUIT) game->magnet_ticks = 300;
    if (game->pulse_charges < 2)
        ++game->pulse_charges;
    game->pulse_flash_ticks = 90;
    if (game->fruit_collected >= game->fruit_required)
    {
        game->dash_overdrive = 1;
        game->dash_charges = 1;
        game->dash_recharge_ticks = game->dash_recharge_limit;
        game->dash_recharge_segment = 6;
        game->dash_flash_ticks = 120;
    }
    pacSetFruitVisible(0);
    pacDrawScore(canvas, game);
    pacShowScoreBonus(game, center_x - 15, center_y - 12,
                      game->fruit_stage * 1000, 7);
    pacDrawAbilityStatus(canvas, game);
    pacDrawStatusLine(canvas, game);
    pacFxBurst(center_x, center_y, 16, 0, 43, 3, 7, 11, 82, 48);
    pacPlaySound(game, PAC_SND_FRUIT);
    if (pacCheckExtraLife(game))
        pacCelebrateExtraLife(game, canvas, center_x, center_y);
}

static void pacUpdateFruit(PAC_Game *game, unsigned char *canvas)
{
    int center_x = PAC_MAP_LEFT + PAC_FRUIT_TILE_X * PAC_TILE_X +
        PAC_TILE_CENTER_X;
    int center_y = PAC_MAP_TOP + PAC_FRUIT_TILE_Y * PAC_TILE_Y +
        PAC_TILE_CENTER_Y;

    if (!game->fruit_active)
    {
        int eaten = game->initial_pellets - game->pellets_remaining;
        int trigger = game->fruit_collected == 0 ? game->fruit_trigger_one :
            (game->fruit_collected == 1 ? game->fruit_trigger_two :
             game->fruit_trigger_three);
        if (game->fruit_respawn_ticks > 0)
        {
            --game->fruit_respawn_ticks;
            return;
        }
        if (game->fruit_collected < game->fruit_required && eaten >= trigger &&
            (game->player.tile_x != PAC_FRUIT_TILE_X ||
             game->player.tile_y != PAC_FRUIT_TILE_Y))
        {
            game->fruit_stage = game->fruit_collected + 1;
            /* The shared actor slot is inactive here, so regenerating the
               next objective fruit cannot race the Object Processor. */
            pacGenerateFruitFrame((game->chapter +
                                  game->fruit_collected) % PAC_CHAPTERS);
            game->fruit_active = 1;
            game->fruit_ticks = game->fruit_limit;
            pacSetFruitVisible(1);
            pacFxBurst(center_x, center_y, 8, 0, 64, 2, 4, 11, 94, 32);
            pacPlaySound(game, PAC_SND_FRUIT);
        }
        return;
    }

    if (game->player.tile_x == PAC_FRUIT_TILE_X &&
        game->player.tile_y == PAC_FRUIT_TILE_Y)
    {
        pacCollectFruit(game, canvas);
        return;
    }

    --game->fruit_ticks;
    if (game->fruit_ticks <= 0)
    {
        game->fruit_active = 0;
        game->fruit_respawn_ticks = game->dash_recharge_limit;
        pacSetFruitVisible(0);
    }
}

static void pacConsumePelletAt(PAC_Game *game, unsigned char *canvas,
                               int x, int y)
{
    int pellet;
    int px;
    int py;
    int index;
    if ((unsigned int)x >= PAC_MAP_W || (unsigned int)y >= PAC_MAP_H) return;
    pellet = pac_pellets[y][x];
    if (pellet == 0) return;
    pac_pellets[y][x] = 0;
    --game->pellets_remaining;
    px = PAC_MAP_LEFT + x * PAC_TILE_X + PAC_TILE_CENTER_X;
    py = PAC_MAP_TOP + y * PAC_TILE_Y + PAC_TILE_CENTER_Y;
    if (pellet == 2)
        pacFill(canvas, px - 3, py - 3, 7, 7, 0);
    else
        pacFill(canvas, px - 2, py - 2, 5, 5, 0);
    if (game->hazard_ticks > 0 && x == game->hazard_x && y == game->hazard_y)
        pacDrawCrawlerHazard(canvas, game, 0);
    if (pellet != 2)
    {
        game->pellet_fx_pending = 8;
        game->pellet_fx_x = px;
        game->pellet_fx_y = py;
    }
    if (game->level >= PAC_LEVEL_SPARK)
    {
        if (game->combo_ticks > 0 && game->combo < 5) ++game->combo;
        else if (game->combo_ticks <= 0) game->combo = 1;
        game->combo_ticks = game->combo_limit;
    }
    else game->combo = 1;
    game->score += (pellet == 2 ? 50 : 10) * game->combo;
    pacDrawScore(canvas, game);
    if (pellet == 2)
    {
        game->pellet_fx_pending = 0;
        game->frightened_ticks = game->frightened_limit;
        game->frightened_audio_ticks = game->frightened_warning_limit >> 1;
        /* A new energizer affects every threat already on the maze. Ghosts
           currently reforming in the house retain their normal/dangerous state
           and may re-release during this same frightened window. */
        game->ghost_recovered_mask = 0U;
        for (index = 0; index < game->ghost_queue_count; ++index)
            game->ghost_recovered_mask |=
                1U << game->ghost_queue[index];
        game->energizer_flash_ticks = 48;
        game->frightened_chain = 0;
        game->dash_charges = 1;
        game->dash_recharge_ticks = game->dash_recharge_limit;
        game->dash_recharge_segment = 6;
        game->shield_charges = 1;
        game->dash_flash_ticks = 90;
        game->shield_flash_ticks = 90;
        pacDrawAbilityStatus(canvas, game);
        pacLoadActorPalettes(game, 1);
        pacShowScoreBonus(game, px - 15, py - 11,
                          50 * game->combo, 15);
        pacFxBurst(px, py, 16, 0, 32, 4, 6, 5, 72, 48);
        pacPlaySound(game, PAC_SND_POWER);
        if (game->active_ghosts >= 7)
        {
            game->otto_notice_ticks = 90;
            pacFxBurst(pacActorX(&game->ghosts[6]),
                       pacActorY(&game->ghosts[6]),
                       12, 16, 43, 3, -5, 8, 86, 36);
            pacPlaySound(game, PAC_SND_OTTO);
        }
    }
    else pacPlaySound(game,
        game->level >= PAC_LEVEL_SPARK && game->combo > 1 ?
        PAC_SND_COMBO : PAC_SND_DOT);
    if (pacCheckExtraLife(game))
        pacCelebrateExtraLife(game, canvas, px, py);
}

static void pacConsumePellet(PAC_Game *game, unsigned char *canvas)
{
    pacConsumePelletAt(game, canvas,
                       game->player.tile_x, game->player.tile_y);
}

static void pacChoosePlayer(PAC_Game *game)
{
    if (pacPlayerCanMove(&game->player, game->queued_direction))
        game->player.direction = game->queued_direction;
    else if (!pacPlayerCanMove(&game->player, game->player.direction))
        game->player.direction = PAC_DIR_NONE;
}

static void pacApplyCornerAssist(PAC_Game *game)
{
    PAC_Actor *player = &game->player;
    PAC_Actor center;
    int direction = game->queued_direction;
    if (!game->player_drive || direction == PAC_DIR_NONE) return;
    if (direction == player->direction ||
        direction == pacOpposite(player->direction)) return;

    center = *player;
    if (player->offset >= 256 - PAC_TURN_GRACE)
    {
        /* The requested turn arrived just before the next junction. */
        center.tile_x += pac_dir_x[player->direction];
        center.tile_y += pac_dir_y[player->direction];
        pacWrap(&center);
    }
    else if (player->offset > PAC_TURN_GRACE)
    {
        return;
    }
    /* A request just after a junction returns to its center before turning.
       This makes narrow vertical lanes practical without changing the map. */
    center.offset = 0;
    if (!pacPlayerCanMove(&center, direction)) return;

    player->tile_x = center.tile_x;
    player->tile_y = center.tile_y;
    player->offset = 0;
    player->direction = direction;
}

static void pacPlayerStep(PAC_Game *game, unsigned char *canvas)
{
    int movement = game->player.speed;
    int remaining;
    if (!game->player_drive) return;
    if (game->dash_ticks > 0) movement += game->player.speed;
    if (game->player.offset == 0)
    {
        pacConsumePellet(game, canvas);
        pacChoosePlayer(game);
    }
    while (movement > 0 && game->player.direction != PAC_DIR_NONE)
    {
        remaining = 256 - game->player.offset;
        if (movement < remaining)
        {
            game->player.offset += movement;
            movement = 0;
        }
        else
        {
            movement -= remaining;
            game->player.tile_x += pac_dir_x[game->player.direction];
            game->player.tile_y += pac_dir_y[game->player.direction];
            pacWrap(&game->player);
            game->player.offset = 0;
            pacConsumePellet(game, canvas);
            pacChoosePlayer(game);
        }
    }
}

static void pacMagnetStep(PAC_Game *game, unsigned char *canvas,
                          unsigned int frame)
{
    int direction;
    int x;
    int y;
    if (game->magnet_ticks <= 0 || (frame & 7U) != 0) return;
    for (direction = PAC_DIR_UP; direction <= PAC_DIR_RIGHT; ++direction)
    {
        x = game->player.tile_x + pac_dir_x[direction];
        y = game->player.tile_y + pac_dir_y[direction];
        if ((unsigned int)x >= PAC_MAP_W || (unsigned int)y >= PAC_MAP_H)
            continue;
        if (pac_pellets[y][x] != 1) continue;
        pacConsumePelletAt(game, canvas, x, y);
        return;
    }
}

static void pacWarpStep(PAC_Game *game)
{
    int x;
    int y;
    if (game->level < PAC_LEVEL_WARP || game->player.offset != 0) return;
    if (game->warp_cooldown > 0) return;
    x = game->player.tile_x;
    y = game->player.tile_y;
    if (x == PAC_WARP_A_X && y == PAC_WARP_A_Y)
    {
        game->player.tile_x = PAC_WARP_B_X;
        game->player.tile_y = PAC_WARP_B_Y;
    }
    else if (x == PAC_WARP_B_X && y == PAC_WARP_B_Y)
    {
        game->player.tile_x = PAC_WARP_A_X;
        game->player.tile_y = PAC_WARP_A_Y;
    }
    else return;
    game->warp_cooldown = 24;
    pacFxBurst(pacActorX(&game->player), pacActorY(&game->player),
               16, 0, 32, 3, 8, 15, 72, 40);
    pacPlaySound(game, PAC_SND_WARP);
}

static void pacUpdateThreatWarnings(PAC_Game *game)
{
    if (game->level >= PAC_LEVEL_SPARK &&
        game->ghosts[4].released && !pacGhostIsQueued(game, 4) &&
        game->ghost_stun_ticks == 0 &&
        !pacGhostIsFrightened(game, 4) &&
        game->player.tile_y == PAC_TUNNEL_Y)
    {
        if (!game->vector_lane_latched)
        {
            game->vector_lane_latched = 1;
            pacFxBurst(pacActorX(&game->ghosts[4]),
                       pacActorY(&game->ghosts[4]),
                       10, 0, 51, 3, 5, 12, 94, 32);
            pacPlaySound(game, PAC_SND_VECTOR);
        }
    }
    else game->vector_lane_latched = 0;
}

static void pacUpdateGhostAlarm(PAC_Game *game)
{
    unsigned int cycle;
    if (game->level < PAC_LEVEL_ALARM) return;
    cycle = (unsigned int)game->ghost_cycle_ticks;
    if (cycle != 0U &&
        cycle != (unsigned int)game->ghost_scatter_window) return;
    pacFxBurst(PAC_MAP_LEFT + (PAC_MAP_W >> 1) * PAC_TILE_X,
               PAC_MAP_TOP + PAC_HOUSE_DOOR_Y * PAC_TILE_Y,
               16, cycle == 0U ? 0 : 256, 32, 4, 12, 9, 70, 46);
    pacPlaySound(game, PAC_SND_ALARM);
}

static void pacUpdatePrismPressure(PAC_Game *game)
{
    if (game->level < PAC_LEVEL_PULSE || game->ghost_stun_ticks > 0) return;
    --game->prism_pressure_ticks;
    if (game->prism_pressure_ticks > 0) return;
    game->prism_pressure_ticks = game->prism_pressure_limit;
    if (game->prism_pressure < 3) ++game->prism_pressure;
    pacFxBurst(pacActorX(&game->player), pacActorY(&game->player),
               8, 0, 64, 2, 13, 9, 94, 30);
    pacPlaySound(game, PAC_SND_ALARM);
}

static unsigned int pacRandom(PAC_Game *game)
{
    /* A shift/XOR generator avoids the 68000 runtime's 32-bit multiply
       helper. Enemy decisions only need a quick, repeatable game seed. */
    game->random ^= game->random << 13;
    game->random ^= game->random >> 17;
    game->random ^= game->random << 5;
    return game->random;
}

static int pacRandomChoice(PAC_Game *game, int count)
{
    unsigned int choice;
    if (count <= 1) return 0;
    if (count == 2) return (int)(pacRandom(game) & 1U);
    if (count >= 4) return (int)(pacRandom(game) & 3U);
    /* Legal direction counts are at most three here. Rejecting the fourth
       two-bit value keeps the choice even without a software modulo call. */
    do choice = pacRandom(game) & 3U;
    while (choice >= 3U);
    return (int)choice;
}

static int pacGhostCrowdPenalty(PAC_Game *game, int ghost_index,
                                int tile_x, int tile_y)
{
    int index;
    int distance;
    int penalty = 0;
    for (index = 0; index < game->active_ghosts; ++index)
    {
        if (index == ghost_index || !game->ghosts[index].released) continue;
        if (game->level >= PAC_LEVEL_CENTIPEDE && ghost_index == 0 &&
            index < PAC_CLASSIC_GHOSTS) continue;
        distance = pacAbs(tile_x - game->ghosts[index].tile_x) +
            pacAbs(tile_y - game->ghosts[index].tile_y);
        if (distance == 0) penalty += 96;
        else if (distance == 1) penalty += 40;
        else if (distance == 2) penalty += 14;
    }
    return penalty;
}

static int pacGhostPressurePenalty(PAC_Game *game, int ghost_index,
                                   int tile_x, int tile_y)
{
    int index;
    int nearby = 0;
    int candidate_distance = pacAbs(tile_x - game->player.tile_x) +
        pacAbs(tile_y - game->player.tile_y);

    /* Eight actors may exist in the late campaign, but they must never behave
       as eight simultaneous hunters. Once two threats are already close, the
       remaining actors prefer the outside of the encounter instead of sealing
       every exit around Particle-Man. The rotating classic hunter keeps its
       pressure role; the articulated centipede is treated as one threat. */
    if (ghost_index == ((game->level - 1) & 3)) return 0;
    for (index = 0; index < game->active_ghosts; ++index)
    {
        int distance;
        if (index == ghost_index || !game->ghosts[index].released) continue;
        if (game->level >= PAC_LEVEL_CENTIPEDE &&
            index > 0 && index < PAC_CLASSIC_GHOSTS) continue;
        distance = pacAbs(game->ghosts[index].tile_x - game->player.tile_x) +
            pacAbs(game->ghosts[index].tile_y - game->player.tile_y);
        if (distance <= 5) ++nearby;
    }
    if (nearby >= 3 && candidate_distance <= 6) return 768;
    if (nearby >= 2 && candidate_distance <= 3) return 384;
    return 0;
}

static void pacTargetAhead(int *x, int *y, int direction, int distance)
{
    if (direction == PAC_DIR_UP) *y -= distance;
    else if (direction == PAC_DIR_LEFT) *x -= distance;
    else if (direction == PAC_DIR_DOWN) *y += distance;
    else if (direction == PAC_DIR_RIGHT) *x += distance;
}

static int pacGhostDirection(PAC_Game *game, int ghost_index)
{
    PAC_Actor *ghost = &game->ghosts[ghost_index];
    int order[4] = {PAC_DIR_UP, PAC_DIR_LEFT, PAC_DIR_DOWN, PAC_DIR_RIGHT};
    int target_x = game->player.tile_x;
    int target_y = game->player.tile_y;
    int player_direction = game->player.direction;
    int reverse = pacOpposite(ghost->direction);
    int best_direction = PAC_DIR_NONE;
    int best_distance = 0x7fffffff;
    int valid = 0;
    int index;
    int direction;
    int nx;
    int ny;
    int dx;
    int dy;
    int distance;
    int choice;
    int accuracy;
    int hunter = (game->level - 1) & 3;
    int scatter = game->ghost_cycle_ticks < game->ghost_scatter_window;

    if (!ghost->released)
    {
        target_x = (PAC_MAP_W >> 1) - 1 + (ghost_index & 1);
        target_y = PAC_HOUSE_DOOR_Y - 1;
    }
    else if (ghost_index == 1)
    {
        pacTargetAhead(&target_x, &target_y, player_direction, 4);
    }
    else if (ghost_index == 2)
    {
        target_x = target_x + target_x - game->ghosts[0].tile_x;
        target_y = target_y + target_y - game->ghosts[0].tile_y;
    }
    else if (ghost_index == 3)
    {
        dx = ghost->tile_x - target_x;
        dy = ghost->tile_y - target_y;
        if (pacDistanceSquared(dx, dy) < 64)
        {
            target_x = 1;
            target_y = PAC_MAP_H - 2;
        }
    }
    else if (ghost_index == 4)
    {
        /* VECTOR WRAITH: predicts much farther ahead and deliberately aims
           through the wrap tunnel whenever Particle-Man enters that row. */
        pacTargetAhead(&target_x, &target_y, player_direction, 7);
        if (game->player.tile_y == PAC_TUNNEL_Y)
        {
            target_x = game->player.tile_x < (PAC_MAP_W >> 1) ?
                -4 : PAC_MAP_W + 3;
            target_y = PAC_TUNNEL_Y;
        }
    }
    else if (ghost_index == 5)
    {
        /* CRAWLER SHADE stays close enough for its temporary corridor hazard
           to matter without simply duplicating the red ghost. */
        pacTargetAhead(&target_x, &target_y, player_direction, -2);
    }
    else if (ghost_index == 6)
    {
        /* OTTO ECHO never scatters and never becomes edible. */
        pacTargetAhead(&target_x, &target_y, player_direction, 2);
    }
    else if (ghost_index == 7)
    {
        /* FLIPPER PHANTOM attacks the lane beside the player's heading, then
           surges when it achieves horizontal or vertical alignment. */
        if (player_direction == PAC_DIR_UP) target_x -= 5;
        else if (player_direction == PAC_DIR_LEFT) target_y += 5;
        else if (player_direction == PAC_DIR_DOWN) target_x += 5;
        else if (player_direction == PAC_DIR_RIGHT) target_y -= 5;
    }

    if (game->level >= PAC_LEVEL_CENTIPEDE && ghost_index > 0 &&
        ghost_index < PAC_CLASSIC_GHOSTS &&
        game->frightened_ticks == 0)
    {
        target_x = game->ghosts[ghost_index - 1].tile_x;
        target_y = game->ghosts[ghost_index - 1].tile_y;
        pacTargetAhead(&target_x, &target_y, player_direction,
                       4 - ghost_index);
    }
    if (scatter && ghost->released && ghost_index < PAC_CLASSIC_GHOSTS &&
        ghost_index != hunter)
    {
        target_x = (ghost_index & 1) ? PAC_MAP_W - 2 : 1;
        target_y = (ghost_index & 2) ? PAC_MAP_H - 2 : 1;
    }

    for (index = 0; index < 4; ++index)
        if (order[index] != reverse &&
            pacGhostCanMove(game, ghost_index, order[index])) ++valid;

    if (pacGhostIsFrightened(game, ghost_index) && valid > 0)
    {
        choice = pacRandomChoice(game, valid);
        for (index = 0; index < 4; ++index)
        {
            direction = order[index];
            if (direction == reverse ||
                !pacGhostCanMove(game, ghost_index, direction)) continue;
            if (choice-- == 0) return direction;
        }
    }


    /* Keep routing mistakes bounded so each target personality stays legible;
       release, speed and mode pressure provide the remaining difficulty. */
    accuracy = game->ghost_accuracy;
    /* One classic hunter carries the immediate pressure. The role rotates
       every level, beginning with red on level one, instead of making the
       whole opening group uniformly unfair. */
    if (ghost_index == hunter) accuracy += 8;
    if (accuracy > 96) accuracy = 96;
    /* Convert percentage to a 0..255 threshold using shifts only. */
    accuracy = (accuracy << 1) + (accuracy >> 1) + (accuracy >> 4);
    if (valid > 0 && !pacGhostIsFrightened(game, ghost_index) &&
        (int)((pacRandom(game) >> 24) & 255U) >= accuracy)
    {
        choice = pacRandomChoice(game, valid);
        for (index = 0; index < 4; ++index)
        {
            direction = order[index];
            if (direction == reverse ||
                !pacGhostCanMove(game, ghost_index, direction)) continue;
            if (choice-- == 0) return direction;
        }
    }

    for (index = 0; index < 4; ++index)
    {
        direction = order[index];
        if (valid > 0 && direction == reverse) continue;
        if (!pacGhostCanMove(game, ghost_index, direction)) continue;
        nx = ghost->tile_x + pac_dir_x[direction];
        ny = ghost->tile_y + pac_dir_y[direction];
        dx = nx - target_x;
        dy = ny - target_y;
        /* The lookup preserves the original squared-distance routing without
           two software 32-bit multiplies at every candidate junction. */
        distance = pacDistanceSquared(dx, dy) +
            pacGhostCrowdPenalty(game, ghost_index, nx, ny) +
            pacGhostPressurePenalty(game, ghost_index, nx, ny);
        if (distance < best_distance)
        {
            best_distance = distance;
            best_direction = direction;
        }
    }
    return best_direction;
}

static void pacGhostStep(PAC_Game *game, int ghost_index)
{
    PAC_Actor *ghost = &game->ghosts[ghost_index];
    int movement = ghost->speed;
    int remaining;
    int forced_flip = 0;
    int aligned = 0;
    movement += game->prism_pressure;
    if (ghost_index == ((game->level - 1) & 3) && game->level >= 6)
        ++movement;
    if (ghost_index == 7 && ghost->released && game->flipper_ticks > 0)
        --game->flipper_ticks;
    if (ghost_index == 7 && ghost->released)
        aligned = ghost->tile_x == game->player.tile_x ||
                  ghost->tile_y == game->player.tile_y;
    if (ghost_index == 7 && aligned)
    {
        movement += ghost->speed / 4;
        if (!game->flipper_surge_latched)
        {
            game->flipper_surge_latched = 1;
            pacFxBurst(pacActorX(ghost), pacActorY(ghost),
                       10, 0, 64, 3, 5, 14, 92, 28);
            pacPlaySound(game, PAC_SND_FLIPPER);
        }
    }
    else if (ghost_index == 7)
        game->flipper_surge_latched = 0;
    if (ghost_index == 7 && ghost->released && ghost->offset == 0 &&
        game->flipper_ticks <= 0 &&
        pacGhostCanMove(game, ghost_index, pacOpposite(ghost->direction)))
    {
        game->flipper_ticks = 181;
        ghost->direction = pacOpposite(ghost->direction);
        forced_flip = 1;
        pacFxBurst(pacActorX(ghost), pacActorY(ghost),
                   12, 0, 43, 3, 7, 14, 86, 34);
        pacPlaySound(game, PAC_SND_FLIPPER);
    }
    if (ghost->offset == 0 && !forced_flip)
        ghost->direction = pacGhostDirection(game, ghost_index);
    while (movement > 0 && ghost->direction != PAC_DIR_NONE)
    {
        remaining = 256 - ghost->offset;
        if (movement < remaining)
        {
            ghost->offset += movement;
            movement = 0;
        }
        else
        {
            movement -= remaining;
            ghost->tile_x += pac_dir_x[ghost->direction];
            ghost->tile_y += pac_dir_y[ghost->direction];
            pacWrap(ghost);
            ghost->offset = 0;
            ghost->direction = pacGhostDirection(game, ghost_index);
        }
    }
    if (!ghost->released && ghost->tile_y < PAC_HOUSE_DOOR_Y)
        ghost->released = 1;
}

static void pacRecordCentipedeHead(PAC_Game *game)
{
    game->centipede_history[game->centipede_history_head] = game->ghosts[0];
    ++game->centipede_history_head;
    if (game->centipede_history_head >= PAC_CENTIPEDE_HISTORY)
        game->centipede_history_head = 0;
    if (game->centipede_history_count < PAC_CENTIPEDE_HISTORY)
        ++game->centipede_history_count;
}

static void pacCentipedeStep(PAC_Game *game)
{
    int index;
    int delay;
    int history_index;
    pacGhostStep(game, 0);
    pacRecordCentipedeHead(game);

    for (index = 1; index < PAC_CLASSIC_GHOSTS; ++index)
    {
        if (pacGhostIsQueued(game, index)) continue;
        delay = index * PAC_CENTIPEDE_SPACING;
        if (game->centipede_history_count <= delay) continue;
        history_index = game->centipede_history_head - 1 - delay;
        while (history_index < 0) history_index += PAC_CENTIPEDE_HISTORY;
        game->ghosts[index] = game->centipede_history[history_index];
    }

    if (game->ghosts[0].released && !game->centipede_announced)
    {
        game->centipede_announced = 1;
        pacFxBurst(pacActorX(&game->ghosts[0]),
                   pacActorY(&game->ghosts[0]),
                   16, 0, 32, 4, 6, 13, 78, 44);
        pacPlaySound(game, PAC_SND_CENTIPEDE);
    }

    for (index = PAC_CLASSIC_GHOSTS;
         index < game->active_ghosts; ++index)
        if (game->play_ticks >= game->ghost_release_at[index])
            pacGhostStep(game, index);
}

static int pacActorX(PAC_Actor *actor)
{
    int tile = actor->tile_x;
    int travel = ((actor->offset << 3) + (actor->offset << 2) +
                  actor->offset) >> 8;
    int x = PAC_MAP_LEFT + (tile << 3) + (tile << 2) + tile +
        PAC_TILE_CENTER_X;
    if (actor->direction == PAC_DIR_LEFT) x -= travel;
    else if (actor->direction == PAC_DIR_RIGHT) x += travel;
    return x;
}

static int pacActorY(PAC_Actor *actor)
{
    int y = PAC_MAP_TOP + (actor->tile_y << 3) + PAC_TILE_CENTER_Y;
    int travel = actor->offset >> 5;
    if (actor->direction == PAC_DIR_UP) y -= travel;
    else if (actor->direction == PAC_DIR_DOWN) y += travel;
    return y;
}

static int pacGhostGazeDirection(PAC_Actor *ghost, int player_x, int player_y,
                                 int ghost_x, int ghost_y)
{
    int dx = player_x - ghost_x;
    int dy = player_y - ghost_y;
    if (pacAbs(dx) > pacAbs(dy))
        return dx < 0 ? PAC_DIR_LEFT : PAC_DIR_RIGHT;
    if (dy != 0) return dy < 0 ? PAC_DIR_UP : PAC_DIR_DOWN;
    return ghost->direction == PAC_DIR_NONE ? PAC_DIR_LEFT : ghost->direction;
}

static void pacUpdateCrawlerHazard(PAC_Game *game, unsigned char *canvas,
                                   unsigned int frame)
{
    PAC_Actor *crawler;
    int cadence;
    int dx;
    int dy;
    if (game->active_ghosts < 6) return;
    crawler = &game->ghosts[5];
    cadence = 330 - game->level * 3;
    if (cadence < 150) cadence = 150;
    if (game->hazard_ticks > 0)
    {
        --game->hazard_ticks;
        if (game->hazard_ticks <= 0)
            pacClearCrawlerHazard(canvas, game);
        else if ((frame & 7U) == 0U)
            pacDrawCrawlerHazard(canvas, game, frame);
        return;
    }
    /* Existing fields remain dangerous until their marked expiry, but a
       stunned or frightened Crawler cannot create a new attack. */
    if (game->ghost_stun_ticks > 0 || pacGhostIsFrightened(game, 5)) return;
    if (game->crawler_spawn_ticks > 0)
    {
        --game->crawler_spawn_ticks;
        return;
    }
    game->crawler_spawn_ticks = cadence;
    if (!crawler->released || game->play_ticks == 0) return;
    dx = crawler->tile_x - game->player.tile_x;
    dy = crawler->tile_y - game->player.tile_y;
    if (pacAbs(dx) + pacAbs(dy) < 4) return;
    game->hazard_x = crawler->tile_x;
    game->hazard_y = crawler->tile_y;
    game->hazard_ticks = 105;
    pacDrawCrawlerHazard(canvas, game, frame);
    pacFxBurst(pacActorX(crawler), pacActorY(crawler),
               16, 0, 32, 3, 6, 11, 106, 38);
    pacPlaySound(game, PAC_SND_CRAWLER);
}

static void pacUpdateParticleStorm(PAC_Game *game, unsigned int frame)
{
    int index;
    int reverse;
    PAC_Actor *ghost;
    if (game->level < PAC_LEVEL_STORM) return;
    --game->storm_ticks;
    if (game->storm_ticks == 60)
    {
        pacFxBurst(pacActorX(&game->player), pacActorY(&game->player),
                   8, (int)(frame * 5U), 64, 2, 9, 13, 94, 28);
        pacPlaySound(game, PAC_SND_ALARM);
    }
    if (game->storm_ticks > 0) return;

    game->storm_ticks = 480;
    game->storm_notice_ticks = 60;
    for (index = 0; index < game->active_ghosts; ++index)
    {
        ghost = &game->ghosts[index];
        reverse = pacOpposite(ghost->direction);
        if (ghost->released && !pacGhostIsQueued(game, index) &&
            pacGhostCanMove(game, index, reverse))
            ghost->direction = reverse;
    }
    pacFxBurst(pacActorX(&game->player), pacActorY(&game->player), 10,
               (int)(frame * 9U), 51, 4, 11, 15, 82, 34);
    pacPlaySound(game, PAC_SND_STORM);
}

static int pacWakeMode(PAC_Game *game, unsigned int frame)
{
    int phase = (int)((frame >> 3) & 7U);
    /* When states overlap, each active field receives a recurring display slot
       instead of being permanently hidden by one fixed priority order. */
    if ((phase == 0 || phase == 5) && game->dash_ticks > 0)
        return PAC_WAKE_MODE_DASH;
    if ((phase == 1 || phase == 7) && game->shield_ticks > 0)
        return PAC_WAKE_MODE_SHIELD;
    if ((phase == 2 || phase == 6) && game->ghost_stun_ticks > 0)
        return PAC_WAKE_MODE_PULSE;
    if (phase == 3 && game->magnet_ticks > 0)
        return PAC_WAKE_MODE_MAGNET;
    if (phase == 4 && game->frightened_ticks > 0)
        return PAC_WAKE_MODE_POWER;
    if (game->dash_ticks > 0) return PAC_WAKE_MODE_DASH;
    if (game->shield_ticks > 0) return PAC_WAKE_MODE_SHIELD;
    if (game->ghost_stun_ticks > 0) return PAC_WAKE_MODE_PULSE;
    if (game->magnet_ticks > 0) return PAC_WAKE_MODE_MAGNET;
    if (game->frightened_ticks > 0) return PAC_WAKE_MODE_POWER;
    return PAC_WAKE_MODE_NORMAL;
}

static void pacUpdateWakeObjects(PAC_Game *game, unsigned int frame)
{
    unsigned int base = (unsigned int)(unsigned long)&PAC_wake_gfx;
    PAC_Actor *actor;
    int direction = game->player.direction;
    int phase = (int)((frame >> 2) & 1U);
    int mode;
    int index;
    int object;
    int field_active = game->dash_ticks > 0 || game->shield_ticks > 0 ||
        game->ghost_stun_ticks > 0 || game->magnet_ticks > 0 ||
        game->frightened_ticks > 0;

    if ((!game->player_drive || direction == PAC_DIR_NONE) && !field_active)
        sprite[PAC_OBJ_PLAYER_WAKE].active = R_is_inactive;
    else
    {
        if (direction == PAC_DIR_NONE) direction = PAC_DIR_LEFT;
        mode = pacWakeMode(game, frame);
        sprite[PAC_OBJ_PLAYER_WAKE].gfxbase = base +
            (mode * 8 + (direction - 1) * 2 + phase) *
            PAC_WAKE_FRAME_BYTES;
        sprite[PAC_OBJ_PLAYER_WAKE].CLUT = 1;
        sprite[PAC_OBJ_PLAYER_WAKE].x =
            PAC_SCREEN_FIXED(pacActorX(&game->player) - 16);
        sprite[PAC_OBJ_PLAYER_WAKE].y =
            PAC_SCREEN_FIXED(pacActorY(&game->player) - 16);
        sprite[PAC_OBJ_PLAYER_WAKE].active = R_is_active;
    }

    for (index = 0; index < PAC_GHOSTS; ++index)
    {
        object = PAC_OBJ_GHOST_WAKE_0 + index;
        actor = &game->ghosts[index];
        direction = actor->direction;
        if (index >= game->active_ghosts || !actor->released ||
            pacGhostIsQueued(game, index) || direction == PAC_DIR_NONE)
        {
            sprite[object].active = R_is_inactive;
            continue;
        }
        sprite[object].gfxbase = base +
            (PAC_WAKE_GHOST_BASE + (direction - 1) * 2 + phase) *
            PAC_WAKE_FRAME_BYTES;
        sprite[object].CLUT = 2 + index;
        sprite[object].x = PAC_SCREEN_FIXED(pacActorX(actor) - 16);
        sprite[object].y = PAC_SCREEN_FIXED(pacActorY(actor) - 16);
        sprite[object].active = R_is_active;
    }
}

static void pacUpdatePickupGlint(PAC_Game *game)
{
    int phase;
    if (game->pellet_fx_pending <= 0)
    {
        sprite[PAC_OBJ_PICKUP_GLINT].active = R_is_inactive;
        return;
    }
    phase = (8 - game->pellet_fx_pending) >> 1;
    if (phase < 0) phase = 0;
    if (phase >= PAC_PICKUP_FRAMES) phase = PAC_PICKUP_FRAMES - 1;
    sprite[PAC_OBJ_PICKUP_GLINT].gfxbase =
        (unsigned int)(unsigned long)&PAC_pickup_gfx +
        phase * PAC_PICKUP_FRAME_BYTES;
    sprite[PAC_OBJ_PICKUP_GLINT].CLUT = 0;
    sprite[PAC_OBJ_PICKUP_GLINT].x =
        PAC_SCREEN_FIXED(game->pellet_fx_x - 7);
    sprite[PAC_OBJ_PICKUP_GLINT].y =
        PAC_SCREEN_FIXED(game->pellet_fx_y - 7);
    sprite[PAC_OBJ_PICKUP_GLINT].active = R_is_active;
    --game->pellet_fx_pending;
}

static void pacSetActorsActive(int active)
{
    int index;
    for (index = PAC_OBJ_PLAYER; index <= PAC_OBJ_GHOST_7; ++index)
        sprite[index].active = active ? R_is_active : R_is_inactive;
    if (!active)
    {
        sprite[PAC_OBJ_PLAYER_WAKE].active = R_is_inactive;
        for (index = PAC_OBJ_GHOST_WAKE_0;
             index <= PAC_OBJ_GHOST_WAKE_7; ++index)
            sprite[index].active = R_is_inactive;
        sprite[PAC_OBJ_PICKUP_GLINT].active = R_is_inactive;
    }
}

static void pacUpdateActors(PAC_Game *game, unsigned int frame)
{
    unsigned int actor_base = (unsigned int)(unsigned long)&PAC_actor_gfx;
    int direction = game->player.direction;
    int player_frame;
    int player_bank;
    int player_x;
    int player_y;
    int ghost_x;
    int ghost_y;
    int gaze_direction;
    int index;
    PAC_Actor *actor;

    if (direction == PAC_DIR_NONE) direction = PAC_DIR_LEFT;
    player_bank = game->frightened_ticks > 0 ? PAC_FRAME_POWER_BASE : 0;
    player_frame = player_bank + (direction - 1) * 2 +
        ((frame >> 3) & 1);
    player_x = pacActorX(&game->player);
    player_y = pacActorY(&game->player);
    sprite[PAC_OBJ_PLAYER].gfxbase = actor_base +
        player_frame * PAC_ACTOR_FRAME_BYTES;
    sprite[PAC_OBJ_PLAYER].x = PAC_SCREEN_FIXED(player_x - 8);
    sprite[PAC_OBJ_PLAYER].y = PAC_SCREEN_FIXED(player_y - 8);

    for (index = 0; index < game->active_ghosts; ++index)
    {
        actor = &game->ghosts[index];
        ghost_x = pacActorX(actor);
        ghost_y = pacActorY(actor);
        gaze_direction = pacGhostGazeDirection(actor, player_x, player_y,
                                               ghost_x, ghost_y);
        if (game->level >= PAC_LEVEL_CENTIPEDE &&
            game->ghost_queue_count == 0 &&
            index > 0 && index < PAC_CLASSIC_GHOSTS)
            sprite[PAC_OBJ_GHOST_0 + index].gfxbase = actor_base +
                (PAC_FRAME_CENTIPEDE_BASE + ((frame >> 3) & 1)) *
                PAC_ACTOR_FRAME_BYTES;
        else
            sprite[PAC_OBJ_GHOST_0 + index].gfxbase = actor_base +
                ((index < PAC_CLASSIC_GHOSTS ? PAC_FRAME_GHOST_BASE :
                  PAC_FRAME_ECHO_BASE + (index - PAC_CLASSIC_GHOSTS) * 8) +
                 (gaze_direction - 1) * 2 +
                 ((frame >> 3) & 1)) * PAC_ACTOR_FRAME_BYTES;
        sprite[PAC_OBJ_GHOST_0 + index].x = PAC_SCREEN_FIXED(ghost_x - 8);
        sprite[PAC_OBJ_GHOST_0 + index].y = PAC_SCREEN_FIXED(ghost_y - 8);
        sprite[PAC_OBJ_GHOST_0 + index].active = R_is_active;
    }
    for (; index < PAC_GHOSTS; ++index)
        sprite[PAC_OBJ_GHOST_0 + index].active = R_is_inactive;
}

static void pacQueueGhost(PAC_Game *game, int ghost_index)
{
    int index;
    for (index = 0; index < game->ghost_queue_count; ++index)
        if (game->ghost_queue[index] == ghost_index) return;
    if (game->ghost_queue_count < PAC_GHOSTS)
        game->ghost_queue[game->ghost_queue_count++] = ghost_index;
    if (game->frightened_ticks > 0)
        game->ghost_recovered_mask |= 1U << ghost_index;
    if (game->ghost_queue_count == 1)
        game->next_ghost_release = (int)game->play_ticks +
            game->ghost_reentry_delay;
}

static void pacReleaseQueuedGhost(PAC_Game *game)
{
    int ghost_index;
    int index;
    if (game->ghost_queue_count <= 0) return;
    if ((int)game->play_ticks < game->next_ghost_release) return;
    ghost_index = game->ghost_queue[0];
    for (index = 1; index < game->ghost_queue_count; ++index)
        game->ghost_queue[index - 1] = game->ghost_queue[index];
    --game->ghost_queue_count;
    if (game->ghost_queue_count == 0)
    {
        game->centipede_announced = 0;
    }
    game->ghost_release_at[ghost_index] = game->play_ticks;
    game->next_ghost_release = (int)game->play_ticks +
        game->ghost_reentry_delay;
}

static void pacUseActions(PAC_Game *game, unsigned char *canvas, int pressed)
{
    int x = pacActorX(&game->player);
    int y = pacActorY(&game->player);
    int dash_direction = game->player.direction == PAC_DIR_NONE ?
        game->queued_direction : game->player.direction;
    int dash_unlimited = pac_unlimited_specials || game->dash_overdrive;
    int changed = 0;
    if ((pressed & JAGPAD_A) &&
        (dash_unlimited || game->dash_charges > 0) &&
        (game->dash_ticks == 0 || dash_unlimited) && game->player_drive &&
        dash_direction != PAC_DIR_NONE)
    {
        if (!dash_unlimited)
        {
            game->dash_charges = 0;
            game->dash_recharge_ticks = 0;
            game->dash_recharge_next = game->dash_recharge_step;
            game->dash_recharge_segment = 0;
        }
        changed = 1;
        game->dash_ticks = 14;
        pacFxBurst(x, y, 12, pac_trail_angle[dash_direction],
                   43, 4, 4, 15, 82, 32);
        pacPlaySound(game, PAC_SND_DASH);
    }
    else if ((pressed & JAGPAD_B) &&
        (pac_unlimited_specials || game->pulse_charges > 0) &&
        game->ghost_stun_ticks == 0)
    {
        if (!pac_unlimited_specials) --game->pulse_charges;
        changed = 1;
        /* A 150-update hold creates a readable opening without making Pulse
           equivalent to an entire frightened period. */
        game->ghost_stun_ticks = 150;
        game->prism_pressure = 0;
        game->prism_pressure_ticks = game->prism_pressure_limit;
        pacFxBurst(x, y, 16, 0, 32, 5, 0, 13, 66, 42);
        pacPlaySound(game, PAC_SND_PULSE);
    }
    else if ((pressed & JAGPAD_C) &&
        (pac_unlimited_specials || game->shield_charges > 0) &&
        game->shield_ticks == 0)
    {
        if (!pac_unlimited_specials) --game->shield_charges;
        changed = 1;
        game->shield_ticks = 180;
        pacFxBurst(x, y, 16, 16, 32, 2, 9, 12, 104, 54);
        pacPlaySound(game, PAC_SND_SHIELD);
    }
    if (changed) pacDrawAbilityStatus(canvas, game);
}

static int pacCheckCollisions(PAC_Game *game, unsigned char *canvas)
{
    int player_x = pacActorX(&game->player);
    int player_y = pacActorY(&game->player);
    int index;
    int ghost_x;
    int ghost_y;
    PAC_Actor *ghost;
    if (game->hazard_ticks > 0 &&
        game->player.tile_x == game->hazard_x &&
        game->player.tile_y == game->hazard_y)
    {
        pacClearCrawlerHazard(canvas, game);
        game->hazard_ticks = 0;
        if (game->shield_ticks > 0)
        {
            game->shield_ticks = 0;
            pacDrawAbilityStatus(canvas, game);
            pacFxBurst(player_x, player_y, 16, 0, 32, 4, 9,
                       15, 76, 48);
            pacPlaySound(game, PAC_SND_SHIELD);
            return 0;
        }
        else
        {
            pacFxBurst(player_x, player_y, 16, 0, 32, 4, 6,
                       11, 72, 54);
            --game->lives;
            pacDrawScore(canvas, game);
            pacPlaySound(game, game->lives <= 0 ?
                         PAC_SND_GAME_OVER : PAC_SND_DEATH);
            return game->lives <= 0 ? 2 : 1;
        }
    }
    for (index = 0; index < game->active_ghosts; ++index)
    {
        ghost = &game->ghosts[index];
        ghost_x = pacActorX(ghost);
        ghost_y = pacActorY(ghost);
        if (pacAbs(player_x - ghost_x) < 5 &&
            pacAbs(player_y - ghost_y) < 5)
        {
            if (pacGhostIsFrightened(game, index))
            {
                int capture_score;
                capture_score = 200 << game->frightened_chain;
                if (game->frightened_chain < 3) ++game->frightened_chain;
                game->score += capture_score;
                pacDrawScore(canvas, game);
                pacShowScoreBonus(game, ghost_x - 15, ghost_y - 12,
                    capture_score, 15);
                pacFxBurst(ghost_x, ghost_y,
                           16, index * 37, 32, 3, 7, 15, 84, 42);
                ghost->tile_x = PAC_HOUSE_LEFT + (index & 3);
                ghost->tile_y = PAC_TUNNEL_Y - (index >> 2);
                ghost->direction = (index & 1) ? PAC_DIR_RIGHT : PAC_DIR_LEFT;
                ghost->offset = 0;
                ghost->released = 0;
                game->ghost_release_at[index] = 0xffffffffU;
                pacQueueGhost(game, index);
                pacLoadActorPalettes(game, 1);
                pacPlaySound(game, PAC_SND_GHOST);
                if (pacCheckExtraLife(game))
                    pacCelebrateExtraLife(game, canvas,
                                          player_x, player_y);
            }
            else if (game->shield_ticks > 0)
            {
                game->shield_ticks = 0;
                pacDrawAbilityStatus(canvas, game);
                ghost->tile_x = PAC_HOUSE_LEFT + (index & 3);
                ghost->tile_y = PAC_TUNNEL_Y - (index >> 2);
                ghost->direction = PAC_DIR_UP;
                ghost->offset = 0;
                ghost->released = 0;
                game->ghost_release_at[index] = 0xffffffffU;
                pacQueueGhost(game, index);
                pacFxBurst(player_x, player_y, 16, 0, 32, 4, 11,
                           15, 70, 50);
                pacPlaySound(game, PAC_SND_SHIELD);
                return 0;
            }
            else
            {
                pacFxBurst(player_x, player_y,
                           16, 0, 32, 4, 0, 15, 64, 58);
                --game->lives;
                pacDrawScore(canvas, game);
                pacPlaySound(game, game->lives <= 0 ?
                             PAC_SND_GAME_OVER : PAC_SND_DEATH);
                return game->lives <= 0 ? 2 : 1;
            }
        }
    }
    return 0;
}

static int pacEnergizerFlashStage(PAC_Game *game)
{
    if (game->phase != PAC_PHASE_PLAY) return 0;
    if (game->energizer_flash_ticks >= 41) return 1;
    if (game->energizer_flash_ticks >= 31) return 2;
    if (game->energizer_flash_ticks >= 23) return 1;
    if (game->energizer_flash_ticks >= 1) return 3;
    return 0;
}

static void pacUpdateMazeGlow(PAC_Game *game, unsigned int frame)
{
    int scheme = game->wall_scheme;
    int flash_stage;
    /* Presentation screens share one stable authored palette. */
    if (game->phase == PAC_PHASE_CREDITS ||
        game->phase == PAC_PHASE_TITLE ||
        game->phase == PAC_PHASE_SCORES) scheme = 7;
    else if (game->phase == PAC_PHASE_END) scheme = 8;
    /* Actors, outlines and particles carry frightened state; wall mass and
       unused space remain true black. */
    pac_palette[0] = 0;
    pac_palette[1] = pac_maze_glow[scheme][0];
    pac_palette[2] = pac_maze_glow[scheme][1];
    pac_palette[3] = pac_maze_glow[scheme][2];
    if (game->phase == PAC_PHASE_PLAY ||
        game->phase == PAC_PHASE_READY ||
        game->phase == PAC_PHASE_PAUSED ||
        game->phase == PAC_PHASE_LEVEL_CLEAR)
        pac_palette[8] = (frame & 16U) ?
            PAC_RGB16(31, 34, 0) : PAC_RGB16(31, 52, 6);
    else
        pac_palette[8] = PAC_RGB16(31, 46, 4);
    flash_stage = pacEnergizerFlashStage(game);
    if (flash_stage > 0)
    {
        pac_palette[1] = pac_energizer_glow[flash_stage - 1][0];
        pac_palette[2] = pac_energizer_glow[flash_stage - 1][1];
        pac_palette[3] = pac_energizer_glow[flash_stage - 1][2];
        pac_palette[8] = flash_stage == 2 ? PAC_RGB16(0, 63, 31) :
            (flash_stage == 3 ? PAC_RGB16(31, 63, 0) :
             PAC_RGB16(31, 63, 31));
    }
    if (game->phase == PAC_PHASE_READY)
        pac_palette[6] = PAC_RGB16(31, 20, 31);
    else if (game->phase == PAC_PHASE_CREDITS ||
             game->phase == PAC_PHASE_TITLE ||
             game->phase == PAC_PHASE_SCORES || game->phase == PAC_PHASE_END)
        pac_palette[6] = PAC_RGB16(31, 24, 8);
    else pac_palette[6] = 0;
}

static void pacUpdatePowerPalette(PAC_Game *game, unsigned int frame)
{
    int flash_stage;
    if (game->phase == PAC_PHASE_CREDITS ||
        game->phase == PAC_PHASE_TITLE ||
        game->phase == PAC_PHASE_SCORES ||
        game->phase == PAC_PHASE_END) return;
    flash_stage = pacEnergizerFlashStage(game);
    if ((frame & 7U) != 0 && flash_stage == pac_actor_flash_stage) return;
    pac_actor_flash_stage = flash_stage;
    if (flash_stage == 1)
    {
        pac_actor_palettes[0][5] = PAC_RGB16(31, 63, 31);
        pac_actor_palettes[0][6] = PAC_RGB16(31, 63, 31);
    }
    else if (flash_stage == 2)
    {
        pac_actor_palettes[0][5] = PAC_RGB16(0, 56, 31);
        pac_actor_palettes[0][6] = PAC_RGB16(24, 63, 31);
    }
    else if (flash_stage == 3)
    {
        pac_actor_palettes[0][5] = PAC_RGB16(31, 44, 0);
        pac_actor_palettes[0][6] = PAC_RGB16(31, 63, 15);
    }
    else if (game->frightened_ticks > 0)
    {
        pac_actor_palettes[0][5] = pac_pulse[(frame >> 3) & 7U];
        pac_actor_palettes[0][6] = PAC_RGB16(31, 63, 31);
    }
    else
    {
        pac_actor_palettes[0][5] = PAC_RGB16(31, 63, 0);
        pac_actor_palettes[0][6] = PAC_RGB16(31, 63, 10);
    }
    jsfLoadClut(pac_actor_palettes[0], 1, 16);
}

static void pacUpdateMainHighlight(PAC_Game *game, unsigned int frame)
{
    int flash_stage = pacEnergizerFlashStage(game);
    if (flash_stage == 2)
        pac_palette[15] = PAC_RGB16(12, 63, 31);
    else if (flash_stage == 3)
        pac_palette[15] = PAC_RGB16(31, 63, 8);
    else if (flash_stage == 1)
        pac_palette[15] = PAC_RGB16(31, 63, 31);
    else if (game->phase == PAC_PHASE_PLAY && game->frightened_ticks > 0)
        pac_palette[15] = pac_pulse[(frame >> 3) & 7U];
    else
        pac_palette[15] = PAC_RGB16(31, 63, 31);
}

static void pacLoadMainPaletteNow(PAC_Game *game, unsigned int frame)
{
    /* Publish transition colors before the VBL-latched canvas pointer. */
    pacUpdateMazeGlow(game, frame);
    pacUpdateMainHighlight(game, frame);
    jsfLoadClut(pac_palette, 0, 16);
}

static void pacPrepareObjects(void)
{
    unsigned int actor_base = (unsigned int)(unsigned long)&PAC_actor_gfx;
    unsigned int wake_base = (unsigned int)(unsigned long)&PAC_wake_gfx;
    int index;
    sprite[PAC_OBJ_CANVAS].gfxbase = (unsigned int)(unsigned long)&PAC_canvas_a;
    /* Attract mode has no particle events. Gameplay enables this transparent
       OP layer when a run starts; RAPTOR's fixed GPU scan remains unchanged. */
    sprite[PAC_OBJ_GPU_PARTICLES].active = R_is_inactive;
    sprite[PAC_OBJ_PLAYER_WAKE].gfxbase = wake_base;
    sprite[PAC_OBJ_PLAYER_WAKE].CLUT = 1;
    sprite[PAC_OBJ_PLAYER_WAKE].active = R_is_inactive;
    for (index = 0; index < PAC_GHOSTS; ++index)
    {
        sprite[PAC_OBJ_GHOST_WAKE_0 + index].gfxbase = wake_base +
            PAC_WAKE_GHOST_BASE * PAC_WAKE_FRAME_BYTES;
        sprite[PAC_OBJ_GHOST_WAKE_0 + index].CLUT = 2 + index;
        sprite[PAC_OBJ_GHOST_WAKE_0 + index].active = R_is_inactive;
    }
    sprite[PAC_OBJ_PICKUP_GLINT].gfxbase =
        (unsigned int)(unsigned long)&PAC_pickup_gfx;
    sprite[PAC_OBJ_PICKUP_GLINT].CLUT = 0;
    sprite[PAC_OBJ_PICKUP_GLINT].active = R_is_inactive;
    sprite[PAC_OBJ_PLAYER].CLUT = 1;
    sprite[PAC_OBJ_PLAYER].gfxbase = actor_base;
    for (index = 0; index < PAC_GHOSTS; ++index)
    {
        sprite[PAC_OBJ_GHOST_0 + index].CLUT = 2 + index;
        sprite[PAC_OBJ_GHOST_0 + index].gfxbase = actor_base +
            (index < PAC_CLASSIC_GHOSTS ? PAC_FRAME_GHOST_BASE :
             PAC_FRAME_ECHO_BASE + (index - PAC_CLASSIC_GHOSTS) * 8) *
            PAC_ACTOR_FRAME_BYTES;
    }
    sprite[PAC_OBJ_FRUIT].CLUT = 0;
    sprite[PAC_OBJ_FRUIT].gfxbase = actor_base +
        PAC_FRAME_FRUIT * PAC_ACTOR_FRAME_BYTES;
    sprite[PAC_OBJ_FRUIT].x = PAC_SCREEN_FIXED(
        PAC_MAP_LEFT + PAC_FRUIT_TILE_X * PAC_TILE_X +
        PAC_TILE_CENTER_X - 8);
    sprite[PAC_OBJ_FRUIT].y = PAC_SCREEN_FIXED(
        PAC_MAP_TOP + PAC_FRUIT_TILE_Y * PAC_TILE_Y +
        PAC_TILE_CENTER_Y - 8);
    pacSetFruitVisible(0);
    sprite[PAC_OBJ_SCORE_POPUP].CLUT = 0;
    sprite[PAC_OBJ_SCORE_POPUP].gfxbase =
        (unsigned int)(unsigned long)&PAC_popup_gfx;
    sprite[PAC_OBJ_SCORE_POPUP].active = R_is_inactive;
    sprite[PAC_OBJ_SENTINEL].active = R_is_active;
    pacSetActorsActive(0);
}

static void pacSetParticlesActive(int active)
{
    sprite[PAC_OBJ_GPU_PARTICLES].active =
        active ? R_is_active : R_is_inactive;
}

static void pacBuildPaused(unsigned char *buffer, PAC_Game *game)
{
    pacBuildPlayfield(buffer, game);
    pacFill(buffer, 87, 79, 146, 49, 0);
    pacTubeBox(buffer, 87, 79, 146, 49);
    pacCenteredText(buffer, 89, "PAUSED", 3, 9);
    pacCenteredText(buffer, 115, "PAUSE OR OPTION RESUMES", 1, 4);
}

static void pacDrawLevelBonusValue(unsigned char *buffer, PAC_Game *game,
                                   unsigned int frame)
{
    int added = game->level_bonus_total - game->level_bonus_remaining;
    int colour = (frame & 8U) ? 15 : 13;
    pacFill(buffer, 111, 112, 98, 16, 0);
    pacCenteredText(buffer, 112, "PARTICLE BONUS", 1, colour);
    pacText(buffer, 136, 121, "+", 1, 8);
    pacNumber(buffer, 144, 121, added, 6, colour);
}

static void pacTickLevelBonus(unsigned char *buffer, PAC_Game *game,
                              unsigned int frame)
{
    int award;
    int new_extra_life;
    int changed = 0;
    if (game->level_bonus_remaining > 0 &&
        (game->level_clear_ticks & 1) == 0)
    {
        award = game->level_bonus_step;
        if (award > game->level_bonus_remaining)
            award = game->level_bonus_remaining;
        game->score += award;
        game->level_bonus_remaining -= award;
        changed = 1;
        pacDrawScore(buffer, game);
        new_extra_life = pacCheckExtraLife(game);
        if (new_extra_life)
        {
            pacCelebrateExtraLife(game, buffer,
                PAC_SCREEN_W >> 1, 144);
        }
    }
    if ((game->level_clear_ticks & 7) == 0 || changed)
        pacDrawLevelBonusValue(buffer, game, frame);
}

static void pacBeginLevelClear(unsigned char *buffer, PAC_Game *game,
                               int pal_mode)
{
    int retained_extra_life = game->score_popup_ticks > 0 &&
        game->score_popup_value == -1;
    game->phase = PAC_PHASE_LEVEL_CLEAR;
    game->level_clear_ticks = 0;
    game->level_clear_limit = pal_mode ? 110 : 132;
    game->level_bonus_total = 1000 + game->level * 100;
    game->level_bonus_remaining = game->level_bonus_total;
    game->level_bonus_step = (game->level_bonus_total + 299) / 300 * 10;
    if (game->level_bonus_step < 10) game->level_bonus_step = 10;
    pacDrawScore(buffer, game);
    pacSetFruitVisible(0);
    pacSetActorsActive(0);
    pacFill(buffer, 65, 76, 190, 58, 0);
    pacTubeBox(buffer, 65, 76, 190, 58);
    pacCenteredText(buffer, 84, "LEVEL CLEAR", 2, 5);
    pacDrawLevelBonusValue(buffer, game, 0);
    if (retained_extra_life)
        pacShowExtraLifePopup(game, (PAC_SCREEN_W - 64) >> 1, 139);
    else
        sprite[PAC_OBJ_SCORE_POPUP].active = R_is_inactive;
    pacFxBurst(PAC_SCREEN_W >> 1, 106, 16, 0, 32,
               5, 9, 15, 58, 64);
    pacPlaySound(game, PAC_SND_CLEAR);
}

static void pacBuildEnd(unsigned char *buffer, PAC_Game *game)
{
    pacZero(buffer, PAC_CLUT4_BYTES);
    pacDrawPresentationGrid(buffer);
    pacCenteredText(buffer, 22, "PARTICLE-MAN", 2, 5);
    pacCenteredText(buffer, 63, "LEVEL 50 CLEARED", 2, 13);
    if (game->cheat_used)
    {
        pacCenteredText(buffer, 98, "CHEAT MODE WON", 3, 9);
        pacCenteredText(buffer, 139, "NO JAGUAR MASTER FOR YOU", 1, 8);
        pacCenteredText(buffer, 153, "TRY AGAIN WITHOUT CODES", 1, 12);
    }
    else
    {
        pacCenteredText(buffer, 105, "THE END", 4, 9);
        pacCenteredText(buffer, 151, "JAGUAR MASTER", 2, 12);
    }
    pacCenteredText(buffer, 176, "FINAL SCORE", 1, 4);
    pacNumber(buffer, 139, 191, game->score, 7, 5);
    pacCenteredText(buffer, 210, "TOLBAT GAMES (TM)", 1, 8);
}

static void pacStartRun(PAC_Game *game, unsigned char *canvas,
                        int pal_mode, int start_level)
{
    /* Hide the shared fruit/actor bank before level configuration regenerates
       its chapter frame. This also makes reset transitions latch cleanly. */
    pacSetFruitVisible(0);
    pacSetActorsActive(0);
    if (start_level <= 1)
        pacGameReset(game, pal_mode);
    else
        pacGameResetAtLevel(game, pal_mode, start_level);
    if (pac_unlimited_specials)
    {
        game->cheat_used = 1;
        game->score_eligible = 0;
    }
    pacBuildPlayfield(canvas, game);
    pacLoadMainPaletteNow(game, 0);
    pacLoadActorPalettes(game, 0);
    sprite[PAC_OBJ_SCORE_POPUP].active = R_is_inactive;
    pacSetParticlesActive(1);
    pacPlaySound(game, PAC_SND_READY);
}

static void pacPresentCanvas(unsigned char **active_canvas,
                             unsigned char **spare_canvas)
{
    unsigned char *old_canvas = *active_canvas;
    sprite[PAC_OBJ_CANVAS].gfxbase =
        (unsigned int)(unsigned long)(*spare_canvas);
    /* RAPTOR copies the changed object state on VBL. Waiting here is essential:
       the old page is still being scanned until this boundary has completed. */
    jsfVsync(0);
    *active_canvas = *spare_canvas;
    *spare_canvas = old_canvas;
}

static void pacShowPreparedCanvas(unsigned char *prepared_canvas)
{
    /* Attract cards are generated once during the startup sequence. Switching
       cards changes only Object 0's phrase-aligned source and waits for its
       normal VBL latch; no visible card is used as a loading screen. */
    sprite[PAC_OBJ_CANVAS].gfxbase =
        (unsigned int)(unsigned long)prepared_canvas;
    jsfVsync(0);
}

static void pacToggleSound(PAC_Game *game)
{
    game->sound_enabled ^= 1;
    pac_sound_enabled = game->sound_enabled;
    if (!game->sound_enabled) pacSilence();
    else pacPlaySound(game, PAC_SND_DOT);
}

void basicmain(void)
{
    unsigned char *canvas_a = (unsigned char *)&PAC_canvas_a;
    unsigned char *canvas_b = (unsigned char *)&PAC_canvas_b;
    unsigned char *particle_surface = (unsigned char *)&RAPTOR_particle_gfx;
    unsigned char *credits_canvas = (unsigned char *)&PAC_credits_gfx;
    unsigned char *title_canvas = (unsigned char *)&PAC_title_gfx;
    unsigned char *scores_canvas = (unsigned char *)&PAC_scores_gfx;
    unsigned char *active_canvas = canvas_a;
    unsigned char *spare_canvas = canvas_b;
    unsigned char *game_canvas = canvas_b;
    PAC_Game game;
    unsigned int frame = 0;
    int held;
    int pressed;
    int previous = 0;
    int pal_mode = rapNTSCFlag == 0;
    int collision;
    int index;
    int cheat_before;
    int code_result;
    int level_jump;
    int ability_status_changed;
    int current_palette_flash;
    int palette_animation_due;
    int palette_phase = -1;

    /* Do not rely on loader/BSS state for the public default. Every cold boot
       begins with finite C/B/A inventory until 2600 is deliberately entered. */
    pac_unlimited_specials = 0;

    /* Establish a deterministic first image before boot-time PCM synthesis.
       This replaces the uninitialized-looking hardware load interval with an
       intentional Jaguar startup card. */
    pacZero(canvas_a, PAC_CLUT4_BYTES);
    pacZero(canvas_b, PAC_CLUT4_BYTES);
    pacZero(particle_surface, PAC_CLUT4_BYTES);
    pacZero((void *)&PAC_popup_gfx, 512);
    pacZero((void *)&PAC_wake_gfx,
            PAC_WAKE_FRAMES * PAC_WAKE_FRAME_BYTES);
    pacZero((void *)&PAC_pickup_gfx,
            PAC_PICKUP_FRAMES * PAC_PICKUP_FRAME_BYTES);
    pacZero((void *)&PAC_sentinel_gfx, 8);
    pacSetParticlesActive(0);
    pacBuildBoot(canvas_a);
    jsfLoadClut(pac_palette, 0, 16);
    jsfVsync(0);

    pacGenerateSounds(canvas_a);
    pacLoadScores();
    pacGameReset(&game, pal_mode);
    game.phase = PAC_PHASE_CREDITS;
    game.presentation_ticks = 0;
    game.presentation_limit = pal_mode ? 250 : 300;
    pacBuildCredits(credits_canvas);
    pacDrawBootProgress(canvas_a, 25);
    jsfVsync(0);
    pacBuildTitle(title_canvas, 0);
    pacDrawBootProgress(canvas_a, 26);
    jsfVsync(0);
    pacBuildScores(scores_canvas);
    pacDrawBootProgress(canvas_a, 27);
    jsfVsync(0);
    pacLoadMainPaletteNow(&game, 0);
    pacGenerateActors();
    pacDrawBootProgress(canvas_a, 28);
    jsfVsync(0);
    pacGenerateWakes();
    pacDrawBootProgress(canvas_a, 29);
    jsfVsync(0);
    pacInitActorPalettes();
    pacLoadActorPalettes(&game, 0);
    pacDrawBootProgress(canvas_a, 30);
    jsfVsync(0);
    jsfLoadClut((unsigned short *)pac_particle_palette, 15, 16);
    pacPrepareObjects();
    pacInitParticlePool();
    pacDrawBootProgress(canvas_a, 31);
    jsfVsync(0);
    jsfLoadClut(pac_palette, 0, 16);
    pacShowPreparedCanvas(credits_canvas);

    for (;;)
    {
        jsfVsync(0);
        pacBeginFxFrame();
        held = jsfGetPad(LEFT_PAD);
        pressed = held & ~previous;
        previous = held;

        cheat_before = game.cheat_step;
        code_result = pacHandleCheatCode(&game, pressed, pal_mode);
        level_jump = code_result == PAC_CODE_LEVEL;
        if (level_jump)
        {
            pacSetActorsActive(0);
            pacSetFruitVisible(0);
            sprite[PAC_OBJ_SCORE_POPUP].active = R_is_inactive;
            pacBuildPlayfield(spare_canvas, &game);
            pacLoadMainPaletteNow(&game, frame);
            pacLoadActorPalettes(&game, 0);
            pacPresentCanvas(&active_canvas, &spare_canvas);
            game_canvas = active_canvas;
            pacSetParticlesActive(1);
            pacPlaySound(&game, PAC_SND_READY);
        }
        else if (code_result == PAC_CODE_UNLIMITED)
        {
            if (game.phase == PAC_PHASE_SCORES)
            {
                pacDrawScoresStatus(scores_canvas);
            }
            else if (game.phase == PAC_PHASE_CREDITS ||
                     game.phase == PAC_PHASE_TITLE)
            {
                /* Immediate feedback without building or flipping a screen. */
                pacFill(game.phase == PAC_PHASE_CREDITS ? credits_canvas :
                        title_canvas, 54, 204, 212, 11, 0);
                pacCenteredText(game.phase == PAC_PHASE_CREDITS ?
                    credits_canvas : title_canvas, 206, pac_unlimited_specials ?
                    "SPECIALS UNLIMITED" : "SPECIALS LIMITED", 1,
                    pac_unlimited_specials ? 5 : 14);
                pacDrawScoresStatus(scores_canvas);
            }
            else if (game.phase == PAC_PHASE_READY ||
                     game.phase == PAC_PHASE_PLAY ||
                     game.phase == PAC_PHASE_PAUSED)
            {
                pacDrawAbilityStatus(game_canvas, &game);
                pacDrawScoresStatus(scores_canvas);
            }
            pacPlaySound(&game, PAC_SND_PULSE);
        }
        else if ((pressed & JAGPAD_8) && cheat_before < 2)
        {
            pacToggleSound(&game);
        }

        if (pressed & (JAGPAD_STAR | JAGPAD_HASH))
        {
            pacSubmitScore(&game);
            pac_unlimited_specials = 0;
            pacSetFruitVisible(0);
            pacSetActorsActive(0);
            pacSetParticlesActive(0);
            sprite[PAC_OBJ_SCORE_POPUP].active = R_is_inactive;
            pacSilence();
            /* STAR/HASH is a cartridge-style soft reset, not just a run
               restart. Latch every shared display object inactive before
               regenerating actor memory or clearing live GPU particles. */
            jsfVsync(0);
            pacInitParticlePool();
            pacGameReset(&game, pal_mode);
            game.phase = PAC_PHASE_TITLE;
            game.presentation_ticks = 0;
            game.presentation_limit = pal_mode ? 400 : 480;
            pacDrawStartLevel(title_canvas);
            pacDrawTitleFooter(title_canvas);
            pacLoadMainPaletteNow(&game, frame);
            pacShowPreparedCanvas(title_canvas);
        }
        else if (game.phase == PAC_PHASE_CREDITS ||
                 game.phase == PAC_PHASE_TITLE ||
                 game.phase == PAC_PHASE_SCORES)
        {
            if (game.phase != PAC_PHASE_TITLE &&
                (pressed & PAC_START_MASK))
            {
                /* Start controls wake the attract loop first. Requiring the
                   next press to start prevents an accidental blind launch. */
                game.phase = PAC_PHASE_TITLE;
                game.presentation_ticks = 0;
                game.presentation_limit = pal_mode ? 400 : 480;
                pacDrawStartLevel(title_canvas);
                pacDrawTitleFooter(title_canvas);
                pacShowPreparedCanvas(title_canvas);
            }
            else if (pressed & PAC_START_MASK)
            {
                /* Publish a clean page first so a slow procedural level build
                   never leaves a frozen menu on screen after Start. */
                pacSetParticlesActive(0);
                pacSetActorsActive(0);
                pacSetFruitVisible(0);
                sprite[PAC_OBJ_SCORE_POPUP].active = R_is_inactive;
                pacZero(spare_canvas, PAC_CLUT4_BYTES);
                pacPresentCanvas(&active_canvas, &spare_canvas);
                pacStartRun(&game, spare_canvas, pal_mode,
                            pac_selected_level);
                pacPresentCanvas(&active_canvas, &spare_canvas);
                game_canvas = active_canvas;
            }
            else
            {
                if (game.phase == PAC_PHASE_TITLE &&
                    (pressed & (JAGPAD_LEFT | JAGPAD_RIGHT)))
                {
                    if ((pressed & JAGPAD_LEFT) && pac_selected_level > 1)
                        --pac_selected_level;
                    else if ((pressed & JAGPAD_RIGHT) &&
                             pac_selected_level < pac_highest_unlocked)
                        ++pac_selected_level;
                    pacDrawStartLevel(title_canvas);
                    pacPlaySound(&game, PAC_SND_DOT);
                }
                ++game.presentation_ticks;
                if (game.phase == PAC_PHASE_TITLE)
                    pacAnimateColourTitle(title_canvas, 27, frame);
                if (game.presentation_ticks >= game.presentation_limit)
                {
                    game.presentation_ticks = 0;
                    if (game.phase == PAC_PHASE_CREDITS)
                    {
                        game.phase = PAC_PHASE_TITLE;
                        game.presentation_limit = pal_mode ? 400 : 480;
                        pacDrawStartLevel(title_canvas);
                        pacDrawTitleFooter(title_canvas);
                        pacShowPreparedCanvas(title_canvas);
                    }
                    else if (game.phase == PAC_PHASE_TITLE)
                    {
                        game.phase = PAC_PHASE_SCORES;
                        game.presentation_limit = pal_mode ? 250 : 300;
                        pacShowPreparedCanvas(scores_canvas);
                    }
                    else
                    {
                        game.phase = PAC_PHASE_CREDITS;
                        game.presentation_limit = pal_mode ? 250 : 300;
                        pacFill(credits_canvas, 54, 204, 212, 11, 0);
                        pacShowPreparedCanvas(credits_canvas);
                    }
                }
            }
        }
        else if (game.phase == PAC_PHASE_PAUSED)
        {
            if (pressed & (JAGPAD_PAUSE | JAGPAD_OPTION))
            {
                game.phase = PAC_PHASE_PLAY;
                pacBuildPlayfield(spare_canvas, &game);
                pacPresentCanvas(&active_canvas, &spare_canvas);
                game_canvas = active_canvas;
                pacSetParticlesActive(1);
                pacSetActorsActive(1);
                pacUpdateActors(&game, frame);
                pacSetFruitVisible(game.fruit_active);
                if (game.score_popup_ticks > 0)
                    sprite[PAC_OBJ_SCORE_POPUP].active = R_is_active;
            }
        }
        else if (game.phase == PAC_PHASE_PLAY &&
                 (pressed & (JAGPAD_PAUSE | JAGPAD_OPTION)))
        {
            game.phase = PAC_PHASE_PAUSED;
            pacBuildPaused(spare_canvas, &game);
            pacPresentCanvas(&active_canvas, &spare_canvas);
            game_canvas = active_canvas;
            pacSetParticlesActive(0);
            pacSetFruitVisible(0);
            sprite[PAC_OBJ_SCORE_POPUP].active = R_is_inactive;
            pacSilence();
        }
        else if (game.phase == PAC_PHASE_READY)
        {
            pacQueueDirection(&game, held, pressed);
            if (game.ready_requires_input)
            {
                if (pressed & PAC_READY_CONTINUE_MASK)
                {
                    game.ready_requires_input = 0;
                    game.ready_ticks = game.ready_limit;
                }
            }
            else
            {
                ++game.ready_ticks;
            }
            if (game.ready_ticks >= game.ready_limit)
            {
                game.phase = PAC_PHASE_PLAY;
                /* Remove the READY panel from the persistent canvas before
                   actor objects become visible. */
                pacBuildPlayfield(spare_canvas, &game);
                pacPresentCanvas(&active_canvas, &spare_canvas);
                game_canvas = active_canvas;
                pacSetActorsActive(1);
                pacUpdateActors(&game, frame);
            }
        }
        else if (game.phase == PAC_PHASE_PLAY)
        {
            ++game.play_ticks;
            --game.siren_ticks;
            if (game.siren_ticks <= 0)
            {
                pacPlaySound(&game, PAC_SND_SIREN);
                game.siren_ticks = pal_mode ? 50 : 60;
            }
            ++game.ghost_cycle_ticks;
            if (game.ghost_cycle_ticks >= 1200)
                game.ghost_cycle_ticks = 0;
            pacQueueDirection(&game, held, pressed);
            pacUseActions(&game, game_canvas, pressed);
            pacApplyImmediateReverse(&game);
            pacApplyCornerAssist(&game);
            if (game.combo_ticks > 0)
            {
                --game.combo_ticks;
                if (game.combo_ticks == 0)
                {
                    game.combo = 1;
                    pacDrawCombo(game_canvas, &game);
                }
            }
            if (game.magnet_ticks > 0) --game.magnet_ticks;
            if (game.storm_notice_ticks > 0) --game.storm_notice_ticks;
            if (game.otto_notice_ticks > 0) --game.otto_notice_ticks;
            if (game.warp_cooldown > 0) --game.warp_cooldown;
            ability_status_changed = 0;
            if (game.dash_ticks > 0)
            {
                --game.dash_ticks;
                if (game.dash_ticks == 0) ability_status_changed = 1;
            }
            if (!pac_unlimited_specials && !game.dash_overdrive &&
                game.dash_charges == 0)
            {
                ++game.dash_recharge_ticks;
                if (game.dash_recharge_ticks >= game.dash_recharge_limit)
                {
                    game.dash_recharge_ticks = game.dash_recharge_limit;
                    game.dash_recharge_segment = 6;
                    game.dash_charges = 1;
                    ability_status_changed = 1;
                }
                else if (game.dash_recharge_ticks >=
                         game.dash_recharge_next)
                {
                    if (game.dash_recharge_segment < 5)
                        ++game.dash_recharge_segment;
                    game.dash_recharge_next += game.dash_recharge_step;
                    ability_status_changed = 1;
                }
            }
            if (game.ghost_stun_ticks > 0)
            {
                --game.ghost_stun_ticks;
                if (game.ghost_stun_ticks == 0) ability_status_changed = 1;
                for (index = 0; index < game.active_ghosts; ++index)
                    if (!game.ghosts[index].released &&
                        game.ghost_release_at[index] != 0xffffffffU)
                        ++game.ghost_release_at[index];
                if (game.ghost_queue_count > 0)
                    ++game.next_ghost_release;
            }
            if (game.shield_ticks > 0)
            {
                --game.shield_ticks;
                if (game.shield_ticks == 0) ability_status_changed = 1;
            }
            if (game.energizer_flash_ticks > 0)
                --game.energizer_flash_ticks;
            if (game.dash_flash_ticks > 0)
            {
                --game.dash_flash_ticks;
                if (game.dash_flash_ticks == 0) ability_status_changed = 1;
            }
            if (game.pulse_flash_ticks > 0)
            {
                --game.pulse_flash_ticks;
                if (game.pulse_flash_ticks == 0) ability_status_changed = 1;
            }
            if (game.shield_flash_ticks > 0)
            {
                --game.shield_flash_ticks;
                if (game.shield_flash_ticks == 0) ability_status_changed = 1;
            }
            if (ability_status_changed)
                pacDrawAbilityStatus(game_canvas, &game);
            if (game.score_popup_ticks > 0)
            {
                --game.score_popup_ticks;
                if (game.score_popup_ticks == 0)
                    sprite[PAC_OBJ_SCORE_POPUP].active = R_is_inactive;
            }
            if (game.frightened_ticks > 0)
            {
                --game.frightened_ticks;
                if (game.frightened_ticks > game.frightened_warning_limit)
                {
                    if (game.frightened_audio_ticks > 0)
                        --game.frightened_audio_ticks;
                    if (game.frightened_audio_ticks <= 0 &&
                        (pressed & (JAGPAD_A | JAGPAD_B | JAGPAD_C)) == 0)
                    {
                        pacPlaySound(&game, PAC_SND_POWER_ACTIVE);
                        game.frightened_audio_ticks =
                            game.frightened_warning_limit >> 1;
                    }
                }
                else if (game.frightened_ticks ==
                         game.frightened_warning_limit)
                {
                    pacPlaySound(&game, PAC_SND_POWER_WARNING);
                    pacLoadActorPalettes(&game, 0);
                }
                else if (game.frightened_ticks > 0 &&
                         (game.frightened_ticks & 7) == 0)
                {
                    int flash_mask = game.frightened_ticks <=
                        (game.frightened_warning_limit >> 1) ? 8 : 16;
                    pacLoadActorPalettes(&game,
                        (game.frightened_ticks & flash_mask) != 0);
                }
                if (game.frightened_ticks == 0)
                {
                    game.ghost_recovered_mask = 0U;
                    pacLoadActorPalettes(&game, 0);
                }
            }
            pacUpdatePrismPressure(&game);
            pacPlayerStep(&game, game_canvas);
            pacMagnetStep(&game, game_canvas, frame);
            pacWarpStep(&game);
            pacUpdateFruit(&game, game_canvas);
            pacReleaseQueuedGhost(&game);
            if (game.ghost_stun_ticks == 0)
            {
                if (game.level >= PAC_LEVEL_CENTIPEDE &&
                    game.ghost_queue_count == 0 &&
                    game.play_ticks >= game.ghost_release_at[0])
                    pacCentipedeStep(&game);
                else
                {
                    for (index = 0; index < game.active_ghosts; ++index)
                        if (game.play_ticks >= game.ghost_release_at[index])
                            pacGhostStep(&game, index);
                    /* Keep a fresh head trail while a captured segment is
                       reforming so the body reconnects without a delayed snap. */
                    if (game.level >= PAC_LEVEL_CENTIPEDE &&
                        game.ghost_queue_count > 0)
                        pacRecordCentipedeHead(&game);
                }
            }
            pacUpdateCrawlerHazard(&game, game_canvas, frame);
            collision = pacCheckCollisions(&game, game_canvas);
            if (collision == 1)
            {
                pacResetActors(&game, pal_mode);
                game.phase = PAC_PHASE_READY;
                game.ready_ticks = 0;
                game.frightened_ticks = 0;
                pacLoadActorPalettes(&game, 0);
                pacSetActorsActive(0);
                pacSetFruitVisible(0);
                sprite[PAC_OBJ_SCORE_POPUP].active = R_is_inactive;
                game.fruit_active = 0;
                pacBuildPlayfield(spare_canvas, &game);
                pacPresentCanvas(&active_canvas, &spare_canvas);
                game_canvas = active_canvas;
                /* The death voice already owns the event channels. Initial
                   and new-level READY cards retain their dedicated cue. */
            }
            else if (collision == 2)
            {
                pacSubmitScore(&game);
                game.phase = PAC_PHASE_SCORES;
                game.presentation_ticks = 0;
                game.presentation_limit = pal_mode ? 250 : 300;
                pacSetActorsActive(0);
                pacSetFruitVisible(0);
                pacSetParticlesActive(0);
                sprite[PAC_OBJ_SCORE_POPUP].active = R_is_inactive;
                pacLoadMainPaletteNow(&game, frame);
                pacShowPreparedCanvas(scores_canvas);
                pacInitParticlePool();
            }
            else if (game.pellets_remaining <= 0)
            {
                pacBuildPlayfield(spare_canvas, &game);
                pacBeginLevelClear(spare_canvas, &game, pal_mode);
                pacPresentCanvas(&active_canvas, &spare_canvas);
                game_canvas = active_canvas;
            }
            else
            {
                /* Continuous motion is now entirely OP-published.  The GPU
                   particle database is reserved for isolated impact events,
                   so movement cannot schedule a periodic missed VBL. */
                pacUpdateActors(&game, frame);
                pacUpdateWakeObjects(&game, frame);
                pacUpdatePickupGlint(&game);
                pacUpdateThreatWarnings(&game);
                pacUpdateGhostAlarm(&game);
                pacUpdateParticleStorm(&game, frame);
                pacUpdateStatusLine(game_canvas, &game);
            }
        }
        else if (game.phase == PAC_PHASE_LEVEL_CLEAR)
        {
            ++game.level_clear_ticks;
            pacTickLevelBonus(game_canvas, &game, frame);
            if (game.level_clear_ticks >= game.level_clear_limit)
            {
                game.score_popup_ticks = 0;
                sprite[PAC_OBJ_SCORE_POPUP].active = R_is_inactive;
                pacRecordLevelClear(&game);
                if (game.level >= PAC_LEVEL_MAX)
                {
                    pacSubmitScore(&game);
                    game.phase = PAC_PHASE_END;
                    game.presentation_ticks = 0;
                    game.presentation_limit = pal_mode ? 400 : 480;
                    pacSetActorsActive(0);
                    pacSetFruitVisible(0);
                    sprite[PAC_OBJ_SCORE_POPUP].active = R_is_inactive;
                    pacBuildEnd(spare_canvas, &game);
                    pacLoadMainPaletteNow(&game, frame);
                    pacPresentCanvas(&active_canvas, &spare_canvas);
                    pacFxBurst(PAC_SCREEN_W >> 1, PAC_SCREEN_H >> 1,
                               16, 0, 32, 6, 13,
                               15, 48, 80);
                    pacPlaySound(&game, PAC_SND_FINALE);
                }
                else
                {
                    ++game.level;
                    pacConfigureLevel(&game, pal_mode);
                    pacBuildPlayfield(spare_canvas, &game);
                    pacLoadMainPaletteNow(&game, frame);
                    pacPresentCanvas(&active_canvas, &spare_canvas);
                    game_canvas = active_canvas;
                    pacLoadActorPalettes(&game, 0);
                    pacSetActorsActive(0);
                    pacSetFruitVisible(0);
                    pacPlaySound(&game, PAC_SND_READY);
                }
            }
        }
        else if (game.phase == PAC_PHASE_END)
        {
            ++game.presentation_ticks;
            if ((frame & 7U) == 0)
                pacFxBurst(24 + (int)((frame * 29U) %
                           (PAC_SCREEN_W - 48)),
                           32 + (int)((frame * 17U) %
                           (PAC_SCREEN_H - 48)),
                           12, (int)(frame * 7U), 43, 5, 11,
                           15, 62, 62);
            if ((pressed & PAC_START_MASK) ||
                game.presentation_ticks >= game.presentation_limit)
            {
                game.phase = PAC_PHASE_SCORES;
                game.presentation_ticks = 0;
                game.presentation_limit = pal_mode ? 250 : 300;
                pacSetParticlesActive(0);
                pacLoadMainPaletteNow(&game, frame);
                pacShowPreparedCanvas(scores_canvas);
                pacInitParticlePool();
            }
        }

        pacUpdatePowerPalette(&game, frame);
        current_palette_flash = pacEnergizerFlashStage(&game);
        palette_animation_due =
            (game.phase == PAC_PHASE_PLAY ||
             game.phase == PAC_PHASE_READY ||
             game.phase == PAC_PHASE_PAUSED ||
             game.phase == PAC_PHASE_LEVEL_CLEAR) &&
            (frame & 7U) == 0U;
        /* Static attract cards need a CLUT upload only on phase entry. */
        if (palette_animation_due || palette_phase != game.phase ||
            current_palette_flash != pac_palette_flash_stage)
        {
            pacUpdateMazeGlow(&game, frame);
            pacUpdateMainHighlight(&game, frame);
            jsfLoadClut(pac_palette, 0, 16);
            palette_phase = game.phase;
            pac_palette_flash_stage = current_palette_flash;
        }
        ++frame;
    }
}
