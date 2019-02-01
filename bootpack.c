#include <stdio.h>
#include "bootpack.h"

extern struct KEYBUFFER keyBuffer;

void HariMain(void)
{
    struct BOOTINFO *bootInfo = (struct BOOTINFO *)ADRESS_BOOTINFO; //boot infoの開始アドレス
    char s[40], mouseCursor[256];
    int mouseX, mouseY, i;

    init_gdtidt();
    init_pic();
    io_sti();       //IDT/GDTの初期化が終わったら割り込み禁止を解除する

    init_palette();
    init_screen(bootInfo->vram, bootInfo->screenX, bootInfo->screenY);
    mouseX = (bootInfo->screenX - 16) / 2; //画面中央に配置
    mouseY = (bootInfo->screenY - 28 - 16) / 2;
    init_mouse_cursor8(mouseCursor, COLOR8_008484);
    putblock8_8(bootInfo->vram, bootInfo->screenX, 16, 16, mouseX, mouseY, mouseCursor, 16);

    sprintf(s, "(%d, %d)", mouseX, mouseY);
    putfonts8_asc(bootInfo->vram, bootInfo->screenX, 0, 0, COLOR8_FFFFFF, s);

    io_out8(PIC0_IMR, 0xf9);//PIC1とキーボードを許可(11111001)
    io_out8(PIC1_IMR, 0xef);//マウスを許可(11101111)

    for (;;)
    {
        io_cli();
        if(keyBuffer.flag == 0)
        {
            io_stihlt();
        }
        else
        {
            i = keyBuffer.data;
            keyBuffer.flag = 0;
            sprintf(s, "%02X", i);
            boxfill8(bootInfo->vram, bootInfo->screenX, COLOR8_008484, 0, 16, 15, 31);
            putfonts8_asc(bootInfo->vram, bootInfo->screenX, 0, 16, COLOR8_FFFFFF, s);
        }
    }
}
