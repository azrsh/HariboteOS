void io_hlt(void);
void io_cli(void);
void io_out8(int port, int data);
int io_load_eflags(void);
void io_store_eflags(int eflags);

void init_palette(void);
void set_palette(int start, int end, unsigned char *rgb);
void boxfill8(unsigned char *vram, int x_size, unsigned char color, int x0, int y0, int x1, int y1);
void init_screen(char *vram, int x_size, int y_size);
void putfont8(unsigned char *vram, int x_size, int x, int y, char c, char *font);
void putfonts8_asc(unsigned char *vram, int x_size, int x, int y, char color, unsigned char *s);

const unsigned char Color8_000000 = 0;
const unsigned char Color8_FF0000 = 1;
const unsigned char Color8_00FF00 = 2;
const unsigned char Color8_FFFF00 = 3;
const unsigned char Color8_0000FF = 4;
const unsigned char Color8_FF00FF = 5;
const unsigned char Color8_00FFFF = 6;
const unsigned char Color8_FFFFFF = 7;
const unsigned char Color8_C6C6C6 = 8;
const unsigned char Color8_840000 = 9;
const unsigned char Color8_008400 = 10;
const unsigned char Color8_848400 = 11;
const unsigned char Color8_000084 = 12;
const unsigned char Color8_840084 = 13;
const unsigned char Color8_008484 = 14;
const unsigned char Color8_848484 = 15;

struct BOOTINFO
{
    char cyls, leds, vmode, reserve;
    short screenX, screenY;
    char *vram;
};

void HariMain(void)
{
    struct BOOTINFO *boot_info = (struct BOOTINFO *)0xff0; //boot infoの開始アドレス

    init_palette();
    init_screen(boot_info->vram, boot_info->screenX, boot_info->screenY);
    putfonts8_asc(boot_info->vram, boot_info->screenX, 8, 8, Color8_FFFFFF, "ABCD 1234");
    putfonts8_asc(boot_info->vram, boot_info->screenX, 31, 31, Color8_000000, "Haribote OS.");
    putfonts8_asc(boot_info->vram, boot_info->screenX, 30, 30, Color8_FFFFFF, "Haribote OS.");

    for (;;)
    {
        io_hlt();
    }
}

void init_palette(void)
{
    static unsigned char table_rgb[16 * 3] =
        {
            0x00, 0x00, 0x00, // 0:黒
            0xff, 0x00, 0x00, // 1:明るい赤
            0x00, 0xff, 0x00, // 2:明るい緑
            0xff, 0xff, 0x00, // 3:明るい黄色
            0x00, 0x00, 0xff, // 4:明るい青
            0xff, 0x00, 0xff, // 5:明るい紫
            0x00, 0xff, 0xff, // 6:明るい水色
            0xff, 0xff, 0xff, // 7:白
            0xc6, 0xc6, 0xc6, // 8:明るい灰色
            0x84, 0x00, 0x00, // 9:暗い赤
            0x00, 0x84, 0x00, //10:暗い緑
            0x84, 0x84, 0x00, //11:暗い黄色
            0x00, 0x00, 0x84, //12:暗い青
            0x84, 0x00, 0x84, //13:暗い紫
            0x00, 0x84, 0x84, //14:暗い水色
            0x84, 0x84, 0x84, //15:暗い灰色
        };
    /*static char命令は、データにしか使えないがDB命令相当*/

    set_palette(0, 15, table_rgb);
    return;
}

void set_palette(int start, int end, unsigned char *rgb)
{
    int i, eflags;
    eflags = io_load_eflags(); //現在の割り込み許可フラグを記録
    io_cli();                  //割り込みフラグを0にして割り込みを禁止
    io_out8(0x03c8, start);
    for (i = start; i < end; i++)
    {
        io_out8(0x03c9, rgb[0] / 4);
        io_out8(0x03c9, rgb[1] / 4);
        io_out8(0x03c9, rgb[2] / 4);
        rgb += 3;
    }
    io_store_eflags(eflags); //元の割り込みフラグの値に戻す
    return;
}

void boxfill8(unsigned char *vram, int x_size, unsigned char color, int x0, int y0, int x1, int y1)
{
    int x, y;
    for (y = y0; y <= y1; y++)
    {
        for (x = x0; x <= x1; x++)
            vram[y * x_size + x] = color;
    }
    return;
}

void init_screen(char *vram, int x_size, int y_size)
{
    //画面の下絵(？)
    boxfill8(vram, x_size, Color8_008484, 0, 0, x_size - 1, y_size - 29);           //デスクトップの背景
    boxfill8(vram, x_size, Color8_C6C6C6, 0, y_size - 28, x_size - 1, y_size - 28); //タスクバーとの境界の灰色線
    boxfill8(vram, x_size, Color8_FFFFFF, 0, y_size - 27, x_size - 1, y_size - 27); //タスクバーとの境界の白線
    boxfill8(vram, x_size, Color8_C6C6C6, 0, y_size - 26, x_size - 1, y_size - 1);  //タスクバーとの境界線とタスクバーの塗りつぶしの灰色

    //タスクバー右側のアイコンの境界線
    boxfill8(vram, x_size, Color8_FFFFFF, 3, y_size - 24, 59, y_size - 24);
    boxfill8(vram, x_size, Color8_FFFFFF, 2, y_size - 24, 2, y_size - 4);
    boxfill8(vram, x_size, Color8_848484, 3, y_size - 4, 59, y_size - 4);
    boxfill8(vram, x_size, Color8_848484, 59, y_size - 23, 59, y_size - 5);
    boxfill8(vram, x_size, Color8_000000, 2, y_size - 3, 59, y_size - 3);
    boxfill8(vram, x_size, Color8_000000, 60, y_size - 24, 60, y_size - 3);

    //タスクバー左側のアイコンの境界線
    boxfill8(vram, x_size, Color8_848484, x_size - 47, y_size - 24, x_size - 4, y_size - 24);
    boxfill8(vram, x_size, Color8_848484, x_size - 47, y_size - 23, x_size - 47, y_size - 4);
    boxfill8(vram, x_size, Color8_FFFFFF, x_size - 47, y_size - 3, x_size - 4, y_size - 3);
    boxfill8(vram, x_size, Color8_FFFFFF, x_size - 3, y_size - 24, x_size - 3, y_size - 3);
}

void putfont8(unsigned char *vram, int x_size, int x, int y, char c, char *font)
{
    int i;
    char *p, d;
    for (i = 0; i < 16; i++)
    {
        p = vram + (y + i) * x_size + x;
        d = font[i];
        if ((d & 0x80) != 0)
            p[0] = c;
        if ((d & 0x40) != 0)
            p[1] = c;
        if ((d & 0x20) != 0)
            p[2] = c;
        if ((d & 0x10) != 0)
            p[3] = c;
        if ((d & 0x08) != 0)
            p[4] = c;
        if ((d & 0x04) != 0)
            p[5] = c;
        if ((d & 0x02) != 0)
            p[6] = c;
        if ((d & 0x01) != 0)
            p[7] = c;
    }
    return;
}

void putfonts8_asc(unsigned char *vram, int x_size, int x, int y, char color, unsigned char *s)
{
    extern char hankaku[4096];
    for (; *s != 0x00; s++)
    {
        putfont8(vram, x_size, x, y, color, hankaku + *s * 16);
        x += 8;
    }
    return;
}
