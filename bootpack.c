#include <stdio.h>
#include "bootpack.h"

void HariMain(void)
{
    struct BOOTINFO *bootInfo = (struct BOOTINFO *)ADRESS_BOOTINFO; //boot infoの開始アドレス
    char s[40], mouseCursor[256], keyBuffer[32], mouseBuffer[128];
    int mouseX, mouseY, i;
    struct MOUSE_DECODE mouseDecode;

    init_gdtidt();
    init_pic();
    io_sti(); //IDT/GDTの初期化が終わったら割り込み禁止を解除する

    fifo8_init(&keyFifo, 32, keyBuffer); //fofoバッファを初期化
    fifo8_init(&mouseFifo, 128, mouseBuffer);

    io_out8(PIC0_IMR, 0xf9); //PIC1とキーボードを許可(11111001)
    io_out8(PIC1_IMR, 0xef); //マウスを許可(11101111)
    init_keyboard();

    init_palette();
    init_screen(bootInfo->vram, bootInfo->screenX, bootInfo->screenY);
    mouseX = (bootInfo->screenX - 16) / 2; //画面中央に配置
    mouseY = (bootInfo->screenY - 28 - 16) / 2;
    init_mouse_cursor8(mouseCursor, COLOR8_008484);
    putblock8_8(bootInfo->vram, bootInfo->screenX, 16, 16, mouseX, mouseY, mouseCursor, 16);
    sprintf(s, "(%d, %d)", mouseX, mouseY);
    putfonts8_asc(bootInfo->vram, bootInfo->screenX, 0, 0, COLOR8_FFFFFF, s);

    enable_mouse(&mouseDecode);

    for (;;)
    {
        io_cli();
        if (fifo8_status(&keyFifo) + fifo8_status(&mouseFifo) == 0)
        {
            io_stihlt();
        }
        else
        {
            if (fifo8_status(&keyFifo) != 0)
            {
                i = fifo8_get(&keyFifo);
                io_sti();
                sprintf(s, "%02X", i);
                boxfill8(bootInfo->vram, bootInfo->screenX, COLOR8_008484, 0, 16, 15, 31);
                putfonts8_asc(bootInfo->vram, bootInfo->screenX, 0, 16, COLOR8_FFFFFF, s);
            }
            else if (fifo8_status(&mouseFifo) != 0)
            {
                i = fifo8_get(&mouseFifo);
                io_sti();

                if (mouse_decode(&mouseDecode, i) != 0)
                {
                    //3バイト貯まったので表示
                    sprintf(s, "[lcr %4d %4d]", mouseDecode.x, mouseDecode.y);
                    if ((mouseDecode.button & 0x01) != 0)
                    {
                        s[1] = 'L';
                    }
                    if ((mouseDecode.button & 0x02) != 0)
                    {
                        s[3] = 'R';
                    }
                    if ((mouseDecode.button & 0x04) != 0)
                    {
                        s[2] = 'C';
                    }

                    boxfill8(bootInfo->vram, bootInfo->screenX, COLOR8_008484, 32, 16, 32 + 15 * 8 - 1, 31);
                    putfonts8_asc(bootInfo->vram, bootInfo->screenX, 32, 16, COLOR8_FFFFFF, s);

                    //カーソルの移動
                    boxfill8(bootInfo->vram, bootInfo->screenX, COLOR8_008484, mouseX, mouseY, mouseX + 15, mouseY + 15); //前のカーソルの削除
                    mouseX += mouseDecode.x;
                    mouseY += mouseDecode.y;

                    if (mouseX < 0)
                        mouseX = 0;
                    if (mouseY < 0)
                        mouseY = 0;
                    if (mouseX > bootInfo->screenX - 16)
                        mouseX = bootInfo->screenX - 16;
                    if (mouseY > bootInfo->screenY - 16)
                        mouseY = bootInfo->screenY - 16;

                    sprintf(s, "(%d, %d)", mouseX, mouseY);
                    boxfill8(bootInfo->vram, bootInfo->screenX, COLOR8_008484, 0, 0, 79, 15);                //マウスの座標表示を消す
                    putfonts8_asc(bootInfo->vram, bootInfo->screenX, 0, 0, COLOR8_FFFFFF, s);                //マウスの座標表示の描画
                    putblock8_8(bootInfo->vram, bootInfo->screenX, 16, 16, mouseX, mouseY, mouseCursor, 16); //カーソルの描画
                }
            }
        }
    }
}
