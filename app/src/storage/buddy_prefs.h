#ifndef BUDDY_PREFS_H
#define BUDDY_PREFS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint8_t brightness;
    bool sound_enabled;
    bool led_enabled;
    bool transcript_enabled;
} buddy_ui_settings_t;

bool buddy_prefs_init(void);
bool buddy_prefs_load_identity(char *name, uint16_t name_size, char *owner, uint16_t owner_size,
                               void *context);
bool buddy_prefs_save_identity(const char *name, const char *owner, void *context);
bool buddy_prefs_load_species(uint8_t *species, void *context);
bool buddy_prefs_save_species(uint8_t species, void *context);
bool buddy_prefs_load_ui_settings(buddy_ui_settings_t *settings);
bool buddy_prefs_save_ui_settings(const buddy_ui_settings_t *settings);
bool buddy_prefs_clear_all(void);

#ifdef __cplusplus
}
#endif

#endif
