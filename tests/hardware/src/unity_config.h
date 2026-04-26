#ifndef BUDDY_HARDWARE_UNITY_CONFIG_H
#define BUDDY_HARDWARE_UNITY_CONFIG_H

#include "rtthread.h"

#define UNITY_OUTPUT_CHAR(a) rt_kprintf("%c", (char)(a))
#define UNITY_OUTPUT_FLUSH()
#define UNITY_OUTPUT_START()
#define UNITY_OUTPUT_COMPLETE()

#endif
