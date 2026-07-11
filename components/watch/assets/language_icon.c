/* ****************************************************************************
   Size: 16 px
   Bpp: 1
   Range: U+F1AB language
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

#ifndef LANGUAGE_ICON
#define LANGUAGE_ICON 1
#endif

#if LANGUAGE_ICON

static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+F1AB */
    0x03, 0xc0, 0x0f, 0xf0, 0x18, 0x18, 0x33, 0xcc,
    0x37, 0xec, 0x36, 0x6c, 0x37, 0xec, 0x33, 0xcc,
    0x01, 0x80, 0x07, 0xe0, 0x0c, 0x30, 0x1b, 0xd8,
    0x3f, 0xfc, 0x61, 0x86, 0x61, 0x86, 0x00, 0x00
};

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 0, .adv_w = 256, .box_w = 16, .box_h = 16, .ofs_x = 0, .ofs_y = -1}
};

static const uint16_t unicode_list_0[] = {
    0x0
};

static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 61867, .range_length = 1, .glyph_id_start = 1,
        .unicode_list = unicode_list_0, .glyph_id_ofs_list = NULL, .list_length = 1,
        .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }
};

#if LVGL_VERSION_MAJOR == 8
static lv_font_fmt_txt_glyph_cache_t cache;
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

#if LVGL_VERSION_MAJOR >= 8
const lv_font_t language_icon = {
#else
lv_font_t language_icon = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,
    .line_height = 18,
    .base_line = 3,
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = 0,
    .underline_thickness = 0,
#endif
    .dsc = &font_dsc,
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};

#endif /* LANGUAGE_ICON */
