#include "bootpack.h"

//PITはProgramable Interval Timerの略
#define PIT_CONTROL 0x0043
#define PIT_COUNT0 0x0040

#define TIMER_FLAGS_FREE 0      //タイマが空いている状態
#define TIMER_FLAGS_ALLOCATED 1 //タイマを確保した状態
#define TIMER_FLAGS_USING 2     //タイマを使用中の状態

struct TIMERCONTROL timerControl;

void init_pit(void)
{
    int i;
    io_out8(PIT_CONTROL, 0x34); //PITの設定変更コマンド
    io_out8(PIT_COUNT0, 0x9c);  //PITの設定値の下位8bit
    io_out8(PIT_COUNT0, 0x2e);  //PITの設定値の上位8bit(併せて0x2e9c=119318で、タイマ割込みは100Hzになる)
    timerControl.count = 0;
    for (i = 0; i < MAX_TIMER; i++)
    {
        timerControl.timers[i].flags = TIMER_FLAGS_FREE;
    }

    return;
}

struct TIMER *timer_allocate(void)
{
    int i;
    for (i = 0; i < MAX_TIMER; i++)
    {
        if (timerControl.timers[i].flags == TIMER_FLAGS_FREE)
        {
            timerControl.timers[i].flags = TIMER_FLAGS_ALLOCATED;
            return &timerControl.timers[i];
        }
    }
    return 0; //見つからなかった
}

void timer_free(struct TIMER *timer)
{
    timer->flags = TIMER_FLAGS_FREE;
}

void timer_init(struct TIMER *timer, struct FIFO8 *fifo, unsigned char data)
{
    timer->fifo = fifo;
    timer->data = data;
    return;
}

void timer_set_time(struct TIMER *timer, unsigned int timeout)
{
    timer->timeout = timeout + timerControl.count;
    timer->flags = TIMER_FLAGS_USING;
    return;
}

//この方式では、count = 0xffffffff以降が設定できない
void inthandler20(int *esp)
{
    int i;
    io_out8(PIC0_OCW2, 0x60); //IRQ-00受付完了をPICに通知
    timerControl.count++;
    for (i = 0; i < MAX_TIMER; i++)
    {
        if (timerControl.timers[i].flags == TIMER_FLAGS_USING) //タイムアウトが設定されているとき
        {
            if (timerControl.timers[i].timeout <= timerControl.count)
            {
                timerControl.timers[i].flags = TIMER_FLAGS_ALLOCATED;
                fifo8_put(timerControl.timers[i].fifo, timerControl.timers[i].data);
            }
        }
    }
    return;
}
