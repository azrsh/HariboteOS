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
    struct TIMER *timer;
    io_out8(PIT_CONTROL, 0x34); //PITの設定変更コマンド
    io_out8(PIT_COUNT0, 0x9c);  //PITの設定値の下位8bit
    io_out8(PIT_COUNT0, 0x2e);  //PITの設定値の上位8bit(併せて0x2e9c=119318で、タイマ割込みは100Hzになる)
    timerControl.count = 0;
    for (i = 0; i < MAX_TIMER; i++)
    {
        timerControl.timers0[i].flags = TIMER_FLAGS_FREE;
    }
    timer = timer_allocate(); //最後尾にINT_MAXをtimeoutとする(つまり永遠に使われないと期待できる)要素を置き、状態を減らす。この手法を番兵という
    timer->timeout = 0xffffffff;
    timer->flags = TIMER_FLAGS_USING;
    timer->next = 0;
    timerControl.t0 = timer;
    timerControl.next_time = timer->timeout;
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
    int eflags;
    struct TIMER *t, *s;
    timer->timeout = timeout + timerControl.count;
    timer->flags = TIMER_FLAGS_USING;

    eflags = io_load_eflags();
    io_cli();

    t = timerControl.t0;
    if (timer->timeout <= t->timeout)
    {
        //先頭に入れる
        timerControl.t0 = timer;
        timer->next = t;
        timerControl.next_time = timer->timeout;
        io_store_eflags(eflags);
        return;
    }
    //挿入すべき場所を探索
    for (;;)
    {
        s = t;
        t = t->next;

        if (timer->timeout <= t->timeout)
        {
            //sとtの間に挿入
            s->next = timer;
            timer->next = t;
            io_store_eflags(eflags);
            return;
        }
    }
}

//この方式では、count = 0xffffffff以降が設定できない
void inthandler20(int *esp)
{
    int i;
    struct TIMER *timer;
    io_out8(PIC0_OCW2, 0x60); //IRQ-00受付完了をPICに通知
    timerControl.count++;
    if (timerControl.next_time > timerControl.count)
    {
        return; //まだ次の時刻に達していないので戻る
    }
    timer = timerControl.t0;
    for (;;)
    {
        //タイマはすべて作動中のはずなので、flagsは確認しない
        if (timer->timeout > timerControl.count)
        {
            break;
        }
        //タイムアウト
        timer->flags = TIMER_FLAGS_ALLOCATED;
        fifo32_put(timer->fifo, timer->data);
        timer = timer->next;
    }

    //新しいずらし方
    timerControl.t0 = timer;
    //最も近いtimmeoutにnextを更新
    timerControl.next_time = timerControl.t0->timeout;
    return;
}
