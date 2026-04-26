#ifndef BUDDY_SIM_RTTHREAD_H
#define BUDDY_SIM_RTTHREAD_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int rt_err_t;
typedef intptr_t rt_base_t;
typedef uint8_t rt_uint8_t;
typedef uint16_t rt_uint16_t;
typedef uint32_t rt_uint32_t;
typedef int16_t rt_int16_t;

struct rt_thread
{
    void *stack_addr;
    rt_uint32_t stack_size;
};

typedef struct rt_thread *rt_thread_t;

#define RT_EOK 0
#define RT_ERROR (-1)
#define RT_NULL NULL
#define RT_WAITING_FOREVER (-1)
#define RT_IPC_FLAG_PRIO 0

#define rt_memcpy memcpy
#define rt_memset memset

int rt_kprintf(const char *fmt, ...);
rt_uint32_t rt_tick_get_millisecond(void);
void rt_thread_mdelay(rt_uint32_t ms);
rt_base_t rt_hw_interrupt_disable(void);
void rt_hw_interrupt_enable(rt_base_t level);
rt_thread_t rt_thread_self(void);
void rt_memory_info(rt_uint32_t *total, rt_uint32_t *used, rt_uint32_t *max_used);

#ifdef __cplusplus
}
#endif

#endif
