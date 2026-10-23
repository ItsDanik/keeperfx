// MiSTer hybrid core support, see mister_port.h

#include "mister_port.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include <SDL2/SDL.h>
#include <mister_hybrid.h>

static const char *const TITLE = "KeeperFX";

// --- OSD options: the first entry of each is 0, the default ---

int MiSTer_MouseSpeed(void)
{
    // "O[27:25],Mouse Speed,100%,75%,50%,25%,150%,200%;"
    static const int speed[8] = {256, 192, 128, 64, 384, 512, 256, 256};
    return speed[MH_OSD_GAME_BITS(MH_OSDStatus(), 25, 3)];
}

int MiSTer_SkipIntro(void)
{
    // "O[28],Intro Movies,On,Off;"
    return MH_OSD_GAME_BITS(MH_OSDStatus(), 28, 1);
}

static int osd_volume(int steps)
{
    // 100%, 90%, ... 0%
    return steps >= 10 ? 0 : 256 * (10 - steps) / 10;
}

int MiSTer_SoundVolume(void)
{
    return osd_volume(MH_OSD_SOUND_VOLUME(MH_OSDStatus()));
}

int MiSTer_MusicVolume(void)
{
    return osd_volume(MH_OSD_MUSIC_VOLUME(MH_OSDStatus()));
}

// --- Video ---

// Development: with MISTER_KEEPERFX_FPS set, the frame rate goes to the log
static Uint64 ScaleTime;
static Uint64 BusyTime; // between two waits for a field
static Uint64 BusyStart;

static void wait_begins(void)
{
    if (BusyStart != 0)
        BusyTime += SDL_GetPerformanceCounter() - BusyStart;
}

static void wait_ends(void)
{
    BusyStart = SDL_GetPerformanceCounter();
}

static void count_frame(void)
{
    static int show_fps = -1;
    static uint32_t fps_field;
    static unsigned fps_frames;
    uint32_t field;

    if (show_fps < 0)
        show_fps = SDL_getenv("MISTER_KEEPERFX_FPS") != NULL;
    if (!show_fps || !MH_IsOpen())
        return;
    field = MH_FieldCounter();
    fps_frames++;
    if (field - fps_field >= 300) {
        const Uint64 us = SDL_GetPerformanceFrequency() / 1000000 * fps_frames;
        fprintf(stderr, "mister: %u frames in %u fields, %u us per frame of which %u to copy it\n", fps_frames,
            (unsigned)(field - fps_field), (unsigned)(BusyTime / us), (unsigned)(ScaleTime / us));
        ScaleTime = 0;
        BusyTime = 0;
        fps_field = field;
        fps_frames = 0;
    }
}

static void scale_frame(SDL_Surface *frame, SDL_Surface *window);

void MiSTer_ScaleFrame(SDL_Surface *frame, SDL_Surface *window)
{
    const Uint64 start = SDL_GetPerformanceCounter();

    scale_frame(frame, window);
    ScaleTime += SDL_GetPerformanceCounter() - start;
    count_frame();
}

static void scale_frame(SDL_Surface *frame, SDL_Surface *window)
{
    const Uint8 *src = frame->pixels;
    int y;

    if (frame->format->palette != NULL && window->format->palette != NULL && frame->w == window->w &&
        frame->h == window->h)
    {
        // 8 bit to the core, which looks up the colours (LbScreenSetup())
        const SDL_Palette *from = frame->format->palette;
        const SDL_Palette *to = window->format->palette;

        if (from->ncolors == to->ncolors && memcmp(from->colors, to->colors, from->ncolors * sizeof(SDL_Color)) != 0)
            SDL_SetPaletteColors(window->format->palette, from->colors, 0, from->ncolors);
        for (y = 0; y < window->h; y++)
            memcpy((Uint8 *)window->pixels + y * window->pitch, src + y * frame->pitch, window->w);
        return;
    }
    // not with the modes the game uses
    SDL_BlitScaled(frame, NULL, window, NULL);
}

// --- Frame pacing ---

// 1 if the game's frames take most of a field: each is drawn a field ahead
// and the library shows it at its own field (MH_PresentAtField()), so that
// one a little slower than a field is still in time, for one field more
// latency. Decided by benchmark, CLAUDE.md ("Frame pacing").
#define DRAW_AHEAD 1

static uint32_t LastField;
static int FieldValid = 0;
// with DRAW_AHEAD: the field the frame being drawn is shown at
static uint32_t AheadField;
static int AheadValid = 0;

static int drawing_ahead(void)
{
    return DRAW_AHEAD && !MH_Interlaced();
}

unsigned MiSTer_FieldsPassed(void)
{
    uint32_t field;
    unsigned fields;

    if (!MH_IsOpen())
        return 0;
    // drawing ahead, the game's time is that of the field its frame is shown at
    field = drawing_ahead() && AheadValid ? AheadField : MH_FieldCounter();
    fields = FieldValid ? field - LastField : 1;
    LastField = field;
    FieldValid = 1;
    return fields;
}

int MiSTer_WaitField(void)
{
    uint32_t field;

    if (!MH_IsOpen())
        return 0;
    wait_begins();
    field = MH_FieldCounter();
    if (drawing_ahead()) {
        // the field after the last frame's, drawn from the field two before it
        uint32_t target = AheadValid ? AheadField + 1 : field + 1;

        MH_WaitField(target - 3);
        field = MH_FieldCounter();
        if ((int32_t)(field + 1 - target) > 0)
            target = field + 1; // too late for it: the next vblank
        MH_PresentAtField(target);
        AheadField = target;
        AheadValid = 1;
    } else {
        AheadValid = 0;
        MH_WaitField(field);
    }
    wait_ends();
    return 1;
}

int MiSTer_WaitFields(unsigned fields)
{
    static uint32_t last;
    uint32_t field;

    if (!MH_IsOpen())
        return 0;
    wait_begins();
    field = MH_FieldCounter();
    // late already, or the first call after a while: from now
    if (field - last >= fields)
        last = field;
    else
        last += fields;
    while ((int32_t)(last - field) > 0) {
        MH_WaitField(field);
        if (MH_FieldCounter() == field) // the core is gone
            break;
        field = MH_FieldCounter();
    }
    wait_ends();
    return 1;
}

// --- Game data ---

// The MiSTer's SD card does not tell upper from lower case, a PC might
static int file_exists(const char *dir, const char *name)
{
    DIR *d = opendir(dir);
    struct dirent *e;
    int found = 0;

    if (d == NULL)
        return 0;
    while (!found && (e = readdir(d)) != NULL)
        found = strcasecmp(e->d_name, name) == 0;
    closedir(d);
    return found;
}

// The game's id in package/games.txt (its logo: logos/<id>.mhl) and its title
#define GAME_ID "DungeonKeeper"
#define GAME_TITLE "Dungeon Keeper"

// 4 lines of 71 at most (MH_MSG_ROWS, MH_MSG_COLS); %s is the game's folder
#define NO_KEEPERFX \
    "KeeperFX 1.4.0 is not installed. Extract keeperfx_1_4_0_complete.7z " \
    "(from keeperfx.net) into %s/, then copy the DATA and SOUND folders of " \
    "your Dungeon Keeper into it, without replacing files."
#define NO_DUNGEON_KEEPER \
    "The files of Dungeon Keeper are missing. Copy its DATA and SOUND folders " \
    "into %s/, without replacing the files that are there."

int MiSTer_StartGame(void)
{
    const char *missing = NULL;
    char message[512];

    if (!file_exists(".", "keeperfx.cfg") || !file_exists(".", "fxdata") || !file_exists("data", "creature.tab"))
        missing = NO_KEEPERFX;
    else if (!file_exists("data", "bluepal.dat"))
        missing = NO_DUNGEON_KEEPER; // the file the game itself takes as the sign of them
    if (missing == NULL) {
        // the lines under the MiSTer logo, then the game's logo while it loads
        if (MH_Open())
            MH_GameLogo(GAME_ID, GAME_TITLE);
        return 1;
    }
    snprintf(message, sizeof(message), missing, MH_GamePath());
    fprintf(stderr, "mister: %s\n", message);
    if (MH_Open()) {
        MH_GameLogoMissing(GAME_ID);
        MH_UI_Message(TITLE, message, MH_UI_WAIT | MH_UI_ERROR);
    }
    return 0;
}

int MiSTer_ExitCode(void)
{
    // The player loaded the core again while the game ran: the launcher
    // starts it again once the core has settled
    if (MH_CoreReloaded())
        return 43;
    return 0;
}

// --- Cheats ---

int MiSTer_Cheats(void)
{
    // "P2T[56],Win Level;" .. "P2T[59],All Rooms and Spells;"
    return MH_OSDTriggers();
}
