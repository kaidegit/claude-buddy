#ifndef BUDDY_UI_H
#define BUDDY_UI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int buddy_ui_init(void);
uint32_t buddy_ui_run_once(void);

#ifdef BUDDY_UI_SIMULATOR
void buddy_ui_sim_post_primary(void);
void buddy_ui_sim_post_secondary(void);
void buddy_ui_sim_post_menu(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
