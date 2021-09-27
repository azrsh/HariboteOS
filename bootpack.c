#include "bootpack.h"
#include <stdio.h>

void make_window8(unsigned char *buffer, int xSize, int ySize, char *title,
                  char active);
void make_wtitle8(unsigned char *buffer, int xSize, char *title, char active);
void putfont8_asc_sheet(struct SHEET *sheet, int x, int y, int color,
                        int backgroundColor, char *s, int length);
void make_textbox8(struct SHEET *sheet, int x0, int y0, int sx, int sy,
                   int color);
void console_task(struct SHEET *sheet);

void HariMain(void) {
  struct BOOTINFO *bootInfo =
      (struct BOOTINFO *)ADRESS_BOOTINFO; // boot infoの開始アドレス
  struct FIFO32 fifo;
  int fifoBuffer[128];
  char s[40];
  struct TIMER *timer;
  int mouseX, mouseY, i, cursorX, cursorColor;
  unsigned int memoryTotal;
  struct MOUSE_DECODE mouseDecode;
  struct MEMORYMANAGER *memoryManager = (struct MEMORYMANAGER *)MEMMAN_ADDR;
  struct SHEETCONTROL *sheetControl;
  struct SHEET *sheetBackground, *sheetMouse, *sheetWindow, *sheetConsole;
  unsigned char *bufferBackgroud, bufferMouse[256], *bufferWindow,
      *bufferConsole;
  static char ketTable[0x54] = {
      0, 0,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '^',
      0, 0,   'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '@', '[',
      0, 0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', ':', 0,
      0, ']', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/', 0,   '*',
      0, ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0, '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.'};
  struct TASK *taskA, *taskConsole;
  int keyTo = 0;

  init_gdtidt();
  init_pic();
  io_sti(); // IDT/GDTの初期化が終わったら割り込み禁止を解除する

  fifo32_init(&fifo, 8, fifoBuffer, 0);
  init_pit();
  io_out8(PIC0_IMR, 0xf8); // PITとPIC1とキーボードを許可(11111000)
  io_out8(PIC1_IMR, 0xef); //マウスを許可(11101111)

  timer = timer_allocate(); // 50/100Hz = 0.5秒
  timer_init(timer, &fifo, 1);
  timer_set_time(timer, 50);

  init_keyboard(&fifo, 256);
  enable_mouse(&fifo, 512, &mouseDecode);

  memoryTotal = memory_test(0x00400000, 0xbfffffff);
  memorymanager_init(memoryManager);
  memorymanager_free(memoryManager, 0x00001000,
                     0x0009e000); // 0x00001000 - 0x0009efff
  memorymanager_free(memoryManager, 0x00400000, memoryTotal - 0x00400000);

  init_palette();

  sheetControl = sheetcontrol_init(memoryManager, bootInfo->vram,
                                   bootInfo->screenX, bootInfo->screenY);
  taskA = task_init(memoryManager);
  fifo.task = taskA;
  task_run(taskA, 1, 0);

  // sheetBackground
  sheetBackground = sheet_allocate(sheetControl);
  bufferBackgroud = (unsigned char *)memorymanager_allocate_4k(
      memoryManager, bootInfo->screenX * bootInfo->screenY);
  sheet_set_buffer(sheetBackground, bufferBackgroud, bootInfo->screenX,
                   bootInfo->screenY, -1); //透明色無し
  init_screen(bufferBackgroud, bootInfo->screenX, bootInfo->screenY);

  // sheetConsole
  sheetConsole = sheet_allocate(sheetControl);
  bufferConsole =
      (unsigned char *)memorymanager_allocate_4k(memoryManager, 256 * 165);
  sheet_set_buffer(sheetConsole, bufferConsole, 256, 165, -1); //透明色無し
  make_window8(bufferConsole, 256, 165, "console", 0);
  make_textbox8(sheetConsole, 8, 28, 240, 128, COLOR8_000000);
  taskConsole = task_allocate();
  taskConsole->tss.esp =
      memorymanager_allocate_4k(memoryManager, 64 * 1024) + 64 * 1024 - 8;
  taskConsole->tss.eip = (int)&console_task;
  taskConsole->tss.es = 1 * 8;
  taskConsole->tss.cs = 2 * 8;
  taskConsole->tss.ss = 1 * 8;
  taskConsole->tss.ds = 1 * 8;
  taskConsole->tss.fs = 1 * 8;
  taskConsole->tss.gs = 1 * 8;
  *((int *)(taskConsole->tss.esp + 4)) = (int)sheetConsole;
  task_run(taskConsole, 2, 2); // level=2 priority=2

  // sheetWindow
  sheetWindow = sheet_allocate(sheetControl);
  bufferWindow =
      (unsigned char *)memorymanager_allocate_4k(memoryManager, 160 * 52);
  sheet_set_buffer(sheetWindow, bufferWindow, 144, 52, -1); //透明色無し
  make_window8(bufferWindow, 144, 52, "Task A", 1);
  make_textbox8(sheetWindow, 8, 28, 128, 16, COLOR8_FFFFFF);
  cursorX = 8;
  cursorColor = COLOR8_FFFFFF;

  // sheetMouse
  sheetMouse = sheet_allocate(sheetControl);
  sheet_set_buffer(sheetMouse, bufferMouse, 16, 16, 99); //透明色は99番
  init_mouse_cursor8(bufferMouse, 99);
  mouseX = (bootInfo->screenX - 16) / 2; //画面中央に配置
  mouseY = (bootInfo->screenY - 28 - 16) / 2;

  sheet_slide(sheetBackground, 0, 0);
  sheet_slide(sheetConsole, 32, 4);
  sheet_slide(sheetWindow, 64, 56);
  sheet_slide(sheetMouse, mouseX, mouseY);
  sheet_updown(sheetBackground, 0);
  sheet_updown(sheetConsole, 1);
  sheet_updown(sheetWindow, 2);
  sheet_updown(sheetMouse, 3);
  sprintf(s, "(%3d, %3d)", mouseX, mouseY);
  putfont8_asc_sheet(sheetBackground, 0, 0, COLOR8_FFFFFF, COLOR8_008484, s,
                     10);
  sprintf(s, "memory %dMB    free : %dKB", memoryTotal / (1024 * 1024),
          memorymanager_total(memoryManager) / 1024);
  putfont8_asc_sheet(sheetBackground, 0, 32, COLOR8_FFFFFF, COLOR8_008484, s,
                     40);

  for (;;) {
    io_cli();
    if (fifo32_status(&fifo) == 0) {
      task_sleep(taskA);
      io_sti();
    } else {
      i = fifo32_get(&fifo);
      io_sti();

      if (i >= 256 && i < 512) //キーボードのデータ
      {
        sprintf(s, "%02X", i - 256);
        putfont8_asc_sheet(sheetBackground, 0, 16, COLOR8_FFFFFF, COLOR8_008484,
                           s, 2);
        if (i < 0x54 + 256) {
          if (ketTable[i - 256] != 0 && cursorX < 128) //通常文字
          {
            //一文字表示してカーソルを一つ進める
            s[0] = ketTable[i - 256];
            s[1] = 0;
            putfont8_asc_sheet(sheetWindow, cursorX, 28, COLOR8_000000,
                               COLOR8_C6C6C6, s, 1);
            cursorX += 8;
          }
        }
        if (i == 256 + 0x0e && cursorX > 8) //バックスペース
        {
          //スペースで一文字上書きしてカーソルを戻す
          putfont8_asc_sheet(sheetWindow, cursorX, 28, COLOR8_000000,
                             COLOR8_FFFFFF, " ", 1);
          cursorX -= 8;
        }
        if (i == 256 + 0x0f) { // Tab
          if (keyTo == 0) {
            keyTo = 1;
            make_wtitle8(bufferWindow, sheetWindow->boxXSize, "Task A", 0);
            make_wtitle8(bufferConsole, sheetConsole->boxXSize, "Console", 1);
          } else {
            keyTo = 0;
            make_wtitle8(bufferWindow, sheetWindow->boxXSize, "Task A", 1);
            make_wtitle8(bufferConsole, sheetConsole->boxXSize, "Console", 0);
          }
          sheet_refresh(sheetWindow, 0, 0, sheetWindow->boxXSize, 21);
          sheet_refresh(sheetConsole, 0, 0, sheetConsole->boxXSize, 21);
        }
        //カーソルの再描画
        boxfill8(sheetWindow->buffer, sheetWindow->boxXSize, cursorColor,
                 cursorX, 28, cursorX + 7, 43);
        sheet_refresh(sheetWindow, cursorX, 28, cursorX + 8, 44);
      } else if (i >= 512 && i <= 767) {
        if (mouse_decode(&mouseDecode, i) != 0) {
          // 3バイト貯まったので表示
          sprintf(s, "[lcr %4d %4d]", mouseDecode.x, mouseDecode.y);
          if ((mouseDecode.button & 0x01) != 0) {
            s[1] = 'L';
          }
          if ((mouseDecode.button & 0x02) != 0) {
            s[3] = 'R';
          }
          if ((mouseDecode.button & 0x04) != 0) {
            s[2] = 'C';
          }

          putfont8_asc_sheet(sheetBackground, 32, 16, COLOR8_FFFFFF,
                             COLOR8_008484, s, 15);

          //マウスカーソルの移動
          mouseX += mouseDecode.x;
          mouseY += mouseDecode.y;

          if (mouseX < 0)
            mouseX = 0;
          if (mouseY < 0)
            mouseY = 0;
          if (mouseX > bootInfo->screenX - 1)
            mouseX =
                bootInfo->screenX -
                1; //画面ギリギリをクリックできるようにするための変更だが、このままだとvramの配列外参照するかはみ出した分が次の行に表示される
          if (mouseY > bootInfo->screenY - 1)
            mouseY = bootInfo->screenY - 1;

          sprintf(s, "(%3d, %3d)", mouseX, mouseY);
          putfont8_asc_sheet(sheetBackground, 0, 0, COLOR8_FFFFFF,
                             COLOR8_008484, s, 10);
          sheet_slide(sheetMouse, mouseX,
                      mouseY); //カーソルの描画、sheet_refresh含む

          if ((mouseDecode.button & 0x01) != 0) {
            //左ボタンを押していたらウィンドウを動かす
            sheet_slide(sheetWindow, mouseX - 80, mouseY - 8);
          }
        }
      } else if (i <= 1) //カーソル点滅用タイマ
      {
        if (i != 0) {
          //白い矩形を描画して次のdataを0に
          timer_init(timer, &fifo, 0);
          cursorColor = COLOR8_000000;
        } else {
          //背景と同色の矩形を描画して次のdataを1に
          timer_init(timer, &fifo, 1);
          cursorColor = COLOR8_FFFFFF;
        }
        timer_set_time(timer, 50);
        boxfill8(sheetWindow->buffer, sheetWindow->boxXSize, cursorColor,
                 cursorX, 28, cursorX + 7, 43);
        sheet_refresh(sheetWindow, cursorX, 28, cursorX + 8, 44);
      }
    }
  }
}

void make_window8(unsigned char *buffer, int xSize, int ySize, char *title,
                  char active) {
  boxfill8(buffer, xSize, COLOR8_C6C6C6, 0, 0, xSize - 1, 0);
  boxfill8(buffer, xSize, COLOR8_FFFFFF, 1, 1, xSize - 2, 1);
  boxfill8(buffer, xSize, COLOR8_C6C6C6, 0, 0, 0, ySize - 1);
  boxfill8(buffer, xSize, COLOR8_FFFFFF, 1, 1, 1, ySize - 2);
  boxfill8(buffer, xSize, COLOR8_848484, xSize - 2, 1, xSize - 2, ySize - 2);
  boxfill8(buffer, xSize, COLOR8_000000, xSize - 1, 0, xSize - 1, ySize - 1);
  boxfill8(buffer, xSize, COLOR8_C6C6C6, 2, 2, xSize - 3, ySize - 3);
  boxfill8(buffer, xSize, COLOR8_848484, 1, ySize - 2, xSize - 2, ySize - 2);
  boxfill8(buffer, xSize, COLOR8_000000, 0, ySize - 1, xSize - 1, ySize - 1);
  make_wtitle8(buffer, xSize, title, active);
}

void make_wtitle8(unsigned char *buffer, int xSize, char *title, char active) {
  static char closeButton[14][16] = {
      "OOOOOOOOOOOOOOO@", "OQQQQQQQQQQQQQ$@", "OQQQQQQQQQQQQQ$@",
      "OQQQ@@QQQQ@@QQ$@", "OQQQQ@@QQ@@QQQ$@", "OQQQQQ@@@@QQQQ$@",
      "OQQQQQQ@@QQQQQ$@", "OQQQQQ@@@@QQQQ$@", "OQQQQ@@QQ@@QQQ$@",
      "OQQQ@@QQQQ@@QQ$@", "OQQQQQQQQQQQQQ$@", "OQQQQQQQQQQQQQ$@",
      "O$$$$$$$$$$$$$$@", "@@@@@@@@@@@@@@@@"};

  int x, y;
  char c, titleColor, titleBackgroundColor;
  if (active != 0) {
    titleColor = COLOR8_FFFFFF;
    titleBackgroundColor = COLOR8_000084;
  } else {
    titleColor = COLOR8_C6C6C6;
    titleBackgroundColor = COLOR8_848484;
  }
  boxfill8(buffer, xSize, titleBackgroundColor, 3, 3, xSize - 4, 20);
  putfonts8_asc(buffer, xSize, 24, 4, titleColor, title);
  for (y = 0; y < 14; y++) {
    for (x = 0; x < 16; x++) {
      c = closeButton[y][x];
      if (c == '@') {
        c = COLOR8_000000;
      } else if (c == '$') {
        c = COLOR8_848484;
      } else if (c == 'Q') {
        c = COLOR8_C6C6C6;
      } else {
        c = COLOR8_FFFFFF;
      }
      buffer[(5 + y) * xSize + (xSize - 21 + x)] = c;
    }
  }
}

void putfont8_asc_sheet(struct SHEET *sheet, int x, int y, int color,
                        int backgroundColor, char *s, int length) {
  boxfill8(sheet->buffer, sheet->boxXSize, backgroundColor, x, y,
           x + length * 8 - 1, y + 16 - 1);
  putfonts8_asc(sheet->buffer, sheet->boxXSize, x, y, color, s);
  sheet_refresh(sheet, x, y, x + length * 8, y + 16);
}

void make_textbox8(struct SHEET *sheet, int x0, int y0, int sx, int sy,
                   int color) {
  int x1 = x0 + sx, y1 = y0 + sy;
  boxfill8(sheet->buffer, sheet->boxXSize, COLOR8_848484, x0 - 2, y0 - 3,
           x1 + 1, y0 - 3);
  boxfill8(sheet->buffer, sheet->boxXSize, COLOR8_848484, x0 - 3, y0 - 3,
           x0 - 3, y1 + 1);
  boxfill8(sheet->buffer, sheet->boxXSize, COLOR8_FFFFFF, x0 - 3, y1 + 2,
           x1 + 1, y1 + 2);
  boxfill8(sheet->buffer, sheet->boxXSize, COLOR8_FFFFFF, x1 + 2, y0 - 3,
           x1 + 2, y1 + 2);
  boxfill8(sheet->buffer, sheet->boxXSize, COLOR8_000000, x0 - 1, y0 - 2,
           x1 + 0, y0 - 2);
  boxfill8(sheet->buffer, sheet->boxXSize, COLOR8_000000, x0 - 2, y0 - 2,
           x0 - 2, y1 + 0);
  boxfill8(sheet->buffer, sheet->boxXSize, COLOR8_C6C6C6, x0 - 2, y1 + 1,
           x1 + 0, y1 + 1);
  boxfill8(sheet->buffer, sheet->boxXSize, COLOR8_C6C6C6, x1 + 1, y0 - 2,
           x1 + 1, y1 + 1);
  boxfill8(sheet->buffer, sheet->boxXSize, color, x0 - 1, y0 - 1, x1 + 0,
           y1 + 0);
}

void console_task(struct SHEET *sheet) {
  struct FIFO32 fifo;
  struct TIMER *timer;
  struct TASK *task = task_now();

  int i, fifoBuffer[128], cursorX = 8, cursorColor = COLOR8_000000;
  fifo32_init(&fifo, 128, fifoBuffer, task);

  timer = timer_allocate();
  timer_init(timer, &fifo, 1);
  timer_set_time(timer, 50);

  for (;;) {
    io_cli();
    if (fifo32_status(&fifo) == 0) {
      task_sleep(task);
      io_sti();
    } else {
      i = fifo32_get(&fifo);
      io_sti();
      if (i <= 1) {
        if (i != 0) {
          timer_init(timer, &fifo, 0);
          cursorColor = COLOR8_FFFFFF;
        } else {
          timer_init(timer, &fifo, 1);
          cursorColor = COLOR8_000000;
        }
        timer_set_time(timer, 50);
        boxfill8(sheet->buffer, sheet->boxXSize, cursorColor, cursorX, 28,
                 cursorX + 7, 43);
        sheet_refresh(sheet, cursorX, 28, cursorX + 8, 44);
      }
    }
  }
}
