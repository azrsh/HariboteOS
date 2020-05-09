#include <stdio.h>
#include "bootpack.h"

extern struct FIFO8 keyFifo;
void init_keyboard(void);
void enable_mouse(void);

void HariMain(void)
{
    struct BOOTINFO *bootInfo = (struct BOOTINFO *)ADRESS_BOOTINFO; //boot infoの開始アドレス
    char s[40], mouseCursor[256], keyBuffer[32];
    int mouseX, mouseY, i;

    init_gdtidt();
    init_pic();
    io_sti(); //IDT/GDTの初期化が終わったら割り込み禁止を解除する

    fifo8_init(&keyFifo, 32, keyBuffer); //fofoバッファを初期化
    io_out8(PIC0_IMR, 0xf9);             //PIC1とキーボードを許可(11111001)
    io_out8(PIC1_IMR, 0xef);             //マウスを許可(11101111)
    init_keyboard();

    init_palette();
    init_screen(bootInfo->vram, bootInfo->screenX, bootInfo->screenY);
    mouseX = (bootInfo->screenX - 16) / 2; //画面中央に配置
    mouseY = (bootInfo->screenY - 28 - 16) / 2;
    init_mouse_cursor8(mouseCursor, COLOR8_008484);
    putblock8_8(bootInfo->vram, bootInfo->screenX, 16, 16, mouseX, mouseY, mouseCursor, 16);

    sprintf(s, "(%d, %d)", mouseX, mouseY);
    putfonts8_asc(bootInfo->vram, bootInfo->screenX, 0, 0, COLOR8_FFFFFF, s);

    enable_mouse();

    for (;;)
    {
        io_cli();
        if (fifo8_status(&keyFifo) == 0)
        {
            io_stihlt();
        }
        else
        {
            i = fifo8_get(&keyFifo);
            io_sti();
            sprintf(s, "%02X", i);
            boxfill8(bootInfo->vram, bootInfo->screenX, COLOR8_008484, 0, 16, 15, 31);
            putfonts8_asc(bootInfo->vram, bootInfo->screenX, 0, 16, COLOR8_FFFFFF, s);
        }
    }
}

#define PORT_KEYDAT 0x0060
#define PORT_KEYSTA 0x0064
#define PORT_KEYCMD 0x0064
#define KEYSTA_SEND_NOTREADY 0x02
#define KEYCMD_WRITE_MODE 0x60
#define KBC_MODE 0x47

//キーボードコントローラがデータを送信可能になるまで待機
void wait_KBC_sendready(void)
{
    for (;;)
    {
        if ((io_in8(PORT_KEYSTA) & KEYSTA_SEND_NOTREADY) == 0)
        {
            break;
        }
    }

    return;
}

//キーボードコントローラの初期化
void init_keyboard(void)
{
    wait_KBC_sendready();
    io_out8(PORT_KEYCMD, KEYCMD_WRITE_MODE);
    wait_KBC_sendready();
    io_out8(PORT_KEYDAT, KBC_MODE);
    return;
}

#define KEYCMD_SENDTO_MOUSE 0xd4
#define MOUSECMD_ENABLE 0xf4

//マウスの有効化
void enable_mouse(void)
{
    wait_KBC_sendready();
    io_out8(PORT_KEYCMD, KEYCMD_SENDTO_MOUSE);
    wait_KBC_sendready();
    io_out8(PORT_KEYDAT, MOUSECMD_ENABLE);
    return; //成功するとACK(0xfa)がマウスから送信される
}
