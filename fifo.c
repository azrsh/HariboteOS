//FIFOクラスのメンバ関数定義にあたる気がする

#include "bootpack.h"

#define FLAGS_OVERRUN 0x0001

//FIFOバッファの初期化
void fifo32_init(struct FIFO32 *fifo, int size, int *buffer, struct TASK *task)
{
    fifo->size = size;
    fifo->buffer = buffer;
    fifo->free = size;
    fifo->flags = 0;
    fifo->nextRead = 0;
    fifo->nextWrite = 0;
    fifo->task = task;
}

//FIFOバッファにデータを追加
int fifo32_put(struct FIFO32 *fifo, int data)
{
    //空きが無ければ-1を返す
    if (fifo->free == 0)
    {
        fifo->flags |= FLAGS_OVERRUN;
        return -1;
    }

    fifo->buffer[fifo->nextWrite] = data;
    fifo->nextWrite++;
    if (fifo->nextWrite == fifo->size)
    {
        fifo->nextWrite = 0;
    }
    fifo->free--;

    //taskの起動
    if (fifo->task != 0)
    {
        if (fifo->task->flags != 2) //タスクが動いていなければ
        {
            task_run(fifo->task, 0);
        }
    }

    return 0;
}

//FIFOバッファから値を一つ取得する
int fifo32_get(struct FIFO32 *fifo)
{
    int data;

    //バッファが空なら-1を返す
    if (fifo->free == fifo->size)
    {
        return -1;
    }

    data = fifo->buffer[fifo->nextRead];
    fifo->nextRead++;
    if (fifo->nextRead == fifo->size)
    {
        fifo->nextRead = 0;
    }
    fifo->free++;
    return data;
}

//どれくらいデータがたまっているかを返す
int fifo32_status(struct FIFO32 *fifo)
{
    return fifo->size - fifo->free;
}
