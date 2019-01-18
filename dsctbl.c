//GDT,IDTなどのDescriptorTable関連の処理

struct SEGMENT_DESCRIPTOR
{
    short limit_low, base_low;
    char base_mid, access_right;
    char limit_high, base_high;
};

struct GATE_DESCRIPTOR
{
    short offset_low, selector;
    char dw_count, access_right;
    short offset_high;
};

void init_gdtidt(void);
void set_segment_descriptor(struct SEGMENT_DESCRIPTOR *segmentDescriptor, unsigned int limit, int base, int access_right);
void set_gate_descriptor(struct GATE_DESCRIPTOR *gateDescriptor, int offset, int selector, int access_right);
void load_gdtr(int limit, int adress);
void load_idtr(int limit, int adress);

void init_gdtidt(void)
{
    struct SEGMENT_DESCRIPTOR *gdt = (struct SEGMENT_DESCRIPTOR *)0x00270000;
    struct GATE_DESCRIPTOR *idt = (struct GATE_DESCRIPTOR *)0x0026f800;
    int i = 0;

    //GDTの初期化
    for (i = 0; i < 8192; i++)
        set_segment_descriptor(gdt + i, 0, 0, 0);
    set_segment_descriptor(gdt + 1, 0xffffffff, 0x00000000, 0x4092);
    set_segment_descriptor(gdt + 2, 0x0007ffff, 0x00280000, 0x409a);
    load_gdtr(0xffff, 0x00270000);

    //IDTの初期化
    for (i = 0; i < 256; i++)
        set_gate_descriptor(idt + i, 0, 0, 0);
    load_idtr(0x7ff, 0x0026f800);

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
