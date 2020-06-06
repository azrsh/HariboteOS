#include <stdio.h>
#include "bootpack.h"

void make_window8(unsigned char *buffer, int xSize, int ySize, char *title, char active);
void putfont8_asc_sheet(struct SHEET *sheet, int x, int y, int color, int backgroundColor, char *s, int length);
void make_textbox8(struct SHEET *sheet, int x0, int y0, int sx, int sy, int color);
void taskB_main(struct SHEET *sheetBackground);

void HariMain(void)
{
    struct BOOTINFO *bootInfo = (struct BOOTINFO *)ADRESS_BOOTINFO; //boot infoの開始アドレス
    struct FIFO32 fifo;
    int fifoBuffer[128];
    char s[40];
    struct TIMER *timer;
    int mouseX, mouseY, i, cursorX, cursorColor;
    unsigned int memoryTotal;
    struct MOUSE_DECODE mouseDecode;
    struct MEMORYMANAGER *memoryManager = (struct MEMORYMANAGER *)MEMMAN_ADDR;
    struct SHEETCONTROL *sheetControl;
    struct SHEET *sheetBackground, *sheetMouse, *sheetWindow, *sheetWindowB[3];
    unsigned char *bufferBackgroud, bufferMouse[256], *bufferWindow, *bufferWindowB;
    static char ketTable[0x54] = {
        0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '^', 0, 0,
        'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '@', '[', 0, 0, 'A', 'S',
        'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', ':', 0, 0, ']', 'Z', 'X', 'C', 'V',
        'B', 'N', 'M', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
        '2', '3', '0', '.'};
    struct TASK *taskA, *taskB[3];

    init_gdtidt();
    init_pic();
    io_sti(); //IDT/GDTの初期化が終わったら割り込み禁止を解除する

    fifo32_init(&fifo, 8, fifoBuffer, 0);
    init_pit();
    io_out8(PIC0_IMR, 0xf8); //PITとPIC1とキーボードを許可(11111000)
    io_out8(PIC1_IMR, 0xef); //マウスを許可(11101111)

    timer = timer_allocate(); //50/100Hz = 0.5秒
    timer_init(timer, &fifo, 1);
    timer_set_time(timer, 50);

    init_keyboard(&fifo, 256);
    enable_mouse(&fifo, 512, &mouseDecode);

    memoryTotal = memory_test(0x00400000, 0xbfffffff);
    memorymanager_init(memoryManager);
    memorymanager_free(memoryManager, 0x00001000, 0x0009e000); // 0x00001000 - 0x0009efff
    memorymanager_free(memoryManager, 0x00400000, memoryTotal - 0x00400000);

    init_palette();

    sheetControl = sheetcontrol_init(memoryManager, bootInfo->vram, bootInfo->screenX, bootInfo->screenY);
    taskA = task_init(memoryManager);
    fifo.task = taskA;

    //sheetBackground
    sheetBackground = sheet_allocate(sheetControl);
    bufferBackgroud = (unsigned char *)memorymanager_allocate_4k(memoryManager, bootInfo->screenX * bootInfo->screenY);
    sheet_set_buffer(sheetBackground, bufferBackgroud, bootInfo->screenX, bootInfo->screenY, -1); //透明色無し
    init_screen(bufferBackgroud, bootInfo->screenX, bootInfo->screenY);

    //sheetWindowBs
    for (i = 0; i < 3; i++)
    {
        sheetWindowB[i] = sheet_allocate(sheetControl);
        bufferWindowB = (unsigned char *)memorymanager_allocate_4k(memoryManager, 144 * 52);
        sheet_set_buffer(sheetWindowB[i], bufferWindowB, 144, 52, -1); //透明色無し
        sprintf(s, "taskB%d", i);
        make_window8(bufferWindowB, 144, 52, s, 0);
        taskB[i] = task_allocate();
        taskB[i]->tss.esp = memorymanager_allocate_4k(memoryManager, 64 * 1024) + 64 * 1024 - 8;
        taskB[i]->tss.eip = (int)&taskB_main;
        taskB[i]->tss.es = 1 * 8;
        taskB[i]->tss.cs = 2 * 8;
        taskB[i]->tss.ss = 1 * 8;
        taskB[i]->tss.ds = 1 * 8;
        taskB[i]->tss.fs = 1 * 8;
        taskB[i]->tss.gs = 1 * 8;
        *((int *)(taskB[i]->tss.esp + 4)) = (int)sheetWindowB[i];
        task_run(taskB[i], i + 1);
    }

    //sheetWindow
    sheetWindow = sheet_allocate(sheetControl);
    bufferWindow = (unsigned char *)memorymanager_allocate_4k(memoryManager, 160 * 52);
    sheet_set_buffer(sheetWindow, bufferWindow, 144, 52, -1); //透明色無し
    make_window8(bufferWindow, 144, 52, "taskA", 1);
    make_textbox8(sheetWindow, 8, 28, 128, 16, COLOR8_FFFFFF);
    cursorX = 8;
    cursorColor = COLOR8_FFFFFF;

    //sheetMouse
    sheetMouse = sheet_allocate(sheetControl);
    sheet_set_buffer(sheetMouse, bufferMouse, 16, 16, 99); //透明色は99番
    init_mouse_cursor8(bufferMouse, 99);
    mouseX = (bootInfo->screenX - 16) / 2; //画面中央に配置
    mouseY = (bootInfo->screenY - 28 - 16) / 2;

    sheet_slide(sheetBackground, 0, 0);
    sheet_slide(sheetWindow, 8, 56);
    sheet_slide(sheetWindowB[0], 168, 56);
    sheet_slide(sheetWindowB[1], 8, 116);
    sheet_slide(sheetWindowB[2], 168, 116);
    sheet_slide(sheetMouse, mouseX, mouseY);
    sheet_updown(sheetBackground, 0);
    sheet_updown(sheetWindow, 1);
    sheet_updown(sheetWindowB[0], 2);
    sheet_updown(sheetWindowB[1], 3);
    sheet_updown(sheetWindowB[2], 4);
    sheet_updown(sheetMouse, 5);
    sprintf(s, "(%3d, %3d)", mouseX, mouseY);
    putfont8_asc_sheet(sheetBackground, 0, 0, COLOR8_FFFFFF, COLOR8_008484, s, 10);
    sprintf(s, "memory %dMB    free : %dKB", memoryTotal / (1024 * 1024), memorymanager_total(memoryManager) / 1024);
    putfont8_asc_sheet(sheetBackground, 0, 32, COLOR8_FFFFFF, COLOR8_008484, s, 40);

    for (;;)
    {
        io_cli();
        if (fifo32_status(&fifo) == 0)
        {
            task_sleep(taskA);
            io_sti();
        }
        else
        {
            i = fifo32_get(&fifo);
            io_sti();

            if (i >= 256 && i < 512) //キーボードのデータだった時
            {
                sprintf(s, "%02X", i - 256);
                putfont8_asc_sheet(sheetBackground, 0, 16, COLOR8_FFFFFF, COLOR8_008484, s, 2);
                if (i < 0x54 + 256)
                {
                    if (ketTable[i - 256] != 0 && cursorX < 128) //通常文字
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

                    putfont8_asc_sheet(sheetBackground, 32, 16, COLOR8_FFFFFF, COLOR8_008484, s, 15);

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
                    putfont8_asc_sheet(sheetBackground, 0, 0, COLOR8_FFFFFF, COLOR8_008484, s, 10);
                    sheet_slide(sheetMouse, mouseX, mouseY); //カーソルの描画、sheet_refresh含む

                    if ((mouseDecode.button & 0x01) != 0)
                    {
                        //左ボタンを押していたらウィンドウを動かす
                        sheet_slide(sheetWindow, mouseX - 80, mouseY - 8);
                    }
                }
            }
            else if (i <= 1) //カーソル点滅用タイマ
            {
                if (i != 0)
                {
                    //白い矩形を描画して次のdataを0に
                    timer_init(timer, &fifo, 0);
                    cursorColor = COLOR8_000000;
                }
                else
                {
                    //背景と同色の矩形を描画して次のdataを1に
                    timer_init(timer, &fifo, 1);
                    cursorColor = COLOR8_FFFFFF;
                }
                timer_set_time(timer, 50);
                boxfill8(sheetWindow->buffer, sheetWindow->boxXSize, cursorColor, cursorX, 28, cursorX + 7, 43);
                sheet_refresh(sheetWindow, cursorX, 28, cursorX + 8, 44);
            }
        }
    }
}

void make_window8(unsigned char *buffer, int xSize, int ySize, char *title, char active)
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
    char c, titleColor, titleBackgroundColor;
    if (active != 0)
    {
        titleColor = COLOR8_FFFFFF;
        titleBackgroundColor = COLOR8_000084;
    }
    else
    {
        titleColor = COLOR8_C6C6C6;
        titleBackgroundColor = COLOR8_848484;
    }
    boxfill8(buffer, xSize, COLOR8_C6C6C6, 0, 0, xSize - 1, 0);
    boxfill8(buffer, xSize, COLOR8_FFFFFF, 1, 1, xSize - 2, 1);
    boxfill8(buffer, xSize, COLOR8_C6C6C6, 0, 0, 0, ySize - 1);
    boxfill8(buffer, xSize, COLOR8_FFFFFF, 1, 1, 1, ySize - 2);
    boxfill8(buffer, xSize, COLOR8_848484, xSize - 2, 1, xSize - 2, ySize - 2);
    boxfill8(buffer, xSize, COLOR8_000000, xSize - 1, 0, xSize - 1, ySize - 1);
    boxfill8(buffer, xSize, COLOR8_C6C6C6, 2, 2, xSize - 3, ySize - 3);
    boxfill8(buffer, xSize, titleBackgroundColor, 3, 3, xSize - 4, 20);
    boxfill8(buffer, xSize, COLOR8_848484, 1, ySize - 2, xSize - 2, ySize - 2);
    boxfill8(buffer, xSize, COLOR8_000000, 0, ySize - 1, xSize - 1, ySize - 1);
    putfonts8_asc(buffer, xSize, 24, 4, titleColor, title);
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

void taskB_main(struct SHEET *sheetWindowB)
{
    struct FIFO32 fifo;
    struct TIMER *timer1s;
    int i, fifoBuffer[128], count = 0, count0 = 0;
    char s[12];

    fifo32_init(&fifo, 128, fifoBuffer, 0);
    timer1s = timer_allocate();
    timer_init(timer1s, &fifo, 100);
    timer_set_time(timer1s, 100);

    for (;;)
    {
        count++;
        io_cli();
        if (fifo32_status(&fifo) == 0)
        {
            io_sti();
        }
        else
        {
            i = fifo32_get(&fifo);
            io_sti();
            if (i == 100)
            {
                sprintf(s, "%11d", count - count0);
                putfont8_asc_sheet(sheetWindowB, 24, 28, COLOR8_000000, COLOR8_C6C6C6, s, 11);
                count0 = count;
                timer_set_time(timer1s, 100);
            }
        }
    }
}
