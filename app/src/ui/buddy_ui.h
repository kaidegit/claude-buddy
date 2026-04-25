#ifndef BUDDY_UI_H
#define BUDDY_UI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int buddy_ui_init(void);
uint32_t buddy_ui_run_once(void);

#ifdef __cplusplus
}
#endif

#endif
