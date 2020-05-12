#include "bootpack.h"

#define SHEET_USE 1

void sheet_refreshsub(struct SHEETCONTROL *control, int vramX0, int vramY0, int vramX1, int vramY1, int h0);

struct SHEETCONTROL *sheetcontrol_init(struct MEMORYMANAGER *memoryManager, unsigned char *vram, int xSize, int ySize)
{
    struct SHEETCONTROL *control;
    int i;
    control = (struct SHEETCONTROL *)memorymanager_allocate_4k(memoryManager, sizeof(struct SHEETCONTROL));
    if (control == 0)
    {
        goto error;
    }
    control->vram = vram;
    control->xSize = xSize;
    control->ySize = ySize;
    control->top = -1;
    for (i = 0; i < MAX_SHEETS; i++)
    {
        control->sheets0[i].flags = 0;         //未使用マーク
        control->sheets0[i].control = control; //所属を記録
    }

error:
    return control;
}

struct SHEET *sheet_allocate(struct SHEETCONTROL *control)
{
    struct SHEET *sheet;
    int i;
    for (i = 0; i < MAX_SHEETS; i++)
    {
        if (control->sheets0[i].flags == 0)
        {
            sheet = &control->sheets0[i];
            sheet->flags = SHEET_USE; //使用中フラグを立てる
            sheet->height = -1;       //非表示に設定
            return sheet;
        }
    }

    return 0; //すべてのsheetが使用中だった
}

void sheet_set_buffer(struct SHEET *sheet, unsigned char *buffer, int xSize, int ySize, int colorInvisible)
{
    sheet->buffer = buffer;
    sheet->boxXSize = xSize;
    sheet->boxYSize = ySize;
    sheet->colorInvisible = colorInvisible;
    return;
}

void sheet_updown(struct SHEET *sheet, int height)
{
    int h, old = sheet->height; //設定前の高さを記憶する
    struct SHEETCONTROL *control = sheet->control;

    //指定が高すぎたり低すぎたりしたら修正する
    if (height > control->top + 1)
    {
        height = control->top + 1;
    }
    if (height < -1)
    {
        height = -1;
    }

    sheet->height = height; //高さを設定

    //以下はsheets[]のソート
    if (old > height) //以前よりも低くなるとき
    {
        if (height > 0)
        {
            //間のものを引き上げる
            for (h = old; h > height; h--)
            {
                control->sheets[h] = control->sheets[h - 1];
                control->sheets[h]->height = h;
            }
            control->sheets[height] = sheet;
            sheet_refreshsub(control, sheet->vramX0, sheet->vramY0, sheet->vramX0 + sheet->boxXSize, sheet->vramY0 + sheet->boxYSize, height + 1);
        }
        else //非表示になるとき
        {
            if (control->top > old) //一番上でないとき
            {
                //上になっているものを下ろす
                for (h = old; h < control->top; h++)
                {
                    control->sheets[h] = control->sheets[h + 1];
                    control->sheets[h]->height = h;
                }
            }
            control->top--; //表示中の下敷きが一つへるので、一番上の高さが減る
            sheet_refreshsub(control, sheet->vramX0, sheet->vramY0, sheet->vramX0 + sheet->boxXSize, sheet->vramY0 + sheet->boxYSize, 0);
        }
    }
    else if (old < height) //以前よりも高くなるとき
    {
        if (old >= 0) //もともと表示されていたとき
        {
            //間のものを押し下げる
            for (h = old; h < control->top; h++)
            {
                control->sheets[h] = control->sheets[h + 1];
                control->sheets[h]->height = h;
            }
            control->sheets[height] = sheet;
        }
        else //非表示から表示状態になるとき
        {
            //上になるものを持ち上げる
            for (h = control->top; h >= height; h--)
            {
                control->sheets[h + 1] = control->sheets[h];
                control->sheets[h + 1]->height = h + 1;
            }
            control->sheets[height] = sheet;
            control->top++; //表示中の下敷きが一つ増えるので、一番上の高さが増える
        }
        sheet_refreshsub(control, sheet->vramX0, sheet->vramY0, sheet->vramX0 + sheet->boxXSize, sheet->vramY0 + sheet->boxYSize, height);
    }
    return;
}

void sheet_refresh(struct SHEET *sheet, int boxX0, int boxY0, int boxX1, int boxY1)
{
    if (sheet->height >= 0) //表示中の場合
    {
        //下敷きに沿って画面を書き直す
        sheet_refreshsub(sheet->control, sheet->vramX0 + boxX0, sheet->vramY0 + boxY0, sheet->vramX0 + boxX1, sheet->vramY0 + boxY1, sheet->height);
    }
    return;
}

void sheet_refreshsub(struct SHEETCONTROL *control, int vramX0, int vramY0, int vramX1, int vramY1, int h0)
{
    int h, boxX, boxY, vramX, vramY, boxX0, boxY0, boxX1, boxY1; //左から処理中の高さ、sheet上のX座標、sheet上のY座標、vram上のX座標、vram上のY座標
    unsigned char *buffer, c, *vram = control->vram;
    struct SHEET *sheet;

    //refreshの範囲が画面外に出ていれば補正
    if (vramX0 < 0)
        vramX0 = 0;
    if (vramY0 < 0)
        vramY0 = 0;
    if (vramX1 > control->xSize)
        vramX1 = control->xSize;
    if (vramY1 > control->ySize)
        vramY1 = control->ySize;

    for (h = h0; h <= control->top; h++)
    {
        sheet = control->sheets[h];
        buffer = sheet->buffer;

        //vramX0~vramY1を使って、boxX0~boxY1を計算
        boxX0 = vramX0 - sheet->vramX0;
        boxY0 = vramY0 - sheet->vramY0;
        boxX1 = vramX1 - sheet->vramX0;
        boxY1 = vramY1 - sheet->vramY0;
        if (boxX0 < 0)
            boxX0 = 0;
        if (boxY0 < 0)
            boxY0 = 0;
        if (boxX1 > sheet->boxXSize)
            boxX1 = sheet->boxXSize;
        if (boxY1 > sheet->boxYSize)
            boxY1 = sheet->boxYSize;
        for (boxY = boxY0; boxY < boxY1; boxY++)
        {
            vramY = sheet->vramY0 + boxY;
            for (boxX = boxX0; boxX < boxX1; boxX++)
            {
                vramX = sheet->vramX0 + boxX;
                c = buffer[boxY * sheet->boxXSize + boxX];
                if (c != sheet->colorInvisible)
                {
                    vram[vramY * control->xSize + vramX] = c;
                }
            }
        }
    }
    return;
}

void sheet_slide(struct SHEET *sheet, int vramX0, int vramY0)
{
    int oldVramX0 = sheet->vramX0, oldVramY0 = sheet->vramY0;
    sheet->vramX0 = vramX0;
    sheet->vramY0 = vramY0;
    if (sheet->height >= 0) //表示中の場合
    {
        sheet_refreshsub(sheet->control, oldVramX0, oldVramY0, oldVramX0 + sheet->boxXSize, oldVramY0 + sheet->boxYSize, 0); //vramの更新
        sheet_refreshsub(sheet->control, vramX0, vramY0, vramX0 + sheet->boxXSize, vramY0 + sheet->boxYSize, sheet->height);
    }
    return;
}

void sheet_free(struct SHEET *sheet)
{
    if (sheet->height >= 0) //表示中の場合
    {
        sheet_updown(sheet, -1);
    }
    sheet->flags = 0; //未使用フラグを建てる
    return;
}
