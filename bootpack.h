//asmhead.nasm
struct BOOTINFO
{
    char cyls, leds, vmode, reserve;
    short screenX, screenY;
    char *vram;
};
#define ADRESS_BOOTINFO 0x00000ff0

//naskfunc.nasm
void io_hlt(void);
void io_cli(void);
void io_sti(void);
void io_stihlt(void);
int io_in8(int port);
void io_out8(int port, int data);
int io_load_eflags(void);
void io_store_eflags(int eflags);
void load_gdtr(int limit, int adress);
void load_idtr(int limit, int adress);
int load_cr0(void);
void store_cr0(int cr0);
void load_tr(int tr);
void asm_inthandler20(void);
void asm_inthandler21(void);
void asm_inthandler27(void);
void asm_inthandler2c(void);
unsigned int memory_test_sub(unsigned int start, unsigned int end);
void taskswitch4(void);
void taskswitch3(void);

//fifo.c
struct FIFO8
{
    unsigned char *buffer;
    int nextRead, nextWrite, size, free, flags;
};
void fifo8_init(struct FIFO8 *fifo, int size, unsigned char *buffer);
int fifo8_put(struct FIFO8 *fifo, unsigned char data);
int fifo8_get(struct FIFO8 *fifo);
int fifo8_status(struct FIFO8 *fifo);
struct FIFO32
{
    int *buffer;
    int nextRead, nextWrite, size, free, flags;
};
void fifo32_init(struct FIFO32 *fifo, int size, int *buffer);
int fifo32_put(struct FIFO32 *fifo, int data);
int fifo32_get(struct FIFO32 *fifo);
int fifo32_status(struct FIFO32 *fifo);

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
#define ADRESS_IDT 0x0026f800
#define LIMIT_IDT 0x000007ff
#define ADRESS_GDT 0x00270000
#define LIMIT_GDT 0x0000ffff
#define ADRESS_BOTPACK 0x00280000
#define LIMIT_BOTPACK 0x0007ffff
#define AR_DATA32_RW 0x4092
#define AR_CODE32_ER 0x409a
#define AR_TSS32 0x0089
#define AR_INTGATE32 0x008e

//int.c
void init_pic(void);
void inthandler27(int *esp);
#define PIC0_ICW1 0x0020 //PIC0
#define PIC0_OCW2 0x0020
#define PIC0_IMR 0x0021
#define PIC0_ICW2 0x0021
#define PIC0_ICW3 0x0021
#define PIC0_ICW4 0x0021
#define PIC1_ICW1 0x00a0 //PIC1
#define PIC1_OCW2 0x00a0
#define PIC1_IMR 0x00a1
#define PIC1_ICW2 0x00a1
#define PIC1_ICW3 0x00a1
#define PIC1_ICW4 0x00a1

//keyboard.c
void inthandler2c(int *esp);
void wait_KBC_sendready(void);
void init_keyboard(struct FIFO32 *fifo, int data0);
#define PORT_KEYDAT 0x0060
#define PORT_KEYCMD 0x0064

//mouse.c
struct MOUSE_DECODE
{
    unsigned char buffer[3], phase;
    int x, y, button;
};
void inthandler21(int *esp);
void enable_mouse(struct FIFO32 *fifo, int data0, struct MOUSE_DECODE *mouseDecode);
int mouse_decode(struct MOUSE_DECODE *mouseDecode, unsigned char data);

//memory.c
#define MEMMAN_FREES 4090      //約32KB
#define MEMMAN_ADDR 0x003c0000 //メモリマネージャの先頭アドレス。ここから32KBがメモリマネージャの構造体
struct FREEINFO
{
    unsigned int address, size;
};
struct MEMORYMANAGER
{
    int frees, maxfrees, lostSize, losts;
    struct FREEINFO freeInfo[MEMMAN_FREES];
};
unsigned int memory_test(unsigned int start, unsigned int end);
void memorymanager_init(struct MEMORYMANAGER *memorymanager);
unsigned int memorymanager_total(struct MEMORYMANAGER *memorymanager);
unsigned int memorymanager_allocate(struct MEMORYMANAGER *memorymanager, unsigned int size);
int memorymanager_free(struct MEMORYMANAGER *memorymanager, unsigned int address, unsigned int size);
unsigned int memorymanager_allocate_4k(struct MEMORYMANAGER *memorymanager, unsigned int size);
int memorymanager_free_4k(struct MEMORYMANAGER *memorymanager, unsigned int address, unsigned int size);

//sheet.c
#define MAX_SHEETS 156
//SHEETとSHEETCONTROLで循環参照している
struct SHEET
{
    unsigned char *buffer;
    int boxXSize, boxYSize, vramX0, vramY0, colorInvisible, height, flags;
    struct SHEETCONTROL *control;
};
struct SHEETCONTROL
{
    unsigned char *vram, *map;
    int xSize, ySize, top; //vramのサイズ、最上位のシート
    struct SHEET *sheets[MAX_SHEETS];
    struct SHEET sheets0[MAX_SHEETS];
};
struct SHEETCONTROL *sheetcontrol_init(struct MEMORYMANAGER *memoryManager, unsigned char *vram, int xSize, int ySize);
struct SHEET *sheet_allocate(struct SHEETCONTROL *control);
void sheet_set_buffer(struct SHEET *sheet, unsigned char *buffer, int xSize, int ySize, int colorInvisible);
void sheet_updown(struct SHEET *sheet, int height);
void sheet_refresh(struct SHEET *sheet, int box0, int boxY0, int boxX1, int boxY1);
void sheet_slide(struct SHEET *sheet, int vramX0, int vramY0);
void sheet_free(struct SHEET *sheet);

//timer.c
#define MAX_TIMER 500
struct TIMER
{
    struct TIMER *next;
    unsigned int timeout, flags;
    struct FIFO32 *fifo;
    int data;
};
struct TIMERCONTROL
{
    unsigned int count, next_time;
    struct TIMER *t0;
    struct TIMER timers0[MAX_TIMER];
};
extern struct TIMERCONTROL timerControl;
void init_pit(void);
struct TIMER *timer_allocate(void);
void timer_free(struct TIMER *timer);
void timer_init(struct TIMER *timer, struct FIFO32 *fifo, int data);
void timer_set_time(struct TIMER *timer, unsigned int timeout);
void inthandler20(int *esp);

//TaskStatusSegment32
struct TaskStatusSegment32
{
    int backlink, esp0, ss0, esp1, ss1, esp2, ss2, cr3;
    int eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi;
    int es, cs, ss, ds, fs, gs;
    int ldtr, iomap;
};
