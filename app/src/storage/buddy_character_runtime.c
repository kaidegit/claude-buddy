#include "buddy_character_runtime.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "lvgl.h"
#include "rtthread.h"

#if defined(RT_USING_DFS)
#include "dfs_posix.h"
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define BUDDY_CHARACTER_ACTIVE_DIR "/characters/active"
#define BUDDY_CHARACTER_ACTIVE_MANIFEST "/characters/active/manifest.json"
#define BUDDY_CHARACTER_ACTIVE_NAME "/characters/active_name.txt"
#define BUDDY_CHARACTER_MANIFEST_MAX 2048U
#define BUDDY_CHARACTER_FILE_LEN 64U
#define BUDDY_CHARACTER_STATE_VARIANTS 4U
#define BUDDY_CHARACTER_IDLE_ROTATE_MS 15000U

#if defined(LV_FS_POSIX_LETTER) && LV_FS_POSIX_LETTER != 0
#define BUDDY_CHARACTER_LVGL_LETTER ((char)LV_FS_POSIX_LETTER)
#else
#define BUDDY_CHARACTER_LVGL_LETTER 'A'
#endif

typedef struct
{
    uint8_t count;
    char files[BUDDY_CHARACTER_STATE_VARIANTS][BUDDY_CHARACTER_FILE_LEN];
} buddy_character_runtime_state_t;

static const char *const s_state_names[BUDDY_CHARACTER_STATE_COUNT] = {
    "sleep", "idle", "busy", "attention", "celebrate", "dizzy", "heart",
};

static buddy_character_runtime_state_t s_states[BUDDY_CHARACTER_STATE_COUNT];
static char s_display_name[32];
static bool s_loaded;
static bool s_available;
static volatile bool s_invalidated = true;

static bool buddy_runtime_valid_file(const char *path)
{
    size_t len = 0;

    if (path == NULL || path[0] == '\0' || path[0] == '.')
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

        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
              (c >= 'a' && c <= 'z') || c == '_' || c == '-' || c == '.'))
        {
            return false;
        }

        ++len;
        if (len >= BUDDY_CHARACTER_FILE_LEN)
        {
            return false;
        }
    }

    return true;
}

static bool buddy_runtime_join_active_path(char *out, size_t out_size, const char *file)
{
    int ret;

    if (out == NULL || out_size == 0 || !buddy_runtime_valid_file(file))
    {
        return false;
    }

    ret = snprintf(out, out_size, "%s/%s", BUDDY_CHARACTER_ACTIVE_DIR, file);
    return ret > 0 && (size_t)ret < out_size;
}

static bool buddy_runtime_file_exists(const char *file)
{
#if defined(RT_USING_DFS)
    char path[BUDDY_CHARACTER_RUNTIME_PATH_LEN];
    struct stat st;

    return buddy_runtime_join_active_path(path, sizeof(path), file) &&
           stat(path, &st) == 0 && !S_ISDIR(st.st_mode);
#else
    (void)file;
    return false;
#endif
}

static bool buddy_runtime_copy_file(char *dst, size_t dst_size, const char *src)
{
    if (dst == NULL || dst_size == 0 || !buddy_runtime_valid_file(src) ||
        !buddy_runtime_file_exists(src))
    {
        return false;
    }

    strncpy(dst, src, dst_size - 1U);
    dst[dst_size - 1U] = '\0';
    return true;
}

static bool buddy_runtime_parse_state(cJSON *states, buddy_character_state_t state)
{
    cJSON *value = cJSON_GetObjectItemCaseSensitive(states, s_state_names[state]);
    buddy_character_runtime_state_t *slot = &s_states[state];

    slot->count = 0;
    if (cJSON_IsString(value) && value->valuestring != NULL)
    {
        if (!buddy_runtime_copy_file(slot->files[0], sizeof(slot->files[0]), value->valuestring))
        {
            return false;
        }
        slot->count = 1;
        return true;
    }

    if (cJSON_IsArray(value))
    {
        cJSON *entry = NULL;

        cJSON_ArrayForEach(entry, value)
        {
            if (!cJSON_IsString(entry) || entry->valuestring == NULL ||
                !buddy_runtime_valid_file(entry->valuestring) ||
                !buddy_runtime_file_exists(entry->valuestring))
            {
                return false;
            }

            if (slot->count < BUDDY_CHARACTER_STATE_VARIANTS)
            {
                strncpy(slot->files[slot->count], entry->valuestring,
                        sizeof(slot->files[slot->count]) - 1U);
                slot->files[slot->count][sizeof(slot->files[slot->count]) - 1U] = '\0';
                ++slot->count;
            }
        }

        return slot->count > 0;
    }

    return false;
}

static void buddy_runtime_read_active_name(void)
{
#if defined(RT_USING_DFS)
    int fd;
    int rd;

    s_display_name[0] = '\0';
    fd = open(BUDDY_CHARACTER_ACTIVE_NAME, O_RDONLY | O_BINARY, 0);
    if (fd < 0)
    {
        return;
    }

    rd = read(fd, s_display_name, sizeof(s_display_name) - 1U);
    close(fd);
    if (rd <= 0)
    {
        s_display_name[0] = '\0';
        return;
    }

    s_display_name[rd] = '\0';
    while (rd > 0 && (s_display_name[rd - 1] == '\r' || s_display_name[rd - 1] == '\n'))
    {
        s_display_name[--rd] = '\0';
    }
#else
    s_display_name[0] = '\0';
#endif
}

static bool buddy_runtime_read_manifest(char *out, uint32_t out_size, uint32_t *out_len)
{
#if defined(RT_USING_DFS)
    struct stat st;
    int fd;
    int rd;

    if (out == NULL || out_size == 0 || out_len == NULL ||
        stat(BUDDY_CHARACTER_ACTIVE_MANIFEST, &st) != 0 || S_ISDIR(st.st_mode) ||
        st.st_size <= 0 || st.st_size >= (long)out_size ||
        st.st_size > (long)BUDDY_CHARACTER_MANIFEST_MAX)
    {
        return false;
    }

    fd = open(BUDDY_CHARACTER_ACTIVE_MANIFEST, O_RDONLY | O_BINARY, 0);
    if (fd < 0)
    {
        return false;
    }

    rd = read(fd, out, (size_t)st.st_size);
    close(fd);
    if (rd != (int)st.st_size)
    {
        return false;
    }

    out[rd] = '\0';
    *out_len = (uint32_t)rd;
    return true;
#else
    (void)out;
    (void)out_size;
    (void)out_len;
    return false;
#endif
}

static bool buddy_runtime_load_active(void)
{
    char manifest[BUDDY_CHARACTER_MANIFEST_MAX + 1U];
    uint32_t manifest_len = 0;
    cJSON *root;
    cJSON *states;
    bool ok = true;

    if (s_loaded && !s_invalidated)
    {
        return s_available;
    }

    memset(s_states, 0, sizeof(s_states));
    s_available = false;
    s_loaded = true;
    s_invalidated = false;
    buddy_runtime_read_active_name();

    if (!buddy_runtime_read_manifest(manifest, sizeof(manifest), &manifest_len))
    {
        return false;
    }

    root = cJSON_ParseWithLength(manifest, manifest_len);
    if (root == NULL || !cJSON_IsObject(root))
    {
        if (root != NULL)
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

    for (uint8_t i = 0; i < (uint8_t)BUDDY_CHARACTER_STATE_COUNT; ++i)
    {
        if (!buddy_runtime_parse_state(states, (buddy_character_state_t)i))
        {
            ok = false;
            break;
        }
    }

    cJSON_Delete(root);
    s_available = ok;
    if (ok)
    {
        rt_kprintf("Buddy character runtime loaded: %s\n",
                   s_display_name[0] != '\0' ? s_display_name : "active");
    }
    return ok;
}

static buddy_character_state_t buddy_runtime_fallback_state(buddy_character_state_t state)
{
    if (state < BUDDY_CHARACTER_STATE_COUNT && s_states[state].count > 0)
    {
        return state;
    }

    if (s_states[BUDDY_CHARACTER_STATE_IDLE].count > 0)
    {
        return BUDDY_CHARACTER_STATE_IDLE;
    }

    return BUDDY_CHARACTER_STATE_SLEEP;
}

bool buddy_character_runtime_get_lvgl_path(buddy_character_state_t state, uint32_t now_ms,
                                           char *out_path, uint16_t out_path_size)
{
    buddy_character_runtime_state_t *slot;
    const char *file;
    uint8_t index = 0;
    int ret;

    if (out_path == NULL || out_path_size == 0 || !buddy_runtime_load_active())
    {
        return false;
    }

    if (state >= BUDDY_CHARACTER_STATE_COUNT)
    {
        state = BUDDY_CHARACTER_STATE_SLEEP;
    }

    state = buddy_runtime_fallback_state(state);
    slot = &s_states[state];
    if (slot->count == 0)
    {
        return false;
    }

    if (state == BUDDY_CHARACTER_STATE_IDLE && slot->count > 1)
    {
        index = (uint8_t)((now_ms / BUDDY_CHARACTER_IDLE_ROTATE_MS) % slot->count);
    }

    file = slot->files[index];
    ret = snprintf(out_path, out_path_size, "%c:%s/%s", BUDDY_CHARACTER_LVGL_LETTER,
                   BUDDY_CHARACTER_ACTIVE_DIR, file);
    return ret > 0 && (uint16_t)ret < out_path_size;
}

bool buddy_character_runtime_available(void)
{
    return buddy_runtime_load_active();
}

const char *buddy_character_runtime_display_name(void)
{
    (void)buddy_runtime_load_active();
    return s_display_name;
}

void buddy_character_runtime_invalidate(void)
{
    s_invalidated = true;
}
