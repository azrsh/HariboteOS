#include <stdio.h>
#include "bootpack.h"

void HariMain(void)
{
    struct BOOTINFO *boot_info = (struct BOOTINFO *)0xff0; //boot infoの開始アドレス
    char s[40], mouse_cursor[256];
    int mouse_x, mouse_y;

    init_palette();
    init_screen(boot_info->vram, boot_info->screenX, boot_info->screenY);

    mouse_x = (boot_info->screenX - 16) / 2; //画面中央に配置
    mouse_y = (boot_info->screenY - 28 - 16) / 2;
    init_mouse_cursor8(mouse_cursor, COLOR8_008484);
    putblock8_8(boot_info->vram, boot_info->screenX, 16, 16, mouse_x, mouse_y, mouse_cursor, 16);

    sprintf(s, "(%d, %d)", mouse_x, mouse_y);
    putfonts8_asc(boot_info->vram, boot_info->screenX, 0, 0, COLOR8_FFFFFF, s);

    for (;;)
    {
        io_hlt();
    }
}
