/* ****************************************************************************
   Size: 20 px
   Bpp: 1
   Opts: --bpp 1 --size 20 --no-compress --stride 1 --align 1 --font Font Awesome 7 Free-Solid-900.otf --range 61958,61982,63244,61507 --format lvgl -o workout_health_icon.c
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



#ifndef WORKOUT_HEALTH_ICON
#define WORKOUT_HEALTH_ICON 1
#endif

#if WORKOUT_HEALTH_ICON

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+F043 "" */
    0x3, 0x80, 0x7, 0x0, 0x1f, 0x0, 0x7f, 0x0,
    0xfe, 0x3, 0xfe, 0xf, 0xfe, 0x1f, 0xfc, 0x7f,
    0xfc, 0xff, 0xfb, 0xff, 0xff, 0xff, 0xfe, 0x7f,
    0xfc, 0xff, 0xf9, 0xff, 0xb9, 0xfe, 0x78, 0x7c,
    0x78, 0xf0, 0x7f, 0xc0, 0x3e, 0x0,

    /* U+F206 "" */
    0x0, 0x7, 0xc0, 0x0, 0x7, 0xc0, 0x7, 0x6,
    0x0, 0xf, 0x83, 0x0, 0x1, 0xff, 0x0, 0x1,
    0xff, 0x80, 0x1, 0xc7, 0x80, 0x1f, 0xe6, 0xf8,
    0x7f, 0x6d, 0xfe, 0x66, 0x6d, 0xe6, 0xce, 0x3b,
    0x63, 0xcf, 0xfb, 0x33, 0xcf, 0xf3, 0x33, 0xc0,
    0xc3, 0x3, 0x61, 0xc1, 0x86, 0x7f, 0x81, 0xfe,
    0x1e, 0x0, 0x78,

    /* U+F21E "" */
    0x1e, 0x7, 0x83, 0xf9, 0xfc, 0x7f, 0xff, 0xef,
    0xff, 0xff, 0xfe, 0xff, 0xff, 0xcf, 0xff, 0xfc,
    0x63, 0xff, 0x86, 0x3f, 0x1, 0x48, 0x0, 0x30,
    0xc0, 0x3f, 0x9f, 0xc1, 0xf9, 0xf8, 0xf, 0xff,
    0x0, 0x7f, 0xe0, 0x3, 0xfc, 0x0, 0x1f, 0x80,
    0x0, 0xf0, 0x0,

    /* U+F70C "" */
    0x0, 0x60, 0x0, 0xf0, 0x0, 0xf0, 0x0, 0x60,
    0x0, 0x0, 0x0, 0x0, 0x1f, 0xc0, 0x3f, 0xe0,
    0x3b, 0xf0, 0x73, 0xf0, 0x33, 0xff, 0x3, 0xdf,
    0x7, 0x80, 0x3, 0xe0, 0x1, 0xf8, 0x0, 0x78,
    0xc, 0x38, 0x1e, 0x38, 0x3c, 0x30, 0x78, 0x70,
    0xf0, 0x60, 0xc0, 0x60
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 240, .box_w = 15, .box_h = 20, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 38, .adv_w = 400, .box_w = 24, .box_h = 17, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 89, .adv_w = 320, .box_w = 20, .box_h = 17, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 132, .adv_w = 280, .box_w = 16, .box_h = 22, .ofs_x = 0, .ofs_y = -3}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_0[] = {
    0x0, 0x1c3, 0x1db, 0x6c9
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 61507, .range_length = 1738, .glyph_id_start = 1,
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
const lv_font_t workout_health_icon = {
#else
lv_font_t workout_health_icon = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 22,          /*The maximum line height required by the font*/
    .base_line = 3,             /*Baseline measured from the bottom of the line*/
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



#endif /*#if WORKOUT&HEALTH_ICON*/
