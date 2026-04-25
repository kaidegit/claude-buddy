#ifndef BUDDY_APP_HPP
#define BUDDY_APP_HPP

#include <stdint.h>

#include "BuddyProtocol.hpp"
#include "JsonLineAssembler.hpp"

namespace buddy {

typedef bool (*BuddyBoolCallback)(void *context);
typedef int (*BuddySendCallback)(const char *line, uint16_t len, void *context);
typedef int (*BuddyUnpairCallback)(void *context);

struct BuddyPlatformHooks {
    BuddySendCallback send = nullptr;
    BuddyBoolCallback is_connected = nullptr;
    BuddyBoolCallback is_encrypted = nullptr;
    BuddyUnpairCallback unpair = nullptr;
    void *context = nullptr;
    BuddyProtocol::PreferenceHooks prefs;
    BuddyProtocol::ResetHooks reset;
    BuddyProtocol::CharacterStorageHooks character_storage;
};

class BuddyApp final {
public:
    static constexpr uint32_t kPairingPasskeyDisplayMs = 120000U;

    void init();
    void set_platform_hooks(const BuddyPlatformHooks &hooks);
    void on_ble_rx(const uint8_t *data, uint16_t len);
    void on_ble_passkey(uint32_t passkey);
    bool send_permission_once();
    bool send_permission_deny();
    bool delete_character();
    bool factory_reset();
    bool set_species(uint8_t species);
    void tick(uint32_t now_ms);

    bool initialized() const;
    uint32_t tick_count() const;
    uint32_t last_tick_ms() const;
    uint32_t rx_line_count() const;
    uint32_t rx_overflow_count() const;
    bool has_pairing_passkey() const;
    uint32_t last_pairing_passkey() const;
    const char *device_name() const;
    const char *owner() const;
    uint8_t current_species() const;
    const BuddyProtocol::Snapshot &snapshot() const;
    BuddyProtocol::RuntimeStatus runtime_status() const;

private:
    static void on_json_line(const char *line, uint16_t len, void *context);

    bool initialized_ = false;
    uint32_t tick_count_ = 0;
    uint32_t last_tick_ms_ = 0;
    BuddyPlatformHooks platform_;
    JsonLineAssembler line_assembler_;
    BuddyProtocol protocol_;
    bool has_pairing_passkey_ = false;
    uint32_t last_pairing_passkey_ = 0;
    uint32_t pairing_passkey_ms_ = 0;
};

}  // namespace buddy

#endif
