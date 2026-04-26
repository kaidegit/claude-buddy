#ifndef LV_DRV_CONF_H
#define LV_DRV_CONF_H

#include "lv_conf.h"

#define LV_DRV_DELAY_INCLUDE <stdint.h>
#define LV_DRV_DELAY_US(us)
#define LV_DRV_DELAY_MS(ms)

#define USE_SDL 1
#define USE_SDL_GPU 0
#define USE_MONITOR 0
#define USE_X11 0
#define USE_MOUSE 0
#define USE_MOUSEWHEEL 0
#define USE_KEYBOARD 0

#define SDL_HOR_RES 390
#define SDL_VER_RES 450
#define MONITOR_HOR_RES SDL_HOR_RES
#define MONITOR_VER_RES SDL_VER_RES
#define SDL_ZOOM 1
#define SDL_DOUBLE_BUFFERED 0
#define SDL_INCLUDE_PATH <SDL.h>
#define SDL_DUAL_DISPLAY 0
#define SDL_WINDOW_TITLE "Claude Buddy LVGL PC Simulator"

#endif
