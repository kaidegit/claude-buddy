#ifndef BUDDY_UI_DATA_H
#define BUDDY_UI_DATA_H

#include <stdbool.h>
#include <stdint.h>

#include "bridge/buddy_app_c_api.h"
#include "storage/buddy_character_runtime.h"
#include "storage/buddy_prefs.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Explicit frontend data contract.
 *
 * UI frontends may render and navigate differently per board, but they should
 * depend on this API instead of reaching into app, storage, or transport code.
 */
bool buddy_ui_data_get_model(buddy_ui_model_t *out_model);

bool buddy_ui_data_send_permission_once(void);
bool buddy_ui_data_send_permission_deny(void);
bool buddy_ui_data_set_species(uint8_t species);
bool buddy_ui_data_factory_reset(void);

bool buddy_ui_data_load_settings(buddy_ui_settings_t *settings);
bool buddy_ui_data_save_settings(const buddy_ui_settings_t *settings);

bool buddy_ui_data_character_available(void);
bool buddy_ui_data_character_get_lvgl_path(buddy_character_state_t state,
                                           uint32_t now_ms,
                                           char *out_path,
                                           uint32_t out_path_size);
void buddy_ui_data_character_invalidate(void);

#ifdef __cplusplus
}
#endif

#endif
