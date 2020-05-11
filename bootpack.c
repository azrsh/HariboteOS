#include <stdio.h>
#include "bootpack.h"

void HariMain(void)
{
    struct BOOTINFO *bootInfo = (struct BOOTINFO *)ADRESS_BOOTINFO; //boot infoの開始アドレス
    char s[40], mouseCursor[256], keyBuffer[32], mouseBuffer[128];
    int mouseX, mouseY, i;
    unsigned int memoryTotal;
    struct MOUSE_DECODE mouseDecode;
    struct MEMORYMANAGER *memoryManager = (struct MEMORYMANAGER *)MEMMAN_ADDR;
    struct SHEETCONTROL *sheetControl;
    struct SHEET *sheetBackgroud, *sheetMouse;
    unsigned char *bufferBackgroud, bufferMouse[256];

    init_gdtidt();
    init_pic();
    io_sti(); //IDT/GDTの初期化が終わったら割り込み禁止を解除する

    fifo8_init(&keyFifo, 32, keyBuffer); //fofoバッファを初期化
    fifo8_init(&mouseFifo, 128, mouseBuffer);

    io_out8(PIC0_IMR, 0xf9); //PIC1とキーボードを許可(11111001)
    io_out8(PIC1_IMR, 0xef); //マウスを許可(11101111)

    init_keyboard();
    enable_mouse(&mouseDecode);

    memoryTotal = memory_test(0x00400000, 0xbfffffff);
    memorymanager_init(memoryManager);
    memorymanager_free(memoryManager, 0x00001000, 0x0009e000); // 0x00001000 - 0x0009efff
    memorymanager_free(memoryManager, 0x00400000, memoryTotal - 0x00400000);

    init_palette();

    sheetControl = sheetcontrol_init(memoryManager, bootInfo->vram, bootInfo->screenX, bootInfo->screenY);
    sheetBackgroud = sheet_allocate(sheetControl);
    sheetMouse = sheet_allocate(sheetControl);
    bufferBackgroud = (unsigned char *)memorymanager_allocate_4k(memoryManager, bootInfo->screenX * bootInfo->screenY);
    sheet_set_buffer(sheetBackgroud, bufferBackgroud, bootInfo->screenX, bootInfo->screenY, -1); //透明色無し
    sheet_set_buffer(sheetMouse, bufferMouse, 16, 16, 99);                                       //透明色は99番

    init_screen(bufferBackgroud, bootInfo->screenX, bootInfo->screenY);
    init_mouse_cursor8(bufferMouse, 99);
    sheet_slide(sheetControl, sheetBackgroud, 0, 0);
    mouseX = (bootInfo->screenX - 16) / 2; //画面中央に配置
    mouseY = (bootInfo->screenY - 28 - 16) / 2;
    sheet_slide(sheetControl, sheetMouse, mouseX, mouseY);
    sheet_updown(sheetControl, sheetBackgroud, 0);
    sheet_updown(sheetControl, sheetMouse, 1);
    sprintf(s, "(%3d, %3d)", mouseX, mouseY);
    putfonts8_asc(bufferBackgroud, bootInfo->screenX, 0, 0, COLOR8_FFFFFF, s);
    sprintf(s, "memory %dMB    free : %dKB", memoryTotal / (1024 * 1024), memorymanager_total(memoryManager) / 1024);
    putfonts8_asc(bufferBackgroud, bootInfo->screenX, 0, 32, COLOR8_FFFFFF, s);
    sheet_refresh(sheetControl, sheetBackgroud, 0, 0, bootInfo->screenX, 48);

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
                boxfill8(bufferBackgroud, bootInfo->screenX, COLOR8_008484, 0, 16, 15, 31);
                putfonts8_asc(bufferBackgroud, bootInfo->screenX, 0, 16, COLOR8_FFFFFF, s);
                sheet_refresh(sheetControl, sheetBackgroud, 0, 16, 16, 32);
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

                    boxfill8(bufferBackgroud, bootInfo->screenX, COLOR8_008484, 32, 16, 32 + 15 * 8 - 1, 31);
                    putfonts8_asc(bufferBackgroud, bootInfo->screenX, 32, 16, COLOR8_FFFFFF, s);
                    sheet_refresh(sheetControl, sheetBackgroud, 32, 16, 32 + 15 * 8, 32);

                    //カーソルの移動
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

                    sprintf(s, "(%3d, %3d)", mouseX, mouseY);
                    boxfill8(bufferBackgroud, bootInfo->screenX, COLOR8_008484, 0, 0, 79, 15); //マウスの座標表示を消す
                    putfonts8_asc(bufferBackgroud, bootInfo->screenX, 0, 0, COLOR8_FFFFFF, s); //マウスの座標表示の描画
                    sheet_refresh(sheetControl, sheetBackgroud, 0, 0, 80, 16);
                    sheet_slide(sheetControl, sheetMouse, mouseX, mouseY); //カーソルの描画、sheet_refresh含む
                }
            }
        }
    }
}
