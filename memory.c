#include "bootpack.h"

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

//4KB単位(切り上げ)のメモリ確保
unsigned int memorymanager_allocate_4k(struct MEMORYMANAGER *memoryManager, unsigned int size)
{
    unsigned int address;
    size = (size + 0xfff) & 0xfffff000; //下3桁切り上げ
    address = memorymanager_allocate(memoryManager, size);
    return address;
}

//4KB単位(切り上げ)のメモリ解放
int memorymanager_free_4k(struct MEMORYMANAGER *memoryManager, unsigned int address, unsigned int size)
{
    int i;
    size = (size + 0xfff) & 0xfffff000; //下3桁切り上げ
    i = memorymanager_free(memoryManager, address, size);
    return i;
}
