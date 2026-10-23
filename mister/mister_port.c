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

// Palette entries as two words that weighted sums can be taken of without
// the colours running into each other: red and blue in one, green in the other
static Uint32 PalRB[256];
static Uint32 PalG[256];
static Uint16 Pal565[256];
static Uint32 PalVersion;
static const SDL_Palette *PalSource;

// Rows of the frame that make up one row of the window, with weights that
// add up to 128
#define MAX_ROWS 4
static struct {
    int row[MAX_ROWS];
    int weight[MAX_ROWS];
    int count;
} RowMap[MISTER_MENU_HEIGHT];
static int RowMapFrom, RowMapTo;

static void update_palette(const SDL_Palette *palette)
{
    int i;

    if (palette == PalSource && palette->version == PalVersion)
        return;
    PalSource = palette;
    PalVersion = palette->version;
    for (i = 0; i < 256; i++) {
        const SDL_Color c = i < palette->ncolors ? palette->colors[i] : (SDL_Color){0, 0, 0, 0};
        PalRB[i] = ((Uint32)c.r << 16) | c.b;
        PalG[i] = c.g;
        Pal565[i] = (Uint16)(((c.r >> 3) << 11) | ((c.g >> 2) << 5) | (c.b >> 3));
    }
}

static void update_row_map(int from, int to)
{
    int y, k;

    if (from == RowMapFrom && to == RowMapTo)
        return;
    RowMapFrom = from;
    RowMapTo = to;
    for (y = 0; y < to; y++) {
        // in units of 1/to of a source row
        const int top = y * from;
        const int bottom = top + from;
        const int first = top / to;
        int total = 0;

        RowMap[y].count = 0;
        for (k = 0; k < MAX_ROWS; k++) {
            const int row_top = (first + k) * to;
            const int row_bottom = row_top + to;
            const int covered = SDL_min(bottom, row_bottom) - SDL_max(top, row_top);
            if (covered <= 0 || first + k >= from)
                break;
            RowMap[y].row[k] = first + k;
            RowMap[y].weight[k] = covered * 128 / from;
            total += RowMap[y].weight[k];
            RowMap[y].count++;
        }
        // what rounding lost goes to the first row
        RowMap[y].weight[0] += 128 - total;
    }
}

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
        fprintf(stderr, "mister: %u frames in %u fields, %u us per frame of which %u to convert it\n", fps_frames,
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
    const int width = window->w;
    int x, y, k;

    if (frame->format->palette != NULL && window->format->palette != NULL && frame->w == width &&
        frame->h == window->h)
    {
        // 8 bit to the core, which looks up the colours (LbScreenSetup())
        const SDL_Palette *from = frame->format->palette;
        const SDL_Palette *to = window->format->palette;

        if (from->ncolors == to->ncolors && memcmp(from->colors, to->colors, from->ncolors * sizeof(SDL_Color)) != 0)
            SDL_SetPaletteColors(window->format->palette, from->colors, 0, from->ncolors);
        for (y = 0; y < window->h; y++)
            memcpy((Uint8 *)window->pixels + y * window->pitch, src + y * frame->pitch, width);
        return;
    }
    if (frame->format->palette == NULL || window->format->format != SDL_PIXELFORMAT_RGB565) {
        SDL_BlitScaled(frame, NULL, window, NULL);
        return;
    }
    update_palette(frame->format->palette);

    if (frame->w == width && frame->h == window->h) {
        for (y = 0; y < window->h; y++) {
            const Uint8 *s = src + y * frame->pitch;
            Uint16 *d = (Uint16 *)((Uint8 *)window->pixels + y * window->pitch);
            for (x = 0; x < width; x++)
                d[x] = Pal565[s[x]];
        }
        return;
    }
    // as wide as the window or twice as wide, and up to three rows into one
    if ((frame->w != width && frame->w != width * 2) || width > MISTER_MENU_WIDTH ||
        window->h > MISTER_MENU_HEIGHT || frame->h < window->h || frame->h > window->h * (MAX_ROWS - 1))
    {
        SDL_BlitScaled(frame, NULL, window, NULL);
        return;
    }

    update_row_map(frame->h, window->h);
    for (y = 0; y < window->h; y++) {
        Uint16 *d = (Uint16 *)((Uint8 *)window->pixels + y * window->pitch);
        Uint32 rb[MISTER_MENU_WIDTH];
        Uint32 g[MISTER_MENU_WIDTH];

        for (k = 0; k < RowMap[y].count; k++) {
            const Uint8 *s = src + RowMap[y].row[k] * frame->pitch;
            // the weights of a row add up to 256 for every pixel of the window
            const Uint32 w = RowMap[y].weight[k];
            if (frame->w == width) {
                for (x = 0; x < width; x++) {
                    const Uint32 vrb = PalRB[s[x]] * (w * 2);
                    const Uint32 vg = PalG[s[x]] * (w * 2);
                    rb[x] = k == 0 ? vrb : rb[x] + vrb;
                    g[x] = k == 0 ? vg : g[x] + vg;
                }
            } else {
                for (x = 0; x < width; x++) {
                    const Uint32 vrb = (PalRB[s[2 * x]] + PalRB[s[2 * x + 1]]) * w;
                    const Uint32 vg = (PalG[s[2 * x]] + PalG[s[2 * x + 1]]) * w;
                    rb[x] = k == 0 ? vrb : rb[x] + vrb;
                    g[x] = k == 0 ? vg : g[x] + vg;
                }
            }
        }
        // red is in bits 24..31 of rb, blue in 8..15; green in 8..15 of g
        for (x = 0; x < width; x++)
            d[x] = (Uint16)(((rb[x] >> 16) & 0xF800) | ((g[x] >> 5) & 0x07E0) | ((rb[x] >> 11) & 0x001F));
    }
}

// --- Frame pacing ---

static uint32_t LastField;
static int FieldValid = 0;

unsigned MiSTer_FieldsPassed(void)
{
    uint32_t field;
    unsigned fields;

    if (!MH_IsOpen())
        return 0;
    field = MH_FieldCounter();
    fields = FieldValid ? field - LastField : 1;
    LastField = field;
    FieldValid = 1;
    return fields;
}

int MiSTer_WaitField(void)
{
    if (!MH_IsOpen())
        return 0;
    wait_begins();
    MH_WaitField(MH_FieldCounter());
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

int MiSTer_CheckData(void)
{
    const char *message = NULL;

    if (!file_exists(".", "keeperfx.cfg") || !file_exists(".", "fxdata") || !file_exists("data", "creature.tab")) {
        message = "KeeperFX is not installed.\n\n"
                  "Copy the contents of KeeperFX 1.4.0 (keeperfx_1_4_0_complete.7z from keeperfx.net) to "
                  "games/KeeperFX on the SD card, then the files of Dungeon Keeper as the README says.";
    } else if (!file_exists("data", "bluepal.dat")) {
        // the file the game itself takes as the sign of a complete installation
        message = "The files of Dungeon Keeper are missing.\n\n"
                  "Copy the folders data and sound of your Dungeon Keeper into games/KeeperFX on the SD "
                  "card, without replacing the files that are there. See the README.";
    }
    if (message == NULL) {
        // it takes the game a while to get to its own first picture
        if (MH_Open())
            MH_UI_Message(TITLE, "Loading...", 0);
        return 1;
    }
    fprintf(stderr, "mister: %s\n", message);
    if (MH_Open())
        MH_UI_Message(TITLE, message, MH_UI_WAIT | MH_UI_ERROR);
    return 0;
}
