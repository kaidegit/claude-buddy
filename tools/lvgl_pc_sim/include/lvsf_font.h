#ifndef BUDDY_SIM_LVSF_FONT_H
#define BUDDY_SIM_LVSF_FONT_H

#include <stdint.h>
#include "lvgl.h"

#define FONT_BIGL 36
#define FONT_TITLE 24
#define FONT_NORMAL 20
#define FONT_SMALL 12

#ifdef __cplusplus
extern "C" {
#endif

lv_font_t *lvsf_get_font_by_name(const char *name, uint16_t size);

#ifdef __cplusplus
}
#endif

#endif
