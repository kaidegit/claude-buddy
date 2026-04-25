#ifndef BUDDY_HOST_RTTHREAD_H
#define BUDDY_HOST_RTTHREAD_H

#include <string.h>

/* Host tests compile the SDK cJSON source directly. It includes <rtthread.h>
 * unconditionally and uses RT-Thread's memcpy/memset aliases.
 */
#define rt_memcpy memcpy
#define rt_memset memset

#endif
