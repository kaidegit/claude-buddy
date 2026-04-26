#ifndef BUDDY_PROTOCOL_HPP
#define BUDDY_PROTOCOL_HPP

#include <stdbool.h>
#include <stdint.h>

namespace buddy {

class BuddyProtocol final {
public:
    static constexpr uint8_t kMaxSnapshotEntries = 5;
    static constexpr uint16_t kSnapshotMsgLength = 64;
    static constexpr uint16_t kSnapshotEntryLength = 64;
    static constexpr uint16_t kPromptIdLength = 64;
    static constexpr uint16_t kPromptToolLength = 32;
    static constexpr uint16_t kPromptHintLength = 96;
    static constexpr uint16_t kCharacterNameLength = 32;
    static constexpr uint16_t kCharacterPathLength = 64;
    static constexpr uint16_t kCharacterManifestLength = 2048;
    static constexpr uint16_t kMaxDecodedChunkLength = 1536;
    static constexpr uint8_t kAsciiSpeciesCount = 18;
    static constexpr uint8_t kGifSpeciesSentinel = 0xFF;
    static constexpr uint8_t kVelocitySampleCount = 8;
    static constexpr uint32_t kTokensPerLevel = 50000U;

    struct RuntimeStatus {
        bool connected = false;
        bool encrypted = false;
        uint32_t uptime_ms = 0;
        uint32_t tick_count = 0;
        uint32_t rx_lines = 0;
        uint32_t rx_overflowed = 0;
    };

    struct Snapshot {
        uint32_t total = 0;
        uint32_t running = 0;
        uint32_t waiting = 0;
        uint32_t tokens = 0;
        uint32_t tokens_today = 0;
        uint32_t last_snapshot_ms = 0;
        uint32_t prompt_started_ms = 0;
        uint8_t entry_count = 0;
        bool has_prompt = false;
        char msg[kSnapshotMsgLength] = "";
        char entries[kMaxSnapshotEntries][kSnapshotEntryLength] = {};
        char prompt_id[kPromptIdLength] = "";
        char prompt_tool[kPromptToolLength] = "";
        char prompt_hint[kPromptHintLength] = "";
    };

    struct TimeSync {
        bool synced = false;
        uint32_t epoch_seconds = 0;
        int32_t timezone_offset_seconds = 0;
        uint32_t last_sync_ms = 0;
    };

    struct PetStats {
        uint32_t nap_seconds = 0;
        uint16_t approvals = 0;
        uint16_t denials = 0;
        uint16_t velocity[kVelocitySampleCount] = {};
        uint8_t velocity_index = 0;
        uint8_t velocity_count = 0;
        uint8_t level = 0;
        uint32_t tokens = 0;
    };

    struct PetStatsView {
        uint32_t nap_seconds = 0;
        uint16_t approvals = 0;
        uint16_t denials = 0;
        uint16_t median_velocity = 0;
        uint8_t level = 0;
        uint32_t tokens = 0;
        uint8_t mood = 2;
        uint8_t fed = 0;
        uint8_t energy = 3;
    };

    typedef int (*SendCallback)(const char *line, uint16_t len, void *context);
    typedef int (*UnpairCallback)(void *context);
    typedef bool (*LoadIdentityCallback)(char *name, uint16_t name_size, char *owner,
                                         uint16_t owner_size, void *context);
    typedef bool (*SaveIdentityCallback)(const char *name, const char *owner, void *context);
    typedef bool (*LoadSpeciesCallback)(uint8_t *species, void *context);
    typedef bool (*SaveSpeciesCallback)(uint8_t species, void *context);
    typedef bool (*LoadStatsCallback)(PetStats *stats, void *context);
    typedef bool (*SaveStatsCallback)(const PetStats *stats, void *context);
    typedef bool (*DeleteCharacterCallback)(void *context);
    typedef bool (*FactoryResetCallback)(void *context);
    typedef bool (*CharacterBeginCallback)(const char *safe_name, uint32_t total_size,
                                           void *context);
    typedef bool (*CharacterOpenFileCallback)(const char *safe_name, const char *path,
                                              uint32_t expected_size, void *context);
    typedef bool (*CharacterWriteCallback)(const uint8_t *data, uint16_t len, void *context);
    typedef bool (*CharacterCloseFileCallback)(const char *safe_name, const char *path,
                                               uint32_t written, void *context);
    typedef bool (*CharacterReadFileCallback)(const char *safe_name, const char *path,
                                              char *dst, uint16_t dst_size, uint32_t *out_len,
                                              void *context);
    typedef bool (*CharacterFileExistsCallback)(const char *safe_name, const char *path,
                                                void *context);
    typedef bool (*CharacterCommitCallback)(const char *safe_name, const char *display_name,
                                            void *context);
    typedef void (*CharacterAbortCallback)(void *context);

    struct CharacterStorageHooks {
        CharacterBeginCallback begin = nullptr;
        CharacterOpenFileCallback open_file = nullptr;
        CharacterWriteCallback write = nullptr;
        CharacterCloseFileCallback close_file = nullptr;
        CharacterReadFileCallback read_file = nullptr;
        CharacterFileExistsCallback file_exists = nullptr;
        CharacterCommitCallback commit = nullptr;
        CharacterAbortCallback abort = nullptr;
        void *context = nullptr;
    };

    struct PreferenceHooks {
        LoadIdentityCallback load_identity = nullptr;
        SaveIdentityCallback save_identity = nullptr;
        LoadSpeciesCallback load_species = nullptr;
        SaveSpeciesCallback save_species = nullptr;
        LoadStatsCallback load_stats = nullptr;
        SaveStatsCallback save_stats = nullptr;
        void *context = nullptr;
    };

    struct ResetHooks {
        DeleteCharacterCallback delete_character = nullptr;
        FactoryResetCallback factory_reset = nullptr;
        void *context = nullptr;
    };

    struct Hooks {
        SendCallback send = nullptr;
        UnpairCallback unpair = nullptr;
        void *context = nullptr;
        PreferenceHooks prefs;
        ResetHooks reset;
        CharacterStorageHooks character_storage;
    };

    void set_hooks(const Hooks &hooks);
    void handle_line(const char *line, uint16_t len, const RuntimeStatus &status);

    bool send_permission_once(uint32_t now_ms = 0);
    bool send_permission_deny();
    bool delete_character();
    bool factory_reset();
    bool set_species(uint8_t species);
    bool record_nap_end(uint32_t seconds, uint32_t now_ms);

    const char *device_name() const;
    const char *owner() const;
    const char *current_prompt_id() const;
    uint8_t current_species() const;
    const Snapshot &snapshot() const;
    const TimeSync &time_sync() const;
    const PetStats &pet_stats() const;
    PetStatsView pet_stats_view(uint32_t now_ms) const;

private:
    bool handle_command(void *json_root, const char *cmd, const RuntimeStatus &status);
    bool handle_snapshot(void *json_root, const RuntimeStatus &status);
    bool handle_time(void *json_root, const RuntimeStatus &status, bool ack);
    bool handle_character_begin(void *json_root);
    bool handle_character_file(void *json_root);
    bool handle_character_chunk(void *json_root);
    bool handle_character_file_end(void *json_root);
    bool handle_character_end(void *json_root);
    bool handle_species(void *json_root);
    bool parse_time_payload(void *json_root, const RuntimeStatus &status);
    void update_bridge_tokens(uint32_t bridge_total);
    void record_approval(uint32_t now_ms);
    void record_denial();
    bool validate_character_manifest(const char *manifest, uint32_t manifest_len, char *display_name,
                                     uint16_t display_name_size);
    bool validate_manifest_state(void *states, const char *name);
    bool decode_base64(const char *src, uint8_t *dst, uint16_t dst_size, uint16_t *out_len) const;
    bool emit_json(void *json_root);
    bool emit_ack(const char *ack, bool ok, uint32_t n, const char *error);
    bool emit_simple_ack(const char *ack, bool ok, const char *error);
    bool emit_status(const RuntimeStatus &status);
    bool emit_permission(const char *decision);
    void load_persisted_identity();
    void load_persisted_species();
    void load_persisted_stats();
    bool save_identity() const;
    bool save_species() const;
    bool save_stats() const;
    void reset_identity();
    void reset_species();
    void reset_stats();
    bool is_valid_species(uint8_t species) const;
    uint16_t stats_median_velocity() const;
    uint8_t stats_mood_tier() const;
    uint8_t stats_fed_progress() const;
    uint8_t stats_energy_tier(uint32_t now_ms) const;
    bool storage_ready() const;
    void abort_character_transfer();
    void reset_character_transfer();
    void clear_prompt();
    void copy_string(char *dst, uint16_t dst_size, const char *src) const;
    void sanitize_character_name(char *dst, uint16_t dst_size, const char *src) const;
    bool is_valid_character_path(const char *path) const;

    struct CharacterTransfer {
        bool active = false;
        bool file_open = false;
        bool saw_manifest = false;
        uint32_t total_expected = 0;
        uint32_t total_written = 0;
        uint32_t file_expected = 0;
        uint32_t file_written = 0;
        char safe_name[kCharacterNameLength] = "";
        char display_name[kCharacterNameLength] = "";
        char file_path[kCharacterPathLength] = "";
    };

    Hooks hooks_;
    char device_name_[32] = "Claude Buddy";
    char owner_[32] = "";
    Snapshot snapshot_;
    TimeSync time_sync_;
    PetStats pet_stats_;
    CharacterTransfer character_transfer_;
    uint8_t species_ = kGifSpeciesSentinel;
    bool permission_sent_ = false;
    uint32_t last_bridge_tokens_ = 0;
    bool tokens_synced_ = false;
    uint32_t last_nap_end_ms_ = 0;
    uint8_t energy_at_nap_ = 3;
};

}  // namespace buddy

#endif
