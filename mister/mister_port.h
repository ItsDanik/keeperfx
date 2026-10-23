#ifndef MISTER_PORT_H
#define MISTER_PORT_H

// MiSTer hybrid core: the game runs on the ARM, the FPGA core shows the
// picture at 15kHz and provides audio and input. SDL's "mister" drivers
// (hybrid/sdl2) do most of the work; this is what is specific to the game.
// Everything is a no-op without the core, so the PC build still runs.

#ifdef __cplusplus
extern "C" {
#endif

struct SDL_Surface;

// The core's two pictures the game uses, and so the sizes of its window:
// 320x200 for the dungeon and the movies, the resolution of the game's own
// artwork, and 640x480 (interlaced on a 15kHz screen) for the menus, which
// the game only has for 640x480.
#define MISTER_SCREEN_WIDTH 320
#define MISTER_SCREEN_HEIGHT 200
#define MISTER_MENU_WIDTH 640
#define MISTER_MENU_HEIGHT 480

// Joystick buttons as SDL sees them: the buttons of the core's "J1," list
// (core/KeeperFX.sv) in order, then the two the SDL driver adds.
enum {
    MISTER_JOY_LEFT_CLICK,
    MISTER_JOY_RIGHT_CLICK,
    MISTER_JOY_MODIFIER,
    MISTER_JOY_MAP,
    MISTER_JOY_ROTATE_LEFT,
    MISTER_JOY_ROTATE_RIGHT,
    MISTER_JOY_SPACE,
    MISTER_JOY_MENU,
    MISTER_JOY_CORE_OK,
    MISTER_JOY_CORE_BACK,

    MISTER_JOY_MENU_OK = 28,
    MISTER_JOY_MENU_BACK = 29
};

// The game draws 8-bit pictures of 320x200 (in the dungeon) and 640x480 (its
// menus). Puts one into the window's surface, which is 8-bit and of its size:
// a copy, with the palette of `frame` (the core looks up the colours).
void MiSTer_ScaleFrame(struct SDL_Surface *frame, struct SDL_Surface *window);

// Frame pacing in the dungeon. The core shows 59.64 fields per second and the
// game does 20 turns per second, drawing as often as it can between them: a
// turn is taken to be three fields, and every frame is shown from the start
// of a field.
// The number of fields since the last call; 0 without the core, the game
// then uses the system clock. Drawing ahead (DRAW_AHEAD in mister_port.c,
// not in the interlaced menus), the fields up to the one the frame being drawn
// is shown at.
unsigned MiSTer_FieldsPassed(void);
// Sleeps until the next field starts, or drawing ahead until the field two
// before the next frame's own (MH_PresentAtField()). Returns 0 without the core.
int MiSTer_WaitField(void);
// For the menus, which do one step every 1/20s: sleeps until `fields` fields
// after the last call have passed, or returns at once if they have.
int MiSTer_WaitFields(unsigned fields);
#define MISTER_FIELDS_PER_SECOND 60

// OSD options of the core (CONF_STR in core/KeeperFX.sv)
// Mouse Speed, for the mouse and for the hand moved with the gamepad: 256 is 100%
int MiSTer_MouseSpeed(void);
// Intro Movies: Off
int MiSTer_SkipIntro(void);
// Sound Volume and Music Volume on top of the game's own: 0..256
int MiSTer_SoundVolume(void);
int MiSTer_MusicVolume(void);

// Before the game starts: are its files in the current directory? If they
// are, the game's logo comes while it loads (MH_GameLogo()) and 1. If not,
// the message under the MiSTer logo says which ones go where, until the
// player leaves the core, and 0.
int MiSTer_StartGame(void);
// The exit code of the process: 43 after the core was loaded again while the
// game ran (the launcher starts it again), else 0 (back to the MiSTer menu)
int MiSTer_ExitCode(void);

// The OSD's Cheats page: the triggers picked since the last call, from one
// place every frame
enum {
    MISTER_CHEAT_WIN_LEVEL = 1 << 0,
    MISTER_CHEAT_REVEAL_MAP = 1 << 1,
    MISTER_CHEAT_GOLD = 1 << 2,
    MISTER_CHEAT_ROOMS_SPELLS = 1 << 3
};
#define MISTER_CHEAT_GOLD_AMOUNT 10000
int MiSTer_Cheats(void);

#ifdef __cplusplus
}
#endif

#endif
