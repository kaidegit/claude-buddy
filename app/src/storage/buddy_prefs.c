#include "buddy_prefs.h"

#include <stddef.h>
#include <string.h>

#include "rtthread.h"

#if defined(BSP_SHARE_PREFS)
#include "share_prefs.h"
#if defined(PKG_USING_FLASHDB)
#include "flashdb.h"
#endif
#if defined(RT_USING_DFS)
#include "dfs_posix.h"
#include "dfs_fs.h"
#endif
#endif

#define BUDDY_PREFS_KEY_NAME "name"
#define BUDDY_PREFS_KEY_OWNER "owner"
#define BUDDY_PREFS_KEY_BRIGHTNESS "brightness"
#define BUDDY_PREFS_KEY_SOUND "sound"
#define BUDDY_PREFS_KEY_LED "led"
#define BUDDY_PREFS_KEY_TRANSCRIPT "transcript"
#define BUDDY_PREFS_KEY_SPECIES "species"
#define BUDDY_PREFS_KEY_STATS "stats"

#define BUDDY_PREFS_DEFAULT_BRIGHTNESS 80
#define BUDDY_PREFS_MIN_BRIGHTNESS 20
#define BUDDY_PREFS_MAX_BRIGHTNESS 100
#define BUDDY_PREFS_DEFAULT_SPECIES 0xFF

#if defined(BSP_SHARE_PREFS)
static const char g_prefs_name[SHARE_PREFS_MAX_NAME_LEN] = "buddy";
static share_prefs_t *g_prefs;
static bool g_prefs_ready;
#if defined(RT_USING_MUTEX)
static struct rt_mutex g_prefs_mutex;
static bool g_prefs_mutex_ready;
#endif
#endif

static uint8_t buddy_prefs_clamp_brightness(int32_t value)
{
    if (value < BUDDY_PREFS_MIN_BRIGHTNESS)
    {
        return BUDDY_PREFS_MIN_BRIGHTNESS;
    }
    if (value > BUDDY_PREFS_MAX_BRIGHTNESS)
    {
        return BUDDY_PREFS_MAX_BRIGHTNESS;
    }
    return (uint8_t)value;
}

static void buddy_prefs_stats_defaults(buddy_pet_stats_t *stats)
{
    if (stats != NULL)
    {
        memset(stats, 0, sizeof(*stats));
    }
}

#if defined(BSP_SHARE_PREFS)
static bool buddy_prefs_dfs_ready(void)
{
#if defined(RT_USING_DFS)
    struct statfs fs;

    return statfs("/", &fs) == 0 && fs.f_blocks > 0 && fs.f_bsize > 0;
#else
    return true;
#endif
}

static void buddy_prefs_lock(void)
{
#if defined(RT_USING_MUTEX)
    if (g_prefs_mutex_ready)
    {
        rt_mutex_take(&g_prefs_mutex, RT_WAITING_FOREVER);
    }
#endif
}

static void buddy_prefs_unlock(void)
{
#if defined(RT_USING_MUTEX)
    if (g_prefs_mutex_ready)
    {
        rt_mutex_release(&g_prefs_mutex);
    }
#endif
}

static bool buddy_prefs_read_string(const char *key, char *dst, uint16_t dst_size)
{
    int32_t len;

    if (key == NULL || dst == NULL || dst_size == 0)
    {
        return false;
    }

    dst[0] = '\0';
    len = share_prefs_get_string(g_prefs, key, dst, (int32_t)(dst_size - 1U));
    if (len < 0)
    {
        dst[0] = '\0';
        return false;
    }
    if (len >= dst_size)
    {
        len = dst_size - 1U;
    }
    dst[len] = '\0';
    return true;
}

static bool buddy_prefs_remove_ok(rt_err_t ret)
{
    if (ret == RT_EOK)
    {
        return true;
    }

#if defined(PKG_USING_FLASHDB)
    return ret == FDB_KV_NAME_ERR;
#else
    return false;
#endif
}
#endif

bool buddy_prefs_init(void)
{
#if defined(BSP_SHARE_PREFS)
    if (g_prefs_ready)
    {
        return true;
    }

#if defined(RT_USING_MUTEX)
    if (!g_prefs_mutex_ready &&
        rt_mutex_init(&g_prefs_mutex, "buddy_pref", RT_IPC_FLAG_PRIO) == RT_EOK)
    {
        g_prefs_mutex_ready = true;
    }
#endif

#if defined(RT_USING_DFS)
    if (!buddy_prefs_dfs_ready())
    {
        rt_kprintf("Buddy prefs unavailable: DFS root is not mounted\n");
        return false;
    }
    mkdir("prefdb", 0);
#endif

    g_prefs = share_prefs_open(g_prefs_name, SHAREPREFS_MODE_PRIVATE);
    g_prefs_ready = g_prefs != NULL;
    if (!g_prefs_ready)
    {
        rt_kprintf("Buddy prefs unavailable\n");
    }
    return g_prefs_ready;
#else
    return false;
#endif
}

bool buddy_prefs_load_identity(char *name, uint16_t name_size, char *owner, uint16_t owner_size,
                               void *context)
{
    bool ok;

    (void)context;

    if (!buddy_prefs_init() || name == NULL || owner == NULL)
    {
        return false;
    }

#if defined(BSP_SHARE_PREFS)
    buddy_prefs_lock();
    ok = buddy_prefs_read_string(BUDDY_PREFS_KEY_NAME, name, name_size) &&
         buddy_prefs_read_string(BUDDY_PREFS_KEY_OWNER, owner, owner_size);
    buddy_prefs_unlock();
    return ok;
#else
    return false;
#endif
}

bool buddy_prefs_save_identity(const char *name, const char *owner, void *context)
{
    bool ok;

    (void)context;

    if (!buddy_prefs_init() || name == NULL)
    {
        return false;
    }

#if defined(BSP_SHARE_PREFS)
    buddy_prefs_lock();
    ok = share_prefs_set_string(g_prefs, BUDDY_PREFS_KEY_NAME, name) == RT_EOK &&
         share_prefs_set_string(g_prefs, BUDDY_PREFS_KEY_OWNER,
                                owner != NULL ? owner : "") == RT_EOK;
    buddy_prefs_unlock();
    return ok;
#else
    return false;
#endif
}

bool buddy_prefs_load_species(uint8_t *species, void *context)
{
    int32_t value;

    (void)context;

    if (species == NULL || !buddy_prefs_init())
    {
        return false;
    }

#if defined(BSP_SHARE_PREFS)
    buddy_prefs_lock();
    value = share_prefs_get_int(g_prefs, BUDDY_PREFS_KEY_SPECIES, BUDDY_PREFS_DEFAULT_SPECIES);
    buddy_prefs_unlock();
    if (value < 0 || value > 0xFF)
    {
        value = BUDDY_PREFS_DEFAULT_SPECIES;
    }
    *species = (uint8_t)value;
    return true;
#else
    return false;
#endif
}

bool buddy_prefs_save_species(uint8_t species, void *context)
{
    bool ok;

    (void)context;

    if (!buddy_prefs_init())
    {
        return false;
    }

#if defined(BSP_SHARE_PREFS)
    buddy_prefs_lock();
    ok = share_prefs_set_int(g_prefs, BUDDY_PREFS_KEY_SPECIES, species) == RT_EOK;
    buddy_prefs_unlock();
    return ok;
#else
    (void)species;
    return false;
#endif
}

bool buddy_prefs_load_stats(buddy_pet_stats_t *stats, void *context)
{
    int32_t len;

    (void)context;

    if (stats == NULL)
    {
        return false;
    }
    buddy_prefs_stats_defaults(stats);

    if (!buddy_prefs_init())
    {
        return false;
    }

#if defined(BSP_SHARE_PREFS)
    buddy_prefs_lock();
    len = share_prefs_get_block(g_prefs, BUDDY_PREFS_KEY_STATS, stats, (int32_t)sizeof(*stats));
    buddy_prefs_unlock();
    if (len != (int32_t)sizeof(*stats))
    {
        buddy_prefs_stats_defaults(stats);
    }
    if (stats->velocity_index >= BUDDY_PREFS_STATS_VELOCITY_COUNT)
    {
        stats->velocity_index = 0;
    }
    if (stats->velocity_count > BUDDY_PREFS_STATS_VELOCITY_COUNT)
    {
        stats->velocity_count = BUDDY_PREFS_STATS_VELOCITY_COUNT;
    }
    return true;
#else
    return false;
#endif
}

bool buddy_prefs_save_stats(const buddy_pet_stats_t *stats, void *context)
{
    bool ok;

    (void)context;

    if (stats == NULL || !buddy_prefs_init())
    {
        return false;
    }

#if defined(BSP_SHARE_PREFS)
    buddy_prefs_lock();
    ok = share_prefs_set_block(g_prefs, BUDDY_PREFS_KEY_STATS, stats,
                               (int32_t)sizeof(*stats)) == RT_EOK;
    buddy_prefs_unlock();
    return ok;
#else
    return false;
#endif
}

bool buddy_prefs_load_ui_settings(buddy_ui_settings_t *settings)
{
    if (settings == NULL || !buddy_prefs_init())
    {
        return false;
    }

#if defined(BSP_SHARE_PREFS)
    buddy_prefs_lock();
    settings->brightness =
        buddy_prefs_clamp_brightness(share_prefs_get_int(g_prefs, BUDDY_PREFS_KEY_BRIGHTNESS,
                                                         BUDDY_PREFS_DEFAULT_BRIGHTNESS));
    settings->sound_enabled = share_prefs_get_int(g_prefs, BUDDY_PREFS_KEY_SOUND, 1) != 0;
    settings->led_enabled = share_prefs_get_int(g_prefs, BUDDY_PREFS_KEY_LED, 1) != 0;
    settings->transcript_enabled =
        share_prefs_get_int(g_prefs, BUDDY_PREFS_KEY_TRANSCRIPT, 1) != 0;
    buddy_prefs_unlock();
    return true;
#else
    return false;
#endif
}

bool buddy_prefs_save_ui_settings(const buddy_ui_settings_t *settings)
{
    bool ok;

    if (settings == NULL || !buddy_prefs_init())
    {
        return false;
    }

#if defined(BSP_SHARE_PREFS)
    buddy_prefs_lock();
    ok = share_prefs_set_int(g_prefs, BUDDY_PREFS_KEY_BRIGHTNESS,
                             buddy_prefs_clamp_brightness(settings->brightness)) == RT_EOK &&
         share_prefs_set_int(g_prefs, BUDDY_PREFS_KEY_SOUND,
                             settings->sound_enabled ? 1 : 0) == RT_EOK &&
         share_prefs_set_int(g_prefs, BUDDY_PREFS_KEY_LED,
                             settings->led_enabled ? 1 : 0) == RT_EOK &&
         share_prefs_set_int(g_prefs, BUDDY_PREFS_KEY_TRANSCRIPT,
                             settings->transcript_enabled ? 1 : 0) == RT_EOK;
    buddy_prefs_unlock();
    return ok;
#else
    return false;
#endif
}

bool buddy_prefs_clear_all(void)
{
    bool ok;

    if (!buddy_prefs_init())
    {
        return false;
    }

#if defined(BSP_SHARE_PREFS)
    buddy_prefs_lock();
    ok = buddy_prefs_remove_ok(share_prefs_remove(g_prefs, BUDDY_PREFS_KEY_NAME));
    ok = buddy_prefs_remove_ok(share_prefs_remove(g_prefs, BUDDY_PREFS_KEY_OWNER)) && ok;
    ok = buddy_prefs_remove_ok(share_prefs_remove(g_prefs, BUDDY_PREFS_KEY_SPECIES)) && ok;
    ok = buddy_prefs_remove_ok(share_prefs_remove(g_prefs, BUDDY_PREFS_KEY_STATS)) && ok;
    ok = buddy_prefs_remove_ok(share_prefs_remove(g_prefs, BUDDY_PREFS_KEY_BRIGHTNESS)) && ok;
    ok = buddy_prefs_remove_ok(share_prefs_remove(g_prefs, BUDDY_PREFS_KEY_SOUND)) && ok;
    ok = buddy_prefs_remove_ok(share_prefs_remove(g_prefs, BUDDY_PREFS_KEY_LED)) && ok;
    ok = buddy_prefs_remove_ok(share_prefs_remove(g_prefs, BUDDY_PREFS_KEY_TRANSCRIPT)) && ok;
    buddy_prefs_unlock();
    return ok;
#else
    return false;
#endif
}
