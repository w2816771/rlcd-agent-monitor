/*******************************************************************************
 * Size: 14 px
 * Bpp: 1
 * Opts: --font managed_components/lvgl__lvgl/tests/src/test_files/fonts/Montserrat-Bold.ttf --size 14 --bpp 1 --format lvgl --no-compress --autohint-strong --range 0x20-0x7E --lv-include lvgl.h --lv-font-name rlcd_font_bold_14 -o main/rlcd_font_bold_14.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef RLCD_FONT_BOLD_14
#define RLCD_FONT_BOLD_14 1
#endif

#if RLCD_FONT_BOLD_14

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xff, 0xf0, 0xf0,

    /* U+0022 "\"" */
    0xde, 0xf7, 0xb0,

    /* U+0023 "#" */
    0x33, 0x19, 0xbf, 0xff, 0xf3, 0x21, 0x33, 0xff,
    0xff, 0x66, 0x33, 0x0,

    /* U+0024 "$" */
    0x18, 0x18, 0x3e, 0xfe, 0xda, 0xd8, 0x78, 0x1e,
    0x1b, 0x9b, 0xff, 0x7c, 0x18, 0x18,

    /* U+0025 "%" */
    0x70, 0x8d, 0x98, 0xd9, 0xd, 0xa0, 0x76, 0x0,
    0x5e, 0xb, 0x30, 0xb3, 0x13, 0x32, 0x1e,

    /* U+0026 "&" */
    0x3c, 0x3f, 0x19, 0x8c, 0x83, 0x83, 0xef, 0x3f,
    0x8e, 0xff, 0xbe, 0x40,

    /* U+0027 "'" */
    0xff,

    /* U+0028 "(" */
    0x66, 0x6c, 0xcc, 0xcc, 0xcc, 0xc6, 0x66,

    /* U+0029 ")" */
    0x66, 0x63, 0x33, 0x33, 0x33, 0x36, 0x66,

    /* U+002A "*" */
    0x33, 0xff, 0xff, 0xfc, 0xc0,

    /* U+002B "+" */
    0x18, 0x30, 0x67, 0xff, 0xe3, 0x6, 0xc,

    /* U+002C "," */
    0xff, 0x80,

    /* U+002D "-" */
    0xff,

    /* U+002E "." */
    0xfc,

    /* U+002F "/" */
    0xc, 0x31, 0x86, 0x18, 0xc3, 0xc, 0x61, 0x86,
    0x30, 0xc3, 0x0,

    /* U+0030 "0" */
    0x3c, 0x7e, 0xe7, 0xc3, 0xc3, 0xc3, 0xc3, 0xe7,
    0x7e, 0x3c,

    /* U+0031 "1" */
    0xff, 0x33, 0x33, 0x33, 0x33,

    /* U+0032 "2" */
    0x7d, 0xfd, 0x18, 0x30, 0xe3, 0x8e, 0x38, 0xff,
    0xfc,

    /* U+0033 "3" */
    0xff, 0xfc, 0x30, 0xc3, 0xc7, 0xc1, 0xc3, 0xfe,
    0xf0,

    /* U+0034 "4" */
    0x6, 0x3, 0x1, 0xc0, 0x60, 0x33, 0x1c, 0xcf,
    0xff, 0xff, 0x3, 0x0, 0xc0,

    /* U+0035 "5" */
    0x3e, 0x3e, 0x70, 0x7c, 0x7f, 0x3, 0x3, 0x43,
    0x7e, 0x3c,

    /* U+0036 "6" */
    0x1e, 0x7e, 0x60, 0xdc, 0xfe, 0xc3, 0xc3, 0xc3,
    0x7e, 0x3c,

    /* U+0037 "7" */
    0xff, 0xff, 0xc7, 0xc6, 0xe, 0xc, 0x1c, 0x18,
    0x38, 0x30,

    /* U+0038 "8" */
    0x3c, 0xff, 0xc3, 0xc3, 0xff, 0xff, 0xc3, 0xc3,
    0xff, 0x3c,

    /* U+0039 "9" */
    0x3c, 0xfe, 0xc3, 0xc3, 0xff, 0x7b, 0x3, 0x6,
    0x7e, 0x78,

    /* U+003A ":" */
    0xfc, 0x3f,

    /* U+003B ";" */
    0xfc, 0x3f, 0xe0,

    /* U+003C "<" */
    0x2, 0x3d, 0xe7, 0xf, 0x7, 0x83, 0x80,

    /* U+003D "=" */
    0xff, 0xfc, 0x0, 0xf, 0xff, 0xc0,

    /* U+003E ">" */
    0x81, 0xc1, 0xf0, 0x71, 0xff, 0x30, 0x0,

    /* U+003F "?" */
    0x3e, 0x7f, 0x63, 0x3, 0x7, 0xe, 0x1c, 0x0,
    0x1c, 0x1c,

    /* U+0040 "@" */
    0xf, 0xc0, 0xff, 0xc7, 0x83, 0x9b, 0xf6, 0xdf,
    0xcf, 0x63, 0x3d, 0x8c, 0xf6, 0x33, 0xcf, 0xf9,
    0xbd, 0xc7, 0x0, 0xf, 0xe0, 0xf, 0x80,

    /* U+0041 "A" */
    0xe, 0x1, 0xc0, 0x7c, 0xd, 0x83, 0x30, 0x63,
    0x1f, 0xe3, 0xfe, 0x60, 0xd8, 0x1c,

    /* U+0042 "B" */
    0xfe, 0x7f, 0xb0, 0xd8, 0x6f, 0xf7, 0xff, 0x7,
    0x83, 0xff, 0xff, 0x80,

    /* U+0043 "C" */
    0x1e, 0x3f, 0xd8, 0x58, 0xc, 0x6, 0x3, 0x0,
    0xc2, 0x7f, 0x8f, 0x0,

    /* U+0044 "D" */
    0xfe, 0x3f, 0xec, 0x3b, 0x3, 0xc0, 0xf0, 0x3c,
    0xf, 0xe, 0xff, 0xbf, 0x80,

    /* U+0045 "E" */
    0xff, 0xff, 0xc0, 0xc0, 0xfe, 0xfe, 0xc0, 0xc0,
    0xff, 0xff,

    /* U+0046 "F" */
    0xff, 0xff, 0xc0, 0xc0, 0xfe, 0xfe, 0xc0, 0xc0,
    0xc0, 0xc0,

    /* U+0047 "G" */
    0x1f, 0x3f, 0xd8, 0x58, 0xc, 0x6, 0xf, 0x6,
    0xc3, 0x7f, 0x8f, 0x80,

    /* U+0048 "H" */
    0xc1, 0xe0, 0xf0, 0x78, 0x3f, 0xff, 0xff, 0x7,
    0x83, 0xc1, 0xe0, 0xc0,

    /* U+0049 "I" */
    0xff, 0xff, 0xf0,

    /* U+004A "J" */
    0x7e, 0xfc, 0x18, 0x30, 0x60, 0xc1, 0xa3, 0xfe,
    0x78,

    /* U+004B "K" */
    0xc3, 0xe3, 0xb3, 0x9b, 0x8f, 0x87, 0xe3, 0xb9,
    0x8c, 0xc7, 0x61, 0xc0,

    /* U+004C "L" */
    0xc1, 0x83, 0x6, 0xc, 0x18, 0x30, 0x60, 0xff,
    0xfc,

    /* U+004D "M" */
    0xc0, 0x7c, 0x1f, 0x83, 0xf8, 0xff, 0xbf, 0xb6,
    0xf3, 0x9e, 0x73, 0xc4, 0x78, 0xc,

    /* U+004E "N" */
    0xc1, 0xf0, 0xfc, 0x7f, 0x3f, 0xde, 0xef, 0x3f,
    0x8f, 0xc3, 0xe0, 0xc0,

    /* U+004F "O" */
    0x1f, 0xf, 0xf1, 0xc3, 0x60, 0x3c, 0x7, 0x80,
    0xf0, 0x1b, 0xe, 0x7f, 0xc3, 0xe0,

    /* U+0050 "P" */
    0xfc, 0xfe, 0xc3, 0xc3, 0xc3, 0xfe, 0xfc, 0xc0,
    0xc0, 0xc0,

    /* U+0051 "Q" */
    0x1f, 0x7, 0xfc, 0x61, 0xcc, 0x6, 0xc0, 0x6c,
    0x6, 0xc0, 0x66, 0x1c, 0x7f, 0xc1, 0xf0, 0x7,
    0xe0, 0x1c,

    /* U+0052 "R" */
    0xfc, 0xfe, 0xc3, 0xc3, 0xc3, 0xfe, 0xfc, 0xce,
    0xc6, 0xc3,

    /* U+0053 "S" */
    0x3e, 0xfe, 0xc2, 0xc0, 0x78, 0x1e, 0x3, 0x83,
    0xff, 0x7c,

    /* U+0054 "T" */
    0xff, 0xff, 0xc6, 0x3, 0x1, 0x80, 0xc0, 0x60,
    0x30, 0x18, 0xc, 0x0,

    /* U+0055 "U" */
    0xc1, 0xe0, 0xf0, 0x78, 0x3c, 0x1e, 0xf, 0x7,
    0xc7, 0x7f, 0x1f, 0x0,

    /* U+0056 "V" */
    0xe0, 0xcc, 0x19, 0x87, 0x38, 0xc3, 0x38, 0x76,
    0x6, 0xc0, 0xf0, 0x1e, 0x1, 0xc0,

    /* U+0057 "W" */
    0xe1, 0x83, 0x61, 0xc7, 0x63, 0xc6, 0x73, 0xc6,
    0x33, 0x6e, 0x36, 0x6c, 0x3e, 0x6c, 0x1e, 0x3c,
    0x1c, 0x38, 0x1c, 0x38,

    /* U+0058 "X" */
    0xe1, 0x9c, 0xc3, 0x70, 0xf8, 0x1c, 0x7, 0x83,
    0xe0, 0xdc, 0x63, 0xb8, 0x60,

    /* U+0059 "Y" */
    0xe1, 0xd8, 0x63, 0x30, 0xec, 0x1e, 0x7, 0x80,
    0xc0, 0x30, 0xc, 0x3, 0x0,

    /* U+005A "Z" */
    0xff, 0xff, 0x6, 0xe, 0x1c, 0x38, 0x70, 0x60,
    0xff, 0xff,

    /* U+005B "[" */
    0xff, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xff,

    /* U+005C "\\" */
    0x60, 0xc1, 0x81, 0x83, 0x6, 0x4, 0xc, 0x18,
    0x10, 0x30, 0x60, 0x40, 0xc0,

    /* U+005D "]" */
    0xff, 0x33, 0x33, 0x33, 0x33, 0x33, 0xff,

    /* U+005E "^" */
    0x30, 0x71, 0xa2, 0x6c, 0xd8, 0x80,

    /* U+005F "_" */
    0xff, 0xfc,

    /* U+0060 "`" */
    0x63,

    /* U+0061 "a" */
    0x7d, 0xfc, 0x1b, 0xff, 0xf8, 0xff, 0xbb,

    /* U+0062 "b" */
    0xc0, 0xc0, 0xc0, 0xdc, 0xfe, 0xe7, 0xc3, 0xc3,
    0xe7, 0xfe, 0xdc,

    /* U+0063 "c" */
    0x3c, 0xff, 0x9e, 0xc, 0x1c, 0xdf, 0x9e,

    /* U+0064 "d" */
    0x3, 0x3, 0x3, 0x3b, 0x7f, 0xe7, 0xc3, 0xc3,
    0xe7, 0x7f, 0x3b,

    /* U+0065 "e" */
    0x3c, 0x7e, 0xe7, 0xff, 0xff, 0xe0, 0x7e, 0x3c,

    /* U+0066 "f" */
    0x3d, 0xe6, 0x3e, 0xf9, 0x86, 0x18, 0x61, 0x86,
    0x0,

    /* U+0067 "g" */
    0x3b, 0x7f, 0xe7, 0xc3, 0xc3, 0xe7, 0x7f, 0x3b,
    0x7, 0x7e, 0x7c,

    /* U+0068 "h" */
    0xc0, 0xc0, 0xc0, 0xde, 0xff, 0xe3, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3,

    /* U+0069 "i" */
    0x30, 0xff, 0xff,

    /* U+006A "j" */
    0x3, 0x0, 0x33, 0x33, 0x33, 0x33, 0x3f, 0xe0,

    /* U+006B "k" */
    0xc0, 0xc0, 0xc0, 0xc6, 0xcc, 0xdc, 0xf8, 0xfc,
    0xec, 0xce, 0xc7,

    /* U+006C "l" */
    0xff, 0xff, 0xfc,

    /* U+006D "m" */
    0xfd, 0xef, 0xff, 0xc6, 0x3c, 0x63, 0xc6, 0x3c,
    0x63, 0xc6, 0x3c, 0x63,

    /* U+006E "n" */
    0xde, 0xff, 0xe3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,

    /* U+006F "o" */
    0x3c, 0x7e, 0xe7, 0xc3, 0xc3, 0xe7, 0x7e, 0x3c,

    /* U+0070 "p" */
    0xdc, 0xfe, 0xe7, 0xc3, 0xc3, 0xe7, 0xfe, 0xdc,
    0xc0, 0xc0, 0xc0,

    /* U+0071 "q" */
    0x3b, 0x7f, 0xe7, 0xc3, 0xc3, 0xe7, 0x7f, 0x3b,
    0x3, 0x3, 0x3,

    /* U+0072 "r" */
    0xdf, 0xf9, 0x8c, 0x63, 0x18,

    /* U+0073 "s" */
    0x7d, 0xfb, 0x7, 0x83, 0xd1, 0xbf, 0x3c,

    /* U+0074 "t" */
    0x61, 0x8f, 0xbe, 0x61, 0x86, 0x18, 0x78, 0xe0,

    /* U+0075 "u" */
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc7, 0xff, 0x3b,

    /* U+0076 "v" */
    0xc3, 0xe3, 0x67, 0x66, 0x36, 0x3c, 0x3c, 0x1c,

    /* U+0077 "w" */
    0xc3, 0x1b, 0x38, 0xd9, 0xcc, 0xdb, 0x63, 0xdb,
    0x1e, 0xf0, 0xe3, 0x83, 0x1c,

    /* U+0078 "x" */
    0xe6, 0x6e, 0x3c, 0x38, 0x38, 0x7c, 0x6e, 0xc7,

    /* U+0079 "y" */
    0xc3, 0xe3, 0x66, 0x76, 0x36, 0x3c, 0x1c, 0x18,
    0x18, 0xf8, 0xf0,

    /* U+007A "z" */
    0xff, 0xfc, 0x70, 0xc3, 0xe, 0x3f, 0xff,

    /* U+007B "{" */
    0x3b, 0xd8, 0xc6, 0x33, 0x9c, 0x63, 0x18, 0xc7,
    0x9c,

    /* U+007C "|" */
    0xff, 0xff, 0xff, 0xf0,

    /* U+007D "}" */
    0xe3, 0xc3, 0xc, 0x30, 0xc3, 0xcf, 0x30, 0xc3,
    0xc, 0xf3, 0x80,

    /* U+007E "~" */
    0x73, 0xfe, 0x70
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 63, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 65, .box_w = 2, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 4, .adv_w = 98, .box_w = 5, .box_h = 4, .ofs_x = 1, .ofs_y = 6},
    {.bitmap_index = 7, .adv_w = 161, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 19, .adv_w = 143, .box_w = 8, .box_h = 14, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 33, .adv_w = 196, .box_w = 12, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 48, .adv_w = 163, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 60, .adv_w = 52, .box_w = 2, .box_h = 4, .ofs_x = 1, .ofs_y = 6},
    {.bitmap_index = 61, .adv_w = 80, .box_w = 4, .box_h = 14, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 68, .adv_w = 80, .box_w = 4, .box_h = 14, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 75, .adv_w = 97, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 5},
    {.bitmap_index = 80, .adv_w = 134, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 87, .adv_w = 59, .box_w = 2, .box_h = 5, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 89, .adv_w = 86, .box_w = 4, .box_h = 2, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 90, .adv_w = 59, .box_w = 2, .box_h = 3, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 91, .adv_w = 88, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 102, .adv_w = 152, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 112, .adv_w = 88, .box_w = 4, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 117, .adv_w = 132, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 126, .adv_w = 133, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 135, .adv_w = 154, .box_w = 10, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 148, .adv_w = 133, .box_w = 8, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 158, .adv_w = 143, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 168, .adv_w = 139, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 178, .adv_w = 148, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 188, .adv_w = 143, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 198, .adv_w = 59, .box_w = 2, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 200, .adv_w = 59, .box_w = 2, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 203, .adv_w = 134, .box_w = 7, .box_h = 7, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 210, .adv_w = 134, .box_w = 7, .box_h = 6, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 216, .adv_w = 134, .box_w = 7, .box_h = 7, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 223, .adv_w = 132, .box_w = 8, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 233, .adv_w = 232, .box_w = 14, .box_h = 13, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 256, .adv_w = 172, .box_w = 11, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 270, .adv_w = 171, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 282, .adv_w = 164, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 294, .adv_w = 185, .box_w = 10, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 307, .adv_w = 150, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 317, .adv_w = 143, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 327, .adv_w = 173, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 339, .adv_w = 181, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 351, .adv_w = 73, .box_w = 2, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 354, .adv_w = 121, .box_w = 7, .box_h = 10, .ofs_x = -1, .ofs_y = 0},
    {.bitmap_index = 363, .adv_w = 166, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 375, .adv_w = 135, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 384, .adv_w = 214, .box_w = 11, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 398, .adv_w = 181, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 410, .adv_w = 189, .box_w = 11, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 424, .adv_w = 164, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 434, .adv_w = 189, .box_w = 12, .box_h = 12, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 452, .adv_w = 165, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 462, .adv_w = 143, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 472, .adv_w = 138, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 484, .adv_w = 177, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 496, .adv_w = 167, .box_w = 11, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 510, .adv_w = 261, .box_w = 16, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 530, .adv_w = 160, .box_w = 10, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 543, .adv_w = 151, .box_w = 10, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 556, .adv_w = 150, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 566, .adv_w = 82, .box_w = 4, .box_h = 14, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 573, .adv_w = 88, .box_w = 7, .box_h = 14, .ofs_x = -1, .ofs_y = -1},
    {.bitmap_index = 586, .adv_w = 82, .box_w = 4, .box_h = 14, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 593, .adv_w = 134, .box_w = 7, .box_h = 6, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 599, .adv_w = 112, .box_w = 7, .box_h = 2, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 601, .adv_w = 134, .box_w = 4, .box_h = 2, .ofs_x = 1, .ofs_y = 9},
    {.bitmap_index = 602, .adv_w = 138, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 609, .adv_w = 155, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 620, .adv_w = 132, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 627, .adv_w = 155, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 638, .adv_w = 141, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 646, .adv_w = 87, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 655, .adv_w = 157, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 666, .adv_w = 155, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 677, .adv_w = 67, .box_w = 2, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 680, .adv_w = 69, .box_w = 4, .box_h = 15, .ofs_x = -1, .ofs_y = -3},
    {.bitmap_index = 688, .adv_w = 148, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 699, .adv_w = 67, .box_w = 2, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 702, .adv_w = 235, .box_w = 12, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 714, .adv_w = 155, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 722, .adv_w = 147, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 730, .adv_w = 155, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 741, .adv_w = 155, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 752, .adv_w = 97, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 757, .adv_w = 119, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 764, .adv_w = 97, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 772, .adv_w = 154, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 780, .adv_w = 134, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 788, .adv_w = 210, .box_w = 13, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 801, .adv_w = 133, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 809, .adv_w = 134, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 820, .adv_w = 122, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 827, .adv_w = 88, .box_w = 5, .box_h = 14, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 836, .adv_w = 69, .box_w = 2, .box_h = 14, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 840, .adv_w = 88, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 851, .adv_w = 134, .box_w = 7, .box_h = 3, .ofs_x = 1, .ofs_y = 4}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};

/*-----------------
 *    KERNING
 *----------------*/


/*Map glyph_ids to kern left classes*/
static const uint8_t kern_left_class_mapping[] =
{
    0, 0, 1, 2, 0, 3, 4, 5,
    2, 6, 7, 8, 9, 10, 9, 10,
    11, 12, 0, 13, 14, 15, 16, 17,
    18, 19, 12, 20, 20, 0, 0, 0,
    21, 22, 23, 24, 25, 22, 26, 27,
    28, 29, 29, 30, 31, 32, 29, 29,
    22, 33, 34, 35, 3, 36, 30, 37,
    37, 38, 39, 40, 41, 42, 43, 0,
    44, 0, 45, 46, 47, 48, 49, 50,
    51, 45, 52, 52, 53, 48, 45, 45,
    46, 46, 54, 55, 56, 57, 51, 58,
    58, 59, 58, 60, 41, 0, 0, 9
};

/*Map glyph_ids to kern right classes*/
static const uint8_t kern_right_class_mapping[] =
{
    0, 0, 1, 2, 0, 3, 4, 5,
    2, 6, 7, 8, 9, 10, 9, 10,
    11, 12, 13, 14, 15, 16, 17, 12,
    18, 19, 20, 21, 21, 0, 0, 0,
    22, 23, 24, 25, 23, 25, 25, 25,
    23, 25, 25, 26, 25, 25, 25, 25,
    23, 25, 23, 25, 3, 27, 28, 29,
    29, 30, 31, 32, 33, 34, 35, 0,
    36, 0, 37, 38, 39, 39, 39, 40,
    39, 38, 41, 42, 38, 38, 43, 43,
    39, 43, 39, 43, 44, 45, 46, 47,
    47, 48, 49, 50, 0, 0, 35, 9
};

/*Kern values between classes*/
static const int8_t kern_class_values[] =
{
    0, 1, 0, 0, 0, 0, 0, 2,
    0, 1, 0, 0, 2, 0, 0, 0,
    0, 1, 0, 0, 0, 0, 0, 0,
    0, 5, 4, 0, 2, 0, 4, 0,
    0, 0, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 10, 0, 5, -4, 0,
    0, 4, 0, -12, -13, 1, 10, 4,
    3, -9, 1, 9, 0, 8, 2, 6,
    0, -9, 0, 0, 3, 0, 0, 0,
    0, 0, 0, 13, 3, -1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -7, 0, 0,
    0, 0, 0, -4, 3, 4, 0, 0,
    -2, 0, -1, 2, 0, -2, 0, -2,
    -1, -4, 0, 0, 0, 0, -2, 0,
    0, -3, -3, 0, 0, -2, 0, -4,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -2, -2, -2, 0, 0, -5,
    0, -28, 0, 0, -4, -11, 4, 7,
    0, 0, -4, 2, 2, 7, 4, -3,
    4, 0, 0, -12, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, -8,
    0, 0, 2, 0, 6, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -3, -9, 0, -9, -1, 0, 0, -7,
    0, -1, 7, 0, -7, -3, -1, 1,
    0, -3, 0, 0, -2, -15, 0, 2,
    0, 6, -5, 0, -4, 0, -9, 2,
    0, -18, -3, 5, 2, 0, 0, 0,
    0, 0, 0, 0, 2, 0, -4, -1,
    -4, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 8, 0, 2, 0,
    0, -4, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -2, 0, 0, 0,
    0, 0, 0, 10, 3, 1, 0, 0,
    0, 0, 0, 23, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -8, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 2, 4,
    2, 7, -2, 0, 0, 4, -2, -8,
    -30, 1, 5, 4, 0, -3, 0, 5,
    0, 6, 0, 6, 0, -22, 0, -3,
    7, 0, 7, -2, 4, 2, 0, 0,
    1, -2, 0, 0, -3, 0, 0, 0,
    18, 0, 7, 0, 8, 4, 8, 3,
    0, 0, -3, -8, 0, 0, 0, -2,
    1, -2, 0, 1, -5, -4, -4, 1,
    0, -2, 0, 0, 0, -9, 1, -5,
    0, -4, -8, 0, -6, -5, -7, 0,
    0, -15, 0, 0, 0, 0, 2, 0,
    0, 0, 0, 0, 2, 0, -4, -6,
    -4, -3, 1, -12, 2, -15, 0, 0,
    0, -8, -2, 0, 20, -3, -4, 2,
    2, 0, 0, -4, 2, 0, 0, -13,
    -4, 7, 0, 12, -7, -2, -9, 0,
    -9, 3, 0, -21, 0, 2, 2, 0,
    -2, 0, 0, 2, 0, 0, -1, -2,
    -7, 0, -7, 0, 0, 13, -4, 0,
    -8, 0, 8, 0, -16, -21, -16, -4,
    7, 0, 0, -14, 0, 2, -6, 0,
    -3, 0, -4, -9, 0, -2, 7, 0,
    7, 0, 7, 0, 0, 3, 7, -26,
    -14, 0, -14, 0, 3, 1, -14, -14,
    -5, -14, -7, -11, -7, -14, 0, 1,
    0, 0, 0, 0, 0, 1, 1, -3,
    -4, 0, -1, -1, -2, 0, 0, -1,
    0, 0, 0, -4, 0, -2, 0, -6,
    -4, 0, -6, -8, -8, -3, 0, -4,
    0, -4, 0, 0, 0, 0, 0, -2,
    0, 0, 2, 0, 1, -2, 1, 0,
    0, 0, 0, 2, -1, 0, 0, 0,
    -1, 2, 2, 0, 0, 0, 0, -2,
    0, 0, 0, 0, 0, 0, 0, 1,
    0, 4, -1, 0, -3, 0, -4, 0,
    0, -1, 0, 7, 0, 0, -2, 0,
    0, 0, 0, 0, 0, 0, 0, -1,
    0, -1, 0, -2, 0, -2, 0, 0,
    0, 0, 0, 0, 0, 0, 0, -1,
    -1, 0, -2, -2, 0, 0, 0, 0,
    0, 0, 0, 0, -2, 0, -2, -2,
    -2, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, -2, 0, 0, 0, 0,
    -2, -4, -2, 0, 0, -7, -1, -7,
    4, 0, 0, -4, 2, 4, 5, 0,
    -6, 0, -2, 0, 0, -9, 2, -1,
    2, -13, 2, 0, 0, 1, -12, 0,
    -13, -2, -20, -1, 0, -11, 0, 4,
    6, 0, 4, 0, 0, 0, 0, 0,
    0, 0, -5, -4, -5, 0, 0, 0,
    0, -2, 0, 0, 0, -2, 0, 0,
    0, 0, 0, -1, -1, 0, -1, -3,
    0, 0, 0, 0, 0, 0, 0, -2,
    -2, 0, -2, -3, -2, 0, 0, -2,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -2, -2, -2, 0,
    0, -1, 0, -4, 2, 0, 0, -2,
    1, 2, 2, 0, 0, 0, 0, 0,
    0, -1, 0, 0, 0, 0, 0, 1,
    0, 0, -2, 0, -2, -1, -2, 0,
    0, 0, 0, 0, 0, 0, 2, 0,
    0, -2, 0, 0, 0, 0, -3, -3,
    -3, 0, 0, 7, -1, 1, -6, 0,
    0, 5, -11, -11, -9, -4, 2, 0,
    -2, -15, -4, 0, -4, 0, -4, 3,
    -4, -13, 0, -5, 0, 0, 1, 0,
    2, -1, 0, 2, 2, -7, -8, 0,
    -11, 0, 0, -5, -4, -7, -2, -5,
    1, -2, 1, -5, 0, 0, 0, -2,
    0, 0, 0, 1, 0, 2, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -2, 0, -1, 0, 0, -2, 0,
    -4, -5, -5, 0, 0, -7, 0, 0,
    0, 0, 0, 0, 0, -2, 0, 0,
    0, 0, 0, -2, 0, 0, 0, 2,
    0, 0, 0, 0, 0, 0, 0, 0,
    12, 0, 0, 0, 0, 0, 0, 1,
    0, 0, 0, -2, 0, 2, 0, 8,
    -2, 0, -6, -2, -9, 0, 0, -5,
    0, 2, 0, 0, 0, 0, 0, 0,
    0, 0, 3, 0, 1, -2, 1, 0,
    -1, 0, 0, 0, -2, 0, 0, 2,
    0, -11, -7, 0, 0, 0, -3, -11,
    0, 0, -2, 2, 0, -4, -1, -10,
    0, -7, 0, 0, -4, -5, -4, -2,
    -2, 0, 0, -3, 0, 0, -2, 0,
    0, 0, 0, 0, 0, 0, 2, 0,
    2, 0, 0, -4, 0, 0, 0, 0,
    2, 0, 1, -4, -4, 0, -2, -2,
    -3, 0, 0, 0, 0, 0, 0, -7,
    0, -2, 0, -3, -2, 0, -5, -6,
    -7, -2, 0, -4, 2, -7, 0, 0,
    0, 0, 0, 18, 0, 0, 1, 0,
    0, -4, 0, 0, 0, -9, 0, 0,
    0, 0, 0, -22, -5, 7, 7, -2,
    -10, 0, 2, -3, 0, -11, -1, -3,
    2, -16, -2, 5, 0, 3, -8, -3,
    -9, -8, -10, 0, 0, -13, 0, 12,
    0, 0, -1, 0, 0, 0, 0, -1,
    -1, -2, -6, -8, -6, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 1,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -2, 0, -1, -2, -3, 0, 0, -4,
    0, -2, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -1, 0, -4, 0, 0, 4,
    -1, 3, 0, -4, 2, -1, 0, -3,
    -2, 0, -3, -2, -2, 0, -3, -3,
    0, 0, -2, -1, -1, -3, -2, 0,
    0, -2, 0, 2, -1, 0, -4, 0,
    0, 0, 0, -4, 0, -3, 0, -3,
    0, -3, 0, 0, 0, 0, 0, 0,
    0, 0, -4, 2, 0, -2, 0, -1,
    -2, -4, -1, -1, -1, 0, -1, -2,
    0, 0, 0, 0, 0, 0, -2, -2,
    -2, 0, 0, 0, 0, 2, -1, 0,
    -1, 0, 0, 0, 0, -1, -2, -1,
    -1, -2, -1, -1, 3, 9, 0, 0,
    -5, 0, -1, 4, 0, -2, -9, -3,
    3, 1, 0, -9, -3, 2, -3, 2,
    0, 1, -2, -6, 0, -3, 1, 0,
    0, -3, 0, 0, 0, 2, 2, -4,
    -3, 0, -3, 0, 0, -3, -2, -2,
    0, -3, 1, -3, 1, -3, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 2,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, -3,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, -2, -2, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -3, 0, 0, -3, 0, 0, -2,
    -2, 0, 0, 0, 0, -2, 0, 0,
    0, 0, 0, -1, 0, 0, 0, 0,
    0, -1, 0, 0, 0, 0, -3, 0,
    -4, 0, 0, 0, -8, 0, 1, -5,
    4, 0, -1, -10, 0, 0, -5, -2,
    0, -9, -6, -7, 0, 0, -11, -2,
    -9, -9, -11, 0, -3, 0, 3, 14,
    -3, 0, -5, 0, 0, 0, -2, -3,
    -5, -4, -9, -10, -9, -6, 0, 0,
    -1, 0, 1, 0, 0, -16, 0, 7,
    4, -4, -8, 0, 1, -4, 0, -11,
    -1, -2, 4, -19, -3, 1, 0, 0,
    -15, -2, -11, -2, -16, 0, 0, -16,
    0, 11, 1, 0, -1, 0, 0, 0,
    0, 0, -1, -1, -8, -1, -8, 0,
    0, 0, 0, 0, -6, 0, -1, 0,
    -1, -6, -10, 0, 0, -1, -3, -7,
    -2, 0, -2, 0, 0, 0, 0, -10,
    -2, -7, -6, -3, -4, -6, -2, -3,
    0, -4, -1, -8, -3, 0, -2, 0,
    0, -2, -2, 0, 1, 0, -1, -7,
    -1, 0, 0, -4, 0, 0, 0, 0,
    2, 0, 1, -4, 11, 0, -2, -2,
    -3, 0, 0, 0, 0, 0, 0, -7,
    0, -2, 0, -3, -2, 0, -5, -6,
    -7, -2, 0, -4, 3, 9, 0, 0,
    0, 0, 0, 18, 0, 0, 1, 0,
    0, -4, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -1, -4, 0, 0, 0, 0,
    0, -1, 0, 0, 0, -2, -2, 0,
    0, -4, -2, 0, 0, -4, 0, 3,
    -1, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 4, 3,
    -1, 0, -6, -2, 0, 7, -8, -7,
    -4, -4, 9, 5, 2, -19, -2, 4,
    -2, 0, -2, 4, -2, -8, 0, -2,
    2, -3, -3, -7, -3, 0, 0, 7,
    4, 0, -6, 0, -12, 0, 0, 10,
    -3, -8, 1, -3, -7, -7, -7, -2,
    2, 0, -3, 0, -5, 0, 3, 7,
    -6, -9, -9, -6, 7, 0, 1, -17,
    -2, 2, -4, -2, -6, 0, -5, -9,
    -3, -3, -3, 0, 0, -6, -6, -2,
    0, 7, 6, -2, -12, 0, -12, 0,
    -1, -4, -8, -14, -1, -7, -4, -7,
    -4, -7, 0, 0, -3, 0, -4, -1,
    0, -2, -5, 0, 3, -8, 2, 0,
    0, -13, 0, -2, -5, -4, -2, -7,
    -6, -8, -5, 0, -7, -2, -6, -2,
    -7, -2, 0, 0, 1, 10, -4, 0,
    -7, 0, 0, 0, -2, -4, -6, -6,
    -7, -10, -7, -4, 4, 0, -3, 0,
    -11, -2, 2, 4, -7, -9, -4, -8,
    8, -2, 1, -22, -5, 4, -5, -4,
    -9, 0, -7, -10, -2, -2, -3, -2,
    -6, -7, -1, 0, 0, 7, 7, -1,
    -15, 0, -13, 0, -2, 8, -8, -16,
    -4, -8, -10, -11, -10, -8, 0, 0,
    0, 0, -2, 0, 0, 2, -2, 4,
    1, -3, 4, 0, 0, -4, 0, 0,
    0, 0, 1, 1, -2, 0, 0, 0,
    0, 0, 0, -2, 0, 0, 0, 0,
    3, 7, 0, 0, -2, 0, 0, 0,
    0, 0, -1, -1, -2, 0, -2, 0,
    1, 3, 0, 0, 0, 0, 3, 1,
    -1, 0, 10, 0, 5, 1, 1, -4,
    0, 4, 0, 0, 0, 3, 0, 0,
    0, 0, 4, 0, 6, 1, 7, 0,
    0, 7, 0, 7, -1, 0, 0, 0,
    0, 22, 0, 0, 0, 0, 0, 0,
    0, 0, 0, -13, 0, -2, 3, 0,
    7, -30, 0, 20, 2, -4, -4, 2,
    2, -1, 1, -11, 0, 0, 12, -13,
    -4, 7, 0, 7, -4, -2, -9, 4,
    -4, 0, 0, -16, 10, 31, 0, 0,
    0, 0, 0, 27, 0, 0, 0, 0,
    4, 0, 4, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -2, 0,
    0, -2, -1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, -1,
    4, -5, 0, 0, 1, -2, 0, 2,
    27, -4, -2, 6, 5, -5, 2, 0,
    0, 2, 2, -4, -7, 12, 7, 17,
    0, -2, -2, 10, -1, 4, 0, -29,
    7, 0, -2, 0, -5, 0, 0, 25,
    0, 2, -4, -5, -4, 8, 4, 3,
    0, 0, 0, -6, 0, 0, 0, -5,
    0, 0, 0, 0, -4, -1, 0, 0,
    0, -4, 0, -2, 0, -10, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -14, 0, 0, 0, 0, 1, 0,
    0, 0, 0, 0, 0, 0, -2, 0,
    -2, 0, 0, -3, 0, -5, 0, 0,
    0, -3, 2, -2, 0, 0, -5, -2,
    -5, 0, 0, -5, 0, -2, 0, -10,
    0, -4, 0, 0, -15, -2, -9, -4,
    -9, 0, 0, -14, 0, -5, -2, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -3, -4, -3, -2, 0, 0, 0, 0,
    -4, 0, -5, 4, -4, 4, 0, -1,
    -5, -1, -3, -2, 0, -2, -1, -1,
    2, -5, -1, 0, 0, 0, -16, -3,
    -6, 0, -9, 0, -1, -10, -1, 0,
    0, -1, -2, 0, -1, 0, 0, 0,
    2, 0, -2, -3, -2, -1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 9, 0, 0, 0, 0, 0,
    0, -4, 0, -1, 0, 0, 0, -4,
    2, 0, 0, 0, -5, -2, -4, 0,
    0, -6, 0, -2, 0, -10, 0, 0,
    0, 0, -21, 0, -4, -9, -11, 0,
    0, -14, 0, -2, -3, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -2, -3,
    -2, -1, 1, 0, 0, 3, -4, 0,
    9, 9, -2, -2, -7, 2, 9, 3,
    4, -5, 2, 8, 2, 6, 4, 5,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 14, 10, -5, -2, 2,
    -2, 2, 2, 10, 1, 0, 0, 0,
    2, 0, 2, 0, 0, 0, -2, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -1, 0, 0, 0, 0, 0, 0, 0,
    0, 6, 0, 0, 0, 0, -14, -2,
    -3, -7, -9, 0, 0, -14, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -2, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -1, 0, 0, 0, 0, 0,
    0, 0, 0, 6, 0, 0, 0, 0,
    -14, -2, -3, -7, -9, 0, 0, -4,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -3, 0, 0, 0,
    -6, 2, 0, -2, 3, 5, 2, -7,
    0, 1, -2, 2, 0, 3, 0, 0,
    0, 0, -2, 0, -1, -1, -4, 0,
    -1, -9, 0, 13, -2, 0, -5, 0,
    0, 0, -1, -4, 0, -2, -7, -4,
    -7, -3, 0, 0, -2, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -1, 0,
    0, 0, 0, 0, 0, 0, 0, 6,
    0, 0, 0, 0, -14, -2, -3, -7,
    -9, 0, 0, -14, 0, 0, 0, 0,
    0, 0, 0, 11, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -2, 0,
    -6, -2, -1, 7, -2, -2, -9, 1,
    2, 1, -1, -5, 0, 5, 0, 3,
    1, 3, -4, -10, -2, 0, -5, -2,
    -5, -9, -8, 0, -2, -4, -2, -4,
    -9, -1, -2, 0, -1, 0, -1, 0,
    3, 0, 3, -1, 3, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -1, -2, -2, 0, 0, -5,
    0, -1, 0, -3, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, -13,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -2, -2, -2, 0,
    0, 0, 0, 0, -2, 0, 0, -3,
    -2, 2, 0, -3, -3, -1, 0, -4,
    -1, -4, -2, -2, 0, -3, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -14, 0, 6, 0, 0, -4, 0,
    0, 0, 0, 0, -3, 0, -2, 0,
    -2, 0, 0, 0, -1, 0, -6, 0,
    0, 8, -4, -7, -8, 1, 4, 4,
    1, -7, 1, 3, 1, 7, 1, 8,
    -1, -6, 0, 0, -4, 0, 0, -7,
    -5, 0, 0, -4, 0, -4, -4, 0,
    -3, 0, 0, -3, 0, -2, 3, 0,
    -3, -7, -3, -2, 0, 0, -1, 0,
    -4, 0, 0, 4, -6, 0, 2, -2,
    3, 2, 0, -8, 0, -1, -1, 0,
    -2, 4, -3, 0, 0, 0, -5, -2,
    -5, 0, -7, 0, 0, -10, 0, 8,
    -2, 0, -4, 0, 0, 6, 0, -2,
    0, -2, -7, 0, -7, -2, 0, 0,
    0, 0, -1, 0, 0, 2, -3, 1,
    0, 0, -2, -1, 0, -2, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, -13,
    0, 4, 0, 0, -2, 0, 0, 0,
    0, 0, 0, 0, -2, -2, -2, 0
};


/*Collect the kern class' data in one place*/
static const lv_font_fmt_txt_kern_classes_t kern_classes =
{
    .class_pair_values   = kern_class_values,
    .left_class_mapping  = kern_left_class_mapping,
    .right_class_mapping = kern_right_class_mapping,
    .left_class_cnt      = 60,
    .right_class_cnt     = 50,
};

/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = &kern_classes,
    .kern_scale = 16,
    .cmap_num = 1,
    .bpp = 1,
    .kern_classes = 1,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t rlcd_font_bold_14 = {
#else
lv_font_t rlcd_font_bold_14 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 16,          /*The maximum line height required by the font*/
    .base_line = 3,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if RLCD_FONT_BOLD_14*/

