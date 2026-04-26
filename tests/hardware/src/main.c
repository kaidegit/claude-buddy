#include <stdint.h>

#include "rtthread.h"

int buddy_hardware_tests_run_all(void);

static int buddy_hwtest_run_once(void)
{
    int failures;

    rt_kprintf("\nBUDDY_HW_TEST_BEGIN\n");
    failures = buddy_hardware_tests_run_all();
    rt_kprintf("BUDDY_HW_TEST_RESULT:%s failures=%d\n",
               failures == 0 ? "PASS" : "FAIL",
               failures);

    return failures == 0 ? 0 : -1;
}

int main(void)
{
    (void)buddy_hwtest_run_once();

    while (1)
    {
        rt_thread_mdelay(1000);
    }

    return 0;
}

#ifdef RT_USING_FINSH
int buddy_hwtest(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    return buddy_hwtest_run_once();
}
MSH_CMD_EXPORT(buddy_hwtest, Run Buddy hardware regression tests);
#endif
