#include <stdio.h>
#include "bootpack.h"

void make_window8(unsigned char *buffer, int xSize, int ySize, char *title);
void putfont8_asc_sheet(struct SHEET *sheet, int x, int y, int color, int backgroundColor, char *s, int length);
void make_textbox8(struct SHEET *sheet, int x0, int y0, int sx, int sy, int color);
void taskB_main(void);

void HariMain(void)
{
    struct BOOTINFO *bootInfo = (struct BOOTINFO *)ADRESS_BOOTINFO; //boot infoの開始アドレス
    struct FIFO32 fifo;
    int fifoBuffer[128];
    char s[40];
    struct TIMER *timer1, *timer2, *timer3, *timerTs;
    int mouseX, mouseY, i, cursorX, cursorColor, taskB_esp;
    unsigned int memoryTotal;
    struct MOUSE_DECODE mouseDecode;
    struct MEMORYMANAGER *memoryManager = (struct MEMORYMANAGER *)MEMMAN_ADDR;
    struct SHEETCONTROL *sheetControl;
    struct SHEET *sheetBackgroud, *sheetMouse, *sheetWindow;
    unsigned char *bufferBackgroud, bufferMouse[256], *bufferWindow;
    static char ketTable[0x54] = {
        0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '^', 0, 0,
        'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '@', '[', 0, 0, 'A', 'S',
        'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', ':', 0, 0, ']', 'Z', 'X', 'C', 'V',
        'B', 'N', 'M', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
        '2', '3', '0', '.'};
    struct TaskStatusSegment32 tssA, tssB;
    struct SEGMENT_DESCRIPTOR *gdt = (struct SEGMENT_DESCRIPTOR *)ADRESS_GDT;

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
    timerTs = timer_allocate(); //50/100Hz = 0.5秒
    timer_init(timerTs, &fifo, 2);
    timer_set_time(timerTs, 2);

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
    make_window8(bufferWindow, 160, 52, "window");
    make_textbox8(sheetWindow, 8, 28, 144, 16, COLOR8_FFFFFF);
    cursorX = 8;
    cursorColor = COLOR8_FFFFFF;
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

    tssA.ldtr = 0;
    tssA.iomap = 0x40000000;
    tssB.ldtr = 0;
    tssB.iomap = 0x40000000;
    set_segment_descriptor(gdt + 3, 103, (int)&tssA, AR_TSS32);
    set_segment_descriptor(gdt + 4, 103, (int)&tssB, AR_TSS32);
    load_tr(3 * 8);
    taskB_esp = memorymanager_allocate_4k(memoryManager, 64 * 1024) + 64 * 1024;
    tssB.eip = (int)&taskB_main;
    tssB.eflags = 0x00000202; //IF = 1;
    tssA.eax = 0;
    tssA.ecx = 0;
    tssA.edx = 0;
    tssA.ebx = 0;
    tssB.esp = taskB_esp;
    tssB.ebp = 0;
    tssB.esi = 0;
    tssB.edi = 0;
    tssB.es = 1 * 8;
    tssB.cs = 2 * 8;
    tssB.ss = 1 * 8;
    tssB.ds = 1 * 8;
    tssB.fs = 1 * 8;
    tssB.gs = 1 * 8;

    for (;;)
    {
        io_cli();
        if (fifo32_status(&fifo) == 0)
        {
            io_stihlt();
        }
        else
        {
            i = fifo32_get(&fifo);
            io_sti();

            if (i == 2)
            {
                farjump(0, 4 * 8);
                timer_set_time(timerTs, 2);
            }
            else if (i >= 256 && i < 512) //キーボードのデータだった時
            {
                sprintf(s, "%02X", i - 256);
                putfont8_asc_sheet(sheetBackgroud, 0, 16, COLOR8_FFFFFF, COLOR8_008484, s, 2);
                if (i < 0x54 + 256)
                {
                    if (ketTable[i - 256] != 0 && cursorX < 144) //通常文字
                    {
                        //一文字表示してカーソルを一つ進める
                        s[0] = ketTable[i - 256];
                        s[1] = 0;
                        putfont8_asc_sheet(sheetWindow, cursorX, 28, COLOR8_000000, COLOR8_C6C6C6, s, 1);
                        cursorX += 8;
                    }
                }
                if (i == 256 + 0x0e && cursorX > 8) //バックスペース
                {
                    //スペースで一文字上書きしてカーソルを戻す
                    putfont8_asc_sheet(sheetWindow, cursorX, 28, COLOR8_000000, COLOR8_FFFFFF, " ", 1);
                    cursorX -= 8;
                }
                //カーソルの再描画
                boxfill8(sheetWindow->buffer, sheetWindow->boxXSize, cursorColor, cursorX, 28, cursorX + 7, 43);
                sheet_refresh(sheetWindow, cursorX, 28, cursorX + 8, 44);
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

                    //マウスカーソルの移動
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

                    if ((mouseDecode.button & 0x01) != 0)
                    {
                        //左ボタンを押していたらウィンドウを動かす
                        sheet_slide(sheetWindow, mouseX - 80, mouseY - 8);
                    }
                }
            }
            else if (i == 10)
            {
                putfont8_asc_sheet(sheetBackgroud, 0, 64, COLOR8_FFFFFF, COLOR8_008484, "10[sec]", 7);
                farjump(0, 4 * 8);
            }
            else if (i == 3)
            {
                putfont8_asc_sheet(sheetBackgroud, 0, 80, COLOR8_FFFFFF, COLOR8_008484, "3[sec]", 6);
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

void make_textbox8(struct SHEET *sheet, int x0, int y0, int sx, int sy, int color)
{
    int x1 = x0 + sx, y1 = y0 + sy;
    boxfill8(sheet->buffer, sheet->boxXSize, COLOR8_848484, x0 - 2, y0 - 3, x1 + 1, y0 - 3);
    boxfill8(sheet->buffer, sheet->boxXSize, COLOR8_848484, x0 - 3, y0 - 3, x0 - 3, y1 + 1);
    boxfill8(sheet->buffer, sheet->boxXSize, COLOR8_FFFFFF, x0 - 3, y1 + 2, x1 + 1, y1 + 2);
    boxfill8(sheet->buffer, sheet->boxXSize, COLOR8_FFFFFF, x1 + 2, y0 - 3, x1 + 2, y1 + 2);
    boxfill8(sheet->buffer, sheet->boxXSize, COLOR8_000000, x0 - 1, y0 - 2, x1 + 0, y0 - 2);
    boxfill8(sheet->buffer, sheet->boxXSize, COLOR8_000000, x0 - 2, y0 - 2, x0 - 2, y1 + 0);
    boxfill8(sheet->buffer, sheet->boxXSize, COLOR8_C6C6C6, x0 - 2, y1 + 1, x1 + 0, y1 + 1);
    boxfill8(sheet->buffer, sheet->boxXSize, COLOR8_C6C6C6, x1 + 1, y0 - 2, x1 + 1, y1 + 1);
    boxfill8(sheet->buffer, sheet->boxXSize, color, x0 - 1, y0 - 1, x1 + 0, y1 + 0);
}

void taskB_main(void)
{
    struct FIFO32 fifo;
    struct TIMER *timerTs;
    int i, fifoBuffer[128];

    fifo32_init(&fifo, 128, fifoBuffer);
    timerTs = timer_allocate();
    timer_init(timerTs, &fifo, 1);
    timer_set_time(timerTs, 2);

    for (;;)
    {
        io_cli();
        if (fifo8_status(&fifo) == 0)
        {
            io_stihlt();
        }
        else
        {
            i = fifo32_get(&fifo);
            io_sti();
            if (i == 1) //5秒でタイムアウト
            {
                farjump(0, 3 * 8);
                timer_set_time(timerTs, 2);
            }
        }
    }
}
