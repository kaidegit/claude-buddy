#include "buddy_ascii.h"

#include <cassert>
#include <cstring>

namespace {

bool has_ink(const buddy_ascii_frame_t &frame)
{
    for (const char *p = frame.text; *p != '\0'; ++p)
    {
        if (*p != ' ' && *p != '\n')
        {
            return true;
        }
    }
    return false;
}

void test_species_registry()
{
    static const char *const kExpected[] = {
        "capybara", "duck",    "goose",    "blob",   "cat",    "dragon",
        "octopus",  "owl",     "penguin",  "turtle", "snail",  "ghost",
        "axolotl",  "cactus",  "robot",    "rabbit", "mushroom", "chonk",
    };

    assert(buddy_ascii_species_count() == 18);
    for (uint8_t i = 0; i < buddy_ascii_species_count(); ++i)
    {
        assert(std::strcmp(buddy_ascii_species_name(i), kExpected[i]) == 0);
    }
    assert(std::strcmp(buddy_ascii_species_name(BUDDY_ASCII_GIF_SENTINEL), "capybara") == 0);
}

void test_all_species_states_render()
{
    buddy_ascii_frame_t frame;

    for (uint8_t species = 0; species < buddy_ascii_species_count(); ++species)
    {
        for (uint8_t state = 0; state < BUDDY_CHARACTER_STATE_COUNT; ++state)
        {
            assert(buddy_ascii_render(species, (buddy_character_state_t)state, 3200, &frame));
            assert(frame.species == species);
            assert(frame.cols == BUDDY_ASCII_COLS);
            assert(frame.rows == BUDDY_ASCII_ROWS);
            assert(frame.species_name != nullptr);
            assert(has_ink(frame));
        }
    }
}

}  // namespace

int main()
{
    test_species_registry();
    test_all_species_states_render();
    return 0;
}
