#include <stdio.h>
#include "bootpack.h"

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

void HariMain(void)
{
    struct BOOTINFO *bootInfo = (struct BOOTINFO *)ADRESS_BOOTINFO; //boot infoの開始アドレス
    char s[40], mouseCursor[256], keyBuffer[32], mouseBuffer[128];
    int mouseX, mouseY, i;
    unsigned int memoryTotal;
    struct MOUSE_DECODE mouseDecode;
    struct MEMORYMANAGER *memoryManager = (struct MEMORYMANAGER *)MEMMAN_ADDR;

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
    init_screen(bootInfo->vram, bootInfo->screenX, bootInfo->screenY);
    mouseX = (bootInfo->screenX - 16) / 2; //画面中央に配置
    mouseY = (bootInfo->screenY - 28 - 16) / 2;
    init_mouse_cursor8(mouseCursor, COLOR8_008484);
    putblock8_8(bootInfo->vram, bootInfo->screenX, 16, 16, mouseX, mouseY, mouseCursor, 16);
    sprintf(s, "(%d, %d)", mouseX, mouseY);
    putfonts8_asc(bootInfo->vram, bootInfo->screenX, 0, 0, COLOR8_FFFFFF, s);

    sprintf(s, "memory %dMB    free : %dKB", memoryTotal / (1024 * 1024), memorymanager_total(memoryManager) / 1024);
    putfonts8_asc(bootInfo->vram, bootInfo->screenX, 0, 32, COLOR8_FFFFFF, s);

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

                    //カーソルの移動
                    boxfill8(bootInfo->vram, bootInfo->screenX, COLOR8_008484, mouseX, mouseY, mouseX + 15, mouseY + 15); //前のカーソルの削除
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

                    sprintf(s, "(%d, %d)", mouseX, mouseY);
                    boxfill8(bootInfo->vram, bootInfo->screenX, COLOR8_008484, 0, 0, 79, 15);                //マウスの座標表示を消す
                    putfonts8_asc(bootInfo->vram, bootInfo->screenX, 0, 0, COLOR8_FFFFFF, s);                //マウスの座標表示の描画
                    putblock8_8(bootInfo->vram, bootInfo->screenX, 16, 16, mouseX, mouseY, mouseCursor, 16); //カーソルの描画
                }
            }
        }
    }
}

#define EFLAGS_AC_BIT 0x00040000
#define CR0_CACHE_DISABLE 0x60000000

unsigned int memory_test(unsigned int start, unsigned int end)
{
    char flag486 = 0;
    unsigned int eflag, cr0, i;

    //386以前(キャッシュメモリ無し)か486(キャッシュメモリあり)以降なのかを確認
    eflag = io_load_eflags();
    eflag |= EFLAGS_AC_BIT; //AC-itを1に変更
    io_store_eflags(eflag);
    eflag = io_load_eflags();
    if ((eflag & EFLAGS_AC_BIT) != 0) //386ではAC=1に変更しても自動で元に戻ってしまう
    {
        flag486 = 1;
    }
    eflag &= ~EFLAGS_AC_BIT; //復元
    io_store_eflags(eflag);

    if (flag486 != 0)
    {
        cr0 = load_cr0();
        cr0 |= CR0_CACHE_DISABLE; //キャッシュ無効化
        store_cr0(cr0);
    }

    i = memory_test_sub(start, end);

    if (flag486 != 0)
    {
        cr0 = load_cr0();
        cr0 &= ~CR0_CACHE_DISABLE; //キャッシュ有効化
        store_cr0(cr0);
    }

    return i;
}

void memorymanager_init(struct MEMORYMANAGER *memorymanager)
{
    memorymanager->frees = 0;    //空き情報の個数
    memorymanager->maxfrees = 0; //状態監視用。freesの最大値
    memorymanager->lostSize = 0; //解放に失敗した合計サイズ
    memorymanager->losts = 0;    //解放に失敗した回数
    return;
}

unsigned int memorymanager_total(struct MEMORYMANAGER *memorymanager)
{
    unsigned int i, total = 0;
    for (i = 0; i < memorymanager->frees; i++)
    {
        total += memorymanager->freeInfo[i].size;
    }
    return total;
}

//メモリ確保
unsigned int memorymanager_allocate(struct MEMORYMANAGER *memorymanager, unsigned int size)
{
    unsigned int i, address;
    for (i = 0; i < memorymanager->frees; i++)
    {
        if (memorymanager->freeInfo[i].size >= size)
        {
            //十分な広さの空きメモリを発見
            address = memorymanager->freeInfo[i].address;
            memorymanager->freeInfo[i].address += size;
            memorymanager->freeInfo[i].size -= size;
            if (memorymanager->freeInfo[i].size == 0)
            {
                //freeInfo[i]が空になったので前に詰める
                memorymanager->frees--;
                for (; i < memorymanager->frees; i++)
                {
                    memorymanager->freeInfo[i] = memorymanager->freeInfo[i + 1];
                }
            }
            return address;
        }
    }

    return 0; //空きがない
}

//メモリ解放
int memorymanager_free(struct MEMORYMANAGER *memorymanager, unsigned int address, unsigned int size)
{
    int i, j;
    //まとめやすさを考えるとfree[]がアドレス順のほうが都合がいいので挿入時にソート
    for (i = 0; i < memorymanager->frees; i++)
    {
        if (memorymanager->freeInfo[i].address > address)
        {
            break;
        }
    }
    //この時点でfree[i-1].address < address < freeInfo[i].addressのはず
    if (i > 0) //前に空き領域が存在するとき
    {
        if (memorymanager->freeInfo[i - 1].address + memorymanager->freeInfo[i - 1].size == address)
        {
            memorymanager->freeInfo[i - 1].size += size; //前の空き領域の終端と開放する領域の先頭が隣接しているので統合
            if (i < memorymanager->frees)                //後ろに空き領域が存在するとき
            {
                if (address + size == memorymanager->freeInfo[i].address)
                {
                    //開放する領域の終端と後ろの領域の先頭が隣接しているので統合する
                    memorymanager->freeInfo[i - 1].size += memorymanager->freeInfo[i].size;

                    //memorymanager->freeInfo[i]を削除して詰める
                    memorymanager->frees--;
                    for (; i < memorymanager->frees; i++)
                    {
                        memorymanager->freeInfo[i] = memorymanager->freeInfo[i + 1];
                    }
                }
            }

            return 0; //成功
        }
    }

    //前の空き領域と統合できなかったとき
    if (i < memorymanager->frees) //後ろに空き領域が存在するとき
    {
        if (address + size == memorymanager->freeInfo[i].address) //開放する領域の終端と後ろの領域の先頭が隣接しているので統合する
        {
            memorymanager->freeInfo[i].address = address;
            memorymanager->freeInfo[i].size += size;
            return 0; //成功
        }
    }

    //前にも後ろにまとめられないとき
    if (memorymanager->frees < MEMMAN_FREES)
    {
        //i以降の要素をすべてずらして隙間を作る
        for (j = memorymanager->frees; j > i; j--)
        {
            memorymanager->freeInfo[j] = memorymanager->freeInfo[j - 1];
        }
        memorymanager->frees++;
        if (memorymanager->maxfrees > memorymanager->frees)
        {
            memorymanager->maxfrees = memorymanager->frees; //最大値の更新
        }
        memorymanager->freeInfo[i].address = address;
        memorymanager->freeInfo[i].size = size;
        return 0; //成功
    }

    memorymanager->losts++;
    memorymanager->lostSize += size;
    return -1; //失敗
}
