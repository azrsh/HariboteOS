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
    timerControl.next = 0xffffffff; //最初は作動中のタイマがないのでINT_MAXを代入
    timerControl.using = 0;         //最初は作動中のタイマがないので0を代入
    for (i = 0; i < MAX_TIMER; i++)
    {
        timerControl.timers0[i].flags = TIMER_FLAGS_FREE;
    }

    return;
}

struct TIMER *timer_allocate(void)
{
    int i;
    for (i = 0; i < MAX_TIMER; i++)
    {
        if (timerControl.timers0[i].flags == TIMER_FLAGS_FREE)
        {
            timerControl.timers0[i].flags = TIMER_FLAGS_ALLOCATED;
            return &timerControl.timers0[i];
        }
    }
    return 0; //見つからなかった
}

void timer_free(struct TIMER *timer)
{
    timer->flags = TIMER_FLAGS_FREE;
}

void timer_init(struct TIMER *timer, struct FIFO32 *fifo, int data)
{
    timer->fifo = fifo;
    timer->data = data;
    return;
}

void timer_set_time(struct TIMER *timer, unsigned int timeout)
{
    int eflags, i, j;
    timer->timeout = timeout + timerControl.count;
    timer->flags = TIMER_FLAGS_USING;

    eflags = io_load_eflags();
    io_cli();

    for (i = 0; i < timerControl.using; i++)
    {
        if (timerControl.timers[i]->timeout >= timer->timeout)
        {
            break;
        }
    }
    //後ろにある要素を一つ後ろへずらす
    for (j = timerControl.using; j > i; j--)
    {
        timerControl.timers[j] = timerControl.timers[j - 1];
    }
    timerControl.using ++;

    timerControl.timers[i] = timer;
    timerControl.next = timerControl.timers[0]->timeout;

    io_store_eflags(eflags);
    return;
}

//この方式では、count = 0xffffffff以降が設定できない
void inthandler20(int *esp)
{
    int i, j;
    io_out8(PIC0_OCW2, 0x60); //IRQ-00受付完了をPICに通知
    timerControl.count++;
    if (timerControl.next > timerControl.count)
    {
        return; //まだ次の時刻に達していないので戻る
    }
    timerControl.next = 0xffffffffffff;
    for (i = 0; i < timerControl.using; i++)
    {
        //タイマはすべて作動中のはずなので、flagsは確認しない
        if (timerControl.timers[i]->timeout > timerControl.count)
        {
            break;
        }
        //タイムアウト
        timerControl.timers[i]->flags = TIMER_FLAGS_ALLOCATED;
        fifo32_put(timerControl.timers[i]->fifo, timerControl.timers[i]->data);
    }

    //i個のタイマがタイムアウト
    timerControl.using -= i;
    //残りをずらす
    for (j = 0; j < timerControl.using; j++)
    {
        timerControl.timers[j] = timerControl.timers[i + j];
    }
    //最も近いtimmeoutにnextを更新
    if (timerControl.using > 0)
    {
        timerControl.next = timerControl.timers[i]->timeout;
    }
    else
    {
        timerControl.next = 0xffffffff;
    }

    return;
}
