; hello-os
; TAB = 4
; DB data byte,1byte指定書き込み
; DW data word,2byte指定書き込み
; DD data double-word,4バイト指定書き込み
; RESB reserve byte,指定数分予約,0で埋める
; MOV 代入命令
; ORG メモリのどこに読み込まれるかを取得
; ADD 加算命令
; [xxx] xxxをメモリアドレスとして扱う
; JMP 指定されたタグにジャンプ
; CMP 比較命令,真の場合の処理が後に続く
; JE jump if equal 前の比較命令が真のとき指定されたタグにジャンプ
; INT interrupt 割り込み命令
; HLT cpu待機命令 省エネ halt:停止

; EQU =,代入演算子？

; 定数定義みたいなもの
CYLS	EQU		10				; どこまで読み込むか


        ORG     0x7c00              ; このプログラムがどこに読み込まれるか,0x7c00はブートセクタが読み込まれるアドレス

; 標準的なFAT12フォーマットのフロッピーディスク用の記述
        JMP     entry
        DB      0x90
        DB      "HARIBOTE"          ; ブートセクタの名称、自由、8byte
        DW      512                 ; 1セクタの大きさ(512byteにしなければならない)
        DB      1                   ; クラスタの大きさ(1セクタにしなくてはならない)
        DW      1                   ; FATがどこから始まるか(普通は1セクタ目からにする)
        DB      2                   ; FATの個数(2にしなければならない)
        DW      224                 ; ルートディレクトリの大きさ(普通は224エントリにする)
        DW      2880                ; このドライブの大きさ(2880セクタにしなくてはならない)
        DB      0xf0                ; メディアのタイプ(0xf0にしなくてはならない)
        DW      9                   ; FAT領域の長さ(9セクタにしなくてはならない)
        DW      18                  ; 1トラックにいくつのセクタがあるか(18にしなければいけない)
        DW      2                   ; ヘッドの数(2にしなくてはいけない)
        DD      0                   ; パーティションを使っていないのでここでは必ず0
        DD      2880                ; このドライブの大きさをもう一度書く
        DB      0,0,0x29            ; よく分からない、この値にしておくといいらしい
        DD      0xffffffff          ; たぶんボリュームのシリアル番号
        DB      "HARIBOTE-OS"       ; ディスクの名前(11byte)
        DB      "FAT12   "          ; フォーマットの名前(8byte)
        RESB    18                  ; とりあえず18byte空けておく

; プログラム本体

entry:
        MOV     AX, 0               ; レジスタの初期化,DB      0xb8, 0x00, 0x00, 0x8e, 0xd0, 0xbc, 0x00, 0x7c
        MOV     SS, AX              ;DB      0x8e, 0xd8, 0x8e, 0xc0, 0xbe, 0x74, 0x7c, 0x8a
        MOV     SP, 0x7c00          ;DB      0x04, 0x83, 0xc6, 0x01, 0x3c, 0x00, 0x74, 0x09
        MOV     DS, AX              ;DB      0xb4, 0x0e, 0xbb, 0x0f, 0x00, 0xcd, 0x10, 0xeb

; ディスクを読み込む

        MOV     AX, 0x0820
        MOV     ES, AX              ; バッファアドレス(読み込み先のメモリの大まかなアドレス)
        MOV     CH, 0               ; シリンダ0(年輪的なアレ)
        MOV     DH, 0               ; ヘッド0(フロッピーディスクの表裏)
        MOV     CL, 2               ; セクタ2(片方のヘッドのあるシリンダを18等分したもの=1セクタ)
readloop:
        MOV     SI, 0               ; 失敗回数を数えるレジスタ
retry:
        MOV     AH, 0x02            ; AH = 0x02 : ディスク読み込み
        MOV     AL, 1               ; 1セクタ
        MOV     BX, 0               ; バッファアドレス(読み込み先のメモリの詳細なアドレス)
        MOV     DL, 0x00            ; Aドライブ(0番のドライブ)
        INT     0x13                ; ディスクbios呼び出し
        JNC     next                 ; errorがなければnextへ
        ADD     SI, 1               
        CMP     SI, 5               ; SIと5を比較
        JAE     error               ; SIが5以上ならerrorへ
        MOV     AH, 0x00
        MOV     DL, 0x00            ; Aドライブ
        INT     0x13                ; システムのリセット
        JMP     retry
next:
        MOV     AX, ES              ; アドレスを0x20進める,0x20 = 512/16
        ADD     AX, 0x0020              ; ADD ES,0x0020ができないのでこうやっている
        MOV     ES, AX                  ; 
        ADD     CL, 1
        CMP     CL, 18              ; セクタ番号が格納されたCLと18を比較
        JBE     readloop            ; CLが18以下ならreadloopへ
        MOV     CL, 1
        ADD     DH, 1
        CMP     DH, 2               ; DHと2を比較
        JB      readloop            ; DH < 2の時readloopへ
        MOV     DH, 0
        ADD     CH, 1
        CMP     CH, CYLS
        JB      readloop            ; CH < CYLSならreadloop

; ここまでフロッピディスクのロード,ロードが完了したのでOS本体のアドレスにジャンプ(アドレスはバイナリエディタで確認)

        MOV     [0x0ff0], CH        ; IPLがどこまで読んだかメモ
        JMP     0xc200              ; 0x8000(ブートセクタの先頭) + 0x4200(イメージファイル内のOS本体のアドレス(ローカルアドレス？)) = 0xc200

; 読み込み後はやることが無いのでとりあえず待機
fin:
        HLT                         ; 何かあるまでCPUを停止
        JMP fin                     ; 無限ループ

error:
        MOV SI, msg

putloop:
        MOV     AL, [SI]
        ADD     SI, 1               ; SIに1加算
        CMP     AL, 0
        JE      fin
        MOV     AH, 0x0e            ; 1文字表示ファンクション,ここ間違えあたら変な表示になった
        MOV     BX, 15              ; カラーコード
        INT     0x10                ; ビデオbios呼び出し
        JMP     putloop
        
msg:
        DB      0x0a, 0x0a          ; 改行2つ
        DB      "load error!"
        DB      0x0a                ; 改行1つ
        DB      0
        
        RESB    0x7dfe-$            ; 0x7dfe - 現在のアドレス => 0x1feまで予約

        DB      0x55, 0xaa