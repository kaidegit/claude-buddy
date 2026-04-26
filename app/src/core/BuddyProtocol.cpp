#include "BuddyProtocol.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "cJSON.h"

namespace buddy {

namespace {

const char *json_string(cJSON *root, const char *first, const char *second = nullptr)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, first);
    if ((!cJSON_IsString(item) || item->valuestring == nullptr) && second != nullptr)
    {
        item = cJSON_GetObjectItemCaseSensitive(root, second);
    }

    return cJSON_IsString(item) ? item->valuestring : nullptr;
}

uint32_t json_uint32(cJSON *root, const char *name, uint32_t fallback)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (!cJSON_IsNumber(item) || item->valuedouble < 0.0 || item->valuedouble > 4294967295.0)
    {
        return fallback;
    }

    return static_cast<uint32_t>(item->valuedouble);
}

bool json_int32(cJSON *root, const char *name, int32_t *out)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (out == nullptr || !cJSON_IsNumber(item) || item->valuedouble < -2147483648.0 ||
        item->valuedouble > 2147483647.0)
    {
        return false;
    }

    *out = static_cast<int32_t>(item->valuedouble);
    return true;
}

bool json_uint32_required(cJSON *root, const char *name, uint32_t *out)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (out == nullptr || !cJSON_IsNumber(item) || item->valuedouble < 0.0 ||
        item->valuedouble > 4294967295.0)
    {
        return false;
    }

    *out = static_cast<uint32_t>(item->valuedouble);
    return true;
}

bool json_uint32_optional(cJSON *root, const char *name, uint32_t *out)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (item == nullptr)
    {
        return false;
    }

    return json_uint32_required(root, name, out);
}

bool ascii_is_alnum(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

char ascii_lower(char c)
{
    if (c >= 'A' && c <= 'Z')
    {
        return static_cast<char>(c - 'A' + 'a');
    }

    return c;
}

int base64_value(char c)
{
    if (c >= 'A' && c <= 'Z')
    {
        return c - 'A';
    }
    if (c >= 'a' && c <= 'z')
    {
        return c - 'a' + 26;
    }
    if (c >= '0' && c <= '9')
    {
        return c - '0' + 52;
    }
    if (c == '+')
    {
        return 62;
    }
    if (c == '/')
    {
        return 63;
    }

    return -1;
}

uint8_t level_from_tokens(uint32_t tokens)
{
    const uint32_t level = tokens / BuddyProtocol::kTokensPerLevel;

    return static_cast<uint8_t>(level > 255U ? 255U : level);
}

const char *const kCharacterStates[] = {
    "sleep", "idle", "busy", "attention", "celebrate", "dizzy", "heart",
};

}  // namespace

void BuddyProtocol::set_hooks(const Hooks &hooks)
{
    hooks_ = hooks;
    load_persisted_identity();
    load_persisted_species();
    load_persisted_stats();
}

void BuddyProtocol::handle_line(const char *line, uint16_t len, const RuntimeStatus &status)
{
    cJSON *root;
    cJSON *cmd_item;
    cJSON *time_item;

    if (line == nullptr || len == 0)
    {
        return;
    }

    root = cJSON_ParseWithLength(line, len);
    if (root == nullptr)
    {
        return;
    }

    cmd_item = cJSON_GetObjectItemCaseSensitive(root, "cmd");
    if (cJSON_IsString(cmd_item) && cmd_item->valuestring != nullptr)
    {
        handle_command(root, cmd_item->valuestring, status);
    }
    else if ((time_item = cJSON_GetObjectItemCaseSensitive(root, "time")) != nullptr)
    {
        (void)time_item;
        handle_time(root, status, false);
    }
    else
    {
        handle_snapshot(root, status);
    }

    cJSON_Delete(root);
}

bool BuddyProtocol::send_permission_once(uint32_t now_ms)
{
    const bool sent = emit_permission("once");
    if (sent)
    {
        record_approval(now_ms);
    }
    return sent;
}

bool BuddyProtocol::send_permission_deny()
{
    const bool sent = emit_permission("deny");
    if (sent)
    {
        record_denial();
    }
    return sent;
}

bool BuddyProtocol::delete_character()
{
    if (hooks_.reset.delete_character == nullptr)
    {
        return false;
    }

    abort_character_transfer();
    return hooks_.reset.delete_character(hooks_.reset.context);
}

bool BuddyProtocol::factory_reset()
{
    bool ok;

    if (hooks_.reset.factory_reset == nullptr)
    {
        return false;
    }

    abort_character_transfer();
    ok = hooks_.reset.factory_reset(hooks_.reset.context);
    if (ok)
    {
        reset_identity();
        reset_species();
        reset_stats();
        snapshot_ = Snapshot();
        time_sync_ = TimeSync();
        permission_sent_ = false;
    }
    return ok;
}

bool BuddyProtocol::set_species(uint8_t species)
{
    if (!is_valid_species(species))
    {
        return false;
    }

    species_ = species;
    return save_species();
}

bool BuddyProtocol::record_nap_end(uint32_t seconds, uint32_t now_ms)
{
    if (4294967295U - pet_stats_.nap_seconds < seconds)
    {
        pet_stats_.nap_seconds = 4294967295U;
    }
    else
    {
        pet_stats_.nap_seconds += seconds;
    }
    last_nap_end_ms_ = now_ms;
    energy_at_nap_ = 5;
    return save_stats();
}

const char *BuddyProtocol::device_name() const
{
    return device_name_;
}

const char *BuddyProtocol::owner() const
{
    return owner_;
}

const char *BuddyProtocol::current_prompt_id() const
{
    return snapshot_.prompt_id;
}

uint8_t BuddyProtocol::current_species() const
{
    return species_;
}

const BuddyProtocol::Snapshot &BuddyProtocol::snapshot() const
{
    return snapshot_;
}

const BuddyProtocol::TimeSync &BuddyProtocol::time_sync() const
{
    return time_sync_;
}

const BuddyProtocol::PetStats &BuddyProtocol::pet_stats() const
{
    return pet_stats_;
}

BuddyProtocol::PetStatsView BuddyProtocol::pet_stats_view(uint32_t now_ms) const
{
    PetStatsView view;

    view.nap_seconds = pet_stats_.nap_seconds;
    view.approvals = pet_stats_.approvals;
    view.denials = pet_stats_.denials;
    view.median_velocity = stats_median_velocity();
    view.level = pet_stats_.level;
    view.tokens = pet_stats_.tokens;
    view.mood = stats_mood_tier();
    view.fed = stats_fed_progress();
    view.energy = stats_energy_tier(now_ms);
    return view;
}

bool BuddyProtocol::handle_command(void *json_root, const char *cmd, const RuntimeStatus &status)
{
    cJSON *root = static_cast<cJSON *>(json_root);

    if (strcmp(cmd, "status") == 0)
    {
        return emit_status(status);
    }

    if (strcmp(cmd, "name") == 0)
    {
        const char *name = json_string(root, "name", "value");
        if (name != nullptr)
        {
            copy_string(device_name_, sizeof(device_name_), name);
            if (!save_identity())
            {
                return emit_simple_ack("name", false, "persist_failed");
            }
        }
        return emit_simple_ack("name", true, nullptr);
    }

    if (strcmp(cmd, "owner") == 0)
    {
        const char *new_owner = json_string(root, "owner", "name");
        if (new_owner == nullptr)
        {
            new_owner = json_string(root, "value");
        }
        if (new_owner != nullptr)
        {
            copy_string(owner_, sizeof(owner_), new_owner);
            if (!save_identity())
            {
                return emit_simple_ack("owner", false, "persist_failed");
            }
        }
        return emit_simple_ack("owner", true, nullptr);
    }

    if (strcmp(cmd, "time") == 0)
    {
        return handle_time(root, status, true);
    }

    if (strcmp(cmd, "unpair") == 0)
    {
        const bool ok = hooks_.unpair == nullptr || hooks_.unpair(hooks_.context) >= 0;
        return emit_simple_ack("unpair", ok, ok ? nullptr : "unpair_failed");
    }

    if (strcmp(cmd, "species") == 0)
    {
        return handle_species(root);
    }

    if (strcmp(cmd, "delete_character") == 0 || strcmp(cmd, "char_delete") == 0)
    {
        const bool ok = delete_character();
        return emit_simple_ack(cmd, ok, ok ? nullptr : "delete_failed");
    }

    if (strcmp(cmd, "factory_reset") == 0)
    {
        const bool ok = factory_reset();
        return emit_simple_ack("factory_reset", ok, ok ? nullptr : "reset_failed");
    }

    if (strcmp(cmd, "char_begin") == 0)
    {
        return handle_character_begin(root);
    }

    if (strcmp(cmd, "file") == 0)
    {
        return handle_character_file(root);
    }

    if (strcmp(cmd, "chunk") == 0)
    {
        return handle_character_chunk(root);
    }

    if (strcmp(cmd, "file_end") == 0)
    {
        return handle_character_file_end(root);
    }

    if (strcmp(cmd, "char_end") == 0)
    {
        return handle_character_end(root);
    }

    return emit_simple_ack(cmd, false, "unsupported");
}

bool BuddyProtocol::handle_snapshot(void *json_root, const RuntimeStatus &status)
{
    cJSON *root = static_cast<cJSON *>(json_root);
    cJSON *prompt = cJSON_GetObjectItemCaseSensitive(root, "prompt");
    cJSON *entries = cJSON_GetObjectItemCaseSensitive(root, "entries");
    const char *msg = json_string(root, "msg");
    const char *previous_prompt_id = snapshot_.prompt_id;
    char previous_prompt_id_copy[kPromptIdLength];
    uint32_t bridge_tokens = 0;

    copy_string(previous_prompt_id_copy, sizeof(previous_prompt_id_copy), previous_prompt_id);

    snapshot_.total = json_uint32(root, "total", 0);
    snapshot_.running = json_uint32(root, "running", 0);
    snapshot_.waiting = json_uint32(root, "waiting", 0);
    snapshot_.tokens = json_uint32(root, "tokens", 0);
    snapshot_.tokens_today = json_uint32(root, "tokens_today", 0);
    snapshot_.last_snapshot_ms = status.uptime_ms;
    copy_string(snapshot_.msg, sizeof(snapshot_.msg), msg);
    if (json_uint32_optional(root, "tokens", &bridge_tokens))
    {
        update_bridge_tokens(bridge_tokens);
    }

    snapshot_.entry_count = 0;
    for (uint8_t i = 0; i < kMaxSnapshotEntries; ++i)
    {
        snapshot_.entries[i][0] = '\0';
    }
    if (cJSON_IsArray(entries))
    {
        cJSON *entry = nullptr;
        cJSON_ArrayForEach(entry, entries)
        {
            if (snapshot_.entry_count >= kMaxSnapshotEntries)
            {
                break;
            }
            if (cJSON_IsString(entry) && entry->valuestring != nullptr)
            {
                copy_string(snapshot_.entries[snapshot_.entry_count],
                            sizeof(snapshot_.entries[snapshot_.entry_count]), entry->valuestring);
                ++snapshot_.entry_count;
            }
        }
    }

    if (cJSON_IsObject(prompt))
    {
        const char *id = json_string(prompt, "id");
        if (id == nullptr)
        {
            clear_prompt();
        }
        else
        {
            snapshot_.has_prompt = true;
            copy_string(snapshot_.prompt_id, sizeof(snapshot_.prompt_id), id);
            copy_string(snapshot_.prompt_tool, sizeof(snapshot_.prompt_tool), json_string(prompt, "tool"));
            copy_string(snapshot_.prompt_hint, sizeof(snapshot_.prompt_hint), json_string(prompt, "hint"));
            if (strcmp(previous_prompt_id_copy, snapshot_.prompt_id) != 0)
            {
                snapshot_.prompt_started_ms = status.uptime_ms;
                permission_sent_ = false;
            }
        }
    }
    else
    {
        clear_prompt();
    }

    return true;
}

bool BuddyProtocol::handle_time(void *json_root, const RuntimeStatus &status, bool ack)
{
    const bool ok = parse_time_payload(json_root, status);

    if (!ack)
    {
        return ok;
    }

    return emit_simple_ack("time", ok, ok ? nullptr : "bad_time");
}

bool BuddyProtocol::parse_time_payload(void *json_root, const RuntimeStatus &status)
{
    cJSON *root = static_cast<cJSON *>(json_root);
    cJSON *time = cJSON_GetObjectItemCaseSensitive(root, "time");
    cJSON *epoch_item;
    cJSON *offset_item;
    int32_t timezone_offset = 0;

    if (cJSON_IsArray(time) && cJSON_GetArraySize(time) >= 2)
    {
        epoch_item = cJSON_GetArrayItem(time, 0);
        offset_item = cJSON_GetArrayItem(time, 1);
        if (!cJSON_IsNumber(epoch_item) || !cJSON_IsNumber(offset_item) ||
            epoch_item->valuedouble < 0.0 || epoch_item->valuedouble > 4294967295.0 ||
            offset_item->valuedouble < -2147483648.0 || offset_item->valuedouble > 2147483647.0)
        {
            return false;
        }

        time_sync_.epoch_seconds = static_cast<uint32_t>(epoch_item->valuedouble);
        time_sync_.timezone_offset_seconds = static_cast<int32_t>(offset_item->valuedouble);
        time_sync_.last_sync_ms = status.uptime_ms;
        time_sync_.synced = true;
        return true;
    }

    if (json_int32(root, "timezone_offset", &timezone_offset) ||
        json_int32(root, "tz", &timezone_offset) ||
        json_int32(root, "offset", &timezone_offset))
    {
        const uint32_t epoch = json_uint32(root, "epoch", 0);
        if (epoch == 0)
        {
            return false;
        }

        time_sync_.epoch_seconds = epoch;
        time_sync_.timezone_offset_seconds = timezone_offset;
        time_sync_.last_sync_ms = status.uptime_ms;
        time_sync_.synced = true;
        return true;
    }

    return false;
}

bool BuddyProtocol::handle_character_begin(void *json_root)
{
    cJSON *root = static_cast<cJSON *>(json_root);
    const char *name = json_string(root, "name");
    uint32_t total_size = 0;
    char safe_name[kCharacterNameLength];

    if (!storage_ready())
    {
        return emit_ack("char_begin", false, 0, "storage_unavailable");
    }

    if (character_transfer_.active)
    {
        return emit_ack("char_begin", false, 0, "transfer_active");
    }

    if (!json_uint32_required(root, "total", &total_size) || total_size == 0)
    {
        return emit_ack("char_begin", false, 0, "bad_total");
    }

    sanitize_character_name(safe_name, sizeof(safe_name), name);
    if (safe_name[0] == '\0')
    {
        return emit_ack("char_begin", false, 0, "bad_name");
    }

    if (!hooks_.character_storage.begin(safe_name, total_size, hooks_.character_storage.context))
    {
        return emit_ack("char_begin", false, 0, "no_space");
    }

    reset_character_transfer();
    character_transfer_.active = true;
    character_transfer_.total_expected = total_size;
    copy_string(character_transfer_.safe_name, sizeof(character_transfer_.safe_name), safe_name);
    copy_string(character_transfer_.display_name, sizeof(character_transfer_.display_name),
                name != nullptr ? name : safe_name);
    return emit_ack("char_begin", true, 0, nullptr);
}

bool BuddyProtocol::handle_character_file(void *json_root)
{
    cJSON *root = static_cast<cJSON *>(json_root);
    const char *path = json_string(root, "path", "name");
    uint32_t expected_size = 0;

    if (!character_transfer_.active)
    {
        return emit_ack("file", false, 0, "no_transfer");
    }

    if (character_transfer_.file_open)
    {
        abort_character_transfer();
        return emit_ack("file", false, 0, "file_open");
    }

    if (!is_valid_character_path(path))
    {
        abort_character_transfer();
        return emit_ack("file", false, 0, "bad_path");
    }

    if (!json_uint32_required(root, "size", &expected_size) ||
        expected_size > character_transfer_.total_expected - character_transfer_.total_written)
    {
        abort_character_transfer();
        return emit_ack("file", false, 0, "bad_size");
    }

    if (!hooks_.character_storage.open_file(character_transfer_.safe_name, path, expected_size,
                                           hooks_.character_storage.context))
    {
        abort_character_transfer();
        return emit_ack("file", false, 0, "open_failed");
    }

    character_transfer_.file_open = true;
    character_transfer_.file_expected = expected_size;
    character_transfer_.file_written = 0;
    copy_string(character_transfer_.file_path, sizeof(character_transfer_.file_path), path);
    return emit_ack("file", true, 0, nullptr);
}

bool BuddyProtocol::handle_character_chunk(void *json_root)
{
    cJSON *root = static_cast<cJSON *>(json_root);
    const char *b64 = json_string(root, "d");
    uint8_t decoded[kMaxDecodedChunkLength];
    uint16_t decoded_len = 0;

    if (!character_transfer_.active || !character_transfer_.file_open)
    {
        return emit_ack("chunk", false, 0, "no_file");
    }

    if (b64 == nullptr || !decode_base64(b64, decoded, sizeof(decoded), &decoded_len))
    {
        const uint32_t written = character_transfer_.file_written;
        abort_character_transfer();
        return emit_ack("chunk", false, written, "bad_base64");
    }

    if (decoded_len > character_transfer_.file_expected - character_transfer_.file_written ||
        decoded_len > character_transfer_.total_expected - character_transfer_.total_written)
    {
        const uint32_t written = character_transfer_.file_written;
        abort_character_transfer();
        return emit_ack("chunk", false, written, "size_mismatch");
    }

    if (decoded_len > 0 &&
        !hooks_.character_storage.write(decoded, decoded_len, hooks_.character_storage.context))
    {
        const uint32_t written = character_transfer_.file_written;
        abort_character_transfer();
        return emit_ack("chunk", false, written, "write_failed");
    }

    character_transfer_.file_written += decoded_len;
    character_transfer_.total_written += decoded_len;
    return emit_ack("chunk", true, character_transfer_.file_written, nullptr);
}

bool BuddyProtocol::handle_character_file_end(void *json_root)
{
    cJSON *root = static_cast<cJSON *>(json_root);
    cJSON *reported_item = cJSON_GetObjectItemCaseSensitive(root, "n");
    uint32_t reported_size = 0;
    const bool has_reported_size = reported_item != nullptr;

    if (!character_transfer_.active || !character_transfer_.file_open)
    {
        return emit_ack("file_end", false, 0, "no_file");
    }

    if (has_reported_size && !json_uint32_required(root, "n", &reported_size))
    {
        const uint32_t written = character_transfer_.file_written;
        abort_character_transfer();
        return emit_ack("file_end", false, written, "bad_size");
    }

    if (character_transfer_.file_written != character_transfer_.file_expected ||
        (has_reported_size && reported_size != character_transfer_.file_written))
    {
        const uint32_t written = character_transfer_.file_written;
        abort_character_transfer();
        return emit_ack("file_end", false, written, "size_mismatch");
    }

    if (!hooks_.character_storage.close_file(character_transfer_.safe_name, character_transfer_.file_path,
                                            character_transfer_.file_written,
                                            hooks_.character_storage.context))
    {
        const uint32_t written = character_transfer_.file_written;
        abort_character_transfer();
        return emit_ack("file_end", false, written, "close_failed");
    }

    if (strcmp(character_transfer_.file_path, "manifest.json") == 0)
    {
        character_transfer_.saw_manifest = true;
    }

    const uint32_t written = character_transfer_.file_written;
    character_transfer_.file_open = false;
    character_transfer_.file_expected = 0;
    character_transfer_.file_written = 0;
    character_transfer_.file_path[0] = '\0';
    return emit_ack("file_end", true, written, nullptr);
}

bool BuddyProtocol::handle_character_end(void *json_root)
{
    (void)json_root;
    char manifest[kCharacterManifestLength];
    uint32_t manifest_len = 0;
    char display_name[kCharacterNameLength];

    if (!character_transfer_.active)
    {
        return emit_ack("char_end", false, 0, "no_transfer");
    }

    if (character_transfer_.file_open)
    {
        abort_character_transfer();
        return emit_ack("char_end", false, 0, "file_open");
    }

    if (character_transfer_.total_written != character_transfer_.total_expected)
    {
        abort_character_transfer();
        return emit_ack("char_end", false, 0, "total_mismatch");
    }

    if (!character_transfer_.saw_manifest ||
        !hooks_.character_storage.read_file(character_transfer_.safe_name, "manifest.json", manifest,
                                           sizeof(manifest), &manifest_len,
                                           hooks_.character_storage.context))
    {
        abort_character_transfer();
        return emit_ack("char_end", false, 0, "missing_manifest");
    }

    copy_string(display_name, sizeof(display_name), character_transfer_.display_name);
    if (!validate_character_manifest(manifest, manifest_len, display_name, sizeof(display_name)))
    {
        abort_character_transfer();
        return emit_ack("char_end", false, 0, "bad_manifest");
    }

    if (!hooks_.character_storage.commit(character_transfer_.safe_name, display_name,
                                         hooks_.character_storage.context))
    {
        abort_character_transfer();
        return emit_ack("char_end", false, 0, "commit_failed");
    }

    reset_character_transfer();
    return emit_ack("char_end", true, 0, nullptr);
}

bool BuddyProtocol::handle_species(void *json_root)
{
    cJSON *root = static_cast<cJSON *>(json_root);
    uint32_t species = kGifSpeciesSentinel;

    if (!json_uint32_required(root, "idx", &species) || species > 0xFFU ||
        !is_valid_species(static_cast<uint8_t>(species)))
    {
        return emit_simple_ack("species", false, "bad_species");
    }

    if (!set_species(static_cast<uint8_t>(species)))
    {
        return emit_simple_ack("species", false, "persist_failed");
    }

    return emit_simple_ack("species", true, nullptr);
}

void BuddyProtocol::update_bridge_tokens(uint32_t bridge_total)
{
    if (!tokens_synced_)
    {
        last_bridge_tokens_ = bridge_total;
        tokens_synced_ = true;
        return;
    }

    if (bridge_total < last_bridge_tokens_)
    {
        last_bridge_tokens_ = bridge_total;
        return;
    }

    const uint32_t delta = bridge_total - last_bridge_tokens_;
    last_bridge_tokens_ = bridge_total;
    if (delta == 0)
    {
        return;
    }

    const uint8_t level_before = level_from_tokens(pet_stats_.tokens);
    if (4294967295U - pet_stats_.tokens < delta)
    {
        pet_stats_.tokens = 4294967295U;
    }
    else
    {
        pet_stats_.tokens += delta;
    }

    const uint8_t level_after = level_from_tokens(pet_stats_.tokens);
    if (level_after > level_before)
    {
        pet_stats_.level = level_after;
        (void)save_stats();
    }
}

void BuddyProtocol::record_approval(uint32_t now_ms)
{
    uint32_t seconds_to_respond = 0;

    if (snapshot_.prompt_started_ms != 0 && now_ms >= snapshot_.prompt_started_ms)
    {
        seconds_to_respond = (now_ms - snapshot_.prompt_started_ms) / 1000U;
    }

    if (pet_stats_.approvals < 65535U)
    {
        ++pet_stats_.approvals;
    }
    pet_stats_.velocity[pet_stats_.velocity_index] =
        static_cast<uint16_t>(seconds_to_respond > 65535U ? 65535U : seconds_to_respond);
    pet_stats_.velocity_index =
        static_cast<uint8_t>((pet_stats_.velocity_index + 1U) % kVelocitySampleCount);
    if (pet_stats_.velocity_count < kVelocitySampleCount)
    {
        ++pet_stats_.velocity_count;
    }
    (void)save_stats();
}

void BuddyProtocol::record_denial()
{
    if (pet_stats_.denials < 65535U)
    {
        ++pet_stats_.denials;
    }
    (void)save_stats();
}

bool BuddyProtocol::validate_character_manifest(const char *manifest, uint32_t manifest_len,
                                                char *display_name, uint16_t display_name_size)
{
    cJSON *root;
    cJSON *states;
    const char *manifest_name;
    bool ok = true;

    if (manifest == nullptr || manifest_len == 0)
    {
        return false;
    }

    root = cJSON_ParseWithLength(manifest, manifest_len);
    if (root == nullptr || !cJSON_IsObject(root))
    {
        if (root != nullptr)
        {
            cJSON_Delete(root);
        }
        return false;
    }

    states = cJSON_GetObjectItemCaseSensitive(root, "states");
    if (!cJSON_IsObject(states))
    {
        states = root;
    }

    for (uint8_t i = 0; i < sizeof(kCharacterStates) / sizeof(kCharacterStates[0]); ++i)
    {
        if (!validate_manifest_state(states, kCharacterStates[i]))
        {
            ok = false;
            break;
        }
    }

    if (ok)
    {
        manifest_name = json_string(root, "name");
        if (manifest_name != nullptr && manifest_name[0] != '\0')
        {
            copy_string(display_name, display_name_size, manifest_name);
        }
    }

    cJSON_Delete(root);
    return ok;
}

bool BuddyProtocol::validate_manifest_state(void *states_root, const char *name)
{
    cJSON *states = static_cast<cJSON *>(states_root);
    cJSON *value = cJSON_GetObjectItemCaseSensitive(states, name);

    if (cJSON_IsString(value) && value->valuestring != nullptr)
    {
        return is_valid_character_path(value->valuestring) &&
               hooks_.character_storage.file_exists(character_transfer_.safe_name, value->valuestring,
                                                    hooks_.character_storage.context);
    }

    if (cJSON_IsArray(value))
    {
        cJSON *entry = nullptr;
        bool saw_entry = false;

        cJSON_ArrayForEach(entry, value)
        {
            if (!cJSON_IsString(entry) || entry->valuestring == nullptr ||
                !is_valid_character_path(entry->valuestring) ||
                !hooks_.character_storage.file_exists(character_transfer_.safe_name,
                                                     entry->valuestring,
                                                     hooks_.character_storage.context))
            {
                return false;
            }
            saw_entry = true;
        }

        return saw_entry;
    }

    return false;
}

bool BuddyProtocol::decode_base64(const char *src, uint8_t *dst, uint16_t dst_size,
                                  uint16_t *out_len) const
{
    size_t len;
    uint16_t written = 0;

    if (src == nullptr || dst == nullptr || out_len == nullptr)
    {
        return false;
    }

    len = strlen(src);
    if (len == 0)
    {
        *out_len = 0;
        return true;
    }

    if ((len % 4U) != 0U)
    {
        return false;
    }

    for (size_t i = 0; i < len; i += 4U)
    {
        const bool last_block = (i + 4U == len);
        const bool pad2 = src[i + 2U] == '=';
        const bool pad3 = src[i + 3U] == '=';
        int v0;
        int v1;
        int v2 = 0;
        int v3 = 0;
        uint8_t out[3];
        uint8_t out_count = 3;

        if (src[i] == '=' || src[i + 1U] == '=' || (!last_block && (pad2 || pad3)) ||
            (pad2 && !pad3))
        {
            return false;
        }

        v0 = base64_value(src[i]);
        v1 = base64_value(src[i + 1U]);
        if (v0 < 0 || v1 < 0)
        {
            return false;
        }

        if (pad2)
        {
            out_count = 1;
        }
        else
        {
            v2 = base64_value(src[i + 2U]);
            if (v2 < 0)
            {
                return false;
            }
            out_count = pad3 ? 2 : 3;
        }

        if (!pad3)
        {
            v3 = base64_value(src[i + 3U]);
            if (v3 < 0)
            {
                return false;
            }
        }

        if (written + out_count > dst_size)
        {
            return false;
        }

        out[0] = static_cast<uint8_t>((v0 << 2) | (v1 >> 4));
        out[1] = static_cast<uint8_t>(((v1 & 0x0F) << 4) | (v2 >> 2));
        out[2] = static_cast<uint8_t>(((v2 & 0x03) << 6) | v3);
        for (uint8_t j = 0; j < out_count; ++j)
        {
            dst[written++] = out[j];
        }
    }

    *out_len = written;
    return true;
}

bool BuddyProtocol::emit_json(void *json_root)
{
    cJSON *root = static_cast<cJSON *>(json_root);
    char *printed;
    char line[1024];
    uint16_t len;

    if (hooks_.send == nullptr)
    {
        return false;
    }

    printed = cJSON_PrintUnformatted(root);
    if (printed == nullptr)
    {
        return false;
    }

    const size_t printed_len = strlen(printed);
    if (printed_len + 1 >= sizeof(line))
    {
        cJSON_free(printed);
        return false;
    }

    memcpy(line, printed, printed_len);
    line[printed_len] = '\n';
    len = static_cast<uint16_t>(printed_len + 1);
    const int ret = hooks_.send(line, len, hooks_.context);
    cJSON_free(printed);
    return ret == len;
}

bool BuddyProtocol::emit_simple_ack(const char *ack, bool ok, const char *error)
{
    return emit_ack(ack, ok, 0, error);
}

bool BuddyProtocol::emit_ack(const char *ack, bool ok, uint32_t n, const char *error)
{
    cJSON *root = cJSON_CreateObject();
    bool sent;

    if (root == nullptr)
    {
        return false;
    }

    cJSON_AddStringToObject(root, "ack", ack);
    cJSON_AddBoolToObject(root, "ok", ok);
    cJSON_AddNumberToObject(root, "n", n);
    if (error != nullptr)
    {
        cJSON_AddStringToObject(root, "error", error);
    }

    sent = emit_json(root);
    cJSON_Delete(root);
    return sent;
}

bool BuddyProtocol::emit_status(const RuntimeStatus &status)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = nullptr;
    cJSON *sys = nullptr;
    cJSON *stats = nullptr;
    const PetStatsView pet = pet_stats_view(status.uptime_ms);
    bool sent = false;

    if (root == nullptr)
    {
        return false;
    }

    cJSON_AddStringToObject(root, "ack", "status");
    cJSON_AddBoolToObject(root, "ok", true);
    data = cJSON_AddObjectToObject(root, "data");
    if (data != nullptr)
    {
        cJSON_AddStringToObject(data, "name", device_name_);
        cJSON_AddStringToObject(data, "owner", owner_);
        cJSON_AddBoolToObject(data, "connected", status.connected);
        cJSON_AddBoolToObject(data, "sec", status.encrypted);
        cJSON_AddNumberToObject(data, "species", species_);
        cJSON_AddNumberToObject(data, "species_count", kAsciiSpeciesCount);
        cJSON_AddNumberToObject(data, "gif_species", kGifSpeciesSentinel);

        sys = cJSON_AddObjectToObject(data, "sys");
        if (sys != nullptr)
        {
            cJSON_AddNumberToObject(sys, "up", status.uptime_ms);
            cJSON_AddNumberToObject(sys, "tick_count", status.tick_count);
        }

        stats = cJSON_AddObjectToObject(data, "stats");
        if (stats != nullptr)
        {
            cJSON_AddNumberToObject(stats, "rx_lines", status.rx_lines);
            cJSON_AddNumberToObject(stats, "rx_overflowed", status.rx_overflowed);
            cJSON_AddNumberToObject(stats, "appr", pet.approvals);
            cJSON_AddNumberToObject(stats, "deny", pet.denials);
            cJSON_AddNumberToObject(stats, "vel", pet.median_velocity);
            cJSON_AddNumberToObject(stats, "nap", pet.nap_seconds);
            cJSON_AddNumberToObject(stats, "lvl", pet.level);
            cJSON_AddNumberToObject(stats, "tokens", pet.tokens);
            cJSON_AddNumberToObject(stats, "mood", pet.mood);
            cJSON_AddNumberToObject(stats, "fed", pet.fed);
            cJSON_AddNumberToObject(stats, "energy", pet.energy);
        }
    }

    sent = emit_json(root);
    cJSON_Delete(root);
    return sent;
}

bool BuddyProtocol::emit_permission(const char *decision)
{
    cJSON *root;
    bool sent;

    if (!snapshot_.has_prompt || snapshot_.prompt_id[0] == '\0' || permission_sent_)
    {
        return false;
    }

    root = cJSON_CreateObject();
    if (root == nullptr)
    {
        return false;
    }

    cJSON_AddStringToObject(root, "cmd", "permission");
    cJSON_AddStringToObject(root, "id", snapshot_.prompt_id);
    cJSON_AddStringToObject(root, "decision", decision);
    sent = emit_json(root);
    if (sent)
    {
        permission_sent_ = true;
    }
    cJSON_Delete(root);
    return sent;
}

void BuddyProtocol::load_persisted_identity()
{
    char name[sizeof(device_name_)] = "";
    char owner[sizeof(owner_)] = "";

    if (hooks_.prefs.load_identity == nullptr ||
        !hooks_.prefs.load_identity(name, sizeof(name), owner, sizeof(owner),
                                    hooks_.prefs.context))
    {
        return;
    }

    if (name[0] != '\0')
    {
        copy_string(device_name_, sizeof(device_name_), name);
    }
    copy_string(owner_, sizeof(owner_), owner);
}

void BuddyProtocol::load_persisted_species()
{
    uint8_t species = kGifSpeciesSentinel;

    if (hooks_.prefs.load_species == nullptr ||
        !hooks_.prefs.load_species(&species, hooks_.prefs.context) ||
        !is_valid_species(species))
    {
        return;
    }

    species_ = species;
}

void BuddyProtocol::load_persisted_stats()
{
    PetStats stats;

    if (hooks_.prefs.load_stats == nullptr ||
        !hooks_.prefs.load_stats(&stats, hooks_.prefs.context))
    {
        return;
    }

    if (stats.velocity_index >= kVelocitySampleCount)
    {
        stats.velocity_index = 0;
    }
    if (stats.velocity_count > kVelocitySampleCount)
    {
        stats.velocity_count = kVelocitySampleCount;
    }
    if (stats.tokens == 0 && stats.level > 0)
    {
        stats.tokens = static_cast<uint32_t>(stats.level) * kTokensPerLevel;
    }
    stats.level = level_from_tokens(stats.tokens);
    pet_stats_ = stats;
}

bool BuddyProtocol::save_identity() const
{
    if (hooks_.prefs.save_identity == nullptr)
    {
        return true;
    }

    return hooks_.prefs.save_identity(device_name_, owner_, hooks_.prefs.context);
}

bool BuddyProtocol::save_species() const
{
    if (hooks_.prefs.save_species == nullptr)
    {
        return true;
    }

    return hooks_.prefs.save_species(species_, hooks_.prefs.context);
}

bool BuddyProtocol::save_stats() const
{
    if (hooks_.prefs.save_stats == nullptr)
    {
        return true;
    }

    return hooks_.prefs.save_stats(&pet_stats_, hooks_.prefs.context);
}

void BuddyProtocol::reset_identity()
{
    copy_string(device_name_, sizeof(device_name_), "Claude Buddy");
    owner_[0] = '\0';
}

void BuddyProtocol::reset_species()
{
    species_ = kGifSpeciesSentinel;
}

void BuddyProtocol::reset_stats()
{
    pet_stats_ = PetStats();
    last_bridge_tokens_ = 0;
    tokens_synced_ = false;
    last_nap_end_ms_ = 0;
    energy_at_nap_ = 3;
}

bool BuddyProtocol::is_valid_species(uint8_t species) const
{
    return species == kGifSpeciesSentinel || species < kAsciiSpeciesCount;
}

uint16_t BuddyProtocol::stats_median_velocity() const
{
    if (pet_stats_.velocity_count == 0)
    {
        return 0;
    }

    uint16_t sorted[kVelocitySampleCount];
    const uint8_t count = pet_stats_.velocity_count > kVelocitySampleCount ?
                              kVelocitySampleCount :
                              pet_stats_.velocity_count;

    for (uint8_t i = 0; i < count; ++i)
    {
        sorted[i] = pet_stats_.velocity[i];
    }

    for (uint8_t i = 1; i < count; ++i)
    {
        const uint16_t value = sorted[i];
        int8_t j = static_cast<int8_t>(i) - 1;
        while (j >= 0 && sorted[j] > value)
        {
            sorted[j + 1] = sorted[j];
            --j;
        }
        sorted[j + 1] = value;
    }

    return sorted[count / 2U];
}

uint8_t BuddyProtocol::stats_mood_tier() const
{
    const uint16_t velocity = stats_median_velocity();
    int8_t tier;

    if (velocity == 0)
    {
        tier = 2;
    }
    else if (velocity < 15U)
    {
        tier = 4;
    }
    else if (velocity < 30U)
    {
        tier = 3;
    }
    else if (velocity < 60U)
    {
        tier = 2;
    }
    else if (velocity < 120U)
    {
        tier = 1;
    }
    else
    {
        tier = 0;
    }

    if (pet_stats_.approvals + pet_stats_.denials >= 3U)
    {
        if (pet_stats_.denials > pet_stats_.approvals)
        {
            tier -= 2;
        }
        else if (pet_stats_.denials * 2U > pet_stats_.approvals)
        {
            tier -= 1;
        }
    }

    if (tier < 0)
    {
        tier = 0;
    }
    return static_cast<uint8_t>(tier);
}

uint8_t BuddyProtocol::stats_fed_progress() const
{
    return static_cast<uint8_t>((pet_stats_.tokens % kTokensPerLevel) / (kTokensPerLevel / 10U));
}

uint8_t BuddyProtocol::stats_energy_tier(uint32_t now_ms) const
{
    uint32_t hours_since = 0;
    uint32_t drained;

    if (now_ms >= last_nap_end_ms_)
    {
        hours_since = (now_ms - last_nap_end_ms_) / 3600000U;
    }
    drained = hours_since / 2U;
    if (drained >= energy_at_nap_)
    {
        return 0;
    }
    return static_cast<uint8_t>(energy_at_nap_ - drained);
}

bool BuddyProtocol::storage_ready() const
{
    return hooks_.character_storage.begin != nullptr &&
           hooks_.character_storage.open_file != nullptr &&
           hooks_.character_storage.write != nullptr &&
           hooks_.character_storage.close_file != nullptr &&
           hooks_.character_storage.read_file != nullptr &&
           hooks_.character_storage.file_exists != nullptr &&
           hooks_.character_storage.commit != nullptr &&
           hooks_.character_storage.abort != nullptr;
}

void BuddyProtocol::abort_character_transfer()
{
    if (character_transfer_.active && hooks_.character_storage.abort != nullptr)
    {
        hooks_.character_storage.abort(hooks_.character_storage.context);
    }

    reset_character_transfer();
}

void BuddyProtocol::reset_character_transfer()
{
    character_transfer_.active = false;
    character_transfer_.file_open = false;
    character_transfer_.saw_manifest = false;
    character_transfer_.total_expected = 0;
    character_transfer_.total_written = 0;
    character_transfer_.file_expected = 0;
    character_transfer_.file_written = 0;
    character_transfer_.safe_name[0] = '\0';
    character_transfer_.display_name[0] = '\0';
    character_transfer_.file_path[0] = '\0';
}

void BuddyProtocol::clear_prompt()
{
    snapshot_.has_prompt = false;
    snapshot_.prompt_id[0] = '\0';
    snapshot_.prompt_tool[0] = '\0';
    snapshot_.prompt_hint[0] = '\0';
    snapshot_.prompt_started_ms = 0;
    permission_sent_ = false;
}

void BuddyProtocol::sanitize_character_name(char *dst, uint16_t dst_size, const char *src) const
{
    uint16_t out = 0;
    bool previous_dash = false;

    if (dst == nullptr || dst_size == 0)
    {
        return;
    }

    dst[0] = '\0';
    if (src == nullptr || src[0] == '\0')
    {
        copy_string(dst, dst_size, "pet");
        return;
    }

    for (const char *p = src; *p != '\0' && out + 1U < dst_size; ++p)
    {
        char c = *p;
        if (ascii_is_alnum(c))
        {
            dst[out++] = ascii_lower(c);
            previous_dash = false;
        }
        else if (c == '-' || c == '_' || c == ' ' || c == '.')
        {
            const char replacement = c == '_' ? '_' : '-';
            if (!previous_dash && out > 0)
            {
                dst[out++] = replacement;
                previous_dash = replacement == '-';
            }
        }
    }

    while (out > 0 && dst[out - 1U] == '-')
    {
        --out;
    }

    dst[out] = '\0';
    if (dst[0] == '\0')
    {
        copy_string(dst, dst_size, "pet");
    }
}

bool BuddyProtocol::is_valid_character_path(const char *path) const
{
    size_t len = 0;

    if (path == nullptr || path[0] == '\0' || path[0] == '.')
    {
        return false;
    }

    if (strcmp(path, ".") == 0 || strcmp(path, "..") == 0)
    {
        return false;
    }

    for (const char *p = path; *p != '\0'; ++p)
    {
        const char c = *p;
        if (c == '/' || c == '\\' || c < 0x21 || c > 0x7E)
        {
            return false;
        }

        if (!ascii_is_alnum(c) && c != '_' && c != '-' && c != '.')
        {
            return false;
        }

        ++len;
        if (len >= kCharacterPathLength)
        {
            return false;
        }
    }

    return true;
}

void BuddyProtocol::copy_string(char *dst, uint16_t dst_size, const char *src) const
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

}  // namespace buddy
