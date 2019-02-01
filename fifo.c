//FIFOクラスのメンバ関数定義にあたる気がする

#include "bootpack.h"

#define FLAGS_OVERRUN 0x0001

//FIFOバッファの初期化
void fifo8_init(struct FIFO8 *fifo, int size, unsigned char *buffer)
{
    fifo->size = size;
    fifo->buffer = buffer;
    fifo->free = size;
    fifo->flags = 0;
    fifo->nextRead = 0;
    fifo->nextWrite = 0;
}

//FIFOバッファにデータを追加
int fifo8_put(struct FIFO8 *fifo, unsigned char data)
{
    //空きが無ければ-1を返す
    if(fifo->free == 0)
    {
        fifo->flags |= FLAGS_OVERRUN;
        return -1;
    }

    fifo->buffer[fifo->nextWrite] = data;
    fifo->nextWrite++;
    if(fifo->nextWrite == fifo->size)
    {
        fifo->nextWrite = 0;
    }
    fifo->free--;
    return 0;
}

//FIFOバッファから値を一つ取得する
int fifo8_get(struct FIFO8 *fifo)
{
    int data;

    //バッファが空なら-1を返す
    if(fifo->free == fifo->size)
    {
        return -1;
    }

    data = fifo->buffer[fifo->nextRead];
    fifo->nextRead++;
    if(fifo->nextRead == fifo->size)
    {
        fifo->nextRead = 0;
    }
    fifo->free++;
    return data;
}

//どれくらいデータがたまっているかを返す
int fifo8_status(struct FIFO8 *fifo)
{
    return fifo->size - fifo->free;
}
