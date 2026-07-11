/* ****************************************************************************
   Size: 14 px
   Bpp: 1
   Opts: --bpp 1 --size 14 --no-compress --stride 1 --align 1 --font Font Awesome 7 Free-Solid-900.otf --range 61452,61453,62189,61553 --format lvgl -o signal_icon.c
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



#ifndef SIGNAL_ICON
#define SIGNAL_ICON 1
#endif

#if SIGNAL_ICON

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+F00C "" */
    0x0, 0x0, 0x3, 0x0, 0x70, 0x6, 0x0, 0xc0,
    0x18, 0x1, 0x8c, 0x30, 0x66, 0x3, 0xe0, 0x1c,
    0x0, 0x80,

    /* U+F00D "" */
    0xc0, 0x5c, 0x19, 0xc6, 0x1d, 0x81, 0xe0, 0x1c,
    0x7, 0xc1, 0xd8, 0x71, 0x9c, 0x1b, 0x1, 0x0,

    /* U+F071 "" */
    0x3, 0x0, 0xc, 0x0, 0x78, 0x3, 0xf0, 0xc,
    0xc0, 0x73, 0x81, 0xce, 0xf, 0x3c, 0x3f, 0xf1,
    0xf3, 0xe7, 0xcf, 0xbf, 0xff, 0xff, 0xfc,

    /* U+F2ED "" */
    0xf, 0xf, 0xff, 0xff, 0xf0, 0x0, 0x7f, 0xe7,
    0xfe, 0x69, 0x66, 0x96, 0x69, 0x66, 0x96, 0x69,
    0x66, 0x96, 0x7f, 0xe7, 0xfe
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 196, .box_w = 12, .box_h = 12, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 18, .adv_w = 168, .box_w = 11, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 34, .adv_w = 224, .box_w = 14, .box_h = 13, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 57, .adv_w = 196, .box_w = 12, .box_h = 14, .ofs_x = 0, .ofs_y = -2}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_0[] = {
    0x0, 0x1, 0x65, 0x2e1
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 61452, .range_length = 738, .glyph_id_start = 1,
        .unicode_list = unicode_list_0, .glyph_id_ofs_list = NULL, .list_length = 4, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
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
    .cmap_num = 1,
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
const lv_font_t signal_icon = {
#else
lv_font_t signal_icon = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 14,          /*The maximum line height required by the font*/
    .base_line = 2,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = 0,
    .underline_thickness = 0,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if SIGNAL_ICON*/
