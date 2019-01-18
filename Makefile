TOOLPATH = ../z_tools/
INCPATH  = ../z_tools/haribote/

MAKE     = $(TOOLPATH)make.exe -r
NASK     = $(TOOLPATH)nask.exe
CC1      = $(TOOLPATH)cc1.exe -I$(INCPATH) -Os -Wall -quiet
GAS2NASK = $(TOOLPATH)gas2nask.exe -a
OBJ2BIM  = $(TOOLPATH)obj2bim.exe
MAKEFONT = $(TOOLPATH)makefont.exe
BIN2OBJ	 = $(TOOLPATH)bin2obj.exe
BIM2HRB  = $(TOOLPATH)bim2hrb.exe
RULEFILE = $(TOOLPATH)haribote/haribote.rul
EDIMG    = $(TOOLPATH)edimg.exe
COPY     = cmd.exe /C copy
DEL      = cmd.exe /C del

# デフォルト動作

default :
	$(MAKE) img

# ファイル生成規則

ipl10.bin : ipl10.nasm Makefile
	$(NASK) ipl10.nasm ipl10.bin ipl10.lst

asmhead.bin : asmhead.nasm Makefile
	$(NASK) asmhead.nasm asmhead.bin asmhead.lst

#bootpack.c to bootpack.obj
bootpack.gas : bootpack.c Makefile
	$(CC1) -o bootpack.gas bootpack.c

bootpack.nasm : bootpack.gas Makefile
	$(GAS2NASK) bootpack.gas bootpack.nasm

bootpack.obj : bootpack.nasm Makefile
	$(NASK) bootpack.nasm bootpack.obj bootpack.lst

#graphic.c to graphic.obj
graphic.gas : graphic.c Makefile
	$(CC1) -o graphic.gas graphic.c

graphic.nasm : graphic.gas Makefile
	$(GAS2NASK) graphic.gas graphic.nasm

graphic.obj : graphic.nasm Makefile
	$(NASK) graphic.nasm graphic.obj graphic.lst

#dsctbl.c to dsctbl.obj
dsctbl.gas : dsctbl.c Makefile
	$(CC1) -o dsctbl.gas dsctbl.c

dsctbl.nasm : dsctbl.gas Makefile
	$(GAS2NASK) dsctbl.gas dsctbl.nasm

dsctbl.obj : dsctbl.nasm Makefile
	$(NASK) dsctbl.nasm dsctbl.obj dsctbl.lst

#naskfunc.nasm to naskfunc.obj
naskfunc.obj : naskfunc.nasm Makefile
	$(NASK) naskfunc.nasm naskfunc.obj naskfunc.lst

#hankaku.txt to hankaku.obj
hankaku.bin : hankaku.txt Makefile
	$(MAKEFONT) hankaku.txt hankaku.bin

hankaku.obj : hankaku.bin Makefile
	$(BIN2OBJ) hankaku.bin hankaku.obj _hankaku

#objs to bootpack.bim
bootpack.bim : bootpack.obj naskfunc.obj hankaku.obj graphic.obj dsctbl.obj Makefile
	$(OBJ2BIM) @$(RULEFILE) out:bootpack.bim stack:3136k map:bootpack.map \
		bootpack.obj naskfunc.obj hankaku.obj graphic.obj dsctbl.obj
# 3MB+64KB=3136KB

bootpack.hrb : bootpack.bim Makefile
	$(BIM2HRB) bootpack.bim bootpack.hrb 0

haribote.sys : asmhead.bin bootpack.hrb Makefile
	$(COPY) /B asmhead.bin+bootpack.hrb haribote.sys

haribote.img : ipl10.bin haribote.sys Makefile
	$(EDIMG)   imgin:../z_tools/fdimg0at.tek \
		wbinimg src:ipl10.bin len:512 from:0 to:0 \
		copy from:haribote.sys to:@: \
		imgout:haribote.img

# コマンド

img :
	$(MAKE) haribote.img

clean :
	-$(DEL) *.bin
	-$(DEL) *.lst
	-$(DEL) *.gas
	-$(DEL) *.obj
	-$(DEL) bootpack.nasm
	-$(DEL) bootpack.map
	-$(DEL) bootpack.bim
	-$(DEL) bootpack.hrb
	-$(DEL) haribote.sys

src_only :
	$(MAKE) clean
	-$(DEL) haribote.img
