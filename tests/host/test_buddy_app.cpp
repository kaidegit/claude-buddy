#include "BuddyApp.hpp"

#include <cassert>

namespace {

struct RuntimeHarness {
    bool encrypted = false;
    int delete_calls = 0;
    int factory_calls = 0;
};

bool encrypted(void *context)
{
    auto *harness = static_cast<RuntimeHarness *>(context);
    return harness != nullptr && harness->encrypted;
}

bool delete_character(void *context)
{
    auto *harness = static_cast<RuntimeHarness *>(context);
    if (harness == nullptr)
    {
        return false;
    }

    ++harness->delete_calls;
    return true;
}

bool factory_reset(void *context)
{
    auto *harness = static_cast<RuntimeHarness *>(context);
    if (harness == nullptr)
    {
        return false;
    }

    ++harness->factory_calls;
    return true;
}

void test_pairing_passkey_state()
{
    buddy::BuddyApp app;

    assert(!app.has_pairing_passkey());
    app.on_ble_passkey(123456);
    assert(app.has_pairing_passkey());
    assert(app.last_pairing_passkey() == 123456);

    app.on_ble_passkey(1234567);
    assert(app.last_pairing_passkey() == 234567);
}

void test_pairing_passkey_clears_on_encryption()
{
    RuntimeHarness harness;
    buddy::BuddyApp app;
    buddy::BuddyPlatformHooks hooks;

    hooks.is_encrypted = encrypted;
    hooks.context = &harness;
    app.set_platform_hooks(hooks);

    app.tick(1000);
    app.on_ble_passkey(654321);
    assert(app.has_pairing_passkey());

    harness.encrypted = true;
    app.tick(1500);
    assert(!app.has_pairing_passkey());
}

void test_pairing_passkey_clears_after_timeout()
{
    buddy::BuddyApp app;

    app.tick(5000);
    app.on_ble_passkey(111222);
    app.tick(5000 + buddy::BuddyApp::kPairingPasskeyDisplayMs);
    assert(app.has_pairing_passkey());

    app.tick(5001 + buddy::BuddyApp::kPairingPasskeyDisplayMs);
    assert(!app.has_pairing_passkey());
}

void test_app_reset_hooks()
{
    RuntimeHarness harness;
    buddy::BuddyApp app;
    buddy::BuddyPlatformHooks hooks;

    hooks.reset.delete_character = delete_character;
    hooks.reset.factory_reset = factory_reset;
    hooks.reset.context = &harness;
    app.set_platform_hooks(hooks);

    app.on_ble_passkey(222333);
    assert(app.delete_character());
    assert(harness.delete_calls == 1);
    assert(app.has_pairing_passkey());

    assert(app.factory_reset());
    assert(harness.factory_calls == 1);
    assert(!app.has_pairing_passkey());
}

}  // namespace

int main()
{
    test_pairing_passkey_state();
    test_pairing_passkey_clears_on_encryption();
    test_pairing_passkey_clears_after_timeout();
    test_app_reset_hooks();
    return 0;
}
