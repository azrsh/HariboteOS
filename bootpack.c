#include <stdio.h>
#include "bootpack.h"

void make_window8(unsigned char *buffer, int xSize, int ySize, char *title);
void putfont8_asc_sheet(struct SHEET *sheet, int x, int y, int color, int backgroundColor, char *s, int length);

void HariMain(void)
{
    struct BOOTINFO *bootInfo = (struct BOOTINFO *)ADRESS_BOOTINFO; //boot infoの開始アドレス
    struct FIFO32 fifo;
    int fifoBuffer[128];
    char s[40];
    struct TIMER *timer1, *timer2, *timer3;
    int mouseX, mouseY, i, count = 0;
    unsigned int memoryTotal;
    struct MOUSE_DECODE mouseDecode;
    struct MEMORYMANAGER *memoryManager = (struct MEMORYMANAGER *)MEMMAN_ADDR;
    struct SHEETCONTROL *sheetControl;
    struct SHEET *sheetBackgroud, *sheetMouse, *sheetWindow;
    unsigned char *bufferBackgroud, bufferMouse[256], *bufferWindow;

    init_gdtidt();
    init_pic();
    io_sti(); //IDT/GDTの初期化が終わったら割り込み禁止を解除する

    fifo32_init(&fifo, 8, fifoBuffer);
    init_pit();
    io_out8(PIC0_IMR, 0xf8); //PITとPIC1とキーボードを許可(11111000)
    io_out8(PIC1_IMR, 0xef); //マウスを許可(11101111)

    timer1 = timer_allocate(); //1000/100Hz = 10秒
    timer_init(timer1, &fifo, 10);
    timer_set_time(timer1, 1000);
    timer2 = timer_allocate(); //300/100Hz = 3秒
    timer_init(timer2, &fifo, 3);
    timer_set_time(timer2, 300);
    timer3 = timer_allocate(); //50/100Hz = 0.5秒
    timer_init(timer3, &fifo, 1);
    timer_set_time(timer3, 50);

    init_keyboard(&fifo, 256);
    enable_mouse(&fifo, 512, &mouseDecode);

    memoryTotal = memory_test(0x00400000, 0xbfffffff);
    memorymanager_init(memoryManager);
    memorymanager_free(memoryManager, 0x00001000, 0x0009e000); // 0x00001000 - 0x0009efff
    memorymanager_free(memoryManager, 0x00400000, memoryTotal - 0x00400000);

    init_palette();

    sheetControl = sheetcontrol_init(memoryManager, bootInfo->vram, bootInfo->screenX, bootInfo->screenY);
    sheetBackgroud = sheet_allocate(sheetControl);
    sheetMouse = sheet_allocate(sheetControl);
    sheetWindow = sheet_allocate(sheetControl);
    bufferBackgroud = (unsigned char *)memorymanager_allocate_4k(memoryManager, bootInfo->screenX * bootInfo->screenY);
    bufferWindow = (unsigned char *)memorymanager_allocate_4k(memoryManager, 160 * 52);
    sheet_set_buffer(sheetBackgroud, bufferBackgroud, bootInfo->screenX, bootInfo->screenY, -1); //透明色無し
    sheet_set_buffer(sheetMouse, bufferMouse, 16, 16, 99);                                       //透明色は99番
    sheet_set_buffer(sheetWindow, bufferWindow, 160, 52, -1);                                    //透明色無し

    init_screen(bufferBackgroud, bootInfo->screenX, bootInfo->screenY);
    init_mouse_cursor8(bufferMouse, 99);
    make_window8(bufferWindow, 160, 52, "counter");
    sheet_slide(sheetBackgroud, 0, 0);
    mouseX = (bootInfo->screenX - 16) / 2; //画面中央に配置
    mouseY = (bootInfo->screenY - 28 - 16) / 2;
    sheet_slide(sheetMouse, mouseX, mouseY);
    sheet_slide(sheetWindow, 80, 72);
    sheet_updown(sheetBackgroud, 0);
    sheet_updown(sheetWindow, 1);
    sheet_updown(sheetMouse, 2);
    sprintf(s, "(%3d, %3d)", mouseX, mouseY);
    putfont8_asc_sheet(sheetBackgroud, 0, 0, COLOR8_FFFFFF, COLOR8_008484, s, 10);
    sprintf(s, "memory %dMB    free : %dKB", memoryTotal / (1024 * 1024), memorymanager_total(memoryManager) / 1024);
    putfont8_asc_sheet(sheetBackgroud, 0, 32, COLOR8_FFFFFF, COLOR8_008484, s, 40);

    for (;;)
    {
        count++;

        io_cli();
        if (fifo32_status(&fifo) == 0)
        {
            io_sti(); //io_stihlt()をやめた
        }
        else
        {
            i = fifo32_get(&fifo);
            io_sti();

            if (i >= 256 && i < 512) //キーボードのデータだった時
            {
                sprintf(s, "%02X", i);
                putfont8_asc_sheet(sheetBackgroud, 0, 16, COLOR8_FFFFFF, COLOR8_008484, s, 2);
            }
            else if (i >= 512 && i <= 767)
            {
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

                    putfont8_asc_sheet(sheetBackgroud, 32, 16, COLOR8_FFFFFF, COLOR8_008484, s, 15);

                    //カーソルの移動
                    mouseX += mouseDecode.x;
                    mouseY += mouseDecode.y;

                    if (mouseX < 0)
                        mouseX = 0;
                    if (mouseY < 0)
                        mouseY = 0;
                    if (mouseX > bootInfo->screenX - 1)
                        mouseX = bootInfo->screenX - 1; //画面ギリギリをクリックできるようにするための変更だが、このままだとvramの配列外参照するかはみ出した分が次の行に表示される
                    if (mouseY > bootInfo->screenY - 1)
                        mouseY = bootInfo->screenY - 1;

                    sprintf(s, "(%3d, %3d)", mouseX, mouseY);
                    putfont8_asc_sheet(sheetBackgroud, 0, 0, COLOR8_FFFFFF, COLOR8_008484, s, 10);
                    sheet_slide(sheetMouse, mouseX, mouseY); //カーソルの描画、sheet_refresh含む
                }
            }
            else if (i == 10)
            {
                putfont8_asc_sheet(sheetBackgroud, 0, 64, COLOR8_FFFFFF, COLOR8_008484, "10[sec]", 7);

                sprintf(s, "%010d", count);
                putfont8_asc_sheet(sheetWindow, 40, 28, COLOR8_000000, COLOR8_C6C6C6, s, 10);
            }
            else if (i == 3)
            {
                putfont8_asc_sheet(sheetBackgroud, 0, 80, COLOR8_FFFFFF, COLOR8_008484, "3[sec]", 6);
                count = 0; //測定開始(初期化にかかる時間は微妙な条件で変化するのでここから開始)
            }
            else if (i == 1)
            {
                //白い矩形を描画して次のdataを0に
                timer_init(timer3, &fifo, 0);
                boxfill8(bufferBackgroud, bootInfo->screenX, COLOR8_FFFFFF, 8, 96, 15, 111);
                timer_set_time(timer3, 50);
                sheet_refresh(sheetBackgroud, 8, 96, 16, 112);
            }
            else if (i == 0)
            {
                //背景と同色の矩形を描画して次のdataを1に
                timer_init(timer3, &fifo, 1);
                boxfill8(bufferBackgroud, bootInfo->screenX, COLOR8_008484, 8, 96, 16, 112);
                timer_set_time(timer3, 50);
                sheet_refresh(sheetBackgroud, 8, 96, 16, 112);
            }
        }
    }
}

void make_window8(unsigned char *buffer, int xSize, int ySize, char *title)
{
    static char closeButton[14][16] = {
        "OOOOOOOOOOOOOOO@",
        "OQQQQQQQQQQQQQ$@",
        "OQQQQQQQQQQQQQ$@",
        "OQQQ@@QQQQ@@QQ$@",
        "OQQQQ@@QQ@@QQQ$@",
        "OQQQQQ@@@@QQQQ$@",
        "OQQQQQQ@@QQQQQ$@",
        "OQQQQQ@@@@QQQQ$@",
        "OQQQQ@@QQ@@QQQ$@",
        "OQQQ@@QQQQ@@QQ$@",
        "OQQQQQQQQQQQQQ$@",
        "OQQQQQQQQQQQQQ$@",
        "O$$$$$$$$$$$$$$@",
        "@@@@@@@@@@@@@@@@"};

    int x, y;
    char c;
    boxfill8(buffer, xSize, COLOR8_C6C6C6, 0, 0, xSize - 1, 0);
    boxfill8(buffer, xSize, COLOR8_FFFFFF, 1, 1, xSize - 2, 1);
    boxfill8(buffer, xSize, COLOR8_C6C6C6, 0, 0, 0, ySize - 1);
    boxfill8(buffer, xSize, COLOR8_FFFFFF, 1, 1, 1, ySize - 2);
    boxfill8(buffer, xSize, COLOR8_848484, xSize - 2, 1, xSize - 2, ySize - 2);
    boxfill8(buffer, xSize, COLOR8_000000, xSize - 1, 0, xSize - 1, ySize - 1);
    boxfill8(buffer, xSize, COLOR8_C6C6C6, 2, 2, xSize - 3, ySize - 3);
    boxfill8(buffer, xSize, COLOR8_000084, 3, 3, xSize - 4, 20);
    boxfill8(buffer, xSize, COLOR8_848484, 1, ySize - 2, xSize - 2, ySize - 2);
    boxfill8(buffer, xSize, COLOR8_000000, 0, ySize - 1, xSize - 1, ySize - 1);
    putfonts8_asc(buffer, xSize, 24, 4, COLOR8_FFFFFF, title);
    for (y = 0; y < 14; y++)
    {
        for (x = 0; x < 16; x++)
        {
            c = closeButton[y][x];
            if (c == '@')
            {
                c = COLOR8_000000;
            }
            else if (c == '$')
            {
                c = COLOR8_848484;
            }
            else if (c == 'Q')
            {
                c = COLOR8_C6C6C6;
            }
            else
            {
                c = COLOR8_FFFFFF;
            }
            buffer[(5 + y) * xSize + (xSize - 21 + x)] = c;
        }
    }
}

void putfont8_asc_sheet(struct SHEET *sheet, int x, int y, int color, int backgroundColor, char *s, int length)
{
    boxfill8(sheet->buffer, sheet->boxXSize, backgroundColor, x, y, x + length * 8 - 1, y + 16 - 1);
    putfonts8_asc(sheet->buffer, sheet->boxXSize, x, y, color, s);
    sheet_refresh(sheet, x, y, x + length * 8, y + 16);
}

void set490timers(struct FIFO32 *fifo, int mode)
{
    int i;
    struct TIMER *timer;
    if (mode != 0)
    {
        for (i = 0; i < 490; i++)
        {
            timer = timer_allocate();
            timer_init(timer, fifo, 1024 + i);
            timer_set_time(timer, 100 * 60 * 60 * 24 * 50 + i * 100);
        }
    }
}
