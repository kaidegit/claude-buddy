#include "buddy_ui_data.h"

bool buddy_ui_data_get_model(buddy_ui_model_t *out_model)
{
    return buddy_app_get_ui_model(out_model);
}

bool buddy_ui_data_send_permission_once(void)
{
    return buddy_app_send_permission_once();
}

bool buddy_ui_data_send_permission_deny(void)
{
    return buddy_app_send_permission_deny();
}

bool buddy_ui_data_set_species(uint8_t species)
{
    return buddy_app_set_species(species);
}

bool buddy_ui_data_factory_reset(void)
{
    return buddy_app_factory_reset();
}

bool buddy_ui_data_load_settings(buddy_ui_settings_t *settings)
{
    return buddy_prefs_load_ui_settings(settings);
}

bool buddy_ui_data_save_settings(const buddy_ui_settings_t *settings)
{
    return buddy_prefs_save_ui_settings(settings);
}

bool buddy_ui_data_character_available(void)
{
    return buddy_character_runtime_available();
}

bool buddy_ui_data_character_get_lvgl_path(buddy_character_state_t state,
                                           uint32_t now_ms,
                                           char *out_path,
                                           uint32_t out_path_size)
{
    return buddy_character_runtime_get_lvgl_path(state, now_ms, out_path, out_path_size);
}

void buddy_ui_data_character_invalidate(void)
{
    buddy_character_runtime_invalidate();
}
