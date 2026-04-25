#ifndef BUDDY_APP_C_API_H
#define BUDDY_APP_C_API_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BUDDY_UI_MAX_ENTRIES 5
#define BUDDY_UI_DEVICE_NAME_LEN 32
#define BUDDY_UI_OWNER_LEN 32
#define BUDDY_UI_MSG_LEN 64
#define BUDDY_UI_ENTRY_LEN 64
#define BUDDY_UI_PROMPT_ID_LEN 64
#define BUDDY_UI_PROMPT_TOOL_LEN 32
#define BUDDY_UI_PROMPT_HINT_LEN 96

typedef enum
{
    BUDDY_UI_PERSONA_SLEEP = 0,
    BUDDY_UI_PERSONA_IDLE,
    BUDDY_UI_PERSONA_BUSY,
    BUDDY_UI_PERSONA_ATTENTION,
    BUDDY_UI_PERSONA_CELEBRATE,
    BUDDY_UI_PERSONA_DIZZY,
    BUDDY_UI_PERSONA_HEART,
} buddy_ui_persona_t;

typedef struct
{
    bool connected;
    bool encrypted;
    bool has_prompt;
    bool has_pairing_passkey;
    buddy_ui_persona_t persona;
    uint8_t entry_count;
    uint32_t total;
    uint32_t running;
    uint32_t waiting;
    uint32_t tokens;
    uint32_t tokens_today;
    uint32_t last_snapshot_ms;
    uint32_t prompt_started_ms;
    uint32_t uptime_ms;
    uint32_t rx_lines;
    uint32_t rx_overflowed;
    uint32_t pairing_passkey;
    uint8_t species;
    uint8_t species_count;
    uint8_t gif_species;
    char device_name[BUDDY_UI_DEVICE_NAME_LEN];
    char owner[BUDDY_UI_OWNER_LEN];
    char msg[BUDDY_UI_MSG_LEN];
    char entries[BUDDY_UI_MAX_ENTRIES][BUDDY_UI_ENTRY_LEN];
    char prompt_id[BUDDY_UI_PROMPT_ID_LEN];
    char prompt_tool[BUDDY_UI_PROMPT_TOOL_LEN];
    char prompt_hint[BUDDY_UI_PROMPT_HINT_LEN];
} buddy_ui_model_t;

void buddy_app_init(void);
void buddy_app_on_ble_rx(const uint8_t *data, uint16_t len);
void buddy_app_on_ble_passkey(uint32_t passkey);
bool buddy_app_send_permission_once(void);
bool buddy_app_send_permission_deny(void);
bool buddy_app_delete_character(void);
bool buddy_app_factory_reset(void);
bool buddy_app_set_species(uint8_t species);
bool buddy_app_get_pairing_passkey(uint32_t *out_passkey);
bool buddy_app_get_ui_model(buddy_ui_model_t *out_model);
void buddy_app_tick(uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif
