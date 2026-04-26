#include "littlevgl2rtt.h"

#include "lv_ex_data.h"
#include "lvsf_font.h"

rt_err_t littlevgl2rtt_init(const char *name)
{
    (void)name;
    return RT_EOK;
}

void lv_ex_data_pool_init(void)
{
}

lv_font_t *lvsf_get_font_by_name(const char *name, uint16_t size)
{
    (void)name;
    (void)size;
    return NULL;
}
