#ifndef BUDDY_CHARACTER_RUNTIME_H
#define BUDDY_CHARACTER_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BUDDY_CHARACTER_RUNTIME_PATH_LEN 128

typedef enum
{
    BUDDY_CHARACTER_STATE_SLEEP = 0,
    BUDDY_CHARACTER_STATE_IDLE,
    BUDDY_CHARACTER_STATE_BUSY,
    BUDDY_CHARACTER_STATE_ATTENTION,
    BUDDY_CHARACTER_STATE_CELEBRATE,
    BUDDY_CHARACTER_STATE_DIZZY,
    BUDDY_CHARACTER_STATE_HEART,
    BUDDY_CHARACTER_STATE_COUNT,
} buddy_character_state_t;

bool buddy_character_runtime_get_lvgl_path(buddy_character_state_t state, uint32_t now_ms,
                                           char *out_path, uint16_t out_path_size);
bool buddy_character_runtime_available(void);
const char *buddy_character_runtime_display_name(void);
void buddy_character_runtime_invalidate(void);

#ifdef __cplusplus
}
#endif

#endif
