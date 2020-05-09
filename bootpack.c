#include <stdio.h>
#include "bootpack.h"

struct MOUSE_DECODE
{
    unsigned char buffer[3], phase;
    int x, y, button;
};

extern struct FIFO8 keyFifo, mouseFifo;
void init_keyboard(void);
void enable_mouse(struct MOUSE_DECODE *mouseDecode);
int mouse_decode(struct MOUSE_DECODE *mouseDecode, unsigned char data);

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
                }
            }
        }
    }
}

#define PORT_KEYDAT 0x0060
#define PORT_KEYSTA 0x0064
#define PORT_KEYCMD 0x0064
#define KEYSTA_SEND_NOTREADY 0x02
#define KEYCMD_WRITE_MODE 0x60
#define KBC_MODE 0x47

//キーボードコントローラがデータを送信可能になるまで待機
void wait_KBC_sendready(void)
{
    for (;;)
    {
        if ((io_in8(PORT_KEYSTA) & KEYSTA_SEND_NOTREADY) == 0)
        {
            break;
        }
    }

    return;
}

//キーボードコントローラの初期化
void init_keyboard(void)
{
    wait_KBC_sendready();
    io_out8(PORT_KEYCMD, KEYCMD_WRITE_MODE);
    wait_KBC_sendready();
    io_out8(PORT_KEYDAT, KBC_MODE);
    return;
}

#define KEYCMD_SENDTO_MOUSE 0xd4
#define MOUSECMD_ENABLE 0xf4

//マウスの有効化
void enable_mouse(struct MOUSE_DECODE *mouseDecode)
{
    wait_KBC_sendready();
    io_out8(PORT_KEYCMD, KEYCMD_SENDTO_MOUSE);
    wait_KBC_sendready();
    io_out8(PORT_KEYDAT, MOUSECMD_ENABLE);
    //成功するとACK(0xfa)がマウスから送信される

    mouseDecode->phase = 0; //マウスの初期化(ACK,0xfa)を待機する状態へ移行

    return;
}

int mouse_decode(struct MOUSE_DECODE *mouseDecode, unsigned char data)
{
    //マウスの情報は3バイトずつ送られてくるため、未初期化と1~3バイト目の4つの状態を管理する
    if (mouseDecode->phase == 0)
    {
        //マウスの初期化(0xfa)を待っている状態だったとき
        if (data == 0xfa)
        {
            mouseDecode->phase = 1;
        }
        return 0;
    }
    else if (mouseDecode->phase == 1)
    {
        //マウスの1バイト目を待っている状態だったとき
        if ((data & 0xc8) == 0x08)
        {
            //データが正しい1バイト目だったとき
            //移動に反応する桁が0~3の範囲かつクリックに反応する桁が8~Fの範囲あることを確かめる
            //このチェックはマウスとの通信が安定しない場合でもバイト列の先頭だと確認できるように入れている

            mouseDecode->buffer[0] = data;
            mouseDecode->phase = 2;
        }
        return 0;
    }
    else if (mouseDecode->phase == 2)
    {
        //マウスの2バイト目を待っている状態だったとき
        mouseDecode->buffer[1] = data;
        mouseDecode->phase = 3;
        return 0;
    }
    else if (mouseDecode->phase == 3)
    {
        //マウスの3バイト目を待っている状態だったとき
        mouseDecode->buffer[2] = data;
        mouseDecode->phase = 1;
        mouseDecode->button = mouseDecode->buffer[0] & 0x07;
        mouseDecode->x = mouseDecode->buffer[1];
        mouseDecode->y = mouseDecode->buffer[2];
        if ((mouseDecode->buffer[0] & 0x10) != 0)
        {
            mouseDecode->x |= 0xffffff00;
        }
        if ((mouseDecode->buffer[0] & 0x20) != 0)
        {
            mouseDecode->y |= 0xffffff00;
        }

        mouseDecode->y *= -1; //マウスではy方向の符号と画面の向きが反対

        return 1;
    }

    return -1; //到達したらおかしい
}
