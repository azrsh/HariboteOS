; hello-os
; TAB = 4

[FORMAT "WCOFF"]                    ; オブジェクトファイルを作るモード
[INSTRSET "i486p"]	    			; 486の命令まで使いたいという記述
[BITS 32]                           ; 32ビットモードの機械語を作らせる


; オブジェクトファイルのための情報
[FILE "naskfunc.nasm"]              ; ソースファイル名の情報

        GLOBAL  _io_hlt, _io_cli, _io_sti, _io_stihlt ; このプログラムに含まれる関数名
        GLOBAL  _io_in8, _io_in16, _io_in32
        GLOBAL  _io_out8, _io_out16, _io_out32
        GLOBAL  _io_load_eflags, _io_store_eflags
        GLOBAL  _load_gdtr, _load_idtr
        GLOBAL  _asm_inthandler21, _asm_inthandler27, _asm_inthandler2c
        EXTERN  _inthandler21, _inthandler27, _inthandler2c

; 以下は実際の関数

[SECTION .text]                     ; オブジェクトファイルではこれを書いてからプログラムを書く

_io_hlt:                            ; void io_hlt(void);
    HLT
    RET

_io_cli:                            ; void io_cli(void);
    CLI
    RET

_io_sti:                            ; void io_sti(void);
    STI
    RET

_io_stihlt:                         ; void io_stihlt(void);
    STI
    HLT
    RET

_io_in8:                            ; int io_in8(int port);
    MOV     EDX, [ESP+4]            ; [ESP+4]にportが入っているのでEDXに読み込む
    MOV     EAX, 0                  ; EAX = 0
    IN      AL, DX                  ; DX番portからALに値を読み出す(AL:今回は8bitだから？,DX:portは16bit?)
    RET   

_io_in16:                           ; int io_in16(int port);
    MOV     EDX, [ESP+4]            ; [ESP+4]にportが入っているのでEDXに読み込む
    MOV     EAX, 0                  ; EAX = 0
    IN      AX, DX                  ; DX番portからAXに値を読み出す
    RET   

_io_in32:                           ; int io_in32(int port);
    MOV     EDX, [ESP+4]            ; [ESP+4]にportが入っているのでEDXに読み込む
    IN      EAX, DX                 ; DX番portからEAXに値を読み出す
    RET   

_io_out8:                           ; void io_out8(int port, int data);
    MOV     EDX, [ESP+4]            ; [ESP+4]にportが入っているのでEDXに読み込む
    MOV     AL, [ESP+8]             ; [ESP+8]にdataが入っているのでALに読み込む
    OUT     DX, AL                  ; DX番portからALの値を書き込む(AL:今回は8bitだから？,DX:portは16bit?)
    RET   

_io_out16:                          ; void io_out16(int port, int data);
    MOV     EDX, [ESP+4]            ; [ESP+4]にportが入っているのでEDXに読み込む
    MOV     EAX, [ESP+8]            ; [ESP+8]にdataが入っているのでEAXに読み込む(AXでは？)
    OUT     DX, AX                  ; DX番portからAXの値を書き込む
    RET   

_io_out32:                          ; void io_out32(int port, int data);
    MOV     EDX, [ESP+4]            ; [ESP+4]にportが入っているのでEDXに読み込む
    MOV     EAX, [ESP+8]            ; [ESP+8]にdataが入っているのでEAXに読み込む
    OUT     DX, EAX                 ; DX番portからEAXの値を書き込む
    RET   

_io_load_eflags:                    ; int io_load_eflags(void);
    PUSHFD                          ; push eflagsの意味
    POP     EAX
    RET

_io_store_eflags:                   ; void io_store_eflags(int data);
    MOV     EAX, [ESP+4]
    PUSH    EAX
    POPFD                           ; pop eflagsの意味
    RET

_load_gdtr                          ; void load_gdtr(int limit,int adress);
    MOV     AX, [ESP+4]             ; limit
    MOV     [ESP+6], AX
    LGDT    [ESP+6]
    RET

_load_idtr                          ; void load_idtr(int limit,int adress);
    MOV     AX, [ESP+4]             ; limit
    MOV     [ESP+6] ,AX
    LIDT    [ESP+6]
    RET

; レジスタの値をいったんFILO型Stackに保存し、割り込み処理の後CPUを元の状態に復帰させる
_asm_inthandler21
    PUSH    ES
    PUSH    DS
    PUSHAD
    MOV     EAX, ESP
    PUSH    EAX
    MOV     AX, SS
    MOV     DS, AX
    MOV     ES, AX
    CALL    _inthandler21
    POP     EAX
    POPAD
    POP     DS
    POP     ES
    IRETD

_asm_inthandler27
    PUSH    ES
    PUSH    DS
    PUSHAD
    MOV     EAX, ESP
    PUSH    EAX
    MOV     AX, SS
    MOV     DS, AX
    MOV     ES, AX
    CALL    _inthandler27
    POP     EAX
    POPAD
    POP     DS
    POP     ES
    IRETD

_asm_inthandler2c
    PUSH    ES
    PUSH    DS
    PUSHAD
    MOV     EAX, ESP
    PUSH    EAX
    MOV     AX, SS
    MOV     DS, AX
    MOV     ES, AX
    CALL    _inthandler2c
    POP     EAX
    POPAD
    POP     DS
    POP     ES
    IRETD
