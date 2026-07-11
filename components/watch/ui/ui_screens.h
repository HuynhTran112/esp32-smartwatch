/* Khai báo danh mục các hàm khởi tạo màn hình giao diện (Screens API)
   File này định nghĩa các nguyên mẫu hàm (prototypes) cho tất cả các giao diện
   chức năng của đồng hồ. Mỗi hàm trả về một con trỏ kiểu `lv_obj_t` biểu diễn đối tượng
   màn hình LVGL để trình điều hướng `ui_nav` quản lý. */

#ifndef UI_SCREENS_H
#define UI_SCREENS_H

#include <stdint.h>
#include "lvgl.h"
void ui_quick_replies_clear_from_ble(void);
void ui_quick_replies_add_from_ble(const char *reply);

/* --- Khởi tạo màn hình lõi --- */
lv_obj_t *ui_watchface_screen_create(void);            // Màn hình mặt đồng hồ chính (Hiển thị giờ/phút/giây/thông tin nhanh)
lv_obj_t *ui_menu_screen_create(void);                 // Màn hình Menu lưới ứng dụng
lv_obj_t *ui_menu_get_screen(void);                    // Truy vấn đối tượng màn hình menu hiện hành

/* --- Thể thao & Hoạt động (Fitness Tracking) --- */
lv_obj_t *ui_activity_screen_create(void);             // Menu lựa chọn môn thể thao (Chạy bộ / Đạp xe)
lv_obj_t *ui_running_screen_create(void);              // Màn hình đo thông số Chạy bộ (Steps, Distance, Pace)
lv_obj_t *ui_cycling_screen_create(void);              // Màn hình đo thông số Đạp xe (Distance, Speed)
uint32_t ui_activity_get_current_session_id(void);     // Lấy mã phiên hoạt động hiện tại
const char *ui_activity_get_current_sport_mode(void);  // Lấy chuỗi ký tự hiển thị chế độ thể thao hiện tại
void ui_activity_check_background_timeout(void);        // Kiểm tra và tự hủy bài tập ngầm quá hạn
bool ui_activity_is_active(void);                      // Kiểm tra xem bài tập thể thao có đang hoạt động hay không

/* --- Thông tin liên lạc & Vị trí (BLE, Notifications, GPS) --- */
lv_obj_t *ui_notifications_screen_create(void);        // Màn hình hiển thị danh sách tin nhắn đồng bộ từ Smartphone
lv_obj_t *ui_gps_screen_create(void);                  // Màn hình hiển thị chỉ đường & trạng thái GPS đồng bộ
lv_obj_t *ui_connect_screen_create(void);              // Màn hình trạng thái kết nối Bluetooth Low Energy (BLE)

/* --- Cài đặt mạng & Cập nhật hệ thống (WiFi & OTA) --- */
lv_obj_t *ui_wifi_screen_create(void);                 // Màn hình quét cấu hình & kết nối mạng Wi-Fi
void ui_wifi_deinit(void);                             // Giải phóng tài nguyên và dừng quét Wi-Fi
lv_obj_t *ui_update_screen_create(void);               // Màn hình kiểm tra & nạp bản cập nhật Firmware OTA qua mạng

/* --- Theo dõi sức khỏe (Biomedical Sensors) --- */
lv_obj_t *ui_health_screen_create(void);               // Màn hình đo và hiển thị nhịp tim (Heart Rate)
lv_obj_t *ui_spo2_screen_create(void);                 // Màn hình đo và hiển thị nồng độ oxy trong máu (SpO2)

/* --- Tiện ích & Công cụ (Utilities) --- */
lv_obj_t *ui_alarm_list_screen_create(void);           // Màn hình hiển thị và thiết lập danh sách báo thức
void ui_alarm_engine_start(void);                      // Khởi động timer kiểm tra báo thức nền
lv_obj_t *ui_stopwatch_screen_create(void);            // Màn hình đồng hồ bấm giờ (Stopwatch) với tính năng đo vòng (Lap)

/* --- Cấu hình & Cài đặt hệ thống (Settings Stack) --- */
lv_obj_t *ui_settings_screen_create(void);             // Menu cài đặt chính
lv_obj_t *ui_personality_settings_screen_create(void);  // Cài đặt cá nhân hóa (Màu hệ thống, Kiểu mặt đồng hồ)
lv_obj_t *ui_display_settings_screen_create(void);     // Cài đặt màn hình (Độ sáng đèn nền, thời gian tắt màn hình)
lv_obj_t *ui_vibration_settings_screen_create(void);   // Cài đặt cường độ chế độ rung phản hồi
lv_obj_t *ui_language_settings_screen_create(void);    // Lựa chọn ngôn ngữ hiển thị (Tiếng Việt / English)
lv_obj_t *ui_set_time_screen_create(void);             // Điều chỉnh thời gian rtc thủ công
lv_obj_t *ui_system_settings_screen_create(void);      // Menu cài đặt cấp hệ thống (Khởi động lại, Reset)
lv_obj_t *ui_about_screen_create(void);                // Màn hình thông tin phiên bản phần cứng và phần mềm

#endif // UI_SCREENS_H
