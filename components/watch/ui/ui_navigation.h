/* Khai báo kiến trúc điều hướng ngăn xếp (Screen Stack Navigation) và Status Bar
   Cung cấp tập hợp các nguyên mẫu hàm điều khiển cấu trúc điều hướng giữa các màn hình,
   quản lý ngăn xếp lịch sử (Stack History) lưu trong RTC memory để khôi phục khi thức giấc,
   và cập nhật giao diện của thanh trạng thái (Status Bar) hiển thị các chỉ số pin, Wi-Fi và Bluetooth. */

#ifndef UI_NAV_H
#define UI_NAV_H

#include "lvgl.h"
#include <stdbool.h>

#define NAV_STACK_MAX 8 // Kích thước ngăn xếp màn hình tối đa là 8 lớp

/* Danh mục mã định danh các màn hình hệ thống (Screen ID) */
typedef enum {
  UI_SCREEN_INVALID = 0,
  UI_SCREEN_WATCHFACE,              // Mặt đồng hồ chính
  UI_SCREEN_MENU,                   // Menu lưới ứng dụng
  UI_SCREEN_ACTIVITY,               // Lựa chọn hoạt động thể thao
  UI_SCREEN_RUNNING,                // Đo quãng đường chạy bộ
  UI_SCREEN_CYCLING,                // Đo tốc độ đạp xe
  UI_SCREEN_NOTIFICATIONS,          // Xem thông báo tin nhắn từ điện thoại
  UI_SCREEN_GPS,                    // Dẫn đường GPS đồng bộ từ app
  UI_SCREEN_SETTINGS,               // Cấu hình tổng quan
  UI_SCREEN_WIFI,                   // Thiết lập kết nối Wi-Fi
  UI_SCREEN_UPDATE,                 // Cập nhật Firmware OTA
  UI_SCREEN_CONNECT,                // Trạng thái ghép đôi BLE
  UI_SCREEN_HEALTH,                 // Đo nhịp tim
  UI_SCREEN_ALARM,                  // Danh sách chuông báo thức
  UI_SCREEN_SPO2,                   // Đo nồng độ Oxy SpO2
  UI_SCREEN_STOPWATCH,              // Bấm giờ thể thao
  UI_SCREEN_PERSONALITY_SETTINGS,   // Cấu hình giao diện (Màu sắc/Mặt đồng hồ)
  UI_SCREEN_DISPLAY_SETTINGS,       // Cấu hình độ sáng màn hình
  UI_SCREEN_VIBRATION_SETTINGS,     // Cấu hình độ mạnh motor rung
  UI_SCREEN_LANGUAGE_SETTINGS,      // Lựa chọn ngôn ngữ
  UI_SCREEN_SET_TIME,               // Điều chỉnh thời gian thủ công
  UI_SCREEN_SYSTEM_SETTINGS,        // Tùy chọn hệ thống chuyên sâu
  UI_SCREEN_ABOUT,                  // Giới thiệu phần cứng/phần mềm
} ui_screen_id_t;

/* ===================================================================
 *  CÁC API ĐIỀU HƯỚNG MÀN HÌNH (Screen Navigation API)
 * =================================================================== */

/* Khởi tạo các biến trạng thái và bộ nhớ định tuyến màn hình */
void     ui_navigation_init(void);

/* Đẩy màn hình mới vào ngăn xếp và tải lên màn hình LCD
   - scr: Đối tượng màn hình LVGL được khởi tạo */
void     ui_navigation_push(lv_obj_t *scr);

/* Thay màn hình trên cùng bằng màn hình mới và xóa màn hình cũ */
void     ui_navigation_replace(lv_obj_t *scr);

/* Quay lại màn hình trước đó trong ngăn xếp (Pop Screen) */
void     ui_navigation_pop(void);

/* Xóa tất cả các màn hình trung gian và quay về màn hình Watchface (Root) */
void     ui_navigation_pop_to_root(void);

/* Quay lại nhiều màn hình cùng lúc một cách an toàn (tránh xung đột transition)
   - count: Số lượng màn hình cần pop */
void     ui_navigation_pop_multiple(int count);

/* Tạo lại màn hình gốc Watchface và dọn dẹp các tài nguyên cũ
   - create_cb: Con trỏ hàm khởi tạo màn hình Watchface */
void     ui_navigation_recreate_root(lv_obj_t *(*create_cb)(void));

/* Đăng ký sự kiện cử chỉ vuốt từ trái sang phải để quay lại (Back Gesture) */
void     ui_navigation_add_gesture(lv_obj_t *scr);

/* Tạo đối tượng màn hình LVGL mặc định với nền tối đen */
lv_obj_t *ui_navigation_create_screen(void);

/* Tạo đối tượng màn hình LVGL với mã ID định danh đi kèm */
lv_obj_t *ui_navigation_create_screen_with_id(ui_screen_id_t screen_id);

/* Gán nhãn ID định danh cho đối tượng màn hình */
void     ui_navigation_set_screen_id(lv_obj_t *scr, ui_screen_id_t screen_id);

/* Truy vấn ID của màn hình đang hiển thị gần nhất */
ui_screen_id_t ui_navigation_get_last_screen_id(void);

/* Khôi phục lại màn hình đang chạy trước khi ngủ sâu từ bộ nhớ RTC
   Trả về: true nếu khôi phục thành công, false nếu ngược lại */
bool     ui_navigation_restore_last_screen(void);

/* Lưu trạng thái màn hình hiện tại và đưa toàn bộ chip vào ngủ sâu tiết kiệm pin */
void     ui_navigation_enter_deep_sleep(void);

/* Kiểm tra xem đèn nền màn hình hiện tại có đang tắt hoàn toàn hay không */
bool     ui_navigation_is_backlight_off(void);

/* Bật/tắt nhanh đèn nền màn hình (Backlight Toggle) */
void     ui_navigation_toggle_backlight(void);

/* Kiểm tra xem hệ thống có đang có một phiên hoạt động hoặc dẫn đường đang hoạt động hay không */
bool     ui_navigation_has_active_session(void);

/* ===================================================================
 *  CÁC API ĐIỀU KHIỂN THANH TRẠNG THÁI (Status Bar API)
 * =================================================================== */

/* Khởi tạo và thiết lập các thành phần giao diện của thanh trạng thái */
void     ui_statusbar_create(void);

/* Cập nhật nhãn thời gian hiện tại từ chip RTC lên góc trái thanh trạng thái */
void     ui_statusbar_update_time(void);

/* Thiết lập ẩn/hiển thị nhãn thời gian trên Status Bar (Ví dụ ẩn ở Watchface để tránh trùng lặp) */
void     ui_statusbar_set_time_visible(bool visible);

/* Cập nhật hiển thị biểu tượng trạng thái kết nối mạng Wi-Fi */
void     ui_statusbar_set_wifi_connected(bool connected);

/* Cập nhật hiển thị biểu tượng trạng thái kết nối Bluetooth (BLE) */
void     ui_statusbar_set_bt_connected(bool connected);

/* Cập nhật phần trăm dung lượng pin và biểu tượng chỉ báo pin
   - percent: Mức sạc pin tính theo phần trăm (0.0f - 100.0f) */
void     ui_statusbar_set_battery(float percent);

#endif // UI_NAV_H
