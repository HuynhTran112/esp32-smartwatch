/* Định nghĩa hàm tiện ích thiết lập cấu trúc hiển thị biểu tượng ứng dụng tròn */

#include "ui_icons.h"
#include "ui.h"
#include <ctype.h>
#include <string.h>

/* Triển khai tạo và cấu hình phong cách biểu tượng trong menu
   Hàm này thiết lập một `lv_obj` hình tròn đóng vai trò làm card nền (container),
   sau đó trích xuất hoặc xử lý chuỗi ký tự UTF-8 được truyền vào từ tham số `symbol`
   để hiển thị một ký tự nhãn (label) căn giữa. Nếu symbol là UTF-8 đa ký tự (như glyph icon),
   hàm sẽ sao chép 3 byte đầu tiên để vẽ chính xác. */
lv_obj_t *ui_icon_create(lv_obj_t *parent, const char *symbol, lv_color_t bg_color, bool monochrome, lv_color_t mono_color) {
    /* 1. Khởi tạo đối tượng nền tròn làm Card chứa biểu tượng */
    lv_obj_t *icon_bg = lv_obj_create(parent);
    lv_obj_set_size(icon_bg, 72, 72);
    lv_obj_set_style_radius(icon_bg, LV_RADIUS_CIRCLE, 0); // Đặt độ bo tròn tối đa tạo hình tròn
    lv_obj_set_style_bg_color(icon_bg, bg_color, 0);
    lv_obj_set_style_bg_opa(icon_bg, LV_OPA_COVER, 0);      // Bật chế độ hiển thị màu nền hoàn toàn
    lv_obj_set_style_border_width(icon_bg, 0, 0);          // Loại bỏ viền ngoài để tăng tính tối giản
    lv_obj_clear_flag(icon_bg, LV_OBJ_FLAG_SCROLLABLE);    // Ngăn chặn cuộn màn hình trên icon

    /* 2. Tạo nhãn ký tự ở trung tâm biểu tượng */
    lv_obj_t *ic = lv_label_create(icon_bg);
    char txt[4] = "?";
    
    if (symbol && symbol[0]) {
        /* Phân loại và giải mã ký tự đầu vào */
        if ((unsigned char)symbol[0] >= 0xe0) {
            /* Nhận diện mã hóa UTF-8 (Glyph icon đặc biệt), sao chép mã code 3-byte */
            strncpy(txt, symbol, 3);
            txt[3] = '\0';
        } else {
            /* Ký tự chữ thường, chuyển thành chữ viết hoa đầu dòng */
            txt[0] = (char)toupper((unsigned char)symbol[0]);
            txt[1] = '\0';
        }
    } else {
        txt[0] = '?'; 
        txt[1] = '\0';
    }
    
    lv_label_set_text(ic, txt);

    /* 3. Cấu hình màu sắc nhãn chữ tùy theo chế độ màu chủ đề (Monochrome / Color) */
    if (monochrome) {
        lv_obj_set_style_text_color(ic, mono_color, 0);
    } else {
        lv_obj_set_style_text_color(ic, lv_color_white(), 0);
    }
    
    lv_obj_set_style_text_font(ic, UI_FONT_20, 0); // Sử dụng phông chữ kích thước trung bình 20px
    lv_obj_center(ic);                              // Căn chỉnh nhãn nhảy vào trung tâm đối tượng tròn

    return icon_bg;
}
