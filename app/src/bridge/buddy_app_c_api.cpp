#include "buddy_app_c_api.h"

#include <stddef.h>
#include <string.h>

#include "core/BuddyApp.hpp"
#include "storage/buddy_character_store.h"
#include "storage/buddy_prefs.h"
#include "transport/ble_nus_sifli.h"

static buddy::BuddyApp g_app;

static void buddy_app_copy_string(char *dst, size_t dst_size, const char *src)
{
    if (dst == nullptr || dst_size == 0)
    {
        return;
    }

    if (src == nullptr)
    {
        dst[0] = '\0';
        return;
    }

    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static buddy_ui_persona_t buddy_app_persona_from_status(const buddy::BuddyProtocol::Snapshot &snapshot,
                                                        const buddy::BuddyProtocol::RuntimeStatus &status)
{
    if (snapshot.has_prompt)
    {
        return BUDDY_UI_PERSONA_ATTENTION;
    }

    if (!status.connected || snapshot.last_snapshot_ms == 0 ||
        (status.uptime_ms >= snapshot.last_snapshot_ms && status.uptime_ms - snapshot.last_snapshot_ms > 30000U))
    {
        return BUDDY_UI_PERSONA_SLEEP;
    }

    if (snapshot.running > 0)
    {
        return BUDDY_UI_PERSONA_BUSY;
    }

    return BUDDY_UI_PERSONA_IDLE;
}

static int buddy_app_ble_send(const char *line, uint16_t len, void *context)
{
    (void)context;
    return buddy_ble_nus_send(reinterpret_cast<const uint8_t *>(line), len);
}

static bool buddy_app_ble_connected(void *context)
{
    (void)context;
    return buddy_ble_nus_is_connected();
}

static bool buddy_app_ble_encrypted(void *context)
{
    (void)context;
    return buddy_ble_nus_is_encrypted();
}

static int buddy_app_ble_unpair(void *context)
{
    (void)context;
    return buddy_ble_nus_unpair();
}

static bool buddy_app_platform_delete_character(void *context)
{
    (void)context;
    return buddy_character_store_delete_active();
}

static bool buddy_app_platform_factory_reset(void *context)
{
    bool ok = true;

    (void)context;

    ok = buddy_character_store_delete_all() && ok;
    ok = buddy_prefs_clear_all() && ok;
    ok = buddy_ble_nus_unpair() >= 0 && ok;
    return ok;
}

static bool buddy_app_load_stats(buddy::BuddyProtocol::PetStats *stats, void *context)
{
    buddy_pet_stats_t stored;

    (void)context;

    if (stats == nullptr || !buddy_prefs_load_stats(&stored, nullptr))
    {
        return false;
    }

    stats->nap_seconds = stored.nap_seconds;
    stats->approvals = stored.approvals;
    stats->denials = stored.denials;
    for (uint8_t i = 0; i < buddy::BuddyProtocol::kVelocitySampleCount; ++i)
    {
        stats->velocity[i] = stored.velocity[i];
    }
    stats->velocity_index = stored.velocity_index;
    stats->velocity_count = stored.velocity_count;
    stats->level = stored.level;
    stats->tokens = stored.tokens;
    return true;
}

static bool buddy_app_save_stats(const buddy::BuddyProtocol::PetStats *stats, void *context)
{
    buddy_pet_stats_t stored;

    (void)context;

    if (stats == nullptr)
    {
        return false;
    }

    memset(&stored, 0, sizeof(stored));
    stored.nap_seconds = stats->nap_seconds;
    stored.approvals = stats->approvals;
    stored.denials = stats->denials;
    for (uint8_t i = 0; i < buddy::BuddyProtocol::kVelocitySampleCount; ++i)
    {
        stored.velocity[i] = stats->velocity[i];
    }
    stored.velocity_index = stats->velocity_index;
    stored.velocity_count = stats->velocity_count;
    stored.level = stats->level;
    stored.tokens = stats->tokens;
    return buddy_prefs_save_stats(&stored, nullptr);
}

extern "C" void buddy_app_init(void)
{
    buddy::BuddyPlatformHooks hooks;
    hooks.send = buddy_app_ble_send;
    hooks.is_connected = buddy_app_ble_connected;
    hooks.is_encrypted = buddy_app_ble_encrypted;
    hooks.unpair = buddy_app_ble_unpair;
    hooks.context = nullptr;
    hooks.prefs.load_identity = buddy_prefs_load_identity;
    hooks.prefs.save_identity = buddy_prefs_save_identity;
    hooks.prefs.load_species = buddy_prefs_load_species;
    hooks.prefs.save_species = buddy_prefs_save_species;
    hooks.prefs.load_stats = buddy_app_load_stats;
    hooks.prefs.save_stats = buddy_app_save_stats;
    hooks.prefs.context = nullptr;
    hooks.reset.delete_character = buddy_app_platform_delete_character;
    hooks.reset.factory_reset = buddy_app_platform_factory_reset;
    hooks.reset.context = nullptr;
    hooks.character_storage.begin = buddy_character_store_begin;
    hooks.character_storage.open_file = buddy_character_store_open_file;
    hooks.character_storage.write = buddy_character_store_write;
    hooks.character_storage.close_file = buddy_character_store_close_file;
    hooks.character_storage.read_file = buddy_character_store_read_file;
    hooks.character_storage.file_exists = buddy_character_store_file_exists;
    hooks.character_storage.commit = buddy_character_store_commit;
    hooks.character_storage.abort = buddy_character_store_abort;
    hooks.character_storage.context = nullptr;

    buddy_character_store_init();
    buddy_prefs_init();
    g_app.set_platform_hooks(hooks);
    buddy_ble_nus_set_rx_callback(buddy_app_on_ble_rx);
    buddy_ble_nus_set_passkey_callback(buddy_app_on_ble_passkey);
    g_app.init();
}

extern "C" void buddy_app_on_ble_rx(const uint8_t *data, uint16_t len)
{
    g_app.on_ble_rx(data, len);
}

extern "C" void buddy_app_on_ble_passkey(uint32_t passkey)
{
    g_app.on_ble_passkey(passkey);
}

extern "C" bool buddy_app_send_permission_once(void)
{
    return g_app.send_permission_once();
}

extern "C" bool buddy_app_send_permission_deny(void)
{
    return g_app.send_permission_deny();
}

extern "C" bool buddy_app_delete_character(void)
{
    return g_app.delete_character();
}

extern "C" bool buddy_app_factory_reset(void)
{
    return g_app.factory_reset();
}

extern "C" bool buddy_app_set_species(uint8_t species)
{
    return g_app.set_species(species);
}

extern "C" bool buddy_app_record_nap_end(uint32_t seconds)
{
    return g_app.record_nap_end(seconds);
}

extern "C" bool buddy_app_get_pairing_passkey(uint32_t *out_passkey)
{
    if (out_passkey == nullptr || !g_app.has_pairing_passkey())
    {
        return false;
    }

    *out_passkey = g_app.last_pairing_passkey();
    return true;
}

extern "C" bool buddy_app_get_ui_model(buddy_ui_model_t *out_model)
{
    if (out_model == nullptr)
    {
        return false;
    }

    memset(out_model, 0, sizeof(*out_model));

    const buddy::BuddyProtocol::RuntimeStatus status = g_app.runtime_status();
    const buddy::BuddyProtocol::Snapshot &snapshot = g_app.snapshot();
    const buddy::BuddyProtocol::PetStatsView pet = g_app.pet_stats_view();

    out_model->connected = status.connected;
    out_model->encrypted = status.encrypted;
    out_model->has_prompt = snapshot.has_prompt;
    out_model->has_pairing_passkey = g_app.has_pairing_passkey();
    out_model->persona = buddy_app_persona_from_status(snapshot, status);
    out_model->entry_count = snapshot.entry_count;
    if (out_model->entry_count > BUDDY_UI_MAX_ENTRIES)
    {
        out_model->entry_count = BUDDY_UI_MAX_ENTRIES;
    }

    out_model->total = snapshot.total;
    out_model->running = snapshot.running;
    out_model->waiting = snapshot.waiting;
    out_model->tokens = snapshot.tokens;
    out_model->tokens_today = snapshot.tokens_today;
    out_model->last_snapshot_ms = snapshot.last_snapshot_ms;
    out_model->prompt_started_ms = snapshot.prompt_started_ms;
    out_model->uptime_ms = status.uptime_ms;
    out_model->rx_lines = status.rx_lines;
    out_model->rx_overflowed = status.rx_overflowed;
    out_model->pairing_passkey = g_app.last_pairing_passkey();
    out_model->pet_nap_seconds = pet.nap_seconds;
    out_model->pet_tokens = pet.tokens;
    out_model->pet_approvals = pet.approvals;
    out_model->pet_denials = pet.denials;
    out_model->pet_velocity_seconds = pet.median_velocity;
    out_model->pet_level = pet.level;
    out_model->pet_mood = pet.mood;
    out_model->pet_fed = pet.fed;
    out_model->pet_energy = pet.energy;
    out_model->species = g_app.current_species();
    out_model->species_count = buddy::BuddyProtocol::kAsciiSpeciesCount;
    out_model->gif_species = buddy::BuddyProtocol::kGifSpeciesSentinel;

    buddy_app_copy_string(out_model->device_name, sizeof(out_model->device_name), g_app.device_name());
    buddy_app_copy_string(out_model->owner, sizeof(out_model->owner), g_app.owner());
    buddy_app_copy_string(out_model->msg, sizeof(out_model->msg), snapshot.msg);
    buddy_app_copy_string(out_model->prompt_id, sizeof(out_model->prompt_id), snapshot.prompt_id);
    buddy_app_copy_string(out_model->prompt_tool, sizeof(out_model->prompt_tool), snapshot.prompt_tool);
    buddy_app_copy_string(out_model->prompt_hint, sizeof(out_model->prompt_hint), snapshot.prompt_hint);

    for (uint8_t i = 0; i < out_model->entry_count; ++i)
    {
        buddy_app_copy_string(out_model->entries[i], sizeof(out_model->entries[i]), snapshot.entries[i]);
    }

    return true;
}

extern "C" void buddy_app_tick(uint32_t now_ms)
{
    g_app.tick(now_ms);
}
