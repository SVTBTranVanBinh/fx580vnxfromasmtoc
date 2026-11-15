/* screen_demo.c - the same drawing calls as tools/screen_demo.lua, run through
 * the C translations instead of the ROM, so the two screen buffers can be
 * compared byte for byte.
 *
 * Build and run it with the emulator demo:
 *     gcc -std=c11 -O2 -o build/screen_demo tools/screen_demo.c \
 *         src/*.c emu/nx8.c -I src
 *     ./build/screen_demo > build/fb_c.txt
 *     cmp build/fb_c.txt build/fb_rom.txt
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/draw.h"
#include "../src/display.h"
#include "../src/print.h"

/* The demo draws one frame and two lines of text into buffer 1. */
#define STR_ADDR 0xD100u
#define SEL_ADDR 0xD139u
#define FONT_SIZE_ADDR 0xD137u

static uint8_t ds[0x10000];

int main(void)
{
    static const char line[] = "HELLO 580VN";
    uint16_t str_out = 0;
    FILE *rom = fopen("resource/rom.bin", "rb");

    memset(ds, 0, sizeof ds);
    /* Everything below 0xD000 is the ROM window, and the glyph bitmaps live
     * there: without the ROM bytes in place the C translations have no font and
     * draw nothing, which is exactly what the first run of this demo showed. */
    if (rom == NULL || fread(ds, 1, 0xD000, rom) != 0xD000) {
        fprintf(stderr, "screen_demo: cannot read resource/rom.bin\n");
        return 1;
    }
    fclose(rom);
    ds[SEL_ADDR] = 1;                        /* nonzero selects buffer 1 */
    ds[FONT_SIZE_ADDR] = 14;                 /* the firmware only ever sets
                                              * 10 or 14; anything else makes
                                              * the ROM read its own stack */
    for (size_t i = 0; i < sizeof line; i++)
        ds[(uint16_t)(STR_ADDR + i)] = (uint8_t)line[i];

    box_frame_thin(ds, 0x0404u, 0xA030u);     /* x = 4, y = 4, to row 0x30 */
    box_frame_thin(ds, 0x0404u, 0x1030u);
    (void)line_print(ds, 0x0808u, STR_ADDR, &str_out);
    (void)line_print(ds, 0x0840u, STR_ADDR, &str_out);

    for (uint16_t a = 0; a < BUF_SIZE; a++)
        printf("%02X%s", ds[(uint16_t)(SCREEN_BUF1 + a)],
               ((a + 1) % 32) == 0 ? "\n" : " ");
    return 0;
}
