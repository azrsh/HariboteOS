#include "bootpack.h"

//PITはProgramable Interval Timerの略
#define PIT_CONTROL 0x0043
#define PIT_COUNT0 0x0040

struct TIMERCONTROL timerControl;

void init_pit(void)
{
    io_out8(PIT_CONTROL, 0x34); //PITの設定変更コマンド
    io_out8(PIT_COUNT0, 0x9c);  //PITの設定値の下位8bit
    io_out8(PIT_COUNT0, 0x2e);  //PITの設定値の上位8bit(併せて0x2e9c=119318で、タイマ割込みは100Hzになる)
    timerControl.count = 0;
    timerControl.timeout = 0;
    return;
}

void inthandler20(int *esp)
{
    io_out8(PIC0_OCW2, 0x60); //IRQ-00受付完了をPICに通知
    timerControl.count++;
    if (timerControl.timeout > 0) //タイムアウトが設定されているとき
    {
        timerControl.timeout--;
        if (timerControl.timeout == 0)
        {
            fifo8_put(timerControl.fifo, timerControl.data);
        }
    }
    return;
}

void set_timer(unsigned int timeout, struct FIFO8 *fifo, unsigned char data)
{
    int eflags;
    eflags = io_load_eflags(); //割込み禁止
    io_cli();
    timerControl.timeout = timeout; //タイマの設定
    timerControl.fifo = fifo;
    timerControl.data = data;
    io_store_eflags(eflags); //割込み再開
    return;
}
