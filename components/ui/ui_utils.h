/* Khai báo các cấu trúc dữ liệu và hàm tiện ích hỗ trợ giao diện người dùng
   Cung cấp định nghĩa các hằng số văn bản đa ngôn ngữ (Localization), kiểu dữ liệu
   chụp nhanh thời gian rtc (Time Snapshot), các tiện ích kiểm tra sự thay đổi của nhãn văn bản
   trước khi vẽ lại nhằm tối ưu tốc độ CPU, và định vị màu sắc thương hiệu của các ứng dụng mạng xã hội. */

#ifndef UI_UTILS_H
#define UI_UTILS_H

#include "lvgl.h"
#include <stddef.h>

/* Cấu trúc lưu trữ dữ liệu thời gian dạng thu gọn (Time Snapshot)
   Sử dụng để biểu diễn thời gian đọc ra từ chip RTC bên trong hoặc các tiến trình
   xử lý định dạng hiển thị mặt đồng hồ chính. */
typedef struct {
  int hour;    // Giờ (0 - 23)
  int minute;  // Phút (0 - 59)
  int second;  // Giây (0 - 59)
  int day;     // Ngày trong tháng (1 - 31)
  int month;   // Tháng (1 - 12)
  int weekday; // Ngày trong tuần (0: Chủ nhật, 1: Thứ hai,...)
} ui_time_snapshot_t;

/* Các tùy chọn ngôn ngữ được hỗ trợ bởi hệ thống */
typedef enum {
  UI_LANG_EN = 0, // Tiếng Anh
  UI_LANG_VI,     // Tiếng Việt
} ui_language_t;

/* Danh mục định danh chuỗi văn bản (String ID Mapping)
   Được sử dụng làm chỉ số trong mảng ánh xạ đa ngôn ngữ s_text_en và s_text_vi. */
typedef enum {
  UI_TXT_WORKOUT,             // "Workout" / "Luyện tập"
  UI_TXT_GPS,                 // "GPS"
  UI_TXT_HEALTH,              // "Health" / "Sức khỏe"
  UI_TXT_ALARM,               // "Alarm" / "Báo thức"
  UI_TXT_WIFI,                // "Wi-Fi"
  UI_TXT_CONNECT,             // "Connect" / "Kết nối"
  UI_TXT_SETTINGS,            // "Settings" / "Cài đặt"
  UI_TXT_SHUTDOWN,            // "Shutdown" / "Tắt nguồn"
  UI_TXT_UPDATE,              // "Update" / "Cập nhật"
  UI_TXT_ABOUT,               // "About" / "Giới thiệu"
  UI_TXT_VIBRATION,           // "Vibration" / "Rung"
  UI_TXT_PERSONALIZATION,     // "Personalization" / "Cá nhân hóa"
  UI_TXT_DISPLAY_BRIGHTNESS,  // "Display & Brightness" / "Hiển thị & độ sáng"
  UI_TXT_LANGUAGE,            // "Language" / "Ngôn ngữ"
  UI_TXT_SET_TIME,            // "Set Time" / "Đặt giờ"
  UI_TXT_WATCH_FACE,          // "Watch Face" / "Mặt đồng hồ"
  UI_TXT_SYSTEM_COLOR,        // "System Color" / "Màu hệ thống"
  UI_TXT_BRIGHTNESS,          // "Brightness" / "Độ sáng"
  UI_TXT_SCREEN_TIMEOUT,      // "Screen Timeout" / "Hẹn giờ tắt màn hình"
  UI_TXT_MEDIUM_STRENGTH,     // "Strength: Medium" / "Độ mạnh: Trung bình"
  UI_TXT_CALLS,               // "Messages" / "Tin nhắn"
  UI_TXT_ALARMS,              // "Alarms" / "Báo thức"
  UI_TXT_ENGLISH,             // "English" / "Tiếng Anh"
  UI_TXT_VIETNAMESE,          // "Vietnamese" / "Tiếng Việt"
  UI_TXT_HEART_RATE,          // "Heart Rate" / "Nhịp tim"
  UI_TXT_WAITING_MAX30102,    // "Waiting for heart rate data" / "Đang chờ dữ liệu nhịp tim"
  UI_TXT_FIRMWARE,            // "Smartwatch Firmware" / "Phần mềm Smartwatch"
  UI_TXT_VERSION,             // "Version" / "Phiên bản"
  UI_TXT_HARDWARE,            // "Hardware" / "Phần cứng"
  UI_TXT_DEFAULT,             // "Default" / "Mặc định"
  UI_TXT_CHOOSE_COLOR,        // "Choose color:" / "Chọn màu:"
  UI_TXT_SIMPLE_DIGITAL,      // "Simple Digital" / "Số đơn giản"
  UI_TXT_GRADIENT_MODERN,     // "Gradient Modern" / "Gradient hiện đại"
  UI_TXT_CLASSIC_ANALOG,      // "Classic Analog" / "Kim cổ điển"
  UI_TXT_CONNECT_BLE,         // "BLE Connection" / "Kết nối BLE"
  UI_TXT_BLUETOOTH_OFF,       // "Bluetooth: Off" / "Bluetooth: Tắt"
  UI_TXT_START_BLE,           // "Start BLE" / "Phát BLE"
  UI_TXT_CONNECTED,           // "Connected" / "Đã kết nối"
  UI_TXT_DISCONNECT,          // "Disconnect" / "Ngắt kết nối"
  UI_TXT_CONNECTING,          // "Connecting..." / "Đang kết nối..."
  UI_TXT_STOP_BLE,            // "Stop BLE" / "Tắt BLE"
  UI_TXT_NOTIFICATIONS_COUNT,  // "%d notifications" / "%d thông báo"
  UI_TXT_CONNECT_WIFI,        // "Connect Wi-Fi" / "Kết nối Wi-Fi"
  UI_TXT_CONNECT_WIFI_TITLE,  // "Connect Wi-Fi" / "Hãy kết nối Wi-Fi"
  UI_TXT_CONNECT_WIFI_OTA_DESC, // "Connect Wi-Fi..." / "Kết nối Wi-Fi..."
  UI_TXT_CONNECTING_SERVER,   // "Connecting to server..." / "Đang kết nối máy chủ..."
  UI_TXT_OTA_SUCCESS,         // "Update successful" / "Cập nhật thành công"
  UI_TXT_OTA_FAILED,          // "Update failed" / "Cập nhật thất bại"
  UI_TXT_READY_RESTART,       // "Ready to restart" / "Sẵn sàng khởi động lại"
  UI_TXT_RESTART,             // "Restart" / "Khởi động lại"
  UI_TXT_RETRY,               // "Retry" / "Thử lại"
  UI_TXT_STARTING_OTA,        // "Starting OTA..." / "Đang bắt đầu OTA..."
  UI_TXT_READY_OTA,           // "Ready for OTA update" / "Sẵn sàng cập nhật OTA"
  UI_TXT_OTA_NOT_CONFIGURED,  // "OTA not configured" / "Chưa cấu hình OTA"
  UI_TXT_CURRENT_VERSION,     // "Current: %s" / "Hiện tại: %s"
  UI_TXT_MISSING_URL,         // "Missing URL" / "Thiếu URL"
  UI_TXT_STOPWATCH,           // "Stopwatch" / "Bấm giờ"
  UI_TXT_READY,               // "Ready" / "Sẵn sàng"
  UI_TXT_TIMING,              // "Timing" / "Đang đếm thời gian"
  UI_TXT_LAP,                 // "Lap" / "Vòng"
  UI_TXT_RUNNING,             // "Running" / "Chạy bộ"
  UI_TXT_OUTDOOR_RUN,         // "Outdoor Run" / "Chạy ngoài trời"
  UI_TXT_CYCLING,             // "Cycling" / "Đạp xe"
  UI_TXT_OUTDOOR_RIDE,        // "Outdoor Ride" / "Đạp xe ngoài trời"
  UI_TXT_DISTANCE,            // "Distance" / "Quãng đường"
  UI_TXT_STEPS,               // "Steps" / "Bước"
  UI_TXT_SPEED,               // "Speed" / "Tốc độ"
  UI_TXT_PACE,                // "Pace" / "Nhịp độ"
  UI_TXT_NOTIFICATIONS,       // "Notifications" / "Thông báo"
  UI_TXT_NO_NOTIFICATIONS,    // "No notifications..." / "Chưa có thông báo..."
  UI_TXT_NEED_BLE,            // "Connect BLE" / "Hãy kết nối BLE"
  UI_TXT_PHONE,               // "Phone" / "Điện thoại"
  UI_TXT_WATCH,               // "Watch" / "Đồng hồ"
  UI_TXT_BLE_GPS_DESC,        // "Connect to phone..." / "Kết nối với điện thoại..."
  UI_TXT_GPS_NAVIGATING,      // "Navigating..." / "Đang dẫn đường..."
  UI_TXT_NEXT_STEP,           // "Next step" / "Bước tiếp theo"
  UI_TXT_REMAINING,           // "Remaining..." / "Còn lại..."
  UI_TXT_UNSUPPORTED,         // "Not supported..." / "Chưa hỗ trợ..."
  UI_TXT_OK,                  // "OK"
  UI_TXT_CANCEL,              // "Cancel" / "Hủy"
  UI_TXT_POWER_OFF_CONFIRM,   // "The device will restart..." / "Thiết bị sẽ khởi động lại..."
  UI_TXT_COUNT_LABEL,         // "Count" / "Số lượng"
  UI_TXT_COUNT,               // Tổng số lượng nhãn dịch thuật (Đánh dấu cuối Enum)
} ui_text_id_t;

/* --- Các hàm API Tiện ích --- */

/* Sao chép chuỗi an toàn trong bộ nhớ heap
   - s: Chuỗi nguồn cần sao chép
   Trả về: char* Con trỏ vùng nhớ mới chứa chuỗi sao chép, cần được free giải phóng */
char *ui_safe_strdup(const char *s);

lv_obj_t *ui_make_card(lv_obj_t *parent, lv_coord_t w, lv_coord_t h);
lv_obj_t *ui_make_icon_circle(lv_obj_t *parent, lv_color_t color, lv_coord_t d);
void ui_haptic_pulse(uint32_t duration_ms);
typedef enum {
  UI_HAPTIC_TICK = 0,
  UI_HAPTIC_CONFIRM,
  UI_HAPTIC_WARNING,
} ui_haptic_type_t;
void ui_haptic_feedback(ui_haptic_type_t type);
void ui_enable_list_scrolling(lv_obj_t *list);

/* Lấy màu sắc biểu tượng đại diện của ứng dụng nguồn
   - app_name: Tên ứng dụng gửi thông báo (ví dụ: Zalo, Telegram, Messenger...)
   Trả về: lv_color_t Giá trị màu sắc RGB tương ứng */
lv_color_t ui_app_color(const char *app_name);

/* Cập nhật văn bản cho đối tượng Label chỉ khi có sự thay đổi nội dung
   Giúp tránh gọi các hàm render nội dung vẽ lại của LVGL một cách vô ích, tiết kiệm điện năng. */
void ui_label_set_text_if_changed(lv_obj_t *label, const char *text);

/* Đọc thông tin thời gian hiện tại từ hệ thống và điền vào Snapshot */
void ui_time_get_snapshot(ui_time_snapshot_t *snapshot);

/* Định dạng thời gian thành dạng chuỗi HH:MM (ví dụ: "08:30") */
void ui_time_format_hhmm(char *buf, size_t buf_size, const ui_time_snapshot_t *snapshot);

/* Thiết lập timezone theo offset phút so với UTC. */
void ui_time_set_timezone_offset(int offset_minutes);

/* Kiểm tra đồng hồ hệ thống đã có epoch hợp lệ hay chưa. */
bool ui_time_is_synced(void);

/* Lấy trạng thái cài đặt ngôn ngữ hiện hành của hệ thống */
ui_language_t ui_language_get(void);

/* Thiết lập ngôn ngữ hệ thống hiển thị */
void ui_language_set(ui_language_t lang);

/* Dịch thuật văn bản dựa trên mã định danh ID và ngôn ngữ hiện tại */
const char *ui_tr(ui_text_id_t id);

/* Hàm chọn chuỗi tĩnh tương ứng với ngôn ngữ hiện hành */
const char *ui_text(const char *en, const char *vi);

#endif /* UI_UTILS_H */
