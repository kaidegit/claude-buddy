#include "rtthread.h"

#include <stdio.h>
#include <time.h>
#include <unistd.h>

static struct rt_thread s_main_thread;
static uint64_t s_start_ms;

static uint64_t buddy_sim_now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((uint64_t)ts.tv_sec * 1000ULL) + ((uint64_t)ts.tv_nsec / 1000000ULL);
}

int rt_kprintf(const char *fmt, ...)
{
    int ret;
    va_list args;

    va_start(args, fmt);
    ret = vprintf(fmt, args);
    va_end(args);
    fflush(stdout);
    return ret;
}

rt_uint32_t rt_tick_get_millisecond(void)
{
    uint64_t now = buddy_sim_now_ms();

    if (s_start_ms == 0)
    {
        s_start_ms = now;
    }
    return (rt_uint32_t)(now - s_start_ms);
}

void rt_thread_mdelay(rt_uint32_t ms)
{
    usleep((useconds_t)ms * 1000U);
}

rt_base_t rt_hw_interrupt_disable(void)
{
    return 0;
}

void rt_hw_interrupt_enable(rt_base_t level)
{
    (void)level;
}

rt_thread_t rt_thread_self(void)
{
    return &s_main_thread;
}

void rt_memory_info(rt_uint32_t *total, rt_uint32_t *used, rt_uint32_t *max_used)
{
    if (total != NULL)
    {
        *total = 1024U * 1024U;
    }
    if (used != NULL)
    {
        *used = 128U * 1024U;
    }
    if (max_used != NULL)
    {
        *max_used = 192U * 1024U;
    }
}
