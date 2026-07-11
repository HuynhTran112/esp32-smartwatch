/* Khai báo các thư viện, phông chữ, bảng màu và các hàm điều khiển giao diện LVGL
   Thiết lập các định nghĩa phông chữ (Fonts) được chuyển đổi từ font hệ thống, định nghĩa bảng màu
   giao diện chế độ nền tối (Dark Theme) sang trọng, và các API điều khiển giao diện lõi. */

#ifndef UI_H
#define UI_H

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

/* --- KHAI BÁO PHÔNG CHỮ HỆ THỐNG (Fonts) --- */
LV_FONT_DECLARE(font_12);
LV_FONT_DECLARE(font_14);
LV_FONT_DECLARE(font_16);
LV_FONT_DECLARE(font_20);
LV_FONT_DECLARE(font_24);
LV_FONT_DECLARE(font_36);
LV_FONT_DECLARE(font_48);

#define UI_FONT_12 (&font_12) // Phông chữ kích thước nhỏ 12px
#define UI_FONT_14 (&font_14) // Phông chữ thông thường 14px
#define UI_FONT_16 (&font_16) // Phông chữ trung bình 16px
#define UI_FONT_20 (&font_20) // Phông chữ tiêu đề nhỏ 20px
#define UI_FONT_24 (&font_24) // Phông chữ tiêu đề lớn 24px
#define UI_FONT_36 (&font_36) // Phông chữ số hiển thị 36px
#define UI_FONT_48 (&font_48) // Phông chữ số hiển thị cực lớn 48px (Watchface/Alarm)

/* ===================================================================
 *  BẢNG MÀU CHỦ ĐỀ GIAO DIỆN (Color Palette - Dark Theme)
 * =================================================================== */
#define COLOR_BG            lv_color_hex(0x05070D)  // Aurora Dark background
#define COLOR_SURFACE       lv_color_hex(0x101827)  // Aurora Dark card
#define COLOR_SURFACE_LT    lv_color_hex(0x1B2A3D)  // Aurora Dark pressed surface
#define COLOR_TEXT          lv_color_hex(0xFFFFFF)  // Màu chữ trắng chính
#define COLOR_TEXT_MUTED    lv_color_hex(0x888888)  // Màu chữ xám mờ phụ
#define COLOR_PRIMARY       lv_color_hex(0x38BDF8)  // Aurora blue
#define COLOR_GREEN         lv_color_hex(0x34D399)  // Aurora green
#define COLOR_RED           lv_color_hex(0xFF453A)  // Màu đỏ (dành cho cảnh báo/đo nhịp tim)
#define COLOR_ORANGE        lv_color_hex(0xFF9F0A)  // Màu cam (dành cho bấm giờ)
#define COLOR_PURPLE        lv_color_hex(0xA78BFA)  // Aurora purple
#define COLOR_CYAN          lv_color_hex(0x22D3EE)  // Aurora cyan
#define COLOR_FB_BLUE       lv_color_hex(0x1877F2)  // Màu xanh dương Facebook (dành cho thông báo mạng xã hội)
#define COLOR_GRAY          lv_color_hex(0xA9A9A9)  // Màu xám (cài đặt chung)
#define COLOR_BATTERY       lv_color_hex(0x32D74B)  // Màu xanh lá tươi chỉ báo pin đầy
#define COLOR_SUCCESS       COLOR_GREEN
#define COLOR_WARNING       COLOR_ORANGE
#define COLOR_DANGER        COLOR_RED

/* Spacing scale (4px base) */
#define SP_XS  4
#define SP_SM  8
#define SP_MD  12
#define SP_LG  16
#define SP_XL  24

/* Radius scale */
#define R_CARD   18
#define R_BUTTON 14
#define R_PILL   LV_RADIUS_CIRCLE

/* Screen safe padding */
#define SCREEN_PAD 14
#define SCREEN_SAFE_TOP 30

/* Solid hairline border, cheaper than translucent shadow */
#define HAIRLINE_COLOR  COLOR_SURFACE_LT
#define HAIRLINE_WIDTH  1

/* ===================================================================
 *  CÁC HÀM GIAO DIỆN CHÍNH
 * =================================================================== */

/* Khởi tạo toàn bộ kiến trúc giao diện đồ họa cho Smartwatch
   Hàm này được gọi trong app_main.c ngay khi luồng hiển thị LVGL port được khóa an toàn. */
void ui_init(void);

/* Gửi tín hiệu yêu cầu hệ thống đi vào chế độ ngủ sâu tắt màn hình */
void ui_request_deep_sleep(void);

/* Kiểm tra hệ thống đã bắt đầu tiến trình vào deep sleep hay chưa. */
bool ui_is_sleep_in_progress(void);

void ui_runtime_mark_alive(void);
uint32_t ui_runtime_last_heartbeat_ms(void);

/* Đặt giá trị độ sáng đèn nền LCD hiệu dụng hiện hành (duty cycle) */
void ui_backlight_set_active_duty(uint32_t duty);

/* Truy vấn giá trị độ sáng đèn nền LCD hiện hành */
uint32_t ui_backlight_get_active_duty(void);

/* Cấu hình chế độ hiển thị màu sắc biểu tượng trong Menu
   - mono: true: hiển thị đơn sắc chủ đề, false: đa sắc mặc định
   - color: Màu chủ đề đơn sắc được chọn */
void ui_menu_set_system_color(bool mono, lv_color_t color);

/* Cấu hình kiểu màn hình mặt đồng hồ chính (Watchface Style)
   - style_id: Mã định danh kiểu mặt hiển thị */
void ui_watchface_set_style(int style_id);

/* Lấy mã kiểu màn hình mặt đồng hồ chính đang chạy */
int ui_watchface_get_style(void);

#endif // UI_H
