/* ****************************************************************************
   Size: 12 px
   Bpp: 1
   Opts: --bpp 1 --size 12 --no-compress --stride 1 --align 1 --font MiSansLatin-Medium.ttf --symbols aăâeêoôơuưyáàảãạắằẳẵặấầẩẫậéèẻẽẹếềểễệíìỉĩịóòỏõọốồổỗộớờởỡợúùủũụứừửữựýỳỷỹỵđÁÀẢÃẠẮẰẲẴẶẤẦẨẪẬÉÈẺẼẸẾỀỂỄỆÍÌỈĨỊÓÒỎÕỌỐỒỔỖỘỚỜỞỠỢÚÙỦŨỤỨỪỬỮỰÝỲỶỸỴĐ --range 32-127 --format lvgl -o font_12.c
   **************************************************************************** */

#ifdef __has_include
    #if __has_include("lvgl.h")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif



#ifndef FONT_12
#define FONT_12 1
#endif

#if FONT_12

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xfc, 0x80,

    /* U+0022 "\"" */
    0xf7, 0x0,

    /* U+0023 "#" */
    0x24, 0x4b, 0xf9, 0x24, 0x9f, 0xd2, 0x24, 0x48,

    /* U+0024 "$" */
    0x10, 0xfb, 0x5c, 0x8d, 0xf, 0x7, 0x89, 0x92,
    0xf8, 0x40,

    /* U+0025 "%" */
    0x61, 0xa4, 0xc9, 0x21, 0x90, 0xc, 0x6, 0xe1,
    0x44, 0x91, 0x43, 0x80,

    /* U+0026 "&" */
    0x38, 0x44, 0x44, 0x78, 0x70, 0x9a, 0x8e, 0x86,
    0x7b,

    /* U+0027 "'" */
    0xe0,

    /* U+0028 "(" */
    0x5a, 0xaa, 0x94,

    /* U+0029 ")" */
    0xa5, 0x55, 0x68,

    /* U+002A "*" */
    0x5f, 0xa0,

    /* U+002B "+" */
    0x20, 0x82, 0x3f, 0x20, 0x80,

    /* U+002C "," */
    0xa8,

    /* U+002D "-" */
    0xf8,

    /* U+002E "." */
    0xc0,

    /* U+002F "/" */
    0x11, 0x32, 0x22, 0x44, 0x40,

    /* U+0030 "0" */
    0x79, 0x28, 0x61, 0x86, 0x18, 0x52, 0x78,

    /* U+0031 "1" */
    0x7c, 0x92, 0x49, 0x20,

    /* U+0032 "2" */
    0x74, 0x42, 0x11, 0x19, 0x88, 0xf8,

    /* U+0033 "3" */
    0xf8, 0x42, 0x1c, 0x8, 0x10, 0x73, 0x78,

    /* U+0034 "4" */
    0x18, 0x62, 0x8a, 0x4b, 0x2f, 0xc2, 0x8,

    /* U+0035 "5" */
    0x7d, 0x4, 0x1e, 0x4c, 0x10, 0x73, 0x78,

    /* U+0036 "6" */
    0x10, 0x86, 0x1e, 0xce, 0x18, 0x73, 0x78,

    /* U+0037 "7" */
    0xfc, 0x30, 0x86, 0x10, 0xc2, 0x18, 0x40,

    /* U+0038 "8" */
    0x7a, 0x18, 0x5e, 0xce, 0x18, 0x73, 0x78,

    /* U+0039 "9" */
    0x7a, 0x38, 0x63, 0x7c, 0x21, 0x8c, 0x20,

    /* U+003A ":" */
    0x82,

    /* U+003B ";" */
    0xc3, 0x80,

    /* U+003C "<" */
    0xc, 0xce, 0x30, 0x60, 0x60, 0x40,

    /* U+003D "=" */
    0xfc, 0x0, 0x3f,

    /* U+003E ">" */
    0xc1, 0x81, 0x83, 0x33, 0x88, 0x0,

    /* U+003F "?" */
    0x76, 0x42, 0x33, 0x10, 0x80, 0x20,

    /* U+0040 "@" */
    0x3e, 0x30, 0xb7, 0xb4, 0x5a, 0x2c, 0xeb, 0x0,
    0xc0, 0x3e, 0x0,

    /* U+0041 "A" */
    0x18, 0x18, 0x1c, 0x24, 0x24, 0x7e, 0x42, 0x42,
    0xc1,

    /* U+0042 "B" */
    0xfa, 0x18, 0x61, 0xfa, 0x18, 0x61, 0xf8,

    /* U+0043 "C" */
    0x3c, 0x8e, 0x4, 0x8, 0x10, 0x20, 0x23, 0x3c,

    /* U+0044 "D" */
    0xf9, 0xa, 0xc, 0x18, 0x30, 0x60, 0xc2, 0xf8,

    /* U+0045 "E" */
    0xfa, 0x8, 0x20, 0xfa, 0x8, 0x20, 0xfc,

    /* U+0046 "F" */
    0xfc, 0x21, 0xf, 0xc2, 0x10, 0x80,

    /* U+0047 "G" */
    0x3c, 0x8e, 0x4, 0x8, 0xf0, 0x60, 0xa3, 0x3c,

    /* U+0048 "H" */
    0x83, 0x6, 0xc, 0x1f, 0xf0, 0x60, 0xc1, 0x82,

    /* U+0049 "I" */
    0xff, 0x80,

    /* U+004A "J" */
    0x8, 0x42, 0x10, 0x84, 0x29, 0x70,

    /* U+004B "K" */
    0x8d, 0x12, 0x45, 0xf, 0x12, 0x26, 0x46, 0x84,

    /* U+004C "L" */
    0x84, 0x21, 0x8, 0x42, 0x10, 0xf8,

    /* U+004D "M" */
    0x80, 0xe0, 0xf8, 0xf4, 0x5b, 0x4c, 0xe6, 0x23,
    0x1, 0x80, 0x80,

    /* U+004E "N" */
    0x83, 0x87, 0x8d, 0x99, 0x33, 0x63, 0xc3, 0x82,

    /* U+004F "O" */
    0x3c, 0x42, 0x81, 0x81, 0x81, 0x81, 0x81, 0x42,
    0x3c,

    /* U+0050 "P" */
    0xfa, 0x18, 0x61, 0xfa, 0x8, 0x20, 0x80,

    /* U+0051 "Q" */
    0x3c, 0x42, 0x81, 0x81, 0x81, 0x81, 0x85, 0x46,
    0x3f, 0x1,

    /* U+0052 "R" */
    0xfa, 0x18, 0x61, 0xfa, 0x68, 0xa3, 0x84,

    /* U+0053 "S" */
    0x7b, 0x38, 0x30, 0x78, 0x30, 0x61, 0x78,

    /* U+0054 "T" */
    0xfe, 0x20, 0x40, 0x81, 0x2, 0x4, 0x8, 0x10,

    /* U+0055 "U" */
    0x83, 0x6, 0xc, 0x18, 0x30, 0x60, 0xa3, 0x78,

    /* U+0056 "V" */
    0xc1, 0x42, 0x42, 0x66, 0x24, 0x24, 0x1c, 0x18,
    0x18,

    /* U+0057 "W" */
    0xc6, 0x24, 0x62, 0x46, 0x24, 0xb6, 0x69, 0x42,
    0x94, 0x39, 0x43, 0xc, 0x10, 0x80,

    /* U+0058 "X" */
    0x42, 0x64, 0x2c, 0x18, 0x18, 0x38, 0x24, 0x66,
    0x42,

    /* U+0059 "Y" */
    0xc6, 0x89, 0xb1, 0x43, 0x82, 0x4, 0x8, 0x10,

    /* U+005A "Z" */
    0xfc, 0x31, 0x84, 0x30, 0x84, 0x30, 0xfc,

    /* U+005B "[" */
    0xf2, 0x49, 0x24, 0x93, 0x80,

    /* U+005C "\\" */
    0x84, 0x44, 0x62, 0x23, 0x10,

    /* U+005D "]" */
    0xe4, 0x92, 0x49, 0x27, 0x80,

    /* U+005E "^" */
    0x21, 0x94, 0x90,

    /* U+005F "_" */
    0xf8,

    /* U+0060 "`" */
    0x10,

    /* U+0061 "a" */
    0x70, 0x42, 0xf8, 0xc5, 0xe0,

    /* U+0062 "b" */
    0x82, 0xb, 0xb3, 0x86, 0x18, 0x73, 0xb8,

    /* U+0063 "c" */
    0x76, 0x61, 0x8, 0x65, 0xe0,

    /* U+0064 "d" */
    0x4, 0x17, 0x73, 0x86, 0x18, 0x53, 0x74,

    /* U+0065 "e" */
    0x7b, 0x38, 0x7f, 0x83, 0x17, 0x80,

    /* U+0066 "f" */
    0x74, 0xf4, 0x44, 0x44, 0x40,

    /* U+0067 "g" */
    0x77, 0x38, 0x61, 0x87, 0x37, 0x41, 0xcd, 0xe0,

    /* U+0068 "h" */
    0x84, 0x2d, 0x98, 0xc6, 0x31, 0x88,

    /* U+0069 "i" */
    0xbf, 0x80,

    /* U+006A "j" */
    0x45, 0x55, 0x57,

    /* U+006B "k" */
    0x84, 0x27, 0x6e, 0x72, 0xd2, 0x88,

    /* U+006C "l" */
    0xaa, 0xaa, 0xc0,

    /* U+006D "m" */
    0xb7, 0x64, 0x62, 0x31, 0x18, 0x8c, 0x46, 0x22,

    /* U+006E "n" */
    0xb6, 0x63, 0x18, 0xc6, 0x20,

    /* U+006F "o" */
    0x7b, 0x38, 0x61, 0x87, 0x37, 0x80,

    /* U+0070 "p" */
    0xbb, 0x38, 0x61, 0x87, 0x3f, 0xa0, 0x82, 0x0,

    /* U+0071 "q" */
    0x77, 0x38, 0x61, 0x87, 0x37, 0x41, 0x4, 0x10,

    /* U+0072 "r" */
    0xbc, 0x88, 0x88, 0x80,

    /* U+0073 "s" */
    0x74, 0x60, 0xe0, 0xc5, 0xc0,

    /* U+0074 "t" */
    0x44, 0xf4, 0x44, 0x44, 0x30,

    /* U+0075 "u" */
    0x8c, 0x63, 0x18, 0xc5, 0xe0,

    /* U+0076 "v" */
    0x85, 0x34, 0x92, 0x38, 0xc3, 0x0,

    /* U+0077 "w" */
    0x8c, 0xb3, 0x24, 0xc9, 0x56, 0x53, 0xc, 0xc2,
    0x30,

    /* U+0078 "x" */
    0x4d, 0x23, 0xc, 0x31, 0x2c, 0xc0,

    /* U+0079 "y" */
    0x85, 0x34, 0x92, 0x28, 0xc3, 0xc, 0x20, 0x80,

    /* U+007A "z" */
    0xf8, 0xcc, 0x46, 0x63, 0xe0,

    /* U+007B "{" */
    0x19, 0x8, 0x42, 0x60, 0x84, 0x21, 0x6,

    /* U+007C "|" */
    0xff, 0xe0,

    /* U+007D "}" */
    0xc1, 0x8, 0x42, 0xc, 0x84, 0x21, 0x30,

    /* U+007E "~" */
    0xcd, 0x80,

    /* U+007F "" */
    0x0,

    /* U+00C0 "À" */
    0x10, 0x18, 0x0, 0x18, 0x18, 0x1c, 0x24, 0x24,
    0x7e, 0x42, 0x42, 0xc1,

    /* U+00C1 "Á" */
    0x8, 0x8, 0x0, 0x18, 0x18, 0x1c, 0x24, 0x24,
    0x7e, 0x42, 0x42, 0xc1,

    /* U+00C3 "Ã" */
    0x14, 0x2c, 0x0, 0x18, 0x18, 0x1c, 0x24, 0x24,
    0x7e, 0x42, 0x42, 0xc1,

    /* U+00C8 "È" */
    0x0, 0x80, 0x3e, 0x82, 0x8, 0x3e, 0x82, 0x8,
    0x3f,

    /* U+00C9 "É" */
    0x0, 0xc0, 0x3e, 0x82, 0x8, 0x3e, 0x82, 0x8,
    0x3f,

    /* U+00CC "Ì" */
    0x11, 0x55, 0x55,

    /* U+00CD "Í" */
    0x22, 0xaa, 0xaa,

    /* U+00D2 "Ò" */
    0x10, 0x10, 0x8, 0x3c, 0x42, 0x81, 0x81, 0x81,
    0x81, 0x81, 0x42, 0x3c,

    /* U+00D3 "Ó" */
    0x8, 0x8, 0x10, 0x3c, 0x42, 0x81, 0x81, 0x81,
    0x81, 0x81, 0x42, 0x3c,

    /* U+00D5 "Õ" */
    0x34, 0x2c, 0x0, 0x3c, 0x42, 0x81, 0x81, 0x81,
    0x81, 0x81, 0x42, 0x3c,

    /* U+00D9 "Ù" */
    0x0, 0x20, 0x4, 0x18, 0x30, 0x60, 0xc1, 0x83,
    0x5, 0x1b, 0xc0,

    /* U+00DA "Ú" */
    0x0, 0x30, 0x4, 0x18, 0x30, 0x60, 0xc1, 0x83,
    0x5, 0x1b, 0xc0,

    /* U+00DD "Ý" */
    0x0, 0x20, 0x6, 0x34, 0x4d, 0x8a, 0x1c, 0x10,
    0x20, 0x40, 0x80,

    /* U+00E0 "à" */
    0x41, 0x0, 0xe0, 0x85, 0xf1, 0x8b, 0xc0,

    /* U+00E1 "á" */
    0x11, 0x88, 0xe0, 0x85, 0xf1, 0x8b, 0xc0,

    /* U+00E2 "â" */
    0x32, 0x9c, 0x10, 0xbe, 0x31, 0x78,

    /* U+00E3 "ã" */
    0x72, 0x9c, 0x10, 0xbe, 0x31, 0x78,

    /* U+00E8 "è" */
    0x20, 0x80, 0x1e, 0xce, 0x1f, 0xe0, 0xc5, 0xe0,

    /* U+00E9 "é" */
    0x10, 0x40, 0x1e, 0xce, 0x1f, 0xe0, 0xc5, 0xe0,

    /* U+00EA "ê" */
    0x31, 0x27, 0xb3, 0x87, 0xf8, 0x31, 0x78,

    /* U+00EC "ì" */
    0x91, 0x55, 0x50,

    /* U+00ED "í" */
    0x62, 0xaa, 0xa0,

    /* U+00F2 "ò" */
    0x20, 0x80, 0x1e, 0xce, 0x18, 0x61, 0xcd, 0xe0,

    /* U+00F3 "ó" */
    0x10, 0x40, 0x1e, 0xce, 0x18, 0x61, 0xcd, 0xe0,

    /* U+00F4 "ô" */
    0x31, 0x27, 0xb3, 0x86, 0x18, 0x73, 0x78,

    /* U+00F5 "õ" */
    0x78, 0x7, 0xb3, 0x86, 0x18, 0x73, 0x78,

    /* U+00F9 "ù" */
    0x41, 0x1, 0x18, 0xc6, 0x31, 0x8b, 0xc0,

    /* U+00FA "ú" */
    0x11, 0x1, 0x18, 0xc6, 0x31, 0x8b, 0xc0,

    /* U+00FD "ý" */
    0x18, 0x40, 0x21, 0x4d, 0x24, 0x8a, 0x30, 0xc3,
    0x8, 0x20,

    /* U+0103 "ă" */
    0x53, 0x9c, 0x10, 0xbe, 0x31, 0x78,

    /* U+0110 "Đ" */
    0x7c, 0x42, 0x41, 0xf9, 0x41, 0x41, 0x41, 0x42,
    0x7c,

    /* U+0111 "đ" */
    0x1e, 0x9, 0xf6, 0x68, 0x50, 0xa1, 0x26, 0x74,

    /* U+0128 "Ĩ" */
    0xfc, 0x24, 0x92, 0x49, 0x20,

    /* U+0129 "ĩ" */
    0xfd, 0x24, 0x92, 0x40,

    /* U+0168 "Ũ" */
    0x34, 0xb0, 0x4, 0x18, 0x30, 0x60, 0xc1, 0x83,
    0x5, 0x1b, 0xc0,

    /* U+0169 "ũ" */
    0x73, 0xa3, 0x18, 0xc6, 0x31, 0x78,

    /* U+01A1 "ơ" */
    0x4, 0x17, 0xf3, 0x86, 0x18, 0x73, 0x78,

    /* U+01B0 "ư" */
    0x6, 0x6, 0x34, 0x48, 0x91, 0x22, 0x44, 0x78,

    /* U+1EA0 "Ạ" */
    0x10, 0x20, 0xa1, 0x46, 0xcf, 0x91, 0x63, 0x82,
    0x0, 0x0, 0x80,

    /* U+1EA1 "ạ" */
    0x70, 0x42, 0xf8, 0xc5, 0xe0, 0x1, 0x0,

    /* U+1EA2 "Ả" */
    0x18, 0x8, 0x0, 0x18, 0x18, 0x28, 0x2c, 0x64,
    0x7c, 0x42, 0xc2, 0x83,

    /* U+1EA3 "ả" */
    0x71, 0x1c, 0x10, 0xbe, 0x31, 0x78,

    /* U+1EA4 "Ấ" */
    0x6, 0x8, 0x18, 0x0, 0x18, 0x18, 0x1c, 0x24,
    0x24, 0x7e, 0x42, 0x42, 0xc1,

    /* U+1EA5 "ấ" */
    0xc, 0x23, 0x14, 0x70, 0x20, 0x9e, 0x8a, 0x27,
    0x80,

    /* U+1EA6 "Ầ" */
    0x8, 0xc, 0x18, 0x0, 0x18, 0x18, 0x1c, 0x24,
    0x24, 0x7e, 0x42, 0x42, 0xc1,

    /* U+1EA7 "ầ" */
    0x10, 0x4c, 0xa7, 0x4, 0x2f, 0x8c, 0x5e,

    /* U+1EA8 "Ẩ" */
    0x6, 0x2, 0x8, 0x14, 0x0, 0x8, 0x1c, 0x14,
    0x34, 0x26, 0x3e, 0x62, 0x43, 0x41,

    /* U+1EA9 "ẩ" */
    0x18, 0x23, 0x14, 0x70, 0x20, 0x9e, 0x8a, 0x27,
    0x80,

    /* U+1EAA "Ẫ" */
    0x14, 0x28, 0x18, 0x18, 0x0, 0x18, 0x18, 0x1c,
    0x24, 0x24, 0x7e, 0x42, 0x42, 0xc1,

    /* U+1EAB "ẫ" */
    0x70, 0xc, 0xa7, 0x4, 0x2f, 0x8c, 0x5e,

    /* U+1EAC "Ậ" */
    0x10, 0x50, 0x40, 0x82, 0x85, 0x1b, 0x3e, 0x45,
    0x8e, 0x8, 0x0, 0x2, 0x0,

    /* U+1EAD "ậ" */
    0x22, 0x9c, 0x10, 0xbe, 0x31, 0x78, 0x0, 0x40,

    /* U+1EAE "Ắ" */
    0x8, 0x24, 0x38, 0x0, 0x18, 0x18, 0x28, 0x2c,
    0x64, 0x7c, 0x42, 0xc2, 0x83,

    /* U+1EAF "ắ" */
    0x0, 0x88, 0xa7, 0x38, 0x21, 0x7c, 0x62, 0xf0,

    /* U+1EB0 "Ằ" */
    0x30, 0x24, 0x38, 0x0, 0x18, 0x18, 0x28, 0x2c,
    0x64, 0x7c, 0x42, 0xc2, 0x83,

    /* U+1EB1 "ằ" */
    0x3, 0x8, 0xa7, 0x38, 0x21, 0x7c, 0x62, 0xf0,

    /* U+1EB2 "Ẳ" */
    0x18, 0x18, 0x28, 0x38, 0x0, 0x18, 0x18, 0x28,
    0x2c, 0x64, 0x7c, 0x42, 0xc2, 0x83,

    /* U+1EB3 "ẳ" */
    0x70, 0x88, 0xa7, 0x38, 0x21, 0x7c, 0x62, 0xf0,

    /* U+1EB4 "Ẵ" */
    0x34, 0x28, 0x20, 0x38, 0x0, 0x18, 0x18, 0x28,
    0x2c, 0x64, 0x7c, 0x42, 0xc2, 0x83,

    /* U+1EB5 "ẵ" */
    0x6a, 0x80, 0xa7, 0x38, 0x21, 0x7c, 0x62, 0xf0,

    /* U+1EB6 "Ặ" */
    0x2c, 0x28, 0x18, 0x10, 0x18, 0x28, 0x28, 0x64,
    0x7c, 0x42, 0xc2, 0x83, 0x0, 0x0, 0x10,

    /* U+1EB7 "ặ" */
    0x53, 0x9c, 0x10, 0xbe, 0x31, 0x78, 0x0, 0x40,

    /* U+1EB8 "Ẹ" */
    0xfe, 0x8, 0x20, 0xfe, 0x8, 0x20, 0xfc, 0x0,
    0x4,

    /* U+1EB9 "ẹ" */
    0x3c, 0x8e, 0xf, 0xf8, 0x8, 0x4f, 0x0, 0x0,
    0x20,

    /* U+1EBA "Ẻ" */
    0x30, 0xc0, 0x3f, 0x82, 0x8, 0x3e, 0x82, 0x8,
    0x3f,

    /* U+1EBB "ẻ" */
    0x30, 0x47, 0xb3, 0x87, 0xf8, 0x31, 0x78,

    /* U+1EBC "Ẽ" */
    0x69, 0x40, 0x3e, 0x82, 0x8, 0x3e, 0x82, 0x8,
    0x3f,

    /* U+1EBD "ẽ" */
    0x78, 0x7, 0xb3, 0x87, 0xf8, 0x31, 0x78,

    /* U+1EBE "Ế" */
    0x8, 0x87, 0x0, 0xfa, 0x8, 0x20, 0xfa, 0x8,
    0x20, 0xfc,

    /* U+1EBF "ế" */
    0x4, 0x33, 0x12, 0x7b, 0x38, 0x7f, 0x83, 0x17,
    0x80,

    /* U+1EC0 "Ề" */
    0x10, 0x87, 0x10, 0xfa, 0x8, 0x20, 0xfa, 0x8,
    0x20, 0xfc,

    /* U+1EC1 "ề" */
    0x8, 0x23, 0x12, 0x7b, 0x38, 0x7f, 0x83, 0x17,
    0x80,

    /* U+1EC2 "Ể" */
    0x18, 0x23, 0x1c, 0x3, 0xf8, 0x20, 0x83, 0xe8,
    0x20, 0x83, 0xf0,

    /* U+1EC3 "ể" */
    0xc, 0x13, 0x12, 0x7b, 0x38, 0x7f, 0x83, 0x17,
    0x80,

    /* U+1EC4 "Ễ" */
    0x71, 0x42, 0x1c, 0x3, 0xe8, 0x20, 0x83, 0xe8,
    0x20, 0x83, 0xf0,

    /* U+1EC5 "ễ" */
    0x69, 0x60, 0xc, 0x49, 0xec, 0xe1, 0xfe, 0xc,
    0x5e,

    /* U+1EC6 "Ệ" */
    0x31, 0xf, 0xe0, 0x82, 0xf, 0xe0, 0x82, 0xf,
    0xc0, 0x0, 0x40,

    /* U+1EC7 "ệ" */
    0x38, 0x88, 0xf2, 0x38, 0x3f, 0xe0, 0x21, 0x3c,
    0x0, 0x0, 0x80,

    /* U+1EC8 "Ỉ" */
    0xec, 0x24, 0x92, 0x49, 0x20,

    /* U+1EC9 "ỉ" */
    0xe9, 0x24, 0x92, 0x40,

    /* U+1ECA "Ị" */
    0xff, 0x90,

    /* U+1ECB "ị" */
    0xbf, 0x90,

    /* U+1ECC "Ọ" */
    0x3c, 0x42, 0x81, 0x81, 0x81, 0x81, 0x81, 0x42,
    0x3c, 0x0, 0x0, 0x8,

    /* U+1ECD "ọ" */
    0x7b, 0x38, 0x61, 0x87, 0x37, 0x80, 0x0, 0x80,

    /* U+1ECE "Ỏ" */
    0x18, 0x8, 0x0, 0x3c, 0x42, 0x81, 0x81, 0x81,
    0x81, 0x81, 0x42, 0x3c,

    /* U+1ECF "ỏ" */
    0x30, 0x47, 0xb3, 0x86, 0x18, 0x73, 0x78,

    /* U+1ED0 "Ố" */
    0x2, 0x4, 0x18, 0x18, 0x0, 0x3c, 0x42, 0x81,
    0x81, 0x81, 0x81, 0x81, 0x42, 0x3c,

    /* U+1ED1 "ố" */
    0x4, 0x23, 0x12, 0x7b, 0x38, 0x61, 0x87, 0x37,
    0x80,

    /* U+1ED2 "Ồ" */
    0x8, 0x4, 0x18, 0x1c, 0x0, 0x3c, 0x42, 0x81,
    0x81, 0x81, 0x81, 0x81, 0x42, 0x3c,

    /* U+1ED3 "ồ" */
    0x8, 0x23, 0x12, 0x7b, 0x38, 0x61, 0x87, 0x37,
    0x80,

    /* U+1ED4 "Ổ" */
    0xe, 0x6, 0x18, 0x18, 0x0, 0x3c, 0x42, 0x81,
    0x81, 0x81, 0x81, 0x81, 0x42, 0x3c,

    /* U+1ED5 "ổ" */
    0x1c, 0x13, 0x12, 0x7b, 0x38, 0x61, 0x87, 0x37,
    0x80,

    /* U+1ED6 "Ỗ" */
    0x34, 0x2c, 0x18, 0x18, 0x0, 0x3c, 0x42, 0x81,
    0x81, 0x81, 0x81, 0x81, 0x42, 0x3c,

    /* U+1ED7 "ỗ" */
    0x69, 0x60, 0xc, 0x49, 0xec, 0xe1, 0x86, 0x1c,
    0xde,

    /* U+1ED8 "Ộ" */
    0x8, 0x14, 0x3c, 0x42, 0x81, 0x81, 0x81, 0x81,
    0x81, 0x42, 0x3c, 0x0, 0x0, 0x8,

    /* U+1ED9 "ộ" */
    0x31, 0x47, 0xb3, 0x86, 0x18, 0x73, 0x78, 0x0,
    0x8,

    /* U+1EDA "Ớ" */
    0x0, 0x9, 0x11, 0x3e, 0x42, 0x81, 0x81, 0x81,
    0x81, 0x81, 0x42, 0x3c,

    /* U+1EDB "ớ" */
    0x10, 0x50, 0x5f, 0xce, 0x18, 0x61, 0xcd, 0xe0,

    /* U+1EDC "Ờ" */
    0x10, 0x11, 0x1, 0x3e, 0x42, 0x81, 0x81, 0x81,
    0x81, 0x81, 0x42, 0x3c,

    /* U+1EDD "ờ" */
    0x20, 0x90, 0x5f, 0xce, 0x18, 0x61, 0xcd, 0xe0,

    /* U+1EDE "Ở" */
    0x19, 0x9, 0x1, 0x3e, 0x42, 0x81, 0x81, 0x81,
    0x81, 0x81, 0x42, 0x3c,

    /* U+1EDF "ở" */
    0x30, 0x57, 0xf3, 0x86, 0x18, 0x73, 0x78,

    /* U+1EE0 "Ỡ" */
    0x35, 0x29, 0x1, 0x3e, 0x42, 0x81, 0x81, 0x81,
    0x81, 0x81, 0x42, 0x3c,

    /* U+1EE1 "ỡ" */
    0x7c, 0x17, 0xf3, 0x86, 0x18, 0x73, 0x78,

    /* U+1EE2 "Ợ" */
    0x1, 0x1, 0x3e, 0x42, 0x81, 0x81, 0x81, 0x81,
    0x81, 0x42, 0x3c, 0x0, 0x0, 0x8,

    /* U+1EE3 "ợ" */
    0x4, 0x17, 0xf3, 0x86, 0x18, 0x73, 0x78, 0x0,
    0x8,

    /* U+1EE4 "Ụ" */
    0x83, 0x6, 0xc, 0x18, 0x30, 0x60, 0xa3, 0x78,
    0x0, 0x0, 0x80,

    /* U+1EE5 "ụ" */
    0x8c, 0x63, 0x18, 0xc5, 0xe0, 0x1, 0x0,

    /* U+1EE6 "Ủ" */
    0x38, 0x30, 0x4, 0x18, 0x30, 0x60, 0xc1, 0x83,
    0x5, 0x1b, 0xc0,

    /* U+1EE7 "ủ" */
    0x71, 0x23, 0x18, 0xc6, 0x31, 0x78,

    /* U+1EE8 "Ứ" */
    0x0, 0x19, 0x1, 0x83, 0x82, 0x82, 0x82, 0x82,
    0x82, 0x82, 0x46, 0x78,

    /* U+1EE9 "ứ" */
    0x10, 0x4c, 0xc, 0x68, 0x91, 0x22, 0x44, 0x88,
    0xf0,

    /* U+1EEA "Ừ" */
    0x0, 0x31, 0x1, 0x83, 0x82, 0x82, 0x82, 0x82,
    0x82, 0x82, 0x46, 0x78,

    /* U+1EEB "ừ" */
    0x40, 0x4c, 0xc, 0x68, 0x91, 0x22, 0x44, 0x88,
    0xf0,

    /* U+1EEC "Ử" */
    0x39, 0x19, 0x1, 0x83, 0x82, 0x82, 0x82, 0x82,
    0x82, 0x82, 0x46, 0x78,

    /* U+1EED "ử" */
    0x70, 0x46, 0x3c, 0x48, 0x91, 0x22, 0x44, 0x78,

    /* U+1EEE "Ữ" */
    0x35, 0x59, 0x1, 0x83, 0x82, 0x82, 0x82, 0x82,
    0x82, 0x82, 0x46, 0x78,

    /* U+1EEF "ữ" */
    0x72, 0xe6, 0x34, 0x48, 0x91, 0x22, 0x44, 0x78,

    /* U+1EF0 "Ự" */
    0x1, 0x1, 0x83, 0x82, 0x82, 0x82, 0x82, 0x82,
    0x82, 0x46, 0x78, 0x0, 0x0, 0x10,

    /* U+1EF1 "ự" */
    0x6, 0x6, 0x34, 0x48, 0x91, 0x22, 0x44, 0x78,
    0x0, 0x1, 0x0,

    /* U+1EF2 "Ỳ" */
    0x0, 0x20, 0x6, 0x34, 0x4d, 0x8a, 0x1c, 0x10,
    0x20, 0x40, 0x80,

    /* U+1EF3 "ỳ" */
    0x20, 0xc0, 0x21, 0x4d, 0x24, 0x8a, 0x30, 0xc3,
    0x8, 0x20,

    /* U+1EF4 "Ỵ" */
    0xc6, 0x89, 0xb1, 0x43, 0x82, 0x4, 0x8, 0x10,
    0x0, 0x0, 0x80,

    /* U+1EF5 "ỵ" */
    0x8c, 0x72, 0xa5, 0x38, 0x84, 0x2a, 0x40,

    /* U+1EF6 "Ỷ" */
    0x38, 0x30, 0x6, 0x14, 0x6c, 0x8a, 0x1c, 0x10,
    0x20, 0x40, 0x80,

    /* U+1EF7 "ỷ" */
    0x30, 0x88, 0xe2, 0x49, 0x65, 0xc, 0x30, 0x82,
    0x18,

    /* U+1EF8 "Ỹ" */
    0x38, 0x70, 0x6, 0x34, 0x4d, 0x8a, 0x1c, 0x10,
    0x20, 0x40, 0x80,

    /* U+1EF9 "ỹ" */
    0x29, 0x68, 0x53, 0x49, 0x22, 0x8c, 0x30, 0xc2,
    0x8
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 55, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 56, .box_w = 1, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 3, .adv_w = 68, .box_w = 3, .box_h = 3, .ofs_x = 1, .ofs_y = 7},
    {.bitmap_index = 5, .adv_w = 136, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 13, .adv_w = 112, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 23, .adv_w = 159, .box_w = 10, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 35, .adv_w = 138, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 44, .adv_w = 37, .box_w = 1, .box_h = 3, .ofs_x = 1, .ofs_y = 7},
    {.bitmap_index = 45, .adv_w = 61, .box_w = 2, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 48, .adv_w = 61, .box_w = 2, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 51, .adv_w = 85, .box_w = 3, .box_h = 4, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 53, .adv_w = 118, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 58, .adv_w = 53, .box_w = 2, .box_h = 3, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 59, .adv_w = 87, .box_w = 5, .box_h = 1, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 60, .adv_w = 52, .box_w = 1, .box_h = 2, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 61, .adv_w = 78, .box_w = 4, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 66, .adv_w = 122, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 73, .adv_w = 80, .box_w = 3, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 77, .adv_w = 110, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 83, .adv_w = 114, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 90, .adv_w = 114, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 97, .adv_w = 116, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 104, .adv_w = 117, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 111, .adv_w = 102, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 118, .adv_w = 121, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 125, .adv_w = 116, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 132, .adv_w = 52, .box_w = 1, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 133, .adv_w = 55, .box_w = 1, .box_h = 9, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 135, .adv_w = 118, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 141, .adv_w = 118, .box_w = 6, .box_h = 4, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 144, .adv_w = 118, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 150, .adv_w = 95, .box_w = 5, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 156, .adv_w = 159, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 167, .adv_w = 130, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 176, .adv_w = 127, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 183, .adv_w = 133, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 191, .adv_w = 142, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 199, .adv_w = 113, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 206, .adv_w = 108, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 212, .adv_w = 141, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 220, .adv_w = 140, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 228, .adv_w = 48, .box_w = 1, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 230, .adv_w = 93, .box_w = 5, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 236, .adv_w = 125, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 244, .adv_w = 109, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 250, .adv_w = 170, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 261, .adv_w = 142, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 269, .adv_w = 149, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 278, .adv_w = 121, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 285, .adv_w = 150, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 295, .adv_w = 123, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 302, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 309, .adv_w = 117, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 317, .adv_w = 136, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 325, .adv_w = 131, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 334, .adv_w = 189, .box_w = 12, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 348, .adv_w = 125, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 357, .adv_w = 121, .box_w = 7, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 365, .adv_w = 116, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 372, .adv_w = 67, .box_w = 3, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 377, .adv_w = 66, .box_w = 4, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 382, .adv_w = 67, .box_w = 3, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 387, .adv_w = 86, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 7},
    {.bitmap_index = 390, .adv_w = 88, .box_w = 5, .box_h = 1, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 391, .adv_w = 51, .box_w = 2, .box_h = 3, .ofs_x = 0, .ofs_y = 7},
    {.bitmap_index = 392, .adv_w = 104, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 397, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 404, .adv_w = 102, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 409, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 416, .adv_w = 107, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 422, .adv_w = 67, .box_w = 4, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 427, .adv_w = 114, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 435, .adv_w = 112, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 441, .adv_w = 48, .box_w = 1, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 443, .adv_w = 48, .box_w = 2, .box_h = 12, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 446, .adv_w = 99, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 452, .adv_w = 49, .box_w = 2, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 455, .adv_w = 170, .box_w = 9, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 463, .adv_w = 112, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 468, .adv_w = 110, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 474, .adv_w = 115, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 482, .adv_w = 115, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 490, .adv_w = 74, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 494, .adv_w = 91, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 499, .adv_w = 70, .box_w = 4, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 504, .adv_w = 112, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 509, .adv_w = 99, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 515, .adv_w = 154, .box_w = 10, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 524, .adv_w = 96, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 530, .adv_w = 100, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 538, .adv_w = 92, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 543, .adv_w = 68, .box_w = 5, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 550, .adv_w = 44, .box_w = 1, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 552, .adv_w = 68, .box_w = 5, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 559, .adv_w = 102, .box_w = 5, .box_h = 2, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 561, .adv_w = 0, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 562, .adv_w = 130, .box_w = 8, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 574, .adv_w = 130, .box_w = 8, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 586, .adv_w = 130, .box_w = 8, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 598, .adv_w = 113, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 607, .adv_w = 113, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 616, .adv_w = 48, .box_w = 2, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 619, .adv_w = 48, .box_w = 2, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 622, .adv_w = 149, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 634, .adv_w = 149, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 646, .adv_w = 149, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 658, .adv_w = 136, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 669, .adv_w = 136, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 680, .adv_w = 121, .box_w = 7, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 691, .adv_w = 104, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 698, .adv_w = 104, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 705, .adv_w = 104, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 711, .adv_w = 104, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 717, .adv_w = 107, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 725, .adv_w = 107, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 733, .adv_w = 107, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 740, .adv_w = 45, .box_w = 2, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 743, .adv_w = 45, .box_w = 2, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 746, .adv_w = 110, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 754, .adv_w = 110, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 762, .adv_w = 110, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 769, .adv_w = 110, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 776, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 783, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 790, .adv_w = 100, .box_w = 6, .box_h = 13, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 800, .adv_w = 104, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 806, .adv_w = 149, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 815, .adv_w = 116, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 823, .adv_w = 48, .box_w = 3, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 828, .adv_w = 49, .box_w = 3, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 832, .adv_w = 136, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 843, .adv_w = 112, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 849, .adv_w = 110, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 856, .adv_w = 118, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 864, .adv_w = 130, .box_w = 7, .box_h = 12, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 875, .adv_w = 104, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 882, .adv_w = 130, .box_w = 8, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 894, .adv_w = 104, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 900, .adv_w = 130, .box_w = 8, .box_h = 13, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 913, .adv_w = 103, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 922, .adv_w = 130, .box_w = 8, .box_h = 13, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 935, .adv_w = 103, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 942, .adv_w = 130, .box_w = 8, .box_h = 14, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 956, .adv_w = 104, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 965, .adv_w = 130, .box_w = 8, .box_h = 14, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 979, .adv_w = 104, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 986, .adv_w = 130, .box_w = 7, .box_h = 14, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 999, .adv_w = 104, .box_w = 5, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1007, .adv_w = 130, .box_w = 8, .box_h = 13, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1020, .adv_w = 104, .box_w = 5, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1028, .adv_w = 130, .box_w = 8, .box_h = 13, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1041, .adv_w = 104, .box_w = 5, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1049, .adv_w = 130, .box_w = 8, .box_h = 14, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1063, .adv_w = 104, .box_w = 5, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1071, .adv_w = 130, .box_w = 8, .box_h = 14, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1085, .adv_w = 104, .box_w = 5, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1093, .adv_w = 130, .box_w = 8, .box_h = 15, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 1108, .adv_w = 104, .box_w = 5, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1116, .adv_w = 113, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1125, .adv_w = 107, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1134, .adv_w = 113, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1143, .adv_w = 107, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1150, .adv_w = 113, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1159, .adv_w = 107, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1166, .adv_w = 115, .box_w = 6, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1176, .adv_w = 107, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1185, .adv_w = 113, .box_w = 6, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1195, .adv_w = 107, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1204, .adv_w = 114, .box_w = 6, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1215, .adv_w = 106, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1224, .adv_w = 113, .box_w = 6, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1235, .adv_w = 107, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1244, .adv_w = 113, .box_w = 6, .box_h = 14, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1255, .adv_w = 107, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1266, .adv_w = 48, .box_w = 3, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1271, .adv_w = 46, .box_w = 3, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1275, .adv_w = 48, .box_w = 1, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1277, .adv_w = 48, .box_w = 1, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1279, .adv_w = 149, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1291, .adv_w = 110, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1299, .adv_w = 149, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1311, .adv_w = 110, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1318, .adv_w = 149, .box_w = 8, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1332, .adv_w = 110, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1341, .adv_w = 149, .box_w = 8, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1355, .adv_w = 110, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1364, .adv_w = 149, .box_w = 8, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1378, .adv_w = 111, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1387, .adv_w = 149, .box_w = 8, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1401, .adv_w = 112, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1410, .adv_w = 149, .box_w = 8, .box_h = 14, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1424, .adv_w = 110, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1433, .adv_w = 149, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1445, .adv_w = 110, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1453, .adv_w = 149, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1465, .adv_w = 110, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1473, .adv_w = 149, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1485, .adv_w = 110, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1492, .adv_w = 149, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1504, .adv_w = 110, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1511, .adv_w = 149, .box_w = 8, .box_h = 14, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1525, .adv_w = 110, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1534, .adv_w = 136, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1545, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1552, .adv_w = 136, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1563, .adv_w = 112, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1569, .adv_w = 140, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1581, .adv_w = 118, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1590, .adv_w = 140, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1602, .adv_w = 118, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1611, .adv_w = 140, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1623, .adv_w = 118, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1631, .adv_w = 140, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1643, .adv_w = 118, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1651, .adv_w = 140, .box_w = 8, .box_h = 14, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1665, .adv_w = 118, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1676, .adv_w = 121, .box_w = 7, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1687, .adv_w = 100, .box_w = 6, .box_h = 13, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 1697, .adv_w = 121, .box_w = 7, .box_h = 12, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 1708, .adv_w = 100, .box_w = 5, .box_h = 10, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 1715, .adv_w = 121, .box_w = 7, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1726, .adv_w = 100, .box_w = 6, .box_h = 12, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 1735, .adv_w = 121, .box_w = 7, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1746, .adv_w = 100, .box_w = 6, .box_h = 12, .ofs_x = 0, .ofs_y = -3}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_1[] = {
    0x0, 0x1, 0x3, 0x8, 0x9, 0xc, 0xd, 0x12,
    0x13, 0x15, 0x19, 0x1a, 0x1d, 0x20, 0x21, 0x22,
    0x23, 0x28, 0x29, 0x2a, 0x2c, 0x2d, 0x32, 0x33,
    0x34, 0x35, 0x39, 0x3a, 0x3d, 0x43, 0x50, 0x51,
    0x68, 0x69, 0xa8, 0xa9, 0xe1, 0xf0
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 96, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    },
    {
        .range_start = 192, .range_length = 241, .glyph_id_start = 97,
        .unicode_list = unicode_list_1, .glyph_id_ofs_list = NULL, .list_length = 38, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    },
    {
        .range_start = 7840, .range_length = 90, .glyph_id_start = 135,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
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
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 3,
    .bpp = 1,
    .kern_classes = 0,
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
const lv_font_t font_12 = {
#else
lv_font_t font_12 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 17,          /*The maximum line height required by the font*/
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



#endif /*#if FONT_12*/
