#include "ui_utils.h"

#include "vibration_motor.h"
#include "watch_settings.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static ui_language_t s_ui_language = UI_LANG_EN;

static const char *const s_text_en[UI_TXT_COUNT] = {
  [UI_TXT_WORKOUT] = "Workout",
  [UI_TXT_GPS] = "GPS",
  [UI_TXT_HEALTH] = "Health",
  [UI_TXT_ALARM] = "Alarm",
  [UI_TXT_WIFI] = "Wi-Fi",
  [UI_TXT_CONNECT] = "Connect",
  [UI_TXT_SETTINGS] = "Settings",
  [UI_TXT_SHUTDOWN] = "Shutdown",
  [UI_TXT_UPDATE] = "Update",
  [UI_TXT_ABOUT] = "About",
  [UI_TXT_VIBRATION] = "Vibration",
  [UI_TXT_PERSONALIZATION] = "Personalization",
  [UI_TXT_DISPLAY_BRIGHTNESS] = "Display & Brightness",
  [UI_TXT_LANGUAGE] = "Language",
  [UI_TXT_SET_TIME] = "Set Time",
  [UI_TXT_WATCH_FACE] = "Watch Face",
  [UI_TXT_SYSTEM_COLOR] = "System Color",
  [UI_TXT_BRIGHTNESS] = "Brightness",
  [UI_TXT_SCREEN_TIMEOUT] = "Screen Timeout",
  [UI_TXT_MEDIUM_STRENGTH] = "Strength: Medium",
  [UI_TXT_CALLS] = "Messages",
  [UI_TXT_ALARMS] = "Alarms",
  [UI_TXT_ENGLISH] = "English",
  [UI_TXT_VIETNAMESE] = "Vietnamese",
  [UI_TXT_HEART_RATE] = "Heart Rate",
  [UI_TXT_WAITING_MAX30102] = "Waiting for heart rate data",
  [UI_TXT_FIRMWARE] = "Smartwatch Firmware",
  [UI_TXT_VERSION] = "Version",
  [UI_TXT_HARDWARE] = "Hardware",
  [UI_TXT_DEFAULT] = "Default",
  [UI_TXT_CHOOSE_COLOR] = "Choose color:",
  [UI_TXT_SIMPLE_DIGITAL] = "Simple Digital",
  [UI_TXT_GRADIENT_MODERN] = "Gradient Modern",
  [UI_TXT_CLASSIC_ANALOG] = "Classic Analog",
  [UI_TXT_CONNECT_BLE] = "BLE Connection",
  [UI_TXT_BLUETOOTH_OFF] = "Bluetooth: Off",
  [UI_TXT_START_BLE] = "Start BLE",
  [UI_TXT_CONNECTED] = "Connected",
  [UI_TXT_DISCONNECT] = "Disconnect",
  [UI_TXT_CONNECTING] = "Connecting...",
  [UI_TXT_STOP_BLE] = "Stop BLE",
  [UI_TXT_NOTIFICATIONS_COUNT] = "%d notifications",
  [UI_TXT_CONNECT_WIFI] = "Connect Wi-Fi",
  [UI_TXT_CONNECT_WIFI_TITLE] = "Connect Wi-Fi",
  [UI_TXT_CONNECT_WIFI_OTA_DESC] = "Connect Wi-Fi to check\nand install firmware updates.",
  [UI_TXT_CONNECTING_SERVER] = "Connecting to server...",
  [UI_TXT_OTA_SUCCESS] = "Update successful",
  [UI_TXT_OTA_FAILED] = "Update failed",
  [UI_TXT_READY_RESTART] = "Ready to restart",
  [UI_TXT_RESTART] = "Restart",
  [UI_TXT_RETRY] = "Retry",
  [UI_TXT_STARTING_OTA] = "Starting OTA...",
  [UI_TXT_READY_OTA] = "Ready for OTA update",
  [UI_TXT_OTA_NOT_CONFIGURED] = "OTA not configured",
  [UI_TXT_CURRENT_VERSION] = "Current: %s",
  [UI_TXT_MISSING_URL] = "Missing URL",
  [UI_TXT_STOPWATCH] = "Stopwatch",
  [UI_TXT_READY] = "Ready",
  [UI_TXT_TIMING] = "Timing",
  [UI_TXT_LAP] = "Lap",
  [UI_TXT_RUNNING] = "Running",
  [UI_TXT_OUTDOOR_RUN] = "Outdoor Run",
  [UI_TXT_CYCLING] = "Cycling",
  [UI_TXT_OUTDOOR_RIDE] = "Outdoor Ride",
  [UI_TXT_DISTANCE] = "Distance",
  [UI_TXT_STEPS] = "Steps",
  [UI_TXT_SPEED] = "Speed",
  [UI_TXT_PACE] = "Pace",
  [UI_TXT_NOTIFICATIONS] = "Notifications",
  [UI_TXT_NO_NOTIFICATIONS] = "No notifications.\nConnect BLE with your\nphone to receive messages.",
  [UI_TXT_NEED_BLE] = "Connect BLE",
  [UI_TXT_PHONE] = "Phone",
  [UI_TXT_WATCH] = "Watch",
  [UI_TXT_BLE_GPS_DESC] = "Connect to your phone to sync\nGPS, maps and messages.",
  [UI_TXT_GPS_NAVIGATING] = "Navigating...",
  [UI_TXT_NEXT_STEP] = "Next step",
  [UI_TXT_REMAINING] = "Remaining: --",
  [UI_TXT_UNSUPPORTED] = "Not supported in this firmware.",
  [UI_TXT_OK] = "OK",
  [UI_TXT_CANCEL] = "Cancel",
  [UI_TXT_POWER_OFF_CONFIRM] = "The device will restart now.",
  [UI_TXT_COUNT_LABEL] = "Count",
};

static const char *const s_text_vi[UI_TXT_COUNT] = {
  [UI_TXT_WORKOUT] = "Luyện tập",
  [UI_TXT_GPS] = "GPS",
  [UI_TXT_HEALTH] = "Sức khỏe",
  [UI_TXT_ALARM] = "Báo thức",
  [UI_TXT_WIFI] = "Wi-Fi",
  [UI_TXT_CONNECT] = "Kết nối",
  [UI_TXT_SETTINGS] = "Cài đặt",
  [UI_TXT_SHUTDOWN] = "Tắt nguồn",
  [UI_TXT_UPDATE] = "Cập nhật",
  [UI_TXT_ABOUT] = "Giới thiệu",
  [UI_TXT_VIBRATION] = "Rung",
  [UI_TXT_PERSONALIZATION] = "Cá nhân hóa",
  [UI_TXT_DISPLAY_BRIGHTNESS] = "Hiển thị & độ sáng",
  [UI_TXT_LANGUAGE] = "Ngôn ngữ",
  [UI_TXT_SET_TIME] = "Đặt giờ",
  [UI_TXT_WATCH_FACE] = "Mặt đồng hồ",
  [UI_TXT_SYSTEM_COLOR] = "Màu hệ thống",
  [UI_TXT_BRIGHTNESS] = "Độ sáng",
  [UI_TXT_SCREEN_TIMEOUT] = "Hẹn giờ tắt màn hình",
  [UI_TXT_MEDIUM_STRENGTH] = "Độ mạnh: Trung bình",
  [UI_TXT_CALLS] = "Tin nhắn",
  [UI_TXT_ALARMS] = "Báo thức",
  [UI_TXT_ENGLISH] = "Tiếng Anh",
  [UI_TXT_VIETNAMESE] = "Tiếng Việt",
  [UI_TXT_HEART_RATE] = "Nhịp tim",
  [UI_TXT_WAITING_MAX30102] = "Đang chờ dữ liệu nhịp tim",
  [UI_TXT_FIRMWARE] = "Phần mềm Smartwatch",
  [UI_TXT_VERSION] = "Phiên bản",
  [UI_TXT_HARDWARE] = "Phần cứng",
  [UI_TXT_DEFAULT] = "Mặc định",
  [UI_TXT_CHOOSE_COLOR] = "Chọn màu:",
  [UI_TXT_SIMPLE_DIGITAL] = "Số đơn giản",
  [UI_TXT_GRADIENT_MODERN] = "Gradient hiện đại",
  [UI_TXT_CLASSIC_ANALOG] = "Kim cổ điển",
  [UI_TXT_CONNECT_BLE] = "Kết nối BLE",
  [UI_TXT_BLUETOOTH_OFF] = "Bluetooth: Tắt",
  [UI_TXT_START_BLE] = "Phát BLE",
  [UI_TXT_CONNECTED] = "Đã kết nối",
  [UI_TXT_DISCONNECT] = "Ngắt kết nối",
  [UI_TXT_CONNECTING] = "Đang kết nối...",
  [UI_TXT_STOP_BLE] = "Tắt BLE",
  [UI_TXT_NOTIFICATIONS_COUNT] = "%d thông báo",
  [UI_TXT_CONNECT_WIFI] = "Kết nối Wi-Fi",
  [UI_TXT_CONNECT_WIFI_TITLE] = "Hãy kết nối Wi-Fi",
  [UI_TXT_CONNECT_WIFI_OTA_DESC] = "Kết nối Wi-Fi để kiểm tra\nvà cập nhật phần mềm mới.",
  [UI_TXT_CONNECTING_SERVER] = "Đang kết nối máy chủ...",
  [UI_TXT_OTA_SUCCESS] = "Cập nhật thành công",
  [UI_TXT_OTA_FAILED] = "Cập nhật thất bại",
  [UI_TXT_READY_RESTART] = "Sẵn sàng khởi động lại",
  [UI_TXT_RESTART] = "Khởi động lại",
  [UI_TXT_RETRY] = "Thử lại",
  [UI_TXT_STARTING_OTA] = "Đang bắt đầu OTA...",
  [UI_TXT_READY_OTA] = "Sẵn sàng cập nhật OTA",
  [UI_TXT_OTA_NOT_CONFIGURED] = "Chưa cấu hình OTA",
  [UI_TXT_CURRENT_VERSION] = "Hiện tại: %s",
  [UI_TXT_MISSING_URL] = "Thiếu URL",
  [UI_TXT_STOPWATCH] = "Bấm giờ",
  [UI_TXT_READY] = "Sẵn sàng",
  [UI_TXT_TIMING] = "Đang đếm thời gian",
  [UI_TXT_LAP] = "Vòng",
  [UI_TXT_RUNNING] = "Chạy bộ",
  [UI_TXT_OUTDOOR_RUN] = "Chạy ngoài trời",
  [UI_TXT_CYCLING] = "Đạp xe",
  [UI_TXT_OUTDOOR_RIDE] = "Đạp xe ngoài trời",
  [UI_TXT_DISTANCE] = "Quãng đường",
  [UI_TXT_STEPS] = "Bước",
  [UI_TXT_SPEED] = "Tốc độ",
  [UI_TXT_PACE] = "Nhịp độ",
  [UI_TXT_NOTIFICATIONS] = "Thông báo",
  [UI_TXT_NO_NOTIFICATIONS] = "Chưa có thông báo.\nHãy kết nối BLE với\nđiện thoại để nhận\ntin nhắn.",
  [UI_TXT_NEED_BLE] = "Hãy kết nối BLE",
  [UI_TXT_PHONE] = "Điện thoại",
  [UI_TXT_WATCH] = "Đồng hồ",
  [UI_TXT_BLE_GPS_DESC] = "Kết nối với điện thoại để đồng bộ\nGPS, bản đồ và tin nhắn.",
  [UI_TXT_GPS_NAVIGATING] = "Đang dẫn đường...",
  [UI_TXT_NEXT_STEP] = "Bước tiếp theo",
  [UI_TXT_REMAINING] = "Còn lại: --",
  [UI_TXT_UNSUPPORTED] = "Chưa hỗ trợ trong firmware hiện tại.",
  [UI_TXT_OK] = "OK",
  [UI_TXT_CANCEL] = "Hủy",
  [UI_TXT_POWER_OFF_CONFIRM] = "Thiết bị sẽ khởi động lại ngay bây giờ.",
  [UI_TXT_COUNT_LABEL] = "Số lượng",
};

/* Bản sao chuỗi ký tự động cấp phát an toàn (Dynamic String Duplication)
   Cấp phát vùng nhớ động trên Heap đủ dung lượng chứa chuỗi và ký tự kết thúc '\0',
   tránh lỗi con trỏ lơ lửng (dangling pointer) khi đối tượng nguồn bị giải phóng. */
char *ui_safe_strdup(const char *s) {
  if (!s) s = "";
  size_t len = strlen(s) + 1;
  char *copy = malloc(len);
  if (copy) {
    memcpy(copy, s, len);
  }
  return copy;
}

lv_obj_t *ui_make_card(lv_obj_t *parent, lv_coord_t w, lv_coord_t h) {
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_set_size(card, w, h);
  lv_obj_set_style_bg_color(card, COLOR_SURFACE, 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, R_CARD, 0);
  lv_obj_set_style_border_width(card, HAIRLINE_WIDTH, 0);
  lv_obj_set_style_border_color(card, HAIRLINE_COLOR, 0);
  lv_obj_set_style_border_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(card, SP_MD, 0);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  return card;
}

lv_obj_t *ui_make_icon_circle(lv_obj_t *parent, lv_color_t color, lv_coord_t d) {
  lv_obj_t *circle = lv_obj_create(parent);
  lv_obj_set_size(circle, d, d);
  lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(circle, color, 0);
  lv_obj_set_style_bg_opa(circle, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(circle, 0, 0);
  lv_obj_set_style_pad_all(circle, 0, 0);
  lv_obj_clear_flag(circle, LV_OBJ_FLAG_SCROLLABLE);
  return circle;
}

bool watch_alarm_is_ringing(void);

void ui_haptic_pulse(uint32_t duration_ms) {
  if (watch_alarm_is_ringing()) return;
  const watch_settings_t *settings = watch_settings_get();
  if (!settings || !settings->vibration_enabled) return;
  watch_vibration_pulse(settings->vibration_strength, duration_ms ? duration_ms : 30);
}

void ui_haptic_feedback(ui_haptic_type_t type) {
  if (watch_alarm_is_ringing()) return;
  const watch_settings_t *settings = watch_settings_get();
  if (!settings || !settings->vibration_enabled) return;

  if (type == UI_HAPTIC_WARNING) {
    watch_vibration_notify(settings->vibration_strength);
  } else {
    watch_vibration_pulse(settings->vibration_strength,
                          type == UI_HAPTIC_CONFIRM ? 55 : 25);
  }
}

void ui_enable_list_scrolling(lv_obj_t *list) {
  if (!list) return;
  lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLL_MOMENTUM);
  lv_obj_set_scroll_snap_y(list, LV_SCROLL_SNAP_START);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_scroll_dir(list, LV_DIR_VER); // Avoid horizontal scrolling so swipe-back gesture works reliably
}

/* Ánh xạ màu sắc đặc trưng của ứng dụng Smartphone gửi thông báo
   Trả về màu sắc chủ đạo của app (ví dụ Zalo màu xanh nước biển, Messenger xanh nhạt,
   SMS xanh lá...) để hiển thị biểu tượng thông báo sinh động hơn. */
lv_color_t ui_app_color(const char *app_name) {
  if (!app_name) return COLOR_PURPLE;
  if (strcmp(app_name, "Messenger") == 0 || strcmp(app_name, "messenger") == 0) {
    return lv_color_hex(0x0084FF); // Màu xanh Messenger
  }
  if (strcmp(app_name, "Zalo") == 0 || strcmp(app_name, "zalo") == 0) {
    return lv_color_hex(0x0068FF); // Màu xanh Zalo
  }
  if (strcmp(app_name, "Telegram") == 0 || strcmp(app_name, "telegram") == 0) {
    return lv_color_hex(0x0088CC); // Màu xanh Telegram
  }
  if (strcmp(app_name, "SMS") == 0 || strcmp(app_name, "sms") == 0) {
    return COLOR_GREEN;            // Màu xanh lá cho tin nhắn SMS hệ thống
  }
  return COLOR_PURPLE;             // Màu mặc định
}

/* So sánh và cập nhật văn bản nhãn (Label Text Refresh Cache)
   Kiểm tra xem chuỗi văn bản mới có khác chuỗi hiện hành không. Nếu giống nhau,
   bỏ qua việc vẽ lại để tiết kiệm CPU và tài nguyên dựng hình của LVGL. */
void ui_label_set_text_if_changed(lv_obj_t *label, const char *text) {
  if (!label) return;
  if (!text) text = "";

  const char *current = lv_label_get_text(label);
  if (current && strcmp(current, text) == 0) return;

  lv_label_set_text(label, text);
}

/* Đọc thông tin thời gian thực từ API POSIX time và cấu trúc tm
   Chụp nhanh (Snapshot) các trường giờ, phút, giây, ngày, tháng và thứ trong tuần. */
void ui_time_get_snapshot(ui_time_snapshot_t *snapshot) {
  if (!snapshot) return;

  time_t now;
  struct tm ti;
  time(&now);
  localtime_r(&now, &ti); // Sử dụng hàm thread-safe thay thế cho localtime

  snapshot->hour = ti.tm_hour;
  snapshot->minute = ti.tm_min;
  snapshot->second = ti.tm_sec;
  snapshot->day = ti.tm_mday;
  snapshot->month = ti.tm_mon + 1; // tm_mon chạy từ 0 đến 11
  snapshot->weekday = ti.tm_wday;
}

void ui_time_set_timezone_offset(int offset_minutes) {
  if (offset_minutes < -14 * 60) offset_minutes = -14 * 60;
  if (offset_minutes > 14 * 60) offset_minutes = 14 * 60;

  int abs_minutes = offset_minutes < 0 ? -offset_minutes : offset_minutes;
  char tz[24];
  /* POSIX TZ uses the inverse sign: UTC-7 means local time is UTC+7. */
  snprintf(tz, sizeof(tz), "UTC%c%02d:%02d",
           offset_minutes >= 0 ? '-' : '+',
           abs_minutes / 60, abs_minutes % 60);
  setenv("TZ", tz, 1);
  tzset();
}

bool ui_time_is_synced(void) {
  const time_t valid_epoch = 1704067200; /* 2024-01-01 00:00:00 UTC */
  return time(NULL) >= valid_epoch;
}

/* Định dạng thời gian thành chuỗi biểu diễn dạng "HH:MM" */
void ui_time_format_hhmm(char *buf, size_t buf_size, const ui_time_snapshot_t *snapshot) {
  if (!buf || buf_size == 0 || !snapshot) return;
  snprintf(buf, buf_size, "%02d:%02d", snapshot->hour, snapshot->minute);
}

/* Lấy ngôn ngữ hiện tại của giao diện người dùng */
ui_language_t ui_language_get(void) {
  return s_ui_language;
}

/* Thiết lập ngôn ngữ hiển thị hệ thống */
void ui_language_set(ui_language_t lang) {
  s_ui_language = (lang == UI_LANG_VI) ? UI_LANG_VI : UI_LANG_EN;
}

/* Trích xuất văn bản tương ứng với ngôn ngữ đã cài đặt
   Sử dụng chỉ số ID của từ khóa để tìm trong mảng dịch thuật s_text_vi hoặc s_text_en. */
const char *ui_tr(ui_text_id_t id) {
  if (id < 0 || id >= UI_TXT_COUNT) return "";
  const char *text = (s_ui_language == UI_LANG_VI) ? s_text_vi[id] : s_text_en[id];
  return text ? text : "";
}

/* Chọn động chuỗi tiếng Anh hoặc tiếng Việt dựa trên trạng thái ngôn ngữ */
const char *ui_text(const char *en, const char *vi) {
  return (s_ui_language == UI_LANG_VI && vi) ? vi : (en ? en : "");
}
