#ifndef BUDDY_ASCII_H
#define BUDDY_ASCII_H

#include <stdbool.h>
#include <stdint.h>

#include "storage/buddy_character_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BUDDY_ASCII_SPECIES_COUNT 18U
#define BUDDY_ASCII_GIF_SENTINEL 0xFFU
#define BUDDY_ASCII_COLS 24U
#define BUDDY_ASCII_ROWS 10U
#define BUDDY_ASCII_TEXT_LEN ((BUDDY_ASCII_COLS + 1U) * BUDDY_ASCII_ROWS + 1U)

typedef struct
{
    uint8_t species;
    uint8_t cols;
    uint8_t rows;
    uint16_t body_color;
    const char *species_name;
    char text[BUDDY_ASCII_TEXT_LEN];
} buddy_ascii_frame_t;

uint8_t buddy_ascii_species_count(void);
bool buddy_ascii_species_valid(uint8_t species);
uint8_t buddy_ascii_effective_species(uint8_t species);
const char *buddy_ascii_species_name(uint8_t species);
bool buddy_ascii_render(uint8_t species, buddy_character_state_t state, uint32_t now_ms,
                        buddy_ascii_frame_t *out_frame);

#ifdef __cplusplus
}
#endif

#endif
