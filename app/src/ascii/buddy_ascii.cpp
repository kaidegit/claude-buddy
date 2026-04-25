#include "buddy_ascii.h"

#include <stddef.h>
#include <string.h>

#include "M5StickCPlus.h"
#include "buddy.h"
#include "buddy_common.h"

TFT_eSprite spr;

enum {
    B_SLEEP,
    B_IDLE,
    B_BUSY,
    B_ATTENTION,
    B_CELEBRATE,
    B_DIZZY,
    B_HEART,
};

const int BUDDY_X_CENTER = 67;
const int BUDDY_CANVAS_W = 135;
const int BUDDY_Y_BASE = 30;
const int BUDDY_Y_OVERLAY = 6;
const int BUDDY_CHAR_W = 6;
const int BUDDY_CHAR_H = 8;

const uint16_t BUDDY_BG = 0x0000;
const uint16_t BUDDY_HEART = 0xF810;
const uint16_t BUDDY_DIM = 0x8410;
const uint16_t BUDDY_YEL = 0xFFE0;
const uint16_t BUDDY_WHITE = 0xFFFF;
const uint16_t BUDDY_CYAN = 0x07FF;
const uint16_t BUDDY_GREEN = 0x07E0;
const uint16_t BUDDY_PURPLE = 0xA01F;
const uint16_t BUDDY_RED = 0xF800;
const uint16_t BUDDY_BLUE = 0x041F;

extern const Species CAPYBARA_SPECIES;
extern const Species DUCK_SPECIES;
extern const Species GOOSE_SPECIES;
extern const Species BLOB_SPECIES;
extern const Species CAT_SPECIES;
extern const Species DRAGON_SPECIES;
extern const Species OCTOPUS_SPECIES;
extern const Species OWL_SPECIES;
extern const Species PENGUIN_SPECIES;
extern const Species TURTLE_SPECIES;
extern const Species SNAIL_SPECIES;
extern const Species GHOST_SPECIES;
extern const Species AXOLOTL_SPECIES;
extern const Species CACTUS_SPECIES;
extern const Species ROBOT_SPECIES;
extern const Species RABBIT_SPECIES;
extern const Species MUSHROOM_SPECIES;
extern const Species CHONK_SPECIES;

namespace {

constexpr uint32_t kTickMs = 200;

struct RenderContext {
    buddy_ascii_frame_t *frame = nullptr;
    uint16_t color = BUDDY_WHITE;
    int cursor_x = 0;
    int cursor_y = 0;
};

RenderContext *g_render = nullptr;

const Species *const kSpeciesTable[] = {
    &CAPYBARA_SPECIES, &DUCK_SPECIES,   &GOOSE_SPECIES,   &BLOB_SPECIES,
    &CAT_SPECIES,      &DRAGON_SPECIES, &OCTOPUS_SPECIES, &OWL_SPECIES,
    &PENGUIN_SPECIES,  &TURTLE_SPECIES, &SNAIL_SPECIES,   &GHOST_SPECIES,
    &AXOLOTL_SPECIES,  &CACTUS_SPECIES, &ROBOT_SPECIES,   &RABBIT_SPECIES,
    &MUSHROOM_SPECIES, &CHONK_SPECIES,
};

void clear_frame(buddy_ascii_frame_t *frame)
{
    size_t pos = 0;

    frame->cols = BUDDY_ASCII_COLS;
    frame->rows = BUDDY_ASCII_ROWS;
    for (uint8_t row = 0; row < BUDDY_ASCII_ROWS; ++row)
    {
        for (uint8_t col = 0; col < BUDDY_ASCII_COLS; ++col)
        {
            frame->text[pos++] = ' ';
        }
        frame->text[pos++] = '\n';
    }
    frame->text[pos] = '\0';
}

void set_cell(int col, int row, char ch)
{
    if (g_render == nullptr || g_render->frame == nullptr || ch == '\0' ||
        col < 0 || row < 0 || col >= (int)BUDDY_ASCII_COLS || row >= (int)BUDDY_ASCII_ROWS)
    {
        return;
    }

    g_render->frame->text[(row * (BUDDY_ASCII_COLS + 1U)) + col] = ch;
}

void put_at_pixel(int x, int y, char ch)
{
    int col = x / BUDDY_CHAR_W;
    int row = y / BUDDY_CHAR_H;

    if (x < 0)
    {
        col = -1;
    }
    if (y < 0)
    {
        row = -1;
    }

    set_cell(col, row, ch);
}

}  // namespace

void buddyPrintLine(const char *line, int yPx, uint16_t color, int xOff)
{
    int len;
    int x;

    (void)color;
    if (line == nullptr)
    {
        return;
    }

    len = (int)strlen(line);
    while (len > 0 && line[len - 1] == ' ')
    {
        --len;
    }
    while (len > 0 && *line == ' ')
    {
        ++line;
        --len;
    }

    x = BUDDY_X_CENTER - (len * BUDDY_CHAR_W) / 2 + xOff;
    for (int i = 0; i < len; ++i)
    {
        put_at_pixel(x + i * BUDDY_CHAR_W, yPx, line[i]);
    }
}

void buddyPrintSprite(const char *const *lines, uint8_t nLines, int yOffset, uint16_t color,
                      int xOff)
{
    if (lines == nullptr)
    {
        return;
    }

    for (uint8_t i = 0; i < nLines; ++i)
    {
        buddyPrintLine(lines[i], BUDDY_Y_BASE + yOffset + i * BUDDY_CHAR_H, color, xOff);
    }
}

void buddySetCursor(int x, int y)
{
    if (g_render == nullptr)
    {
        return;
    }

    g_render->cursor_x = x;
    g_render->cursor_y = y;
}

void buddySetColor(uint16_t fg)
{
    if (g_render != nullptr)
    {
        g_render->color = fg;
    }
}

void buddyPrint(const char *s)
{
    if (g_render == nullptr || s == nullptr)
    {
        return;
    }

    for (const char *p = s; *p != '\0'; ++p)
    {
        put_at_pixel(g_render->cursor_x, g_render->cursor_y, *p);
        g_render->cursor_x += BUDDY_CHAR_W;
    }
}

extern "C" uint8_t buddy_ascii_species_count(void)
{
    return (uint8_t)(sizeof(kSpeciesTable) / sizeof(kSpeciesTable[0]));
}

extern "C" bool buddy_ascii_species_valid(uint8_t species)
{
    return species < buddy_ascii_species_count();
}

extern "C" uint8_t buddy_ascii_effective_species(uint8_t species)
{
    return buddy_ascii_species_valid(species) ? species : 0;
}

extern "C" const char *buddy_ascii_species_name(uint8_t species)
{
    species = buddy_ascii_effective_species(species);
    return kSpeciesTable[species]->name;
}

extern "C" bool buddy_ascii_render(uint8_t species, buddy_character_state_t state, uint32_t now_ms,
                                   buddy_ascii_frame_t *out_frame)
{
    RenderContext render;
    const Species *selected;
    StateFn draw;
    uint8_t state_index;

    if (out_frame == nullptr)
    {
        return false;
    }

    species = buddy_ascii_effective_species(species);
    selected = kSpeciesTable[species];
    clear_frame(out_frame);
    out_frame->species = species;
    out_frame->body_color = selected->bodyColor;
    out_frame->species_name = selected->name;

    state_index = state < BUDDY_CHARACTER_STATE_COUNT ? (uint8_t)state : (uint8_t)B_IDLE;
    draw = selected->states[state_index];
    if (draw == nullptr)
    {
        draw = selected->states[B_IDLE];
    }
    if (draw == nullptr)
    {
        return false;
    }

    render.frame = out_frame;
    render.color = selected->bodyColor;
    g_render = &render;
    draw(now_ms / kTickMs);
    g_render = nullptr;
    return true;
}
