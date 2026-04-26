#ifndef BUDDY_SIM_LITTLEVGL2RTT_H
#define BUDDY_SIM_LITTLEVGL2RTT_H

#include "lvgl.h"
#include "rtthread.h"

#ifdef __cplusplus
extern "C" {
#endif

rt_err_t littlevgl2rtt_init(const char *name);

#ifdef __cplusplus
}
#endif

#endif
