/* Quản lý điều hướng ngăn xếp màn hình và Status Bar hệ thống.
   Hỗ trợ Navigation Stack, khôi phục trạng thái từ RTC Fast Memory sau Deep Sleep,
   và hiển thị thông tin Pin, WiFi, Bluetooth trên Status Bar. */

#include "ui_navigation.h"
#include "board_config.h"
#include "ui.h"
#include "ui_screens.h"
#include "ui_utils.h"
#include "network_state.h"
#include "bluetooth_manager.h"
#include "hardware_i2c_sensor.h"
#include "ota_service.h"
#include "gps_tracker.h"
#include "imu.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static const char *TAG = "UI_NAV";

/* Khai báo ngoại vi các phông chữ và biểu tượng icon chuyển đổi từ FontAwesome */
LV_FONT_DECLARE(bat_icon);
LV_FONT_DECLARE(menu_icon_2);
LV_FONT_DECLARE(wifi_16);
LV_FONT_DECLARE(ble);

/* Mã Hex biểu diễn các glyph icon trong bộ font custom */
#define ICON_BAT_FULL          "\xef\x89\x80"
#define ICON_BAT_THREE_QUARTER "\xef\x89\x81"
#define ICON_BAT_HALF          "\xef\x89\x82"
#define ICON_BAT_QUARTER       "\xef\x89\x83"
#define ICON_BAT_EMPTY         "\xef\x89\x84"
#define ICON_STATUS_WIFI       "\xef\x87\xab"
#define ICON_STATUS_BT         "\xef\x8a\x94"

/* ===== Cấu trúc ngăn xếp điều hướng (Navigation Stack) ===== */
static lv_obj_t *s_nav_stack[NAV_STACK_MAX];
static ui_screen_id_t s_nav_screen_ids[NAV_STACK_MAX];
static int s_nav_top = -1;

// Mã xác thực dữ liệu định tuyến trong RTC Fast Memory
#define UI_NAV_RTC_MAGIC 0x57415443U
RTC_DATA_ATTR static uint32_t s_rtc_screen_magic;
RTC_DATA_ATTR static uint32_t s_rtc_last_screen_id;

static ui_screen_id_t s_current_screen_id = UI_SCREEN_WATCHFACE;
static ui_screen_id_t s_last_screen_id = UI_SCREEN_WATCHFACE;

/* ===== Các phần tử đồ họa của thanh trạng thái (Status Bar Widgets) ===== */
static lv_obj_t *s_statusbar_time_label;
static lv_obj_t *s_statusbar_battery_pct_label;
static lv_obj_t *s_statusbar_battery_label;
static lv_obj_t *s_statusbar_wifi_label;
static lv_obj_t *s_statusbar_bt_label;
static lv_timer_t *s_statusbar_timer = NULL;
static uint32_t s_statusbar_last_battery_ms = 0;
static uint32_t s_active_backlight_duty = WATCH_LCD_BACKLIGHT_DUTY_DEFAULT;
static bool s_backlight_dimmed = false;
static bool s_backlight_off = false;
static lv_point_t s_gesture_press_point;
static bool s_gesture_started_at_left_edge;

// Vùng biên nhận cử chỉ Back vuốt từ mép trái
#define UI_BACK_EDGE_WIDTH 50

// Chu kỳ làm tươi thời gian và kết nối trên Status Bar (1 giây)
#define STATUSBAR_REFRESH_MS 1000

// Chu kỳ cập nhật chỉ số pin trên Status Bar (5 giây)
#define STATUSBAR_BATTERY_REFRESH_MS 5000

/* Kiểm tra xem một ID màn hình có nằm trong dải hợp lệ hay không */
static bool ui_navigation_screen_id_is_valid(ui_screen_id_t screen_id) {
  return screen_id > UI_SCREEN_INVALID && screen_id <= UI_SCREEN_ABOUT;
}

/* Xác định xem màn hình này có thể được khôi phục sau khi thức dậy hay không
   Các màn hình menu cài đặt hoặc đo lường cần khôi phục lại để tránh người dùng bị mất phiên làm việc. */
static bool ui_navigation_screen_is_restorable(ui_screen_id_t screen_id) {
  switch (screen_id) {
  case UI_SCREEN_ACTIVITY:
  case UI_SCREEN_RUNNING:
  case UI_SCREEN_CYCLING:
  case UI_SCREEN_NOTIFICATIONS:
  case UI_SCREEN_GPS:
  case UI_SCREEN_SETTINGS:
  case UI_SCREEN_WIFI:
  case UI_SCREEN_UPDATE:
  case UI_SCREEN_CONNECT:
  case UI_SCREEN_HEALTH:
  case UI_SCREEN_ALARM:
  case UI_SCREEN_SPO2:
  case UI_SCREEN_STOPWATCH:
  case UI_SCREEN_PERSONALITY_SETTINGS:
  case UI_SCREEN_DISPLAY_SETTINGS:
  case UI_SCREEN_VIBRATION_SETTINGS:
  case UI_SCREEN_LANGUAGE_SETTINGS:
  case UI_SCREEN_SET_TIME:
  case UI_SCREEN_SYSTEM_SETTINGS:
  case UI_SCREEN_ABOUT:
    return true;
  default:
    return false;
  }
}

/* Lấy mã ID màn hình được lưu trong User Data của đối tượng lv_obj_t */
static ui_screen_id_t ui_navigation_screen_id_from_obj(lv_obj_t *scr) {
  if (!scr) return UI_SCREEN_INVALID;
  return (ui_screen_id_t)(uintptr_t)lv_obj_get_user_data(scr);
}

/* Ghi nhớ màn hình hiện tại vào RTC RAM trước khi đi ngủ sâu */
static void ui_navigation_persist_last_screen(ui_screen_id_t screen_id) {
  if (!ui_navigation_screen_is_restorable(screen_id)) return;
  s_last_screen_id = screen_id;
  s_rtc_screen_magic = UI_NAV_RTC_MAGIC;
  s_rtc_last_screen_id = (uint32_t)screen_id;
}

/* Ghi nhận màn hình đang hoạt động trên màn hình */
static void ui_navigation_note_active_screen(lv_obj_t *scr) {
  ui_screen_id_t screen_id = ui_navigation_screen_id_from_obj(scr);
  if (!ui_navigation_screen_id_is_valid(screen_id)) return;
  s_current_screen_id = screen_id;
  ui_navigation_persist_last_screen(screen_id);
}

/* Đồng bộ trạng thái hiển thị của các thành phần Status Bar phù hợp với màn hình hiện tại */
static void ui_navigation_sync_statusbar_for_screen(ui_screen_id_t screen_id) {
  ui_statusbar_set_time_visible(screen_id != UI_SCREEN_WATCHFACE);
}

/* Tạo đối tượng màn hình dựa trên mã ID được yêu cầu */
static lv_obj_t *ui_navigation_create_screen_for_id(ui_screen_id_t screen_id) {
  switch (screen_id) {
  case UI_SCREEN_MENU: return ui_menu_screen_create();
  case UI_SCREEN_ACTIVITY: return ui_activity_screen_create();
  case UI_SCREEN_RUNNING: return ui_running_screen_create();
  case UI_SCREEN_CYCLING: return ui_cycling_screen_create();
  case UI_SCREEN_NOTIFICATIONS: return ui_notifications_screen_create();
  case UI_SCREEN_GPS: return ui_gps_screen_create();
  case UI_SCREEN_SETTINGS: return ui_settings_screen_create();
  case UI_SCREEN_WIFI: return ui_wifi_screen_create();
  case UI_SCREEN_UPDATE: return ui_update_screen_create();
  case UI_SCREEN_CONNECT: return ui_connect_screen_create();
  case UI_SCREEN_HEALTH: return ui_health_screen_create();
  case UI_SCREEN_ALARM: return ui_alarm_list_screen_create();
  case UI_SCREEN_SPO2: return ui_spo2_screen_create();
  case UI_SCREEN_STOPWATCH: return ui_stopwatch_screen_create();
  case UI_SCREEN_PERSONALITY_SETTINGS: return ui_personality_settings_screen_create();
  case UI_SCREEN_DISPLAY_SETTINGS: return ui_display_settings_screen_create();
  case UI_SCREEN_VIBRATION_SETTINGS: return ui_vibration_settings_screen_create();
  case UI_SCREEN_LANGUAGE_SETTINGS: return ui_language_settings_screen_create();
  case UI_SCREEN_SET_TIME: return ui_set_time_screen_create();
  case UI_SCREEN_SYSTEM_SETTINGS: return ui_system_settings_screen_create();
  case UI_SCREEN_ABOUT: return ui_about_screen_create();
  default: return NULL;
  }
}



/* Ánh xạ màu sắc hiển thị phần trăm Pin tương ứng
   - pct: Phần trăm pin thực tế
   Trả về: lv_color_t Màu sắc (Xanh lá > 50%, Cam > 20%, Đỏ <= 20%) */
static lv_color_t ui_statusbar_battery_color(float pct) {
  if (pct > 50.0f) return COLOR_BATTERY;        /* Mức pin an toàn - màu xanh lá */
  if (pct > 20.0f) return COLOR_ORANGE;         /* Mức trung bình - cảnh báo màu cam */
  return COLOR_RED;                              /* Mức pin yếu - khẩn cấp màu đỏ */
}

/* Hàm Callback gọi theo chu kỳ của Timer làm tươi Status Bar (statusbar_refresh_timer_cb)
   Thực hiện:
   1. Làm mới hiển thị giờ trên Status Bar.
   2. Kiểm tra trạng thái kết nối Wi-Fi & BLE hiện hành để ẩn/hiển thị icon tương ứng.
   3. Kiểm tra thời gian nhàn rỗi (inactivity timeout) của người dùng để giảm độ sáng đèn nền hoặc đi vào Deep Sleep.
   4. Truy vấn chỉ số dung lượng pin từ IC Gauge MAX17048G và vẽ lại giao diện chỉ báo pin. */
static void statusbar_refresh_timer_cb(lv_timer_t *t) {
  (void)t;
  ui_runtime_mark_alive();
  if (ui_is_sleep_in_progress()) return;

  /* Tự động tắt màn hình nếu quá thời gian nhàn rỗi (ngoại trừ khi có hoạt động thể thao/GPS đang chạy) */
  if (!ui_navigation_is_backlight_off() && !ui_navigation_has_active_session()) {
    int timeout_sec = hardware_i2c_sensor_get_screen_timeout();
    if (timeout_sec > 0) {
      uint32_t inactive_ms = lv_disp_get_inactive_time(NULL);
      if (inactive_ms >= (uint32_t)timeout_sec * 1000) {
        ESP_LOGI(TAG, "Inactivity timeout reached (%d s). Turning off backlight.", timeout_sec);
        ui_navigation_toggle_backlight();
      }
    }
  }

  ui_statusbar_update_time();
  ui_statusbar_set_wifi_connected(watch_network_is_wifi_connected());
  ui_statusbar_set_bt_connected(watch_bluetooth_is_connected());

  /* Kiểm tra và tự hủy bài tập chạy nền quá 1 giờ */
  ui_activity_check_background_timeout();

  /* Tự động tắt Wi-Fi nếu không dùng (không ở màn hình Wi-Fi, màn hình Update, và OTA không chạy) */
  static uint32_t s_wifi_inactive_sec = 0;
  if (watch_network_is_wifi_started()) {
    watch_ota_status_t ota_st;
    watch_ota_get_status(&ota_st);
    bool wifi_in_use = (s_current_screen_id == UI_SCREEN_WIFI) || 
                       (s_current_screen_id == UI_SCREEN_UPDATE) || 
                       (ota_st.state == WATCH_OTA_RUNNING);
    if (!wifi_in_use) {
      s_wifi_inactive_sec++;
      if (s_wifi_inactive_sec >= 300) {
        ESP_LOGI(TAG, "Wi-Fi không sử dụng trong 5 phút. Tự động tắt Wi-Fi để tiết kiệm pin.");
        ESP_ERROR_CHECK_WITHOUT_ABORT(watch_network_wifi_stop());
        s_wifi_inactive_sec = 0;
      }
    } else {
      s_wifi_inactive_sec = 0;
    }
  } else {
    s_wifi_inactive_sec = 0;
  }

  /* Làm tươi mức SOC (%) của Pin định kỳ mỗi 5 giây */
  uint32_t now_ms = lv_tick_get();
  if (s_statusbar_last_battery_ms != 0 &&
      (uint32_t)(now_ms - s_statusbar_last_battery_ms) < STATUSBAR_BATTERY_REFRESH_MS) {
    return;
  }
  s_statusbar_last_battery_ms = now_ms;

  /* Giao tiếp bus I2C đọc dữ liệu điện áp từ MAX17048G */
  watch_battery_data_t bat;
  if (hardware_i2c_sensor_get_battery(&bat)) {
    ui_statusbar_set_battery(bat.soc_percent);
  }
}

/* Tạo đối tượng màn hình LVGL thô làm nền tối tối ưu năng lượng
   Quy ước quản lý bộ nhớ của bộ điều hướng (Navigation Stack Memory Cleanup):
   - Khi đẩy một màn hình bằng `ui_navigation_push()`, ngăn xếp sẽ nắm quyền sở hữu màn hình đó.
   - Khi thực hiện `ui_navigation_pop()`, màn hình bị đẩy ra sẽ tự động bị xóa nhờ cơ chế tự giải phóng của LVGL.
   - Màn hình chứa Timer, vùng nhớ động, hoặc dữ liệu con trỏ phải tự đăng ký Callback sự kiện `LV_EVENT_DELETE`
   để dọn dẹp heap riêng. */
lv_obj_t *ui_navigation_create_screen(void) {
  lv_obj_t *scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr, COLOR_BG, 0); // Nền đen tuyền
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_top(scr, SCREEN_SAFE_TOP, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  return scr;
}

/* Tạo đối tượng màn hình mới có gán nhãn ID định dạng */
lv_obj_t *ui_navigation_create_screen_with_id(ui_screen_id_t screen_id) {
  lv_obj_t *scr = ui_navigation_create_screen();
  ui_navigation_set_screen_id(scr, screen_id);
  if (screen_id == UI_SCREEN_WATCHFACE) {
    lv_obj_set_style_pad_top(scr, 0, 0);
  }
  ui_navigation_add_gesture(scr);
  return scr;
}

/* Thiết lập định danh ID màn hình thông qua User Data của lv_obj_t */
void ui_navigation_set_screen_id(lv_obj_t *scr, ui_screen_id_t screen_id) {
  if (!scr) return;
  lv_obj_set_user_data(scr, (void *)(uintptr_t)screen_id);
}

/* Lấy mã màn hình lưu trong RTC gần nhất */
ui_screen_id_t ui_navigation_get_last_screen_id(void) {
  return s_last_screen_id;
}

static bool ui_navigation_is_hor_scroll_active(lv_obj_t *obj) {
  while (obj && lv_obj_is_valid(obj)) {
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_SCROLLABLE) && (lv_obj_get_scroll_dir(obj) & LV_DIR_HOR)) {
      if (lv_obj_get_scroll_left(obj) > 0) {
        return true;
      }
    }
    obj = lv_obj_get_parent(obj);
  }
  return false;
}

static void (*s_orig_read_cb)(struct _lv_indev_drv_t * indev_drv, lv_indev_data_t * data) = NULL;
static lv_indev_state_t s_last_raw_state = LV_INDEV_STATE_RELEASED;
static uint32_t s_last_press_ms = 0;

static void custom_touch_read_cb(struct _lv_indev_drv_t * indev_drv, lv_indev_data_t * data) {
  if (s_orig_read_cb) {
    s_orig_read_cb(indev_drv, data);
  }

  bool backlight_off = ui_navigation_is_backlight_off();

  // Phát hiện nhấn phím thô để tính thời gian double-tap
  if (data->state == LV_INDEV_STATE_PRESSED && s_last_raw_state == LV_INDEV_STATE_RELEASED) {
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);

    if (backlight_off && hardware_i2c_sensor_is_quick_wake_enabled()) {
      uint32_t diff = now - s_last_press_ms;
      ESP_LOGI("QUICK_WAKE", "Chạm phát hiện khi màn hình tắt, diff = %lu ms", (unsigned long)diff);
      if (diff >= 100 && diff <= 500) {
        ESP_LOGI("QUICK_WAKE", "Chạm đúp thành công (Double tap), mở sáng màn hình!");
        ui_navigation_toggle_backlight();
        s_last_press_ms = 0; // Đặt lại để tránh chạm phát thứ 3 kích hoạt tiếp
      } else {
        s_last_press_ms = now;
      }
    } else {
      s_last_press_ms = now;
    }
  }
  s_last_raw_state = data->state;

  // Nếu màn hình đang tắt, loại bỏ sự kiện Pressed gửi lên các Widget của LVGL
  if (backlight_off) {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

/* Phản hồi cử chỉ từ thiết bị nhập Pointer để hỗ trợ vuốt ngược toàn cục */
static void ui_navigation_indev_feedback_cb(struct _lv_indev_drv_t *drv, uint8_t code) {
  if (drv->type != LV_INDEV_TYPE_POINTER) return;
  lv_indev_t *indev = lv_indev_get_act();
  if (!indev) return;

  if (code == LV_EVENT_PRESSED) {
    lv_indev_get_point(indev, &s_gesture_press_point);
    s_gesture_started_at_left_edge = s_gesture_press_point.x <= UI_BACK_EDGE_WIDTH;
    ESP_LOGI("GESTURE_DEBUG", "GLOBAL PRESSED: x=%d, y=%d, from_edge=%d, top=%d", 
             (int)s_gesture_press_point.x, (int)s_gesture_press_point.y, 
             s_gesture_started_at_left_edge, s_nav_top);
  } else if (code == LV_EVENT_RELEASED) {
    ESP_LOGI("GESTURE_DEBUG", "GLOBAL RELEASED: started_edge=%d", s_gesture_started_at_left_edge);
    if (s_gesture_started_at_left_edge) {
      s_gesture_started_at_left_edge = false; // Prevent double-triggering
      lv_point_t release_point;
      lv_indev_get_point(indev, &release_point);
      lv_coord_t dx = release_point.x - s_gesture_press_point.x;
      lv_coord_t dy = release_point.y - s_gesture_press_point.y;
      if (dy < 0) dy = -dy;

      ESP_LOGI("GESTURE_DEBUG", "GLOBAL dx=%d, dy=%d", (int)dx, (int)dy);
      // Vuốt ngược tùy chỉnh: vuốt sang phải >= 50px, lệch dọc <= 50px
      if (s_nav_top > 0 && dx >= 50 && dy <= 50) {
        ESP_LOGI("GESTURE_DEBUG", "GLOBAL Pop screen!");
        ui_navigation_pop();
      }
    }
  }
}

/* Đăng ký bắt cử chỉ vuốt quay lại cho một màn hình bất kỳ */
void ui_navigation_add_gesture(lv_obj_t *scr) {
  // Bộ phản hồi cử chỉ toàn cục (global feedback_cb) trên pointer indev đã xử lý tất cả các cử chỉ.
  // Không cần thiết lập callback cử chỉ cục bộ.
  (void)scr;
}

/* Thiết lập độ sáng hiệu dụng đèn nền LCD */
void ui_backlight_set_active_duty(uint32_t duty) {
  s_active_backlight_duty = duty;
}

/* Đọc độ sáng hiệu dụng hiện tại của đèn nền LCD */
uint32_t ui_backlight_get_active_duty(void) {
  return s_active_backlight_duty;
}


/* Khởi tạo ngăn xếp điều hướng
   Thiết lập các con trỏ màn hình về NULL và kiểm tra xem vùng nhớ RTC
   có lưu trữ thông tin màn hình trước đó từ đợt Deep Sleep trước hay không. */
void ui_navigation_init(void) {
  s_nav_top = -1;
  memset(s_nav_stack, 0, sizeof(s_nav_stack));
  memset(s_nav_screen_ids, 0, sizeof(s_nav_screen_ids));
  s_current_screen_id = UI_SCREEN_WATCHFACE;
  
  /* Đọc mã hợp lệ (magic word) từ RTC để khôi phục trạng thái cũ */
  if (s_rtc_screen_magic == UI_NAV_RTC_MAGIC &&
      ui_navigation_screen_is_restorable((ui_screen_id_t)s_rtc_last_screen_id)) {
    s_last_screen_id = (ui_screen_id_t)s_rtc_last_screen_id;
  } else {
    s_last_screen_id = UI_SCREEN_WATCHFACE;
  }

  /* Đăng ký phản hồi của thiết bị nhập để bắt vuốt cử chỉ toàn cục */
  lv_indev_t * indev = lv_indev_get_next(NULL);
  while(indev) {
    if(indev->driver->type == LV_INDEV_TYPE_POINTER) {
      indev->driver->feedback_cb = ui_navigation_indev_feedback_cb;
      ESP_LOGI(TAG, "Registered global feedback callback for pointer input device");
      if (indev->driver->read_cb != custom_touch_read_cb) {
        s_orig_read_cb = indev->driver->read_cb;
        indev->driver->read_cb = custom_touch_read_cb;
        ESP_LOGI(TAG, "Hooked touch read_cb for double-tap wake function");
      }
    }
    indev = lv_indev_get_next(indev);
  }
}

/* Đẩy một màn hình mới vào ngăn xếp và hiển thị lên màn hình
   - scr: Đối tượng màn hình LVGL được khởi tạo */
void ui_navigation_push(lv_obj_t *scr) {
  if (!scr)
    return;
  if (s_nav_top >= NAV_STACK_MAX - 1) {
    lv_obj_del(scr);
    ESP_LOGW(TAG, "Navigation stack full; dropped new screen");
    return;
  }
  bool is_root_load = (s_nav_top < 0);
  s_nav_top++;
  s_nav_stack[s_nav_top] = scr;
  s_nav_screen_ids[s_nav_top] = ui_navigation_screen_id_from_obj(scr);
  ui_navigation_note_active_screen(scr);
  
  /* Cập nhật hiển thị Status Bar thích hợp */
  ui_navigation_sync_statusbar_for_screen(s_nav_screen_ids[s_nav_top]);
  
  /* Tải màn hình LVGL mới lên (Load Screen) mà không có hiệu ứng chuyển cảnh chậm để tăng tốc */
  if (!is_root_load) ui_haptic_feedback(UI_HAPTIC_TICK);
  lv_scr_load_anim(scr,
                   is_root_load ? LV_SCR_LOAD_ANIM_NONE : LV_SCR_LOAD_ANIM_MOVE_LEFT,
                   is_root_load ? 0 : 150, 0, false);
}

void ui_navigation_replace(lv_obj_t *scr) {
  if (!scr)
    return;
  if (s_nav_top < 0) {
    lv_obj_del(scr);
    return;
  }

  s_nav_stack[s_nav_top] = scr;
  s_nav_screen_ids[s_nav_top] = ui_navigation_screen_id_from_obj(scr);
  ui_navigation_note_active_screen(scr);
  ui_navigation_sync_statusbar_for_screen(s_nav_screen_ids[s_nav_top]);

  ui_haptic_feedback(UI_HAPTIC_TICK);
  lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 150, 0, true);
}

/* Đẩy màn hình trên cùng ra khỏi ngăn xếp (Pop) và quay về màn hình trước đó */
void ui_navigation_pop(void) {
  if (s_nav_top <= 0)
    return;
    
  s_nav_stack[s_nav_top] = NULL;
  s_nav_screen_ids[s_nav_top] = UI_SCREEN_INVALID;
  s_nav_top--;
  
  s_current_screen_id = s_nav_screen_ids[s_nav_top];
  ui_navigation_sync_statusbar_for_screen(s_current_screen_id);
  
  /* Tải màn hình bên dưới lên và đặt thuộc tính auto_del = true để LVGL tự động dọn dẹp màn hình cũ */
  ui_haptic_feedback(UI_HAPTIC_TICK);
  lv_scr_load_anim(s_nav_stack[s_nav_top], LV_SCR_LOAD_ANIM_MOVE_RIGHT, 150, 0, true);
}

/* Quay về màn hình Watchface (Root) và xóa sạch tất cả các màn hình trung gian */
void ui_navigation_pop_to_root(void) {
  if (s_nav_top <= 0)
    return;
    
  int old_top = s_nav_top;
  
  /* Giải phóng bất đồng bộ tất cả màn hình nằm giữa để tránh quá tải RAM tức thời */
  for (int i = 1; i < old_top; i++) {
    if (s_nav_stack[i] && lv_obj_is_valid(s_nav_stack[i])) {
      lv_obj_del_async(s_nav_stack[i]);
    }
    s_nav_stack[i] = NULL;
    s_nav_screen_ids[i] = UI_SCREEN_INVALID;
  }
  s_nav_top = 0;
  
  /* Đánh dấu giải phóng màn hình trên cùng hiện tại khi màn hình gốc được nạp */
  s_nav_stack[old_top] = NULL;
  s_nav_screen_ids[old_top] = UI_SCREEN_INVALID;
  
  s_current_screen_id = s_nav_screen_ids[0];
  ui_navigation_sync_statusbar_for_screen(s_current_screen_id);
  
  /* Hiệu ứng trượt màn hình sang phải mượt mà khi quay về Home */
  ui_haptic_feedback(UI_HAPTIC_TICK);
  lv_scr_load_anim(s_nav_stack[0], LV_SCR_LOAD_ANIM_MOVE_RIGHT, 150, 0, true);
}

void ui_navigation_pop_multiple(int count) {
  if (count <= 0 || s_nav_top <= 0)
    return;
  if (count > s_nav_top)
    count = s_nav_top;

  int old_top = s_nav_top;
  int target_top = s_nav_top - count;

  /* Giải phóng bất đồng bộ tất cả màn hình trung gian để tránh xung đột transition */
  for (int i = target_top + 1; i < old_top; i++) {
    if (s_nav_stack[i] && lv_obj_is_valid(s_nav_stack[i])) {
      lv_obj_del_async(s_nav_stack[i]);
    }
    s_nav_stack[i] = NULL;
    s_nav_screen_ids[i] = UI_SCREEN_INVALID;
  }

  s_nav_top = target_top;

  /* Giải phóng màn hình trên cùng cũ khi màn hình mới được nạp */
  s_nav_stack[old_top] = NULL;
  s_nav_screen_ids[old_top] = UI_SCREEN_INVALID;

  s_current_screen_id = s_nav_screen_ids[s_nav_top];
  ui_navigation_sync_statusbar_for_screen(s_current_screen_id);

  /* Hiệu ứng trượt màn hình sang phải mượt mà về màn hình đích */
  ui_haptic_feedback(UI_HAPTIC_TICK);
  lv_scr_load_anim(s_nav_stack[s_nav_top], LV_SCR_LOAD_ANIM_MOVE_RIGHT, 150, 0, true);
}

/* Tạo mới lại màn hình gốc (Root) và xóa sạch các vết ngăn xếp cũ */
void ui_navigation_recreate_root(lv_obj_t *(*create_cb)(void)) {
  if (!create_cb)
    return;

  int old_top = s_nav_top;
  
  /* Xóa toàn bộ các đối tượng màn hình cũ */
  for (int i = 0; i < old_top; i++) {
    if (s_nav_stack[i] && lv_obj_is_valid(s_nav_stack[i])) {
      lv_obj_del(s_nav_stack[i]);
    }
    s_nav_stack[i] = NULL;
    s_nav_screen_ids[i] = UI_SCREEN_INVALID;
  }

  /* Gọi callback tạo màn hình Watchface mới */
  lv_obj_t *scr = create_cb();
  if (!scr)
    return;

  for (int i = old_top; i < NAV_STACK_MAX; i++) {
    s_nav_stack[i] = NULL;
    s_nav_screen_ids[i] = UI_SCREEN_INVALID;
  }

  s_nav_top = 0;
  s_nav_stack[0] = scr;
  s_nav_screen_ids[0] = ui_navigation_screen_id_from_obj(scr);
  
  ui_navigation_note_active_screen(scr);
  ui_navigation_sync_statusbar_for_screen(s_nav_screen_ids[0]);
  
  lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 150, 0, old_top >= 0);
}

/* Khôi phục lại màn hình đang chạy dở trước khi thiết bị đi ngủ sâu */
bool ui_navigation_restore_last_screen(void) {
  ui_screen_id_t screen_id = s_last_screen_id;
  if (!ui_navigation_screen_is_restorable(screen_id) || screen_id == s_current_screen_id) {
    return false;
  }

  /* Tạo lại màn hình cũ tương ứng với ID khôi phục */
  lv_obj_t *scr = ui_navigation_create_screen_for_id(screen_id);
  if (!scr) {
    ESP_LOGW(TAG, "Cannot restore screen id %d", (int)screen_id);
    return false;
  }

  ui_navigation_pop_to_root();
  ui_navigation_push(scr);
  return true;
}


/* Điều khiển hệ thống chuyển sang chế độ ngủ sâu (Deep Sleep) tiết kiệm năng lượng
   Các bước thực thi bao gồm:
   1. Persist màn hình hiện tại vào RTC để khôi phục khi wakeup.
   2. Tắt hoàn toàn xung PWM điều khiển đèn nền LCD (ledc_stop) và kéo chân GPIO đèn nền xuống mức 0.
   3. Cấu hình chân Touch Interrupt (WATCH_PIN_TOUCH_INT) và Button Power (WATCH_PIN_BUTTON_POWER) ở chế độ ngõ vào có trở kéo lên (Pull-up).
   4. Chờ hai nút nhấn/chạm nhả ra hoàn toàn (mức logic cao) để tránh việc tự động thức giấc ngay lập tức.
   5. Vô hiệu hóa toàn bộ nguồn wakeup cũ, cấu hình Ext1 Wakeup (cho nút bấm vật lý mức LOW) và Ext0 Wakeup (cho cảm biến chạm nếu Quick Wake được kích hoạt).
   6. Tắt nguồn cảm biến GPS, IMU, cảm biến đo nhịp tim để đạt dòng tiêu thụ cực tiểu (chế độ ngủ sâu ESP32-S3 khoảng vài chục uA).
   7. Gọi esp_deep_sleep_start() để bắt đầu quá trình tắt nguồn lõi CPU. */
void ui_navigation_enter_deep_sleep(void) {
  if (ui_navigation_screen_is_restorable(s_current_screen_id)) {
    ui_navigation_persist_last_screen(s_current_screen_id);
  }

  ESP_LOGI(TAG, "Entering deep sleep, last screen id=%d", (int)s_last_screen_id);

  /* Tắt đèn nền LCD */
  ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_stop(LEDC_LOW_SPEED_MODE, WATCH_LCD_BACKLIGHT_LEDC_CH, 0));
  gpio_set_direction(WATCH_PIN_LCD_BACKLIGHT, GPIO_MODE_OUTPUT);
  gpio_set_level(WATCH_PIN_LCD_BACKLIGHT, 0);

  /* Thiết lập GPIO ngắt chạm làm cổng vào có trở kéo lên */
  gpio_config_t touch_pin_cfg = {
      .pin_bit_mask = 1ULL << WATCH_PIN_TOUCH_INT,
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&touch_pin_cfg));

  /* Thiết lập GPIO nút nguồn làm cổng vào có trở kéo lên */
  gpio_config_t btn_pin_cfg = {
      .pin_bit_mask = (1ULL << WATCH_PIN_BUTTON_POWER),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&btn_pin_cfg));

  /* Vòng lặp chờ thả phím / thả chạm */
  for (int i = 0; i < 150 && gpio_get_level(WATCH_PIN_TOUCH_INT) == 0; i++) {
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  for (int i = 0; i < 150 && gpio_get_level(WATCH_PIN_BUTTON_POWER) == 0; i++) {
    vTaskDelay(pdMS_TO_TICKS(20));
  }

  /* Cấu hình các nguồn đánh thức từ chế độ ngủ sâu */
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);

  /* 1. Luôn cấu hình nút bấm vật lý để đánh thức (GPIO 0 & GPIO 1) */
  esp_err_t err = esp_sleep_enable_ext1_wakeup((1ULL << WATCH_PIN_BUTTON_POWER), ESP_EXT1_WAKEUP_ANY_LOW);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Cannot enable button wakeup: %s", esp_err_to_name(err));
  }

  /* 2. Không cấu hình ngắt chạm để đánh thức từ Deep Sleep (chỉ dùng nút nguồn vật lý để tránh tự khởi động lại khi chạm màn hình) */
  ESP_LOGI(TAG, "Touch wakeup disabled for deep sleep (Power off mode)");

  /* Tắt nguồn toàn bộ phần cứng cảm biến phụ trợ để tiết kiệm điện */
  watch_gps_stop();
  hardware_i2c_sensor_set_heart_rate_enabled(false);

  // Tắt cảm biến nhịp tim trước khi CPU ngủ sâu
  extern void heart_rate_shutdown(void);
  heart_rate_shutdown();

  // Đưa IMU vào chế độ ngủ sâu tiết kiệm pin
  imu_sleep();

  // Đưa màn hình vào chế độ ngủ
  extern void watch_lcd_enter_sleep(void);
  watch_lcd_enter_sleep();

  // Đảm bảo dừng hẳn motor rung trước khi ngủ sâu để tránh motor chạy vô tận
  extern void watch_vibration_stop(void);
  watch_vibration_stop();

  // Ép chân GPIO điều khiển motor về mức thấp và kích hoạt pull-down giữ trạng thái trong lúc ngủ
  gpio_config_t vibrator_pin_cfg = {
      .pin_bit_mask = 1ULL << WATCH_PIN_VIBRATOR,
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_ENABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&vibrator_pin_cfg);
  gpio_set_level(WATCH_PIN_VIBRATOR, 0);
  gpio_hold_en(WATCH_PIN_VIBRATOR);

  vTaskDelay(pdMS_TO_TICKS(100));
  
  /* Bắt đầu ngủ sâu */
  esp_deep_sleep_start();
}

/* ===== Thiết lập và tạo Thanh trạng thái (Status Bar) ===== */
void ui_statusbar_create(void) {
  /* Tạo một Container trên Layer Top (luôn hiển thị đè lên màn hình hiện tại) */
  lv_obj_t *bar = lv_obj_create(lv_layer_top());
  lv_obj_set_size(bar, WATCH_LCD_H_RES, 30);
  lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, 0); // Đặt nền trong suốt
  lv_obj_set_style_border_width(bar, 0, 0);
  lv_obj_set_style_pad_all(bar, 0, 0);
  lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

  /* Khởi tạo nhãn thời gian giờ ở góc trái */
  s_statusbar_time_label = lv_label_create(bar);
  lv_obj_set_style_text_color(s_statusbar_time_label, COLOR_TEXT_MUTED, 0);
  lv_obj_set_style_text_font(s_statusbar_time_label, UI_FONT_14, 0);
  lv_label_set_text(s_statusbar_time_label, "00:00");
  lv_obj_align(s_statusbar_time_label, LV_ALIGN_LEFT_MID, 18, 0);

  /* Khởi tạo nhãn icon Pin ở góc phải */
  s_statusbar_battery_label = lv_label_create(bar);
  lv_obj_set_style_text_color(s_statusbar_battery_label, COLOR_BATTERY, 0);
  lv_obj_set_style_text_font(s_statusbar_battery_label, &bat_icon, 0);
  lv_label_set_text(s_statusbar_battery_label, ICON_BAT_FULL);
  lv_obj_align(s_statusbar_battery_label, LV_ALIGN_RIGHT_MID, -18, 0);

  /* Khởi tạo nhãn hiển thị số phần trăm pin bên cạnh icon */
  s_statusbar_battery_pct_label = lv_label_create(bar);
  lv_obj_set_style_text_color(s_statusbar_battery_pct_label, COLOR_TEXT_MUTED, 0);
  lv_obj_set_style_text_font(s_statusbar_battery_pct_label, UI_FONT_12, 0);
  lv_label_set_text(s_statusbar_battery_pct_label, "--%");
  lv_obj_align_to(s_statusbar_battery_pct_label, s_statusbar_battery_label,
                  LV_ALIGN_OUT_LEFT_MID, -4, 0);

  /* Khởi tạo nhãn icon kết nối mạng Wi-Fi */
  s_statusbar_wifi_label = lv_label_create(bar);
  lv_obj_set_style_text_font(s_statusbar_wifi_label, &wifi_16, 0);
  lv_obj_set_style_text_color(s_statusbar_wifi_label, COLOR_PRIMARY, 0);
  lv_label_set_text(s_statusbar_wifi_label, ICON_STATUS_WIFI);
  lv_obj_align(s_statusbar_wifi_label, LV_ALIGN_RIGHT_MID, -82, 0);
  ui_statusbar_set_wifi_connected(watch_network_is_wifi_connected());

  /* Khởi tạo nhãn icon kết nối Bluetooth BLE */
  s_statusbar_bt_label = lv_label_create(bar);
  lv_obj_set_style_text_font(s_statusbar_bt_label, &ble, 0);
  lv_obj_set_style_text_color(s_statusbar_bt_label, COLOR_PRIMARY, 0);
  lv_label_set_text(s_statusbar_bt_label, ICON_STATUS_BT);
  lv_obj_align(s_statusbar_bt_label, LV_ALIGN_RIGHT_MID, -104, 0);
  ui_statusbar_set_bt_connected(watch_bluetooth_is_connected());

  /* Đăng ký timer làm tươi trạng thái Status Bar */
  if (!s_statusbar_timer) {
    s_statusbar_timer = lv_timer_create(statusbar_refresh_timer_cb, STATUSBAR_REFRESH_MS, NULL);
  }
  statusbar_refresh_timer_cb(NULL);
}

/* Ẩn/hiển thị nhãn thời gian trên thanh trạng thái */
void ui_statusbar_set_time_visible(bool visible) {
  if (!s_statusbar_time_label) return;
  if (visible) {
    ui_statusbar_update_time();
    lv_obj_clear_flag(s_statusbar_time_label, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(s_statusbar_time_label, LV_OBJ_FLAG_HIDDEN);
  }
}

/* Làm mới giá trị thời gian hiển thị lấy từ RTC */
void ui_statusbar_update_time(void) {
  ui_time_snapshot_t timeinfo;
  ui_time_get_snapshot(&timeinfo);
  char buf[8];
  ui_time_format_hhmm(buf, sizeof(buf), &timeinfo);
  ui_label_set_text_if_changed(s_statusbar_time_label, buf);
}

/* Thiết lập biểu tượng pin và phần trăm pin tương ứng */
void ui_statusbar_set_battery(float percent) {
  if (!s_statusbar_battery_label) return;
  const char *icon = ICON_BAT_EMPTY;
  
  /* Phân cấp biểu tượng pin dựa trên dung lượng */
  if (percent > 87.0f) icon = ICON_BAT_FULL;
  else if (percent > 62.0f) icon = ICON_BAT_THREE_QUARTER;
  else if (percent > 37.0f) icon = ICON_BAT_HALF;
  else if (percent > 12.0f) icon = ICON_BAT_QUARTER;
  
  ui_label_set_text_if_changed(s_statusbar_battery_label, icon);
  lv_obj_set_style_text_color(s_statusbar_battery_label, ui_statusbar_battery_color(percent), 0);
  
  if (s_statusbar_battery_pct_label) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", (int)(percent + 0.5f));
    ui_label_set_text_if_changed(s_statusbar_battery_pct_label, buf);
    lv_obj_align_to(s_statusbar_battery_pct_label, s_statusbar_battery_label,
                    LV_ALIGN_OUT_LEFT_MID, -4, 0);
  }
}

/* Cập nhật trạng thái hiển thị của Icon Bluetooth (Ẩn nếu mất kết nối, hiện khi có kết nối) */
void ui_statusbar_set_bt_connected(bool connected) {
  if (!s_statusbar_bt_label) return;
  if (connected) {
    lv_obj_clear_flag(s_statusbar_bt_label, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(s_statusbar_bt_label, LV_OBJ_FLAG_HIDDEN);
  }
}

/* Cập nhật trạng thái hiển thị của Icon Wi-Fi */
void ui_statusbar_set_wifi_connected(bool connected) {
  if (!s_statusbar_wifi_label) return;
  if (connected) {
    lv_obj_clear_flag(s_statusbar_wifi_label, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(s_statusbar_wifi_label, LV_OBJ_FLAG_HIDDEN);
  }
}

bool ui_navigation_is_backlight_off(void) {
  return s_backlight_off;
}

bool ui_navigation_has_active_session(void) {
  return s_current_screen_id == UI_SCREEN_RUNNING ||
         s_current_screen_id == UI_SCREEN_CYCLING ||
         s_current_screen_id == UI_SCREEN_GPS ||
         s_current_screen_id == UI_SCREEN_STOPWATCH ||
         s_current_screen_id == UI_SCREEN_UPDATE ||
         watch_gps_is_active();
}

void ui_navigation_toggle_backlight(void) {
  if (s_backlight_off) {
    s_backlight_off = false;
    s_backlight_dimmed = false;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, WATCH_LCD_BACKLIGHT_LEDC_CH, s_active_backlight_duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, WATCH_LCD_BACKLIGHT_LEDC_CH);
    lv_disp_trig_activity(NULL); // Reset LVGL inactivity timer
  } else {
    s_backlight_off = true;
    s_backlight_dimmed = false;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, WATCH_LCD_BACKLIGHT_LEDC_CH, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, WATCH_LCD_BACKLIGHT_LEDC_CH);
  }
}
