#ifndef BUDDY_PREFS_H
#define BUDDY_PREFS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BUDDY_PREFS_STATS_VELOCITY_COUNT 8U

typedef struct
{
    uint8_t brightness;
    bool sound_enabled;
    bool led_enabled;
    bool transcript_enabled;
} buddy_ui_settings_t;

typedef struct
{
    uint32_t nap_seconds;
    uint16_t approvals;
    uint16_t denials;
    uint16_t velocity[BUDDY_PREFS_STATS_VELOCITY_COUNT];
    uint8_t velocity_index;
    uint8_t velocity_count;
    uint8_t level;
    uint32_t tokens;
} buddy_pet_stats_t;

bool buddy_prefs_init(void);
bool buddy_prefs_load_identity(char *name, uint16_t name_size, char *owner, uint16_t owner_size,
                               void *context);
bool buddy_prefs_save_identity(const char *name, const char *owner, void *context);
bool buddy_prefs_load_species(uint8_t *species, void *context);
bool buddy_prefs_save_species(uint8_t species, void *context);
bool buddy_prefs_load_stats(buddy_pet_stats_t *stats, void *context);
bool buddy_prefs_save_stats(const buddy_pet_stats_t *stats, void *context);
bool buddy_prefs_load_ui_settings(buddy_ui_settings_t *settings);
bool buddy_prefs_save_ui_settings(const buddy_ui_settings_t *settings);
bool buddy_prefs_clear_all(void);

#ifdef __cplusplus
}
#endif

#endif
