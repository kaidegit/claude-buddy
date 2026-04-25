#include <stdint.h>
#include <string.h>

#include "rtthread.h"

#include "bridge/buddy_app_c_api.h"
#include "platform/buddy_charger.h"
#include "transport/ble_nus_sifli.h"
#include "ui/buddy_ui.h"

int main(void)
{
    int ble_ret;
    int ui_ret;

    rt_kprintf("Claude Buddy SiFli boot\n");
    if (buddy_charger_init() != RT_EOK)
    {
        rt_kprintf("Buddy charger init skipped\n");
    }

    buddy_app_init();
    ble_ret = buddy_ble_nus_start();
    if (ble_ret != 0)
    {
        rt_kprintf("Buddy BLE start failed=%d\n", ble_ret);
    }

    ui_ret = buddy_ui_init();
    if (ui_ret != RT_EOK)
    {
        rt_kprintf("Buddy UI init failed=%d\n", ui_ret);
        return ui_ret;
    }

    while (1)
    {
        buddy_app_tick((uint32_t)rt_tick_get_millisecond());
        rt_thread_mdelay(buddy_ui_run_once());
    }

    return 0;
}

#ifdef RT_USING_FINSH
int buddy_perm(int argc, char **argv)
{
    if (argc != 2)
    {
        rt_kprintf("usage: buddy_perm once|deny\n");
        return -1;
    }

    if (strcmp(argv[1], "once") == 0)
    {
        return buddy_app_send_permission_once() ? 0 : -1;
    }

    if (strcmp(argv[1], "deny") == 0)
    {
        return buddy_app_send_permission_deny() ? 0 : -1;
    }

    rt_kprintf("usage: buddy_perm once|deny\n");
    return -1;
}
MSH_CMD_EXPORT(buddy_perm, Send current Claude permission decision);

int buddy_pair(int argc, char **argv)
{
    uint32_t passkey;

    (void)argc;
    (void)argv;

    if (!buddy_app_get_pairing_passkey(&passkey))
    {
        rt_kprintf("No Buddy BLE passkey pending\n");
        return -1;
    }

    rt_kprintf("Buddy BLE passkey %06u\n", (unsigned int)passkey);
    return 0;
}
MSH_CMD_EXPORT(buddy_pair, Show last Buddy BLE pairing passkey);

int buddy_unpair(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    return buddy_ble_nus_unpair();
}
MSH_CMD_EXPORT(buddy_unpair, Clear Buddy BLE bonds);
#endif
