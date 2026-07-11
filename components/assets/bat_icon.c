/* ****************************************************************************
   Size: 14 px
   Bpp: 1
   Opts: --bpp 1 --size 14 --no-compress --stride 1 --align 1 --font Font Awesome 7 Free-Solid-900.otf --range 62016,62017,62018,62019,62020 --format lvgl -o bat_icon.c
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



#ifndef BAT_ICON
#define BAT_ICON 1
#endif

#if BAT_ICON

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+F240 "" */
    0x7f, 0xfe, 0x7f, 0xff, 0xb0, 0x0, 0xd8, 0x0,
    0x6d, 0xff, 0xb6, 0xff, 0xdf, 0x7f, 0xef, 0xbf,
    0xf6, 0xc0, 0x3, 0x7f, 0xff, 0x9f, 0xff, 0x80,

    /* U+F241 "" */
    0x7f, 0xfe, 0x7f, 0xff, 0xb0, 0x0, 0xd8, 0x0,
    0x6d, 0xfe, 0x36, 0xff, 0x1f, 0x7f, 0x8f, 0xbf,
    0xc6, 0xc0, 0x3, 0x7f, 0xff, 0x9f, 0xff, 0x80,

    /* U+F242 "" */
    0x7f, 0xfe, 0x7f, 0xff, 0xb0, 0x0, 0xd8, 0x0,
    0x6d, 0xf0, 0x36, 0xf8, 0x1f, 0x7c, 0xf, 0xbe,
    0x6, 0xc0, 0x3, 0x7f, 0xff, 0x9f, 0xff, 0x80,

    /* U+F243 "" */
    0x7f, 0xfe, 0x7f, 0xff, 0xb0, 0x0, 0xd8, 0x0,
    0x6d, 0xc0, 0x36, 0xe0, 0x1f, 0x70, 0xf, 0xb8,
    0x6, 0xc0, 0x3, 0x7f, 0xff, 0x9f, 0xff, 0x80,

    /* U+F244 "" */
    0x7f, 0xfe, 0x7f, 0xff, 0xb0, 0x0, 0xd8, 0x0,
    0x6c, 0x0, 0x3e, 0x0, 0x1f, 0x0, 0xf, 0x80,
    0x6, 0xc0, 0x3, 0x7f, 0xff, 0x9f, 0xff, 0x80
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 280, .box_w = 17, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 24, .adv_w = 280, .box_w = 17, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 48, .adv_w = 280, .box_w = 17, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 72, .adv_w = 280, .box_w = 17, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 96, .adv_w = 280, .box_w = 17, .box_h = 11, .ofs_x = 1, .ofs_y = -1}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 62016, .range_length = 5, .glyph_id_start = 1,
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
const lv_font_t bat_icon = {
#else
lv_font_t bat_icon = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 12,          /*The maximum line height required by the font*/
    .base_line = 1,             /*Baseline measured from the bottom of the line*/
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



#endif /*#if BAT_ICON*/
