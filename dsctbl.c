//GDT,IDTなどのDescriptorTable関連の処理

#include "bootpack.h"

void init_gdtidt(void)
{
    struct SEGMENT_DESCRIPTOR *gdt = (struct SEGMENT_DESCRIPTOR *)ADRESS_GDT;
    struct GATE_DESCRIPTOR *idt = (struct GATE_DESCRIPTOR *)ADRESS_IDT;
    int i = 0;

    //GDTの初期化
    for (i = 0; i < 8192; i++)
        set_segment_descriptor(gdt + i, 0, 0, 0);
    set_segment_descriptor(gdt + 1, 0xffffffff, 0x00000000, AR_DATA32_RW);
    set_segment_descriptor(gdt + 2, LIMIT_BOTPACK, ADRESS_BOTPACK, AR_CODE32_ER);
    load_gdtr(LIMIT_GDT, ADRESS_GDT);

    //IDTの初期化
    for (i = 0; i < 256; i++)
        set_gate_descriptor(idt + i, 0, 0, 0);
    load_idtr(LIMIT_IDT, ADRESS_IDT);

    //IDTの設定
    //第三引数は所属セグメントを表す。下位3ビットは0である必要があるのでビットシフト
    //なお、第二セグメントはbootpack.hrbを内包するセグメント
    //第四引数はIDTに対する属性設定。割り込み処理用の有効な設定であることを表す
    set_gate_descriptor(idt + 0x20, (int)asm_inthandler20, 2 << 3, AR_INTGATE32);
    set_gate_descriptor(idt + 0x21, (int)asm_inthandler21, 2 << 3, AR_INTGATE32);
    set_gate_descriptor(idt + 0x2c, (int)asm_inthandler2c, 2 << 3, AR_INTGATE32);

    return;
}

void set_segment_descriptor(struct SEGMENT_DESCRIPTOR *segmentDescriptor, unsigned int limit, int base, int access_right)
{
    if (limit > 0xfffff)
    {
        access_right |= 0x8000; //G_bit = 1
        limit /= 0x1000;
    }

    segmentDescriptor->limit_low = limit & 0xffff;
    segmentDescriptor->base_low = base & 0xffff;
    segmentDescriptor->base_mid = (base >> 16) & 0xff;
    segmentDescriptor->access_right = access_right & 0xff;
    segmentDescriptor->limit_high = ((limit >> 16) & 0x0f) | ((access_right >> 8) & 0xf0);
    segmentDescriptor->base_high = (base >> 24) & 0xff;
    return;
}

void set_gate_descriptor(struct GATE_DESCRIPTOR *gateDescriptor, int offset, int selector, int access_right)
{
    gateDescriptor->offset_low = offset & 0xffff;
    gateDescriptor->selector = selector;
    gateDescriptor->dw_count = (access_right >> 8) & 0xff;
    gateDescriptor->access_right = access_right & 0xff;
    gateDescriptor->offset_high = (offset >> 16) & 0xffff;
    return;
}
