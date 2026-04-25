#include "buddy_character_store.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "buddy_character_runtime.h"
#include "rtthread.h"

#if defined(RT_USING_DFS)
#include "bf0_hal.h"
#include "dfs_fs.h"
#include "dfs_posix.h"
#include "drv_flash.h"
#include "rtdevice.h"
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define BUDDY_CHARACTER_ROOT "/characters"
#define BUDDY_CHARACTER_INCOMING "/characters/.incoming"
#define BUDDY_CHARACTER_ACTIVE "/characters/active"
#define BUDDY_CHARACTER_PREVIOUS "/characters/.previous"
#define BUDDY_CHARACTER_ACTIVE_NAME "/characters/active_name.txt"
#define BUDDY_FS_DEVICE "root"
#define BUDDY_FS_TYPE "lfs"
#define BUDDY_CHARACTER_SPACE_MARGIN 4096U
#define BUDDY_CHARACTER_PATH_MAX 128

static int g_character_fd = -1;
static bool g_store_ready = false;

#if defined(RT_USING_DFS)
static bool buddy_store_join_path(char *dst, size_t dst_size, const char *dir, const char *name)
{
    int ret;

    if (dst == NULL || dst_size == 0 || dir == NULL || name == NULL)
    {
        return false;
    }

    ret = snprintf(dst, dst_size, "%s/%s", dir, name);
    return ret > 0 && (size_t)ret < dst_size;
}

static bool buddy_store_is_dir(const char *path)
{
    struct stat st;

    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static bool buddy_store_ensure_dir(const char *path)
{
    if (buddy_store_is_dir(path))
    {
        return true;
    }

    return mkdir(path, 0) == 0 || buddy_store_is_dir(path);
}

static void buddy_store_close_current(void)
{
    if (g_character_fd >= 0)
    {
        fsync(g_character_fd);
        close(g_character_fd);
        g_character_fd = -1;
    }
}

static bool buddy_store_remove_tree(const char *path)
{
    struct stat st;
    DIR *dir;
    struct dirent *entry;
    bool ok = true;

    if (path == NULL || stat(path, &st) != 0)
    {
        return true;
    }

    if (!S_ISDIR(st.st_mode))
    {
        return unlink(path) == 0;
    }

    dir = opendir(path);
    if (dir == NULL)
    {
        return false;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        char child[BUDDY_CHARACTER_PATH_MAX];

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        if (!buddy_store_join_path(child, sizeof(child), path, entry->d_name) ||
            !buddy_store_remove_tree(child))
        {
            ok = false;
            break;
        }
    }

    closedir(dir);
    if (!ok)
    {
        return false;
    }

    return rmdir(path) == 0;
}

static uint32_t buddy_store_free_bytes(void)
{
    struct statfs fs;
    uint64_t bytes;

    if (statfs("/", &fs) != 0)
    {
        return 0;
    }

    bytes = (uint64_t)fs.f_bfree * (uint64_t)fs.f_bsize;
    return bytes > 0xFFFFFFFFULL ? 0xFFFFFFFFU : (uint32_t)bytes;
}

static bool buddy_store_root_mounted(void)
{
    struct statfs fs;

    return statfs("/", &fs) == 0 && fs.f_blocks > 0 && fs.f_bsize > 0;
}

static bool buddy_store_register_device(void)
{
#if defined(FS_REGION_START_ADDR) && defined(FS_REGION_SIZE)
    if (rt_device_find(BUDDY_FS_DEVICE) != RT_NULL)
    {
        return true;
    }

    register_mtd_device(FS_REGION_START_ADDR, FS_REGION_SIZE, (char *)BUDDY_FS_DEVICE);
    if (rt_device_find(BUDDY_FS_DEVICE) != RT_NULL)
    {
        return true;
    }

    rt_kprintf("Buddy character FS device register failed: addr=0x%08x size=%u\n",
               (unsigned int)FS_REGION_START_ADDR,
               (unsigned int)FS_REGION_SIZE);
    return false;
#else
    rt_kprintf("Buddy character FS region is not defined\n");
    return false;
#endif
}

static bool buddy_store_write_text(const char *path, const char *text)
{
    int fd;
    size_t len;
    bool ok;

    if (path == NULL || text == NULL)
    {
        return false;
    }

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0);
    if (fd < 0)
    {
        return false;
    }

    len = strlen(text);
    ok = len == 0 || write(fd, text, len) == (int)len;
    fsync(fd);
    close(fd);
    return ok;
}

static bool buddy_store_mount(void)
{
    if (buddy_store_root_mounted())
    {
        if (buddy_store_ensure_dir(BUDDY_CHARACTER_ROOT))
        {
            return true;
        }
    }

    if (!buddy_store_register_device())
    {
        rt_kprintf("Buddy character FS unavailable\n");
        return false;
    }

    if (dfs_mount(BUDDY_FS_DEVICE, "/", BUDDY_FS_TYPE, 0, 0) == 0)
    {
        return buddy_store_ensure_dir(BUDDY_CHARACTER_ROOT);
    }

    rt_kprintf("Buddy character FS mount failed, formatting LittleFS\n");
    if (dfs_mkfs(BUDDY_FS_TYPE, BUDDY_FS_DEVICE) == 0 &&
        dfs_mount(BUDDY_FS_DEVICE, "/", BUDDY_FS_TYPE, 0, 0) == 0)
    {
        return buddy_store_ensure_dir(BUDDY_CHARACTER_ROOT);
    }

    if (buddy_store_root_mounted())
    {
        return buddy_store_ensure_dir(BUDDY_CHARACTER_ROOT);
    }

    rt_kprintf("Buddy character FS unavailable\n");
    return false;
}

static int buddy_store_auto_mount(void)
{
    buddy_store_mount();
    return RT_EOK;
}
INIT_ENV_EXPORT(buddy_store_auto_mount);
#endif

bool buddy_character_store_init(void)
{
#if defined(RT_USING_DFS)
    g_store_ready = buddy_store_mount();
    if (g_store_ready)
    {
        rt_kprintf("Buddy character store ready, free=%u bytes\n",
                   (unsigned int)buddy_store_free_bytes());
    }
    return g_store_ready;
#else
    g_store_ready = false;
    return false;
#endif
}

bool buddy_character_store_begin(const char *safe_name, uint32_t total_size, void *context)
{
    (void)safe_name;
    (void)context;

#if defined(RT_USING_DFS)
    if (!g_store_ready && !buddy_character_store_init())
    {
        return false;
    }

    buddy_store_close_current();
    if (!buddy_store_remove_tree(BUDDY_CHARACTER_INCOMING) ||
        !buddy_store_ensure_dir(BUDDY_CHARACTER_ROOT))
    {
        return false;
    }

    if (total_size > 0xFFFFFFFFU - BUDDY_CHARACTER_SPACE_MARGIN ||
        total_size + BUDDY_CHARACTER_SPACE_MARGIN > buddy_store_free_bytes())
    {
        return false;
    }

    return buddy_store_ensure_dir(BUDDY_CHARACTER_INCOMING);
#else
    (void)total_size;
    return false;
#endif
}

bool buddy_character_store_open_file(const char *safe_name, const char *path, uint32_t expected_size,
                                     void *context)
{
    char full_path[BUDDY_CHARACTER_PATH_MAX];

    (void)safe_name;
    (void)expected_size;
    (void)context;

#if defined(RT_USING_DFS)
    if (!g_store_ready || path == NULL ||
        !buddy_store_join_path(full_path, sizeof(full_path), BUDDY_CHARACTER_INCOMING, path))
    {
        return false;
    }

    buddy_store_close_current();
    g_character_fd = open(full_path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0);
    return g_character_fd >= 0;
#else
    (void)path;
    (void)full_path;
    return false;
#endif
}

bool buddy_character_store_write(const uint8_t *data, uint16_t len, void *context)
{
    (void)context;

#if defined(RT_USING_DFS)
    if (g_character_fd < 0 || (data == NULL && len > 0))
    {
        return false;
    }

    return len == 0 || write(g_character_fd, data, len) == (int)len;
#else
    (void)data;
    (void)len;
    return false;
#endif
}

bool buddy_character_store_close_file(const char *safe_name, const char *path, uint32_t written,
                                      void *context)
{
    (void)safe_name;
    (void)path;
    (void)written;
    (void)context;

#if defined(RT_USING_DFS)
    if (g_character_fd < 0)
    {
        return false;
    }

    buddy_store_close_current();
    return true;
#else
    return false;
#endif
}

bool buddy_character_store_read_file(const char *safe_name, const char *path, char *dst,
                                     uint16_t dst_size, uint32_t *out_len, void *context)
{
    char full_path[BUDDY_CHARACTER_PATH_MAX];
    struct stat st;
    int fd;
    int rd;

    (void)safe_name;
    (void)context;

#if defined(RT_USING_DFS)
    if (!g_store_ready || path == NULL || dst == NULL || dst_size == 0 || out_len == NULL ||
        !buddy_store_join_path(full_path, sizeof(full_path), BUDDY_CHARACTER_INCOMING, path) ||
        stat(full_path, &st) != 0 || S_ISDIR(st.st_mode) || st.st_size < 0 ||
        (uint32_t)st.st_size + 1U > dst_size)
    {
        return false;
    }

    fd = open(full_path, O_RDONLY | O_BINARY, 0);
    if (fd < 0)
    {
        return false;
    }

    rd = read(fd, dst, (size_t)st.st_size);
    close(fd);
    if (rd != (int)st.st_size)
    {
        return false;
    }

    dst[rd] = '\0';
    *out_len = (uint32_t)rd;
    return true;
#else
    (void)full_path;
    (void)st;
    (void)fd;
    (void)rd;
    return false;
#endif
}

bool buddy_character_store_file_exists(const char *safe_name, const char *path, void *context)
{
    char full_path[BUDDY_CHARACTER_PATH_MAX];
    struct stat st;

    (void)safe_name;
    (void)context;

#if defined(RT_USING_DFS)
    return g_store_ready && path != NULL &&
           buddy_store_join_path(full_path, sizeof(full_path), BUDDY_CHARACTER_INCOMING, path) &&
           stat(full_path, &st) == 0 && !S_ISDIR(st.st_mode);
#else
    (void)full_path;
    (void)st;
    (void)path;
    return false;
#endif
}

bool buddy_character_store_commit(const char *safe_name, const char *display_name, void *context)
{
    bool had_active;

    (void)safe_name;
    (void)context;

#if defined(RT_USING_DFS)
    if (!g_store_ready || !buddy_store_is_dir(BUDDY_CHARACTER_INCOMING))
    {
        return false;
    }

    buddy_store_close_current();
    if (!buddy_store_remove_tree(BUDDY_CHARACTER_PREVIOUS))
    {
        return false;
    }

    had_active = buddy_store_is_dir(BUDDY_CHARACTER_ACTIVE);
    if (had_active && rename(BUDDY_CHARACTER_ACTIVE, BUDDY_CHARACTER_PREVIOUS) != 0)
    {
        return false;
    }

    if (rename(BUDDY_CHARACTER_INCOMING, BUDDY_CHARACTER_ACTIVE) != 0)
    {
        if (had_active)
        {
            rename(BUDDY_CHARACTER_PREVIOUS, BUDDY_CHARACTER_ACTIVE);
        }
        return false;
    }

    buddy_store_remove_tree(BUDDY_CHARACTER_PREVIOUS);
    if (display_name != NULL)
    {
        buddy_store_write_text(BUDDY_CHARACTER_ACTIVE_NAME, display_name);
    }

    rt_kprintf("Buddy character active: %s\n", display_name != NULL ? display_name : safe_name);
    buddy_character_runtime_invalidate();
    return true;
#else
    (void)had_active;
    (void)display_name;
    return false;
#endif
}

void buddy_character_store_abort(void *context)
{
    (void)context;

#if defined(RT_USING_DFS)
    buddy_store_close_current();
    buddy_store_remove_tree(BUDDY_CHARACTER_INCOMING);
#endif
}

bool buddy_character_store_delete_active(void)
{
#if defined(RT_USING_DFS)
    bool ok;

    if (!g_store_ready && !buddy_character_store_init())
    {
        return false;
    }

    buddy_store_close_current();
    ok = buddy_store_remove_tree(BUDDY_CHARACTER_INCOMING);
    ok = buddy_store_remove_tree(BUDDY_CHARACTER_ACTIVE) && ok;
    ok = buddy_store_remove_tree(BUDDY_CHARACTER_PREVIOUS) && ok;
    ok = buddy_store_remove_tree(BUDDY_CHARACTER_ACTIVE_NAME) && ok;
    ok = buddy_store_ensure_dir(BUDDY_CHARACTER_ROOT) && ok;
    if (ok)
    {
        buddy_character_runtime_invalidate();
        rt_kprintf("Buddy character deleted\n");
    }
    return ok;
#else
    return false;
#endif
}

bool buddy_character_store_delete_all(void)
{
#if defined(RT_USING_DFS)
    bool ok;

    if (!g_store_ready && !buddy_character_store_init())
    {
        return false;
    }

    buddy_store_close_current();
    ok = buddy_store_remove_tree(BUDDY_CHARACTER_ROOT);
    ok = buddy_store_ensure_dir(BUDDY_CHARACTER_ROOT) && ok;
    if (ok)
    {
        buddy_character_runtime_invalidate();
        rt_kprintf("Buddy character store reset\n");
    }
    return ok;
#else
    return false;
#endif
}
