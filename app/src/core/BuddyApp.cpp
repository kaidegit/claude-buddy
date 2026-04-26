#include "BuddyApp.hpp"

namespace buddy {

void BuddyApp::init()
{
    initialized_ = true;
    tick_count_ = 0;
    last_tick_ms_ = 0;
}

void BuddyApp::set_platform_hooks(const BuddyPlatformHooks &hooks)
{
    BuddyProtocol::Hooks protocol_hooks;

    platform_ = hooks;
    protocol_hooks.send = hooks.send;
    protocol_hooks.unpair = hooks.unpair;
    protocol_hooks.context = hooks.context;
    protocol_hooks.prefs = hooks.prefs;
    protocol_hooks.reset = hooks.reset;
    protocol_hooks.character_storage = hooks.character_storage;
    protocol_.set_hooks(protocol_hooks);
}

void BuddyApp::on_ble_rx(const uint8_t *data, uint16_t len)
{
    line_assembler_.feed(data, len, &BuddyApp::on_json_line, this);
}

void BuddyApp::on_ble_passkey(uint32_t passkey)
{
    last_pairing_passkey_ = passkey % 1000000U;
    pairing_passkey_ms_ = last_tick_ms_;
    has_pairing_passkey_ = true;
}

bool BuddyApp::send_permission_once()
{
    return protocol_.send_permission_once(last_tick_ms_);
}

bool BuddyApp::send_permission_deny()
{
    return protocol_.send_permission_deny();
}

bool BuddyApp::delete_character()
{
    return protocol_.delete_character();
}

bool BuddyApp::factory_reset()
{
    const bool ok = protocol_.factory_reset();
    if (ok)
    {
        has_pairing_passkey_ = false;
        last_pairing_passkey_ = 0;
        pairing_passkey_ms_ = 0;
    }
    return ok;
}

bool BuddyApp::set_species(uint8_t species)
{
    return protocol_.set_species(species);
}

bool BuddyApp::record_nap_end(uint32_t seconds)
{
    return protocol_.record_nap_end(seconds, last_tick_ms_);
}

void BuddyApp::tick(uint32_t now_ms)
{
    if (!initialized_)
    {
        init();
    }

    last_tick_ms_ = now_ms;
    ++tick_count_;

    if (has_pairing_passkey_)
    {
        const BuddyProtocol::RuntimeStatus status = runtime_status();
        if (status.encrypted ||
            (now_ms >= pairing_passkey_ms_ &&
             now_ms - pairing_passkey_ms_ > kPairingPasskeyDisplayMs))
        {
            has_pairing_passkey_ = false;
        }
    }
}

bool BuddyApp::initialized() const
{
    return initialized_;
}

uint32_t BuddyApp::tick_count() const
{
    return tick_count_;
}

uint32_t BuddyApp::last_tick_ms() const
{
    return last_tick_ms_;
}

uint32_t BuddyApp::rx_line_count() const
{
    return line_assembler_.total_lines();
}

uint32_t BuddyApp::rx_overflow_count() const
{
    return line_assembler_.total_overflowed();
}

bool BuddyApp::has_pairing_passkey() const
{
    return has_pairing_passkey_;
}

uint32_t BuddyApp::last_pairing_passkey() const
{
    return last_pairing_passkey_;
}

const char *BuddyApp::device_name() const
{
    return protocol_.device_name();
}

const char *BuddyApp::owner() const
{
    return protocol_.owner();
}

uint8_t BuddyApp::current_species() const
{
    return protocol_.current_species();
}

const BuddyProtocol::Snapshot &BuddyApp::snapshot() const
{
    return protocol_.snapshot();
}

BuddyProtocol::PetStatsView BuddyApp::pet_stats_view() const
{
    return protocol_.pet_stats_view(last_tick_ms_);
}

void BuddyApp::on_json_line(const char *line, uint16_t len, void *context)
{
    BuddyApp *app = static_cast<BuddyApp *>(context);
    if (app != nullptr)
    {
        app->protocol_.handle_line(line, len, app->runtime_status());
    }
}

BuddyProtocol::RuntimeStatus BuddyApp::runtime_status() const
{
    BuddyProtocol::RuntimeStatus status;
    status.uptime_ms = last_tick_ms_;
    status.tick_count = tick_count_;
    status.rx_lines = line_assembler_.total_lines();
    status.rx_overflowed = line_assembler_.total_overflowed();

    if (platform_.is_connected != nullptr)
    {
        status.connected = platform_.is_connected(platform_.context);
    }

    if (platform_.is_encrypted != nullptr)
    {
        status.encrypted = platform_.is_encrypted(platform_.context);
    }

    return status;
}

}  // namespace buddy
