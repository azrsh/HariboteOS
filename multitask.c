#include "bootpack.h"

struct TIMER *multitaskTimer;
int multitaskRegister;

void multitask_init(void)
{
    multitaskTimer = timer_allocate();
    timer_set_time(multitaskTimer, 2);
    multitaskRegister = 3 * 8;
    return;
}

void multitask_taskswitch(void)
{
    if (multitaskRegister == 3 * 8)
    {
        multitaskRegister = 4 * 8;
    }
    else
    {
        multitaskRegister = 3 * 8;
    }

    timer_set_time(multitaskTimer, 2);
    farjump(0, multitaskRegister);
    return;
}
