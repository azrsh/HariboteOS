; haribote-os
; TAB=4

[INSTRSET "i486p"]

VBEMODE	EQU		0x105   ; 1024 x  768 x 8bitカラー
; （画面モード一覧）
;	0x100 :  640 x  400 x 8bitカラー
;	0x101 :  640 x  480 x 8bitカラー
;	0x103 :  800 x  600 x 8bitカラー
;	0x105 : 1024 x  768 x 8bitカラー
;	0x107 : 1280 x 1024 x 8bitカラー

; 定数定義みたいなもの
BOTPAK	EQU		0x00280000		    ; bootpackのロード先
DSKCAC	EQU		0x00100000		    ; ディスクキャッシュの場所
DSKCAC0	EQU		0x00008000		    ; ディスクキャッシュの場所（リアルモード）

; BOOT_INFO関連の定義
CYLS	EQU		0x0ff0  			; どこまで読み込む/んだか,ブートセクタが設定
LEDS    EQU     0x0ff1
VMODE   EQU     0x0ff2              ; 色の数に関する情報.何bitカラーか
SCRNX   EQU     0x0ff4              ; 解像度のx(screen x)
SCRNY   EQU     0x0ff6              ; 解像度のy(screen y)
VRAM    EQU     0x0ff8              ; グラフィックバッファの開始番地

	ORG	0xc200			    ; このプログラムがどこに読み込まれるのか,数値の根拠などはipl.nasmのOS本体のロードを参照


; VBEの存在を確認
        MOV     AX, 0x9000
        MOV     ES, AX
        MOV     DI, 0
        MOV     AX, 0x4f00
        INT     0x10
        CMP     AX, 0x004f
        JNE     screen320

; VBEのバージョンチェック
        MOV     AX, [ES:DI+4]
        CMP     AX, 0x0200
        JB      screen320               ; if(AX < 0x0200) goto screen320;

; 画面モード情報の取得
        MOV     CX, VBEMODE
        MOV     AX, 0x4f01
        INT     0x10
        CMP     AX, 0x004f
        JNE     screen320

; 画面モード情報の確認
        CMP     BYTE [ES:DI+0x19], 8    ; 色の数が8色か
        JNE     screen320
        CMP     BYTE [ES:DI+0x1b], 4    ; 色の指定方法はパレットモード(4)か
        JNE     screen320
        MOV     AX, [ES:DI+0x00]        ; モード属性のbit7が0ならあきらめる
        AND     AX, 0x0080
        JZ      screen320

; 画面モードの設定
        MOV     BX, VBEMODE+0x4000      ; VBE(VESA BIOS extension)グラフィックス,640x480x8bitカラー
        MOV     AX, 0x4f02              ; 画面モードの切り替え(BX、AXは高解像度の新しい画面モード用のレジスタ。VESA BIOS extension)
        INT     0x10                    ; ビデオBIOSの呼び出し
        MOV     BYTE [VMODE], 8         ; 画面モードの保存
        MOV     AX, [ES:DI+0x12]
        MOV     [SCRNX], AX             ;
        MOV     AX, [ES:DI+0x14]        ;
        MOV     [SCRNY], AX             ;
        MOV     EAX, [ES:DI+0x28]       ;
        MOV     [VRAM], EAX             ; BIOSで指定された情報
        JMP    keyboardstatus

screen320:
        MOV     AL, 0x13                ; VGAグラフィックス、320x200x8bitカラー
        MOV     AH, 0x00
        INT     0x10
        MOV     BYTE [VMODE], 8
        MOV     WORD [SCRNX], 320
        MOV     WORD [SCRNY], 200
        MOV     DWORD [VRAM], 0x000a0000

; キーボードの状態をBIOSに教えてもらう
keyboardstatus:
        MOV     AH, 0x02
        INT     0x16                ; keyboard BIOS
        MOV     [LEDS], AL

; PICがすべての割り込みを拒否するようにする
;   AT互換機の仕様では,PIC(割り込みコントローラ？)を初期化するなら
;   これをCLI前にやっておかないとたまにハングアップする
;   PICの初期化は後でやる

        MOV     AL, 0xff
        OUT     0x21, AL
        NOP                         ; OUT命令を連続させるとうまくいかない機種があるらしいから入れている（らしい）
        OUT     0xa1, AL

        CLI                         ; さらにCPUレベルでも割り込みを禁止

; CPUから1MB以上のメモリにアクセスできるように,A20GATEを設定

        CALL    waitkbdoubt
        MOV     AL, 0xd1
        OUT     0x64, AL
        CALL    waitkbdoubt
        MOV     AL, 0xdf            ; enable A20
        OUT     0x60, AL
        CALL    waitkbdoubt

; プロテクトモードへ移行

[INSTRSET "i486p"]                  ; 486の命令まで使いたいという記述

        LGDT    [GDTR0]             ; 暫定のGDTを設定
        MOV     EAX, CR0
        AND     EAX, 0x7fffffff     ; bit32を0にする(ページングの禁止のため)
        OR      EAX, 0x00000001     ; bit0を1にする(プロテクトモード移行のため)
        MOV     CR0, EAX
        JMP     pipelineflush
pipelineflush:
        MOV     AX, 1*8             ; 読み書き可能なセグメント32bit
        MOV     DS, AX
        MOV     ES, AX
        MOV     FS, AX
        MOV     GS, AX
        MOV     SS, AX

; bootpackの転送

        MOV     ESI, bootpack       ; 転送元
        MOV     EDI, BOTPAK         ; 転送先
        MOV     ECX, 512*1024/4
        CALL    memcpy

; ついでにディスクデータも本来の位置へ転送

; まずはブートセクタ

        MOV     ESI, 0x7c00         ; 転送元
        MOV     EDI, DSKCAC         ; 転送先
        MOV     ECX, 512/4
        CALL    memcpy
        
; 残り全部
        
        MOV     ESI,DSKCAC0+512      ; 転送元
        MOV     EDI,DSKCAC+512      ; 転送元
        MOV     ECX, 0
        MOV     CL, BYTE [CYLS]
        IMUL    ECX, 512*18*2/4     ; シリンダ数からバイト数/4に変換
        SUB     ECX, 512/4          ; IPLの分だけ差し引く
        CALL    memcpy

; asmheadで記述しなければいけないことはすべて終わったので,
;   あとはbootpackに任せる

; bootpackの起動

        MOV     EBX, BOTPAK
        MOV     ECX, [EBX+16]
        add     ecx, 3              ; ecx += 3
        shr     ecx, 2              ; ecx /= 4(ecx >> 2?)
        jz      skip                ; 転送すべきものが無い
        mov     esi, [EBX+20]       ; 転送先
        add     esi, ebx
        mov     edi, [ebx+12]       ; 転送先
        call    memcpy
skip:
        MOV     ESP, [EBX+12]       ; スタック初期値
        JMP     DWORD 2*8:0x0000001b

waitkbdoubt:
        IN      AL, 0x64
        AND     AL, 0x02
        JNZ     waitkbdoubt         ; ANDの結果が0出なければwaitdoubtへ
        RET

memcpy:
        MOV     EAX, [ESI]
        ADD     ESI, 4
        MOV     [EDI], EAX
        ADD     EDI, 4
        SUB     ECX, 1
        JNZ     memcpy              ; 引き算の結果が0出なければmemcpyへ
        RET
; memcpyはアドレスサイズプリフィックスを入れ忘れなければ、ストリング命令でも書ける
    ;意味わからん. 調べる

        ALIGNB  16
GDT0:
        RESB    8                   ;ヌルセレクタ
        DW      0xffff, 0x0000, 0x9200, 0x00cf  ; 読み書き可能セグメント32bit
        DW      0xffff, 0x0000, 0x9a28, 0x0047  ; 実行可能セグメント32bit(bootpack用)

        DW      0
GDTR0:
        DW      8*3-1
        DD      GDT0

        ALIGNB  16
bootpack:
