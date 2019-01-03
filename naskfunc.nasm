; hello-os
; TAB = 4

[FORMAT "WCOFF"]                    ; オブジェクトファイルを作るモード
[INSTRSET "i486p"]	    			; 486の命令まで使いたいという記述
[BITS 32]                           ; 32ビットモードの機械語を作らせる


; オブジェクトファイルのための情報
[FILE "naskfunc.nasm"]              ; ソースファイル名の情報
        GLOBAL  _io_hlt,_write_mem8 ; このプログラムに含まれる関数名

; 以下は実際の関数

[SECTION .text]                     ; オブジェクトファイルではこれを書いてからプログラムを書く

_io_hlt                             ; void io_hlt(void);
    HLT
    RET

_write_mem8:                        ; void write_mem8(int address, int data)
    MOV     ECX, [ESP+4]            ; [ESP+4]にaddressが入っているのでECXに読み込む
    MOV     AL, [ESP+8]             ; [ESP+8]にdataが入っているのでALに読み込む
    MOV     [ECX], AL
    RET