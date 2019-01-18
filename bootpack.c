#include <stdio.h>

void io_hlt(void);
void io_cli(void);
void io_out8(int port, int data);
int io_load_eflags(void);
void io_store_eflags(int eflags);
void load_gdtr(int limit, int adress);
void load_idtr(int limit, int adress);

void init_palette(void);
void set_palette(int start, int end, unsigned char *rgb);
void boxfill8(unsigned char *vram, int x_size, unsigned char color, int x0, int y0, int x1, int y1);
void init_screen(char *vram, int x_size, int y_size);
void putfonts8_asc(unsigned char *vram, int x_size, int x, int y, char color, unsigned char *s);
void putfont8(unsigned char *vram, int x_size, int x, int y, char c, char *font);
void init_mouse_cursor8(char *mouse_buffer, char background_color);
void putblock8_8(char *vram, int vxsize, int pxsize, int pysize, int px0, int py0, char *buffer, int bxsize);

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

struct SEGMENT_DESCRIPTOR
{
    short limit_low, base_low;
    char base_mid, access_right;
    char limit_high, base_high;
};

struct GATE_DESCRIPTOR
{
    short offset_low, selector;
    char dw_count, access_right;
    short offset_high;
};

void init_gdtidt(void);
void set_segment_descriptor(struct SEGMENT_DESCRIPTOR *segmentDescriptor, unsigned int limit, int base, int access_right);
void set_gate_descriptor(struct GATE_DESCRIPTOR *gateDescriptor, int offset, int selector, int access_right);

void HariMain(void)
{
    struct BOOTINFO *boot_info = (struct BOOTINFO *)0xff0; //boot infoの開始アドレス
    char s[40], mouse_cursor[256];
    int mouse_x, mouse_y;

    init_palette();
    init_screen(boot_info->vram, boot_info->screenX, boot_info->screenY);

    mouse_x = (boot_info->screenX - 16) / 2; //画面中央に配置
    mouse_y = (boot_info->screenY - 28 - 16) / 2;
    init_mouse_cursor8(mouse_cursor, Color8_008484);
    putblock8_8(boot_info->vram, boot_info->screenX, 16, 16, mouse_x, mouse_y, mouse_cursor, 16);

    sprintf(s, "(%d, %d)", mouse_x, mouse_y);
    putfonts8_asc(boot_info->vram, boot_info->screenX, 0, 0, Color8_FFFFFF, s);

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

void init_mouse_cursor8(char *mouse_buffer, char background_color)
{
    //カーソルの16x16の画像(？)データ
    static char cursor[16][16] =
        {
            "**************..",
            "*OOOOOOOOOOO*...",
            "*OOOOOOOOOO*....",
            "*OOOOOOOOO*.....",
            "*OOOOOOOO*......",
            "*OOOOOOO*.......",
            "*OOOOOOO*.......",
            "*OOOOOOOO*......",
            "*OOOO**OOO*.....",
            "*OOO*..*OOO*....",
            "*OO*....*OOO*...",
            "*O*......*OOO*..",
            "**........*OOO*.",
            "*..........*OOO*",
            "............*OO*",
            ".............***"};
    int x, y;

    for (y = 0; y < 16; y++)
    {
        for (x = 0; x < 16; x++)
        {
            if (cursor[y][x] == '*')
                mouse_buffer[y * 16 + x] = Color8_000000;
            if (cursor[y][x] == 'O')
                mouse_buffer[y * 16 + x] = Color8_FFFFFF;
            if (cursor[y][x] == '.')
                mouse_buffer[y * 16 + x] = background_color;
        }
    }
    return;
}

void putblock8_8(char *vram, int vxsize, int pxsize, int pysize, int px0, int py0, char *buffer, int bxsize)
{
    int x, y;
    for (y = 0; y < pysize; y++)
    {
        for (x = 0; x < pxsize; x++)
            vram[(py0 + y) * vxsize + (px0 + x)] = buffer[y * bxsize + x];
    }
    return;
}

void init_gdtidt(void)
{
    struct SEGMENT_DESCRIPTOR *gdt = (struct SEGMENT_DESCRIPTOR *)0x00270000;
    struct GATE_DESCRIPTOR *idt = (struct GATE_DESCRIPTOR *)0x0026f800;
    int i = 0;

    //GDTの初期化
    for (i = 0; i < 8192; i++)
        set_segment_descriptor(gdt + i, 0, 0, 0);
    set_segment_descriptor(gdt + 1, 0xffffffff, 0x00000000, 0x4092);
    set_segment_descriptor(gdt + 2, 0x0007ffff, 0x00280000, 0x409a);
    load_gdtr(0xffff, 0x0026f800);

    //IDTの初期化
    for (i = 0; i < 256; i++)
        set_gate_descriptor(idt + i, 0, 0, 0);
    load_idtr(0x7ff, 0x0026f800);

    return;
}

void set_segment_descriptor(struct SEGMENT_DESCRIPTOR *segmentDescriptor, unsigned int limit, int base, int access_right)
{
    if (limit > 0xfffff)
    {
        access_right |= 0x8000; //G_bit = 1
        limit /= 0x1000;
    }

    segmentDescriptor->limit_low = limit & 0xffff;
    segmentDescriptor->base_low = base & 0xffff;
    segmentDescriptor->base_mid = (base >> 16) & 0xff;
    segmentDescriptor->access_right = access_right & 0xff;
    segmentDescriptor->limit_high = ((limit >> 16) & 0x0f) | ((access_right >> 8) & 0xf0);
    segmentDescriptor->base_high = (base >> 24) & 0xff;
    return;
}

void set_gate_descriptor(struct GATE_DESCRIPTOR *gateDescriptor, int offset, int selector, int access_right)
{
    gateDescriptor->offset_low = offset & 0xffff;
    gateDescriptor->selector = selector;
    gateDescriptor->dw_count = (access_right >> 8) & 0xff;
    gateDescriptor->access_right = access_right & 0xff;
    gateDescriptor->offset_high = (offset >> 16) & 0xffff;
    return;
}
