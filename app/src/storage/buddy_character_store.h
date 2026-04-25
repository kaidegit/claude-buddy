#ifndef BUDDY_CHARACTER_STORE_H
#define BUDDY_CHARACTER_STORE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool buddy_character_store_init(void);
bool buddy_character_store_begin(const char *safe_name, uint32_t total_size, void *context);
bool buddy_character_store_open_file(const char *safe_name, const char *path,
                                     uint32_t expected_size, void *context);
bool buddy_character_store_write(const uint8_t *data, uint16_t len, void *context);
bool buddy_character_store_close_file(const char *safe_name, const char *path, uint32_t written,
                                      void *context);
bool buddy_character_store_read_file(const char *safe_name, const char *path, char *dst,
                                     uint16_t dst_size, uint32_t *out_len, void *context);
bool buddy_character_store_file_exists(const char *safe_name, const char *path, void *context);
bool buddy_character_store_commit(const char *safe_name, const char *display_name, void *context);
void buddy_character_store_abort(void *context);
bool buddy_character_store_delete_active(void);
bool buddy_character_store_delete_all(void);

#ifdef __cplusplus
}
#endif

#endif
