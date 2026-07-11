/* Định nghĩa hàm tiện ích tạo biểu tượng ứng dụng (App Icons UI Helper)
   Cung cấp API tạo biểu tượng dạng hình tròn thống nhất với ký hiệu (symbol)
   hoặc chữ cái viết tắt căn giữa. Hỗ trợ thay đổi màu nền động hoặc chuyển đổi
   sang chế độ đơn sắc (monochrome) tùy cấu hình màu sắc hệ thống. */

#ifndef UI_ICONS_H
#define UI_ICONS_H

#include "lvgl.h"

/* Tạo đối tượng biểu tượng hình tròn với ký tự trung tâm
   - parent: Đối tượng LVGL cha chứa biểu tượng này
   - symbol: Ký tự hoặc mã glyph của icon (ví dụ: LV_SYMBOL_SETTINGS, "W",...)
   - bg_color: Màu nền gốc của biểu tượng ứng dụng
   - monochrome: Cờ xác định có bật chế độ đơn sắc cho giao diện hay không
   - mono_color: Màu hiển thị đơn sắc khi cờ monochrome được thiết lập
   Trả về: lv_obj_t* Con trỏ trỏ tới đối tượng nền của biểu tượng được tạo (lv_obj_create) */
lv_obj_t *ui_icon_create(lv_obj_t *parent, const char *symbol, lv_color_t bg_color, bool monochrome, lv_color_t mono_color);

#endif // UI_ICONS_H

