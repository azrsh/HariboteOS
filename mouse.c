#include "bootpack.h"

struct FIFO32 *mouseFifo;
int mouseData0;

// PS/2マウスからの割り込み
void inthandler2c(int *esp) {
  int data;
  io_out8(
      PIC1_OCW2,
      0x64); // IRQ-12受付完了をPIC1(スレーブ)に通知(スレーブはIRQ-08~IRQ15を担当し、IRQ-12はスレーブの4番=0x64に接続されている)
  io_out8(
      PIC0_OCW2,
      0x62); // IRQ-02受付完了をPIC0(マスター)に通知(スレーブはIRQ-02はマスタの2番=0x62に接続されている)
  data = io_in8(PORT_KEYDAT);
  fifo32_put(mouseFifo, data + mouseData0);
  return;
}

#define KEYCMD_SENDTO_MOUSE 0xd4
#define MOUSECMD_ENABLE 0xf4

//マウスの有効化
void enable_mouse(struct FIFO32 *fifo, int data0,
                  struct MOUSE_DECODE *mouseDecode) {
  mouseFifo = fifo;
  mouseData0 = data0;

  wait_KBC_sendready();
  io_out8(PORT_KEYCMD, KEYCMD_SENDTO_MOUSE);
  wait_KBC_sendready();
  io_out8(PORT_KEYDAT, MOUSECMD_ENABLE);
  //成功するとACK(0xfa)がマウスから送信される

  mouseDecode->phase = 0; //マウスの初期化(ACK,0xfa)を待機する状態へ移行

  return;
}

int mouse_decode(struct MOUSE_DECODE *mouseDecode, unsigned char data) {
  //マウスの情報は3バイトずつ送られてくるため、未初期化と1~3バイト目の4つの状態を管理する
  if (mouseDecode->phase == 0) {
    //マウスの初期化(0xfa)を待っている状態だったとき
    if (data == 0xfa) {
      mouseDecode->phase = 1;
    }
    return 0;
  } else if (mouseDecode->phase == 1) {
    //マウスの1バイト目を待っている状態だったとき
    if ((data & 0xc8) == 0x08) {
      //データが正しい1バイト目だったとき
      //移動に反応する桁が0~3の範囲かつクリックに反応する桁が8~Fの範囲あることを確かめる
      //このチェックはマウスとの通信が安定しない場合でもバイト列の先頭だと確認できるように入れている

      mouseDecode->buffer[0] = data;
      mouseDecode->phase = 2;
    }
    return 0;
  } else if (mouseDecode->phase == 2) {
    //マウスの2バイト目を待っている状態だったとき
    mouseDecode->buffer[1] = data;
    mouseDecode->phase = 3;
    return 0;
  } else if (mouseDecode->phase == 3) {
    //マウスの3バイト目を待っている状態だったとき
    mouseDecode->buffer[2] = data;
    mouseDecode->phase = 1;
    mouseDecode->button = mouseDecode->buffer[0] & 0x07;
    mouseDecode->x = mouseDecode->buffer[1];
    mouseDecode->y = mouseDecode->buffer[2];
    if ((mouseDecode->buffer[0] & 0x10) != 0) {
      mouseDecode->x |= 0xffffff00;
    }
    if ((mouseDecode->buffer[0] & 0x20) != 0) {
      mouseDecode->y |= 0xffffff00;
    }

    mouseDecode->y *= -1; //マウスではy方向の符号と画面の向きが反対

    return 1;
  }

  return -1; //到達したらおかしい
}
