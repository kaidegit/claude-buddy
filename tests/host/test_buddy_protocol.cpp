#include "BuddyProtocol.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

struct Harness {
    std::vector<std::string> sent;
    int unpair_calls = 0;
};

struct PrefStore {
    bool load_ok = true;
    bool save_ok = true;
    int save_calls = 0;
    int species_save_calls = 0;
    int stats_save_calls = 0;
    std::string name;
    std::string owner;
    uint8_t species = buddy::BuddyProtocol::kGifSpeciesSentinel;
    buddy::BuddyProtocol::PetStats stats;
};

struct ResetStore {
    bool delete_ok = true;
    bool factory_ok = true;
    int delete_calls = 0;
    int factory_calls = 0;
};

struct CharacterStore {
    uint32_t free_bytes = 2 * 1024 * 1024;
    bool active = false;
    int aborts = 0;
    std::string safe_name;
    std::string display_name;
    std::string current_file;
    std::map<std::string, std::string> incoming;
    std::map<std::string, std::string> committed;
};

int send_line(const char *line, uint16_t len, void *context)
{
    auto *harness = static_cast<Harness *>(context);
    harness->sent.emplace_back(line, len);
    return len;
}

int unpair(void *context)
{
    auto *harness = static_cast<Harness *>(context);
    ++harness->unpair_calls;
    return 0;
}

bool prefs_load_identity(char *name, uint16_t name_size, char *owner, uint16_t owner_size,
                         void *context)
{
    auto *store = static_cast<PrefStore *>(context);
    if (store == nullptr || !store->load_ok || name == nullptr || name_size == 0 ||
        owner == nullptr || owner_size == 0)
    {
        return false;
    }

    std::strncpy(name, store->name.c_str(), name_size - 1U);
    name[name_size - 1U] = '\0';
    std::strncpy(owner, store->owner.c_str(), owner_size - 1U);
    owner[owner_size - 1U] = '\0';
    return true;
}

bool prefs_save_identity(const char *name, const char *owner, void *context)
{
    auto *store = static_cast<PrefStore *>(context);
    if (store == nullptr || !store->save_ok)
    {
        return false;
    }

    store->name = name != nullptr ? name : "";
    store->owner = owner != nullptr ? owner : "";
    ++store->save_calls;
    return true;
}

bool prefs_load_species(uint8_t *species, void *context)
{
    auto *store = static_cast<PrefStore *>(context);
    if (store == nullptr || !store->load_ok || species == nullptr)
    {
        return false;
    }

    *species = store->species;
    return true;
}

bool prefs_save_species(uint8_t species, void *context)
{
    auto *store = static_cast<PrefStore *>(context);
    if (store == nullptr || !store->save_ok)
    {
        return false;
    }

    store->species = species;
    ++store->species_save_calls;
    return true;
}

bool prefs_load_stats(buddy::BuddyProtocol::PetStats *stats, void *context)
{
    auto *store = static_cast<PrefStore *>(context);
    if (store == nullptr || !store->load_ok || stats == nullptr)
    {
        return false;
    }

    *stats = store->stats;
    return true;
}

bool prefs_save_stats(const buddy::BuddyProtocol::PetStats *stats, void *context)
{
    auto *store = static_cast<PrefStore *>(context);
    if (store == nullptr || !store->save_ok || stats == nullptr)
    {
        return false;
    }

    store->stats = *stats;
    ++store->stats_save_calls;
    return true;
}

bool reset_delete_character(void *context)
{
    auto *store = static_cast<ResetStore *>(context);
    if (store == nullptr)
    {
        return false;
    }

    ++store->delete_calls;
    return store->delete_ok;
}

bool reset_factory(void *context)
{
    auto *store = static_cast<ResetStore *>(context);
    if (store == nullptr)
    {
        return false;
    }

    ++store->factory_calls;
    return store->factory_ok;
}

buddy::BuddyProtocol::RuntimeStatus status()
{
    buddy::BuddyProtocol::RuntimeStatus runtime;
    runtime.connected = true;
    runtime.encrypted = true;
    runtime.uptime_ms = 1234;
    runtime.tick_count = 7;
    runtime.rx_lines = 3;
    runtime.rx_overflowed = 1;
    return runtime;
}

void handle(buddy::BuddyProtocol &protocol, const char *line)
{
    protocol.handle_line(line, static_cast<uint16_t>(std::strlen(line)), status());
}

bool contains(const std::string &text, const char *needle)
{
    return text.find(needle) != std::string::npos;
}

std::string b64(const std::string &input)
{
    static const char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;

    for (size_t i = 0; i < input.size(); i += 3)
    {
        const uint32_t b0 = static_cast<unsigned char>(input[i]);
        const uint32_t b1 = i + 1 < input.size() ? static_cast<unsigned char>(input[i + 1]) : 0;
        const uint32_t b2 = i + 2 < input.size() ? static_cast<unsigned char>(input[i + 2]) : 0;

        out.push_back(kAlphabet[b0 >> 2]);
        out.push_back(kAlphabet[((b0 & 0x03U) << 4) | (b1 >> 4)]);
        out.push_back(i + 1 < input.size() ? kAlphabet[((b1 & 0x0FU) << 2) | (b2 >> 6)] : '=');
        out.push_back(i + 2 < input.size() ? kAlphabet[b2 & 0x3FU] : '=');
    }

    return out;
}

bool character_begin(const char *safe_name, uint32_t total_size, void *context)
{
    auto *store = static_cast<CharacterStore *>(context);
    if (store == nullptr || total_size + 4096U > store->free_bytes)
    {
        return false;
    }

    store->active = true;
    store->safe_name = safe_name != nullptr ? safe_name : "";
    store->incoming.clear();
    store->current_file.clear();
    return true;
}

bool character_open_file(const char *, const char *path, uint32_t, void *context)
{
    auto *store = static_cast<CharacterStore *>(context);
    if (store == nullptr || path == nullptr || !store->active || !store->current_file.empty())
    {
        return false;
    }

    store->current_file = path;
    store->incoming[store->current_file].clear();
    return true;
}

bool character_write(const uint8_t *data, uint16_t len, void *context)
{
    auto *store = static_cast<CharacterStore *>(context);
    if (store == nullptr || !store->active || store->current_file.empty())
    {
        return false;
    }

    store->incoming[store->current_file].append(reinterpret_cast<const char *>(data), len);
    return true;
}

bool character_close_file(const char *, const char *, uint32_t, void *context)
{
    auto *store = static_cast<CharacterStore *>(context);
    if (store == nullptr || store->current_file.empty())
    {
        return false;
    }

    store->current_file.clear();
    return true;
}

bool character_read_file(const char *, const char *path, char *dst, uint16_t dst_size,
                         uint32_t *out_len, void *context)
{
    auto *store = static_cast<CharacterStore *>(context);
    if (store == nullptr || path == nullptr || dst == nullptr || out_len == nullptr)
    {
        return false;
    }

    const auto it = store->incoming.find(path);
    if (it == store->incoming.end() || it->second.size() + 1 > dst_size)
    {
        return false;
    }

    std::memcpy(dst, it->second.data(), it->second.size());
    dst[it->second.size()] = '\0';
    *out_len = static_cast<uint32_t>(it->second.size());
    return true;
}

bool character_file_exists(const char *, const char *path, void *context)
{
    auto *store = static_cast<CharacterStore *>(context);
    return store != nullptr && path != nullptr && store->incoming.find(path) != store->incoming.end();
}

bool character_commit(const char *, const char *display_name, void *context)
{
    auto *store = static_cast<CharacterStore *>(context);
    if (store == nullptr || !store->active)
    {
        return false;
    }

    store->committed = store->incoming;
    store->display_name = display_name != nullptr ? display_name : "";
    store->incoming.clear();
    store->active = false;
    return true;
}

void character_abort(void *context)
{
    auto *store = static_cast<CharacterStore *>(context);
    if (store != nullptr)
    {
        store->active = false;
        store->current_file.clear();
        store->incoming.clear();
        ++store->aborts;
    }
}

buddy::BuddyProtocol::CharacterStorageHooks storage_hooks(CharacterStore &store)
{
    buddy::BuddyProtocol::CharacterStorageHooks hooks;
    hooks.begin = character_begin;
    hooks.open_file = character_open_file;
    hooks.write = character_write;
    hooks.close_file = character_close_file;
    hooks.read_file = character_read_file;
    hooks.file_exists = character_file_exists;
    hooks.commit = character_commit;
    hooks.abort = character_abort;
    hooks.context = &store;
    return hooks;
}

buddy::BuddyProtocol::PreferenceHooks prefs_hooks(PrefStore &store)
{
    buddy::BuddyProtocol::PreferenceHooks hooks;
    hooks.load_identity = prefs_load_identity;
    hooks.save_identity = prefs_save_identity;
    hooks.load_species = prefs_load_species;
    hooks.save_species = prefs_save_species;
    hooks.load_stats = prefs_load_stats;
    hooks.save_stats = prefs_save_stats;
    hooks.context = &store;
    return hooks;
}

buddy::BuddyProtocol::ResetHooks reset_hooks(ResetStore &store)
{
    buddy::BuddyProtocol::ResetHooks hooks;
    hooks.delete_character = reset_delete_character;
    hooks.factory_reset = reset_factory;
    hooks.context = &store;
    return hooks;
}

buddy::BuddyProtocol::Hooks protocol_hooks(Harness &harness)
{
    buddy::BuddyProtocol::Hooks hooks;
    hooks.send = send_line;
    hooks.unpair = unpair;
    hooks.context = &harness;
    return hooks;
}

buddy::BuddyProtocol::Hooks protocol_hooks(Harness &harness,
                                           const buddy::BuddyProtocol::ResetHooks &reset)
{
    buddy::BuddyProtocol::Hooks hooks = protocol_hooks(harness);
    hooks.reset = reset;
    return hooks;
}

buddy::BuddyProtocol::Hooks protocol_hooks(Harness &harness,
                                           const buddy::BuddyProtocol::PreferenceHooks &prefs)
{
    buddy::BuddyProtocol::Hooks hooks = protocol_hooks(harness);
    hooks.prefs = prefs;
    return hooks;
}

buddy::BuddyProtocol::Hooks protocol_hooks(Harness &harness,
                                           const buddy::BuddyProtocol::CharacterStorageHooks &storage)
{
    buddy::BuddyProtocol::Hooks hooks = protocol_hooks(harness);
    hooks.character_storage = storage;
    return hooks;
}

void test_status_ack()
{
    Harness harness;
    buddy::BuddyProtocol protocol;
    protocol.set_hooks(protocol_hooks(harness));

    handle(protocol, "{\"cmd\":\"status\"}");

    assert(harness.sent.size() == 1);
    assert(contains(harness.sent[0], "\"ack\":\"status\""));
    assert(contains(harness.sent[0], "\"ok\":true"));
    assert(contains(harness.sent[0], "\"sec\":true"));
    assert(contains(harness.sent[0], "\"species\":255"));
    assert(contains(harness.sent[0], "\"species_count\":18"));
    assert(contains(harness.sent[0], "\"appr\":0"));
    assert(contains(harness.sent[0], "\"deny\":0"));
    assert(contains(harness.sent[0], "\"mood\":2"));
    assert(contains(harness.sent[0], "\"fed\":0"));
    assert(contains(harness.sent[0], "\"energy\":3"));
    assert(harness.sent[0].back() == '\n');
}

void test_name_and_owner()
{
    Harness harness;
    buddy::BuddyProtocol protocol;
    protocol.set_hooks(protocol_hooks(harness));

    handle(protocol, "{\"cmd\":\"name\",\"name\":\"Desk Buddy\"}");
    handle(protocol, "{\"cmd\":\"owner\",\"name\":\"Ada\"}");
    handle(protocol, "{\"cmd\":\"status\"}");

    assert(harness.sent.size() == 3);
    assert(contains(harness.sent[2], "\"name\":\"Desk Buddy\""));
    assert(contains(harness.sent[2], "\"owner\":\"Ada\""));
}

void test_identity_loads_from_preferences()
{
    Harness harness;
    PrefStore prefs;
    prefs.name = "Persisted Buddy";
    prefs.owner = "Grace";
    buddy::BuddyProtocol protocol;
    protocol.set_hooks(protocol_hooks(harness, prefs_hooks(prefs)));

    handle(protocol, "{\"cmd\":\"status\"}");

    assert(harness.sent.size() == 1);
    assert(contains(harness.sent[0], "\"name\":\"Persisted Buddy\""));
    assert(contains(harness.sent[0], "\"owner\":\"Grace\""));
    assert(prefs.save_calls == 0);
}

void test_species_loads_from_preferences()
{
    Harness harness;
    PrefStore prefs;
    prefs.species = 4;
    buddy::BuddyProtocol protocol;
    protocol.set_hooks(protocol_hooks(harness, prefs_hooks(prefs)));

    handle(protocol, "{\"cmd\":\"status\"}");

    assert(harness.sent.size() == 1);
    assert(contains(harness.sent[0], "\"species\":4"));
    assert(protocol.current_species() == 4);
    assert(prefs.species_save_calls == 0);
}

void test_identity_saves_to_preferences()
{
    Harness harness;
    PrefStore prefs;
    buddy::BuddyProtocol protocol;
    protocol.set_hooks(protocol_hooks(harness, prefs_hooks(prefs)));

    handle(protocol, "{\"cmd\":\"name\",\"name\":\"Desk Buddy\"}");
    handle(protocol, "{\"cmd\":\"owner\",\"name\":\"Ada\"}");

    assert(harness.sent.size() == 2);
    assert(contains(harness.sent[0], "\"ack\":\"name\""));
    assert(contains(harness.sent[0], "\"ok\":true"));
    assert(contains(harness.sent[1], "\"ack\":\"owner\""));
    assert(contains(harness.sent[1], "\"ok\":true"));
    assert(prefs.save_calls == 2);
    assert(prefs.name == "Desk Buddy");
    assert(prefs.owner == "Ada");
}

void test_identity_save_failure_is_reported()
{
    Harness harness;
    PrefStore prefs;
    prefs.save_ok = false;
    buddy::BuddyProtocol protocol;
    protocol.set_hooks(protocol_hooks(harness, prefs_hooks(prefs)));

    handle(protocol, "{\"cmd\":\"name\",\"name\":\"Desk Buddy\"}");

    assert(harness.sent.size() == 1);
    assert(contains(harness.sent[0], "\"ack\":\"name\""));
    assert(contains(harness.sent[0], "\"ok\":false"));
    assert(contains(harness.sent[0], "\"error\":\"persist_failed\""));
}

void test_species_command_saves_index()
{
    Harness harness;
    PrefStore prefs;
    buddy::BuddyProtocol protocol;
    protocol.set_hooks(protocol_hooks(harness, prefs_hooks(prefs)));

    handle(protocol, "{\"cmd\":\"species\",\"idx\":17}");
    handle(protocol, "{\"cmd\":\"status\"}");

    assert(harness.sent.size() == 2);
    assert(contains(harness.sent[0], "\"ack\":\"species\""));
    assert(contains(harness.sent[0], "\"ok\":true"));
    assert(contains(harness.sent[1], "\"species\":17"));
    assert(protocol.current_species() == 17);
    assert(prefs.species == 17);
    assert(prefs.species_save_calls == 1);
}

void test_species_command_accepts_gif_sentinel()
{
    Harness harness;
    PrefStore prefs;
    prefs.species = 2;
    buddy::BuddyProtocol protocol;
    protocol.set_hooks(protocol_hooks(harness, prefs_hooks(prefs)));

    handle(protocol, "{\"cmd\":\"species\",\"idx\":255}");

    assert(harness.sent.size() == 1);
    assert(contains(harness.sent[0], "\"ack\":\"species\""));
    assert(contains(harness.sent[0], "\"ok\":true"));
    assert(protocol.current_species() == buddy::BuddyProtocol::kGifSpeciesSentinel);
    assert(prefs.species == buddy::BuddyProtocol::kGifSpeciesSentinel);
}

void test_species_command_rejects_invalid_index()
{
    Harness harness;
    PrefStore prefs;
    prefs.species = 3;
    buddy::BuddyProtocol protocol;
    protocol.set_hooks(protocol_hooks(harness, prefs_hooks(prefs)));

    handle(protocol, "{\"cmd\":\"species\",\"idx\":18}");

    assert(harness.sent.size() == 1);
    assert(contains(harness.sent[0], "\"ack\":\"species\""));
    assert(contains(harness.sent[0], "\"ok\":false"));
    assert(contains(harness.sent[0], "\"error\":\"bad_species\""));
    assert(protocol.current_species() == 3);
    assert(prefs.species == 3);
    assert(prefs.species_save_calls == 0);
}

void test_permission_decisions_update_pet_stats()
{
    Harness harness;
    PrefStore prefs;
    buddy::BuddyProtocol protocol;
    protocol.set_hooks(protocol_hooks(harness, prefs_hooks(prefs)));

    handle(protocol, "{\"prompt\":{\"id\":\"req_1\"}}");
    assert(protocol.send_permission_once(6234));

    const auto fast = protocol.pet_stats_view(6234);
    assert(fast.approvals == 1);
    assert(fast.denials == 0);
    assert(fast.median_velocity == 5);
    assert(fast.mood == 4);
    assert(prefs.stats_save_calls == 1);

    handle(protocol, "{\"prompt\":{\"id\":\"req_2\"}}");
    assert(protocol.send_permission_deny());

    const auto denied = protocol.pet_stats_view(7000);
    assert(denied.approvals == 1);
    assert(denied.denials == 1);
    assert(prefs.stats_save_calls == 2);
}

void test_token_delta_updates_level_and_fed()
{
    Harness harness;
    PrefStore prefs;
    buddy::BuddyProtocol protocol;
    protocol.set_hooks(protocol_hooks(harness, prefs_hooks(prefs)));

    handle(protocol, "{\"tokens\":100000,\"tokens_today\":100000}");
    assert(protocol.pet_stats_view(1234).tokens == 0);
    assert(prefs.stats_save_calls == 0);

    handle(protocol, "{\"tokens\":156000,\"tokens_today\":156000}");

    const auto view = protocol.pet_stats_view(1234);
    assert(view.tokens == 56000);
    assert(view.level == 1);
    assert(view.fed == 1);
    assert(prefs.stats_save_calls == 1);
}

void test_stats_load_and_nap_energy()
{
    Harness harness;
    PrefStore prefs;
    prefs.stats.approvals = 3;
    prefs.stats.denials = 1;
    prefs.stats.velocity[0] = 3;
    prefs.stats.velocity[1] = 20;
    prefs.stats.velocity[2] = 8;
    prefs.stats.velocity_count = 3;
    prefs.stats.tokens = 125000;
    buddy::BuddyProtocol protocol;
    protocol.set_hooks(protocol_hooks(harness, prefs_hooks(prefs)));

    auto loaded = protocol.pet_stats_view(0);
    assert(loaded.approvals == 3);
    assert(loaded.denials == 1);
    assert(loaded.median_velocity == 8);
    assert(loaded.level == 2);
    assert(loaded.fed == 5);

    assert(protocol.record_nap_end(3660, 1000));
    auto rested = protocol.pet_stats_view(1000);
    assert(rested.nap_seconds == 3660);
    assert(rested.energy == 5);
    auto drained = protocol.pet_stats_view(1000 + 4U * 3600000U);
    assert(drained.energy == 3);
    assert(prefs.stats_save_calls == 1);
}

void test_unknown_cmd()
{
    Harness harness;
    buddy::BuddyProtocol protocol;
    protocol.set_hooks(protocol_hooks(harness));

    handle(protocol, "{\"cmd\":\"not_a_command\"}");

    assert(harness.sent.size() == 1);
    assert(contains(harness.sent[0], "\"ack\":\"not_a_command\""));
    assert(contains(harness.sent[0], "\"ok\":false"));
    assert(contains(harness.sent[0], "\"error\":\"unsupported\""));
}

void test_unpair()
{
    Harness harness;
    buddy::BuddyProtocol protocol;
    protocol.set_hooks(protocol_hooks(harness));

    handle(protocol, "{\"cmd\":\"unpair\"}");

    assert(harness.unpair_calls == 1);
    assert(harness.sent.size() == 1);
    assert(contains(harness.sent[0], "\"ack\":\"unpair\""));
    assert(contains(harness.sent[0], "\"ok\":true"));
}

void test_delete_character_command()
{
    Harness harness;
    ResetStore reset;
    buddy::BuddyProtocol protocol;
    protocol.set_hooks(protocol_hooks(harness, reset_hooks(reset)));

    handle(protocol, "{\"cmd\":\"delete_character\"}");

    assert(reset.delete_calls == 1);
    assert(reset.factory_calls == 0);
    assert(harness.sent.size() == 1);
    assert(contains(harness.sent[0], "\"ack\":\"delete_character\""));
    assert(contains(harness.sent[0], "\"ok\":true"));
}

void test_delete_character_failure()
{
    Harness harness;
    ResetStore reset;
    reset.delete_ok = false;
    buddy::BuddyProtocol protocol;
    protocol.set_hooks(protocol_hooks(harness, reset_hooks(reset)));

    handle(protocol, "{\"cmd\":\"char_delete\"}");

    assert(reset.delete_calls == 1);
    assert(harness.sent.size() == 1);
    assert(contains(harness.sent[0], "\"ack\":\"char_delete\""));
    assert(contains(harness.sent[0], "\"ok\":false"));
    assert(contains(harness.sent[0], "\"error\":\"delete_failed\""));
}

void test_factory_reset_command_resets_runtime_identity()
{
    Harness harness;
    ResetStore reset;
    buddy::BuddyProtocol protocol;
    protocol.set_hooks(protocol_hooks(harness, reset_hooks(reset)));

    handle(protocol, "{\"cmd\":\"name\",\"name\":\"Desk Buddy\"}");
    handle(protocol, "{\"cmd\":\"owner\",\"name\":\"Ada\"}");
    handle(protocol, "{\"cmd\":\"factory_reset\"}");
    handle(protocol, "{\"cmd\":\"status\"}");

    assert(reset.factory_calls == 1);
    assert(harness.sent.size() == 4);
    assert(contains(harness.sent[2], "\"ack\":\"factory_reset\""));
    assert(contains(harness.sent[2], "\"ok\":true"));
    assert(contains(harness.sent[3], "\"name\":\"Claude Buddy\""));
    assert(contains(harness.sent[3], "\"owner\":\"\""));
}

void test_factory_reset_failure_preserves_runtime_identity()
{
    Harness harness;
    ResetStore reset;
    reset.factory_ok = false;
    buddy::BuddyProtocol protocol;
    protocol.set_hooks(protocol_hooks(harness, reset_hooks(reset)));

    handle(protocol, "{\"cmd\":\"name\",\"name\":\"Desk Buddy\"}");
    handle(protocol, "{\"cmd\":\"factory_reset\"}");
    handle(protocol, "{\"cmd\":\"status\"}");

    assert(reset.factory_calls == 1);
    assert(harness.sent.size() == 3);
    assert(contains(harness.sent[1], "\"ack\":\"factory_reset\""));
    assert(contains(harness.sent[1], "\"ok\":false"));
    assert(contains(harness.sent[1], "\"error\":\"reset_failed\""));
    assert(contains(harness.sent[2], "\"name\":\"Desk Buddy\""));
}

void test_permission_requires_current_prompt()
{
    Harness harness;
    buddy::BuddyProtocol protocol;
    protocol.set_hooks(protocol_hooks(harness));

    assert(!protocol.send_permission_once());
    handle(protocol, "{\"prompt\":{\"id\":\"req_1\"}}");
    assert(protocol.send_permission_once());
    assert(!protocol.send_permission_deny());
    handle(protocol, "{\"prompt\":{\"id\":\"req_2\"}}");
    assert(protocol.send_permission_deny());
    handle(protocol, "{\"prompt\":null}");
    assert(!protocol.send_permission_once());

    assert(harness.sent.size() == 2);
    assert(contains(harness.sent[0], "\"cmd\":\"permission\""));
    assert(contains(harness.sent[0], "\"id\":\"req_1\""));
    assert(contains(harness.sent[0], "\"decision\":\"once\""));
    assert(contains(harness.sent[1], "\"id\":\"req_2\""));
    assert(contains(harness.sent[1], "\"decision\":\"deny\""));
}

void test_snapshot_parse()
{
    Harness harness;
    buddy::BuddyProtocol protocol;
    protocol.set_hooks(protocol_hooks(harness));

    handle(protocol,
           "{\"total\":3,\"running\":1,\"waiting\":1,\"msg\":\"approve: Bash\","
           "\"entries\":[\"10:42 git push\",\"10:41 yarn test\",\"10:39 reading file\",\"older\"],"
           "\"tokens\":184502,\"tokens_today\":31200,"
           "\"prompt\":{\"id\":\"req_abc123\",\"tool\":\"Bash\",\"hint\":\"rm -rf /tmp/foo\"}}");

    const auto &snapshot = protocol.snapshot();
    assert(snapshot.total == 3);
    assert(snapshot.running == 1);
    assert(snapshot.waiting == 1);
    assert(snapshot.tokens == 184502);
    assert(snapshot.tokens_today == 31200);
    assert(snapshot.last_snapshot_ms == 1234);
    assert(snapshot.entry_count == 4);
    assert(std::strcmp(snapshot.msg, "approve: Bash") == 0);
    assert(std::strcmp(snapshot.entries[0], "10:42 git push") == 0);
    assert(snapshot.has_prompt);
    assert(snapshot.prompt_started_ms == 1234);
    assert(std::strcmp(snapshot.prompt_id, "req_abc123") == 0);
    assert(std::strcmp(snapshot.prompt_tool, "Bash") == 0);
    assert(std::strcmp(snapshot.prompt_hint, "rm -rf /tmp/foo") == 0);
}

void test_prompt_start_time_stays_until_prompt_changes()
{
    Harness harness;
    buddy::BuddyProtocol protocol;
    protocol.set_hooks(protocol_hooks(harness));

    buddy::BuddyProtocol::RuntimeStatus runtime = status();
    const char *first_prompt = "{\"prompt\":{\"id\":\"req_1\"}}";
    protocol.handle_line(first_prompt, static_cast<uint16_t>(std::strlen(first_prompt)), runtime);
    assert(protocol.snapshot().prompt_started_ms == 1234);

    runtime.uptime_ms = 2000;
    const char *same_prompt = "{\"prompt\":{\"id\":\"req_1\"},\"running\":1}";
    protocol.handle_line(same_prompt, static_cast<uint16_t>(std::strlen(same_prompt)), runtime);
    assert(protocol.snapshot().prompt_started_ms == 1234);

    runtime.uptime_ms = 3000;
    const char *next_prompt = "{\"prompt\":{\"id\":\"req_2\"}}";
    protocol.handle_line(next_prompt, static_cast<uint16_t>(std::strlen(next_prompt)), runtime);
    assert(protocol.snapshot().prompt_started_ms == 3000);

    runtime.uptime_ms = 4000;
    const char *clear_prompt = "{\"prompt\":null}";
    protocol.handle_line(clear_prompt, static_cast<uint16_t>(std::strlen(clear_prompt)), runtime);
    assert(!protocol.snapshot().has_prompt);
    assert(protocol.snapshot().prompt_started_ms == 0);
}

void test_time_sync_command_and_oneshot()
{
    Harness harness;
    buddy::BuddyProtocol protocol;
    protocol.set_hooks(protocol_hooks(harness));

    handle(protocol, "{\"time\":[1775731234,-25200]}");
    assert(harness.sent.empty());
    assert(protocol.time_sync().synced);
    assert(protocol.time_sync().epoch_seconds == 1775731234);
    assert(protocol.time_sync().timezone_offset_seconds == -25200);
    assert(protocol.time_sync().last_sync_ms == 1234);

    handle(protocol, "{\"cmd\":\"time\",\"time\":[1775731240,28800]}");
    assert(harness.sent.size() == 1);
    assert(contains(harness.sent[0], "\"ack\":\"time\""));
    assert(contains(harness.sent[0], "\"ok\":true"));
    assert(contains(harness.sent[0], "\"n\":0"));
    assert(protocol.time_sync().epoch_seconds == 1775731240);
    assert(protocol.time_sync().timezone_offset_seconds == 28800);
}

void send_file(buddy::BuddyProtocol &protocol, const char *path, const std::string &data)
{
    std::string file_cmd = std::string("{\"cmd\":\"file\",\"path\":\"") + path + "\",\"size\":" +
                           std::to_string(data.size()) + "}";
    std::string chunk_cmd = std::string("{\"cmd\":\"chunk\",\"d\":\"") + b64(data) + "\"}";

    handle(protocol, file_cmd.c_str());
    handle(protocol, chunk_cmd.c_str());
    handle(protocol, "{\"cmd\":\"file_end\"}");
}

void test_character_package_success()
{
    Harness harness;
    CharacterStore store;
    buddy::BuddyProtocol protocol;
    auto storage = storage_hooks(store);
    protocol.set_hooks(protocol_hooks(harness, storage));

    const std::string manifest =
        "{\"name\":\"Bufo\",\"states\":{\"sleep\":\"sleep.gif\","
        "\"idle\":[\"idle_0.gif\",\"idle_1.gif\"],\"busy\":\"busy.gif\","
        "\"attention\":\"attention.gif\",\"celebrate\":\"celebrate.gif\","
        "\"dizzy\":\"dizzy.gif\",\"heart\":\"heart.gif\"}}";
    const std::string gif = "GIF";
    const size_t total = manifest.size() + gif.size() * 8U;
    const std::string begin =
        std::string("{\"cmd\":\"char_begin\",\"name\":\"My Bufo\",\"total\":") +
        std::to_string(total) + "}";

    handle(protocol, begin.c_str());
    send_file(protocol, "manifest.json", manifest);
    send_file(protocol, "sleep.gif", gif);
    send_file(protocol, "idle_0.gif", gif);
    send_file(protocol, "idle_1.gif", gif);
    send_file(protocol, "busy.gif", gif);
    send_file(protocol, "attention.gif", gif);
    send_file(protocol, "celebrate.gif", gif);
    send_file(protocol, "dizzy.gif", gif);
    send_file(protocol, "heart.gif", gif);
    handle(protocol, "{\"cmd\":\"char_end\"}");

    assert(contains(harness.sent.front(), "\"ack\":\"char_begin\""));
    assert(contains(harness.sent.front(), "\"ok\":true"));
    assert(contains(harness.sent.back(), "\"ack\":\"char_end\""));
    assert(contains(harness.sent.back(), "\"ok\":true"));
    assert(store.safe_name == "my-bufo");
    assert(store.display_name == "Bufo");
    assert(store.committed.size() == 9);
    assert(store.committed["manifest.json"] == manifest);
    assert(store.aborts == 0);
}

void test_character_package_rejects_bad_path()
{
    Harness harness;
    CharacterStore store;
    buddy::BuddyProtocol protocol;
    auto storage = storage_hooks(store);
    protocol.set_hooks(protocol_hooks(harness, storage));

    handle(protocol, "{\"cmd\":\"char_begin\",\"name\":\"pet\",\"total\":3}");
    handle(protocol, "{\"cmd\":\"file\",\"path\":\"../bad.gif\",\"size\":3}");

    assert(harness.sent.size() == 2);
    assert(contains(harness.sent[1], "\"ack\":\"file\""));
    assert(contains(harness.sent[1], "\"ok\":false"));
    assert(contains(harness.sent[1], "\"error\":\"bad_path\""));
    assert(store.aborts == 1);
    assert(store.committed.empty());
}

void test_character_package_rejects_bad_base64()
{
    Harness harness;
    CharacterStore store;
    buddy::BuddyProtocol protocol;
    auto storage = storage_hooks(store);
    protocol.set_hooks(protocol_hooks(harness, storage));

    handle(protocol, "{\"cmd\":\"char_begin\",\"name\":\"pet\",\"total\":3}");
    handle(protocol, "{\"cmd\":\"file\",\"path\":\"sleep.gif\",\"size\":3}");
    handle(protocol, "{\"cmd\":\"chunk\",\"d\":\"not-base64\"}");

    assert(harness.sent.size() == 3);
    assert(contains(harness.sent[2], "\"ack\":\"chunk\""));
    assert(contains(harness.sent[2], "\"ok\":false"));
    assert(contains(harness.sent[2], "\"error\":\"bad_base64\""));
    assert(store.aborts == 1);
    assert(store.committed.empty());
}

void test_character_package_rejects_bad_manifest()
{
    Harness harness;
    CharacterStore store;
    buddy::BuddyProtocol protocol;
    auto storage = storage_hooks(store);
    protocol.set_hooks(protocol_hooks(harness, storage));

    const std::string manifest = "{\"states\":{\"sleep\":\"sleep.gif\",\"idle\":[]}}";
    const std::string gif = "GIF";
    const size_t total = manifest.size() + gif.size();
    const std::string begin =
        std::string("{\"cmd\":\"char_begin\",\"name\":\"pet\",\"total\":") +
        std::to_string(total) + "}";

    handle(protocol, begin.c_str());
    send_file(protocol, "manifest.json", manifest);
    send_file(protocol, "sleep.gif", gif);
    handle(protocol, "{\"cmd\":\"char_end\"}");

    assert(contains(harness.sent.back(), "\"ack\":\"char_end\""));
    assert(contains(harness.sent.back(), "\"ok\":false"));
    assert(contains(harness.sent.back(), "\"error\":\"bad_manifest\""));
    assert(store.aborts == 1);
    assert(store.committed.empty());
}

}  // namespace

int main()
{
    test_status_ack();
    test_name_and_owner();
    test_identity_loads_from_preferences();
    test_species_loads_from_preferences();
    test_identity_saves_to_preferences();
    test_identity_save_failure_is_reported();
    test_species_command_saves_index();
    test_species_command_accepts_gif_sentinel();
    test_species_command_rejects_invalid_index();
    test_permission_decisions_update_pet_stats();
    test_token_delta_updates_level_and_fed();
    test_stats_load_and_nap_energy();
    test_unknown_cmd();
    test_unpair();
    test_delete_character_command();
    test_delete_character_failure();
    test_factory_reset_command_resets_runtime_identity();
    test_factory_reset_failure_preserves_runtime_identity();
    test_permission_requires_current_prompt();
    test_snapshot_parse();
    test_prompt_start_time_stays_until_prompt_changes();
    test_time_sync_command_and_oneshot();
    test_character_package_success();
    test_character_package_rejects_bad_path();
    test_character_package_rejects_bad_base64();
    test_character_package_rejects_bad_manifest();
    return 0;
}
