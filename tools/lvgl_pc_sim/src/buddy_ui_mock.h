#ifndef BUDDY_UI_MOCK_H
#define BUDDY_UI_MOCK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

const char *buddy_ui_mock_scene_name(void);
void buddy_ui_mock_next_scene(void);
void buddy_ui_mock_previous_scene(void);
void buddy_ui_mock_tick(uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif
