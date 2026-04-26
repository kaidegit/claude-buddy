#include "buddy_ui_data.h"

#include <stdio.h>
#include <string.h>

#include "ascii/buddy_ascii.h"
#include "buddy_ui_mock.h"
#include "rtthread.h"

typedef enum
{
    BUDDY_MOCK_DISCONNECTED = 0,
    BUDDY_MOCK_IDLE,
    BUDDY_MOCK_BUSY,
    BUDDY_MOCK_APPROVAL,
    BUDDY_MOCK_PAIRING,
    BUDDY_MOCK_SCENE_COUNT,
} buddy_mock_scene_t;

static buddy_mock_scene_t s_scene = BUDDY_MOCK_IDLE;
static uint32_t s_now_ms;
static uint8_t s_species = 0;
static buddy_ui_settings_t s_settings = {
    .brightness = 80,
    .sound_enabled = true,
    .led_enabled = true,
    .transcript_enabled = true,
};

static void buddy_mock_copy(char *dst, size_t dst_size, const char *src)
{
    if (dst == NULL || dst_size == 0)
    {
        return;
    }

    snprintf(dst, dst_size, "%s", src != NULL ? src : "");
}

static const char *buddy_mock_scene_label(buddy_mock_scene_t scene)
{
    switch (scene)
    {
    case BUDDY_MOCK_DISCONNECTED:
        return "Disconnected";
    case BUDDY_MOCK_BUSY:
        return "Busy";
    case BUDDY_MOCK_APPROVAL:
        return "Approval";
    case BUDDY_MOCK_PAIRING:
        return "Pairing";
    case BUDDY_MOCK_IDLE:
    default:
        return "Idle";
    }
}

const char *buddy_ui_mock_scene_name(void)
{
    return buddy_mock_scene_label(s_scene);
}

void buddy_ui_mock_next_scene(void)
{
    s_scene = (buddy_mock_scene_t)(((uint8_t)s_scene + 1U) % BUDDY_MOCK_SCENE_COUNT);
    rt_kprintf("Buddy simulator scene: %s\n", buddy_ui_mock_scene_name());
}

void buddy_ui_mock_previous_scene(void)
{
    s_scene = (buddy_mock_scene_t)(s_scene == 0 ? BUDDY_MOCK_SCENE_COUNT - 1 : (uint8_t)s_scene - 1U);
    rt_kprintf("Buddy simulator scene: %s\n", buddy_ui_mock_scene_name());
}

void buddy_ui_mock_tick(uint32_t now_ms)
{
    s_now_ms = now_ms;
}

bool buddy_ui_data_get_model(buddy_ui_model_t *out_model)
{
    buddy_ui_model_t model;

    if (out_model == NULL)
    {
        return false;
    }

    memset(&model, 0, sizeof(model));
    model.connected = s_scene != BUDDY_MOCK_DISCONNECTED;
    model.encrypted = model.connected;
    model.has_prompt = s_scene == BUDDY_MOCK_APPROVAL;
    model.has_pairing_passkey = s_scene == BUDDY_MOCK_PAIRING;
    model.persona = BUDDY_UI_PERSONA_IDLE;
    model.entry_count = BUDDY_UI_MAX_ENTRIES;
    model.total = 42;
    model.running = s_scene == BUDDY_MOCK_BUSY ? 3U : 0U;
    model.waiting = s_scene == BUDDY_MOCK_APPROVAL ? 1U : 0U;
    model.tokens = 18420;
    model.tokens_today = 2310;
    model.last_snapshot_ms = s_now_ms;
    model.prompt_started_ms = model.has_prompt && s_now_ms > 12000U ? s_now_ms - 12000U : 1U;
    model.uptime_ms = s_now_ms;
    model.rx_lines = 128;
    model.rx_overflowed = 0;
    model.pairing_passkey = 426817;
    model.pet_nap_seconds = 3600;
    model.pet_tokens = 18420;
    model.pet_approvals = 7;
    model.pet_denials = 1;
    model.pet_velocity_seconds = 42;
    model.pet_level = 4;
    model.pet_mood = 3;
    model.pet_fed = 8;
    model.pet_energy = s_scene == BUDDY_MOCK_BUSY ? 2U : 4U;
    model.species = s_species;
    model.species_count = buddy_ascii_species_count();
    model.gif_species = BUDDY_ASCII_GIF_SENTINEL;

    if (s_scene == BUDDY_MOCK_DISCONNECTED)
    {
        model.persona = BUDDY_UI_PERSONA_SLEEP;
        buddy_mock_copy(model.msg, sizeof(model.msg), "Waiting for Claude Desktop");
    }
    else if (s_scene == BUDDY_MOCK_BUSY)
    {
        model.persona = BUDDY_UI_PERSONA_BUSY;
        buddy_mock_copy(model.msg, sizeof(model.msg), "Claude is working on 3 tool calls");
    }
    else if (s_scene == BUDDY_MOCK_APPROVAL)
    {
        model.persona = BUDDY_UI_PERSONA_ATTENTION;
        buddy_mock_copy(model.msg, sizeof(model.msg), "Permission requested");
    }
    else if (s_scene == BUDDY_MOCK_PAIRING)
    {
        model.persona = BUDDY_UI_PERSONA_ATTENTION;
        buddy_mock_copy(model.msg, sizeof(model.msg), "Bluetooth pairing pending");
    }
    else
    {
        buddy_mock_copy(model.msg, sizeof(model.msg), "Ready");
    }

    buddy_mock_copy(model.device_name, sizeof(model.device_name), "Claude-52B0");
    buddy_mock_copy(model.owner, sizeof(model.owner), "SiFli Dev Kit");
    buddy_mock_copy(model.entries[0], sizeof(model.entries[0]), "Claude: scanning project files");
    buddy_mock_copy(model.entries[1], sizeof(model.entries[1]), "Tool: read app/src/ui");
    buddy_mock_copy(model.entries[2], sizeof(model.entries[2]), "User: adjust the home layout");
    buddy_mock_copy(model.entries[3], sizeof(model.entries[3]), "Claude: simulator preview ready");
    buddy_mock_copy(model.entries[4], sizeof(model.entries[4]), "Status: keyboard controls active");
    buddy_mock_copy(model.prompt_id, sizeof(model.prompt_id), "mock-prompt-001");
    buddy_mock_copy(model.prompt_tool, sizeof(model.prompt_tool), "Bash");
    buddy_mock_copy(model.prompt_hint, sizeof(model.prompt_hint), "Run firmware size check");

    *out_model = model;
    return true;
}

bool buddy_ui_data_send_permission_once(void)
{
    rt_kprintf("Buddy simulator approval: once\n");
    s_scene = BUDDY_MOCK_IDLE;
    return true;
}

bool buddy_ui_data_send_permission_deny(void)
{
    rt_kprintf("Buddy simulator approval: deny\n");
    s_scene = BUDDY_MOCK_IDLE;
    return true;
}

bool buddy_ui_data_set_species(uint8_t species)
{
    if (!buddy_ascii_species_valid(species))
    {
        return false;
    }

    s_species = species;
    rt_kprintf("Buddy simulator species: %s\n", buddy_ascii_species_name(s_species));
    return true;
}

bool buddy_ui_data_factory_reset(void)
{
    s_scene = BUDDY_MOCK_IDLE;
    s_species = 0;
    s_settings.brightness = 80;
    s_settings.sound_enabled = true;
    s_settings.led_enabled = true;
    s_settings.transcript_enabled = true;
    rt_kprintf("Buddy simulator factory reset\n");
    return true;
}

bool buddy_ui_data_load_settings(buddy_ui_settings_t *settings)
{
    if (settings == NULL)
    {
        return false;
    }

    *settings = s_settings;
    return true;
}

bool buddy_ui_data_save_settings(const buddy_ui_settings_t *settings)
{
    if (settings == NULL)
    {
        return false;
    }

    s_settings = *settings;
    return true;
}

bool buddy_ui_data_character_available(void)
{
    return false;
}

bool buddy_ui_data_character_get_lvgl_path(buddy_character_state_t state,
                                           uint32_t now_ms,
                                           char *out_path,
                                           uint32_t out_path_size)
{
    (void)state;
    (void)now_ms;
    if (out_path != NULL && out_path_size > 0)
    {
        out_path[0] = '\0';
    }
    return false;
}

void buddy_ui_data_character_invalidate(void)
{
}
