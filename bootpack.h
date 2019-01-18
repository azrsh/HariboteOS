//asmhead.nasm
struct BOOTINFO
{
    char cyls, leds, vmode, reserve;
    short screenX, screenY;
    char *vram;
};

//naskfunc.nasm
void io_hlt(void);
void io_cli(void);
void io_out8(int port, int data);
int io_load_eflags(void);
void io_store_eflags(int eflags);
void load_gdtr(int limit, int adress);
void load_idtr(int limit, int adress);

//graphic.c
void init_palette(void);
void set_palette(int start, int end, unsigned char *rgb);
void boxfill8(unsigned char *vram, int x_size, unsigned char color, int x0, int y0, int x1, int y1);
void init_screen(char *vram, int x_size, int y_size);
void putfonts8_asc(unsigned char *vram, int x_size, int x, int y, char color, unsigned char *s);
void putfont8(unsigned char *vram, int x_size, int x, int y, char c, char *font);
void init_mouse_cursor8(char *mouse_buffer, char background_color);
void putblock8_8(char *vram, int vxsize, int pxsize, int pysize, int px0, int py0, char *buffer, int bxsize);
//constに変更したい
#define COLOR8_000000 0
#define COLOR8_FF0000 1
#define COLOR8_00FF00 2
#define COLOR8_FFFF00 3
#define COLOR8_0000FF 4
#define COLOR8_FF00FF 5
#define COLOR8_00FFFF 6
#define COLOR8_FFFFFF 7
#define COLOR8_C6C6C6 8
#define COLOR8_840000 9
#define COLOR8_008400 10
#define COLOR8_848400 11
#define COLOR8_000084 12
#define COLOR8_840084 13
#define COLOR8_008484 14
#define COLOR8_848484 15

//dsctbl.c
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
