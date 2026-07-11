/* Giao diện Mặt đồng hồ chính (Watchface) đa chủ đề (Digital, Modern, Analog).
   Hiển thị thời gian, ngày tháng, đếm bước chân (IMU), dung lượng pin,
   và hỗ trợ cử chỉ vuốt để vào Menu. */

#include "ui.h"
#include "ui_navigation.h"
#include "ui_screens.h"
#include "ui_utils.h"
#include "hardware_i2c_sensor.h"
#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

LV_FONT_DECLARE(bat_icon);

/* Các con trỏ lưu trữ đối tượng hiển thị của mặt đồng hồ */
static lv_obj_t *s_watchface_time_label = NULL;
static lv_obj_t *s_watchface_weekday_label = NULL;
static lv_obj_t *s_watchface_date_label = NULL;
static lv_obj_t *s_watchface_battery_info_label = NULL;
static lv_obj_t *s_watchface_steps_label = NULL;
static lv_obj_t *s_watchface_battery_arc = NULL;
static lv_obj_t *s_watchface_second_hand = NULL;
static lv_obj_t *s_watchface_minute_hand = NULL;
static lv_obj_t *s_watchface_hour_hand = NULL;
static int32_t s_watchface_second_angle = -1;

static lv_point_t s_minute_hand_points[2];
static lv_point_t s_hour_hand_points[2];
static lv_point_t s_second_hand_points[2];
static int s_watchface_battery_arc_value = -1;
static uint32_t s_watchface_battery_arc_color = 0;

static lv_timer_t *s_watchface_time_timer = NULL; // Bộ định thời cập nhật màn hình
static lv_obj_t *s_watchface_scr = NULL;
static int s_watchface_style = 1;                  /* Mặc định style: 1 (0: Simple, 1: Gradient, 2: Analog) */

// Chu kỳ làm tươi mặt đồng hồ dạng số (1 phút)
#define WATCHFACE_DIGITAL_REFRESH_MS 60000

// Chu kỳ làm tươi mặt đồng hồ dạng kim (1 giây)
#define WATCHFACE_ANALOG_REFRESH_MS 1000

// Số lần gõ màn hình liên tiếp để khôi phục nhanh màn hình (Triple Tap)
#define WATCHFACE_RESTORE_TAP_COUNT 3

// Khung thời gian gõ tối đa của cử chỉ (1.5 giây)
#define WATCHFACE_RESTORE_TAP_WINDOW_MS 1500

/* Tên viết tắt các ngày trong tuần phục vụ chuyển đổi ngôn ngữ */
static const char *s_weekday_names_en[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
static const char *s_weekday_names_vi[] = {"Chủ nhật", "Thứ hai", "Thứ ba", "Thứ tư", "Thứ năm", "Thứ sáu", "Thứ bảy"};

/* Chuẩn hóa mã kiểu mặt đồng hồ luôn nằm trong giới hạn [0, 2] */
static int watchface_normalize_style(int style_id) {
  int style = style_id % 3;
  return (style < 0) ? style + 3 : style;
}

/* Xác định chu kỳ timer dựa trên kiểu mặt đồng hồ đang chạy */
static uint32_t watchface_timer_period_ms(void) {
  if (!ui_time_is_synced()) return 1000;
  return (s_watchface_style == 2) ? WATCHFACE_ANALOG_REFRESH_MS : WATCHFACE_DIGITAL_REFRESH_MS;
}

static lv_color_t watchface_battery_soc_color(float soc) {
  if (soc >= 50.0f) return lv_color_hex(0x10B981);
  if (soc >= 20.0f) return lv_color_hex(0xF59E0B);
  return COLOR_RED;
}

static void watchface_set_second_hand_line_angle(void *obj, int32_t angle_10) {
  float angle_rad = (angle_10 / 10.0f) * M_PI / 180.0f;
  int tail_len = 15;
  int head_len = 90;
  s_second_hand_points[0].x = 105 - (int16_t)(tail_len * sinf(angle_rad));
  s_second_hand_points[0].y = 105 + (int16_t)(tail_len * cosf(angle_rad));
  s_second_hand_points[1].x = 105 + (int16_t)(head_len * sinf(angle_rad));
  s_second_hand_points[1].y = 105 - (int16_t)(head_len * cosf(angle_rad));
  lv_line_set_points((lv_obj_t *)obj, s_second_hand_points, 2);
}

static void watchface_add_dial_tick(lv_obj_t *dial, int angle_deg, bool major) {
  const int cx = 105;
  const int cy = 105;
  const int radius = major ? 94 : 96;
  const int size = major ? 4 : 2;
  int x = cx + ((radius * lv_trigo_sin(angle_deg)) >> LV_TRIGO_SHIFT) - (size / 2);
  int y = cy - ((radius * lv_trigo_cos(angle_deg)) >> LV_TRIGO_SHIFT) - (size / 2);

  lv_obj_t *tick = lv_obj_create(dial);
  lv_obj_set_size(tick, size, size);
  lv_obj_set_pos(tick, x, y);
  lv_obj_set_style_bg_color(tick, major ? lv_color_white() : COLOR_SURFACE_LT, 0);
  lv_obj_set_style_bg_opa(tick, major ? LV_OPA_80 : LV_OPA_60, 0);
  lv_obj_set_style_border_width(tick, 0, 0);
  lv_obj_set_style_radius(tick, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_pad_all(tick, 0, 0);
  lv_obj_clear_flag(tick, LV_OBJ_FLAG_SCROLLABLE);
}

static void watchface_add_dial_number(lv_obj_t *dial, const char *text, int angle_deg) {
  const int radius = 74;
  lv_obj_t *label = lv_label_create(dial);
  lv_obj_set_style_text_font(label, UI_FONT_12, 0);
  lv_obj_set_style_text_color(label, COLOR_TEXT_MUTED, 0);
  lv_label_set_text(label, text);
  int dx = (radius * lv_trigo_sin(angle_deg)) >> LV_TRIGO_SHIFT;
  int dy = - ((radius * lv_trigo_cos(angle_deg)) >> LV_TRIGO_SHIFT);
  lv_obj_align(label, LV_ALIGN_CENTER, dx, dy);
}

/* Điều chỉnh lại tần số quét của timer khi đổi kiểu mặt đồng hồ */
static void watchface_sync_timer_period(void) {
  if (!s_watchface_time_timer) return;
  lv_timer_set_period(s_watchface_time_timer, watchface_timer_period_ms());
  lv_timer_reset(s_watchface_time_timer);
}

/* Hàm callback cập nhật dữ liệu hiển thị (Thực hiện chu kỳ quét) */
static void watchface_update_time_cb(lv_timer_t *t) {
  (void)t;
  if (!s_watchface_scr) return;
  ui_time_snapshot_t ti;
  ui_time_get_snapshot(&ti); // Đọc thời gian thực hiện tại

  char buf[32];
  
  if (s_watchface_style == 0) { /* 1. Mặt số đơn giản (Digital Simple) */
    ui_time_format_hhmm(buf, sizeof(buf), &ti);
    ui_label_set_text_if_changed(s_watchface_time_label, buf);
    const char *const *weekday_names = (ui_language_get() == UI_LANG_VI) ? s_weekday_names_vi : s_weekday_names_en;
    ui_label_set_text_if_changed(s_watchface_weekday_label, weekday_names[ti.weekday]);
  } 
  else if (s_watchface_style == 1) { /* 2. Mặt số hiện đại (Gradient Modern) */
    snprintf(buf, sizeof(buf), "%02d\n%02d", ti.hour, ti.minute);
    ui_label_set_text_if_changed(s_watchface_time_label, buf);
  } 
  else if (s_watchface_style == 2) { /* 3. Mặt kim cổ điển (Analog Minimal) */
    if (s_watchface_second_hand) {
        int32_t second_angle = (ti.second * 60) % 3600;
        if (s_watchface_second_angle < 0) {
            s_watchface_second_angle = second_angle;
            watchface_set_second_hand_line_angle(s_watchface_second_hand, second_angle);
        } else {
            int32_t target_angle = second_angle;
            while (target_angle <= s_watchface_second_angle) target_angle += 3600;
            lv_anim_del(s_watchface_second_hand, watchface_set_second_hand_line_angle);
            lv_anim_t anim;
            lv_anim_init(&anim);
            lv_anim_set_var(&anim, s_watchface_second_hand);
            lv_anim_set_exec_cb(&anim, watchface_set_second_hand_line_angle);
            lv_anim_set_values(&anim, s_watchface_second_angle, target_angle);
            lv_anim_set_time(&anim, 900);
            lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
            lv_anim_start(&anim);
            s_watchface_second_angle = target_angle;
        }
    }
    if (s_watchface_minute_hand) {
        int32_t min_angle = ((ti.minute * 60) + ti.second) % 3600;
        float angle_rad = (min_angle / 10.0f) * M_PI / 180.0f;
        int tail_len = 10;
        int head_len = 80;
        s_minute_hand_points[0].x = 105 - (int16_t)(tail_len * sinf(angle_rad));
        s_minute_hand_points[0].y = 105 + (int16_t)(tail_len * cosf(angle_rad));
        s_minute_hand_points[1].x = 105 + (int16_t)(head_len * sinf(angle_rad));
        s_minute_hand_points[1].y = 105 - (int16_t)(head_len * cosf(angle_rad));
        lv_line_set_points(s_watchface_minute_hand, s_minute_hand_points, 2);
    }
    if (s_watchface_hour_hand) {
        int32_t hr_angle = (((ti.hour % 12) * 300) + (ti.minute * 5)) % 3600;
        float angle_rad = (hr_angle / 10.0f) * M_PI / 180.0f;
        int tail_len = 8;
        int head_len = 55;
        s_hour_hand_points[0].x = 105 - (int16_t)(tail_len * sinf(angle_rad));
        s_hour_hand_points[0].y = 105 + (int16_t)(tail_len * cosf(angle_rad));
        s_hour_hand_points[1].x = 105 + (int16_t)(head_len * sinf(angle_rad));
        s_hour_hand_points[1].y = 105 - (int16_t)(head_len * cosf(angle_rad));
        lv_line_set_points(s_watchface_hour_hand, s_hour_hand_points, 2);
    }
  }

  /* Định dạng và vẽ chuỗi ngày tháng đa ngôn ngữ */
  const char *const *weekday_names = (ui_language_get() == UI_LANG_VI) ? s_weekday_names_vi : s_weekday_names_en;
  if (s_watchface_style == 0) {
    snprintf(buf, sizeof(buf), "%02d/%02d", ti.day, ti.month);
  } else {
    snprintf(buf, sizeof(buf), "%s, %02d/%02d", weekday_names[ti.weekday], ti.day, ti.month);
  }
  if (s_watchface_date_label) {
    ui_label_set_text_if_changed(s_watchface_date_label, buf);
  }

  /* Cập nhật các thông số cảm biến tương thích theo từng style */
  if (s_watchface_style == 0) {
    /* Đọc thông số pin sạc */
    watch_battery_data_t bat;
    if (hardware_i2c_sensor_get_battery(&bat)) {
      snprintf(buf, sizeof(buf), "%d%%", (int)(bat.soc_percent + 0.5f));
      if (s_watchface_battery_info_label) {
        ui_label_set_text_if_changed(s_watchface_battery_info_label, buf);
      }
    }
    /* Đọc số bước chân từ IMU */
    watch_imu_data_t imu;
    if (hardware_i2c_sensor_get_imu(&imu)) {
      snprintf(buf, sizeof(buf), "Bước: %lu", (unsigned long)imu.step_count);
      if (s_watchface_steps_label) {
        ui_label_set_text_if_changed(s_watchface_steps_label, buf);
        lv_obj_clear_flag(s_watchface_steps_label, LV_OBJ_FLAG_HIDDEN);
      }
    } else {
      if (s_watchface_steps_label) {
        lv_obj_add_flag(s_watchface_steps_label, LV_OBJ_FLAG_HIDDEN);
      }
    }
  } 
  else if (s_watchface_style == 1) {
    /* Cập nhật thanh cung tròn pin bao quanh */
    watch_battery_data_t bat;
    if (hardware_i2c_sensor_get_battery(&bat) && s_watchface_battery_arc) {
      int arc_val = (int)(bat.soc_percent + 0.5f);
      if (arc_val != s_watchface_battery_arc_value) {
        lv_arc_set_value(s_watchface_battery_arc, arc_val);
        s_watchface_battery_arc_value = arc_val;
      }
      uint32_t arc_color = lv_color_to32(watchface_battery_soc_color(bat.soc_percent));
      if (arc_color != s_watchface_battery_arc_color) {
        lv_obj_set_style_arc_color(s_watchface_battery_arc,
                                   watchface_battery_soc_color(bat.soc_percent),
                                   LV_PART_INDICATOR);
        s_watchface_battery_arc_color = arc_color;
      }
      if (s_watchface_battery_info_label) {
        snprintf(buf, sizeof(buf), "Pin %d%%", (int)(bat.soc_percent + 0.5f));
        ui_label_set_text_if_changed(s_watchface_battery_info_label, buf);
      }
    }
  }

  if (s_watchface_time_timer &&
      s_watchface_time_timer->period != watchface_timer_period_ms()) {
    lv_timer_set_period(s_watchface_time_timer, watchface_timer_period_ms());
  }
}

/* Bắt sự kiện cử chỉ vuốt (Gesture) để chuyển hướng màn hình */
static void watchface_gesture_cb(lv_event_t *e) {
  lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
  if (dir == LV_DIR_LEFT) {
    /* Vuốt trái -> Đẩy màn hình menu lưới (Menu Screen) vào stack */
    lv_obj_t *scr_menu = ui_menu_get_screen();
    ui_statusbar_set_time_visible(true); // Hiển thị giờ trên status bar ở các màn hình con
    ui_navigation_push(scr_menu);
  } else if (dir == LV_DIR_BOTTOM) {
    /* Quick access to display, vibration and connectivity settings. */
    ui_statusbar_set_time_visible(true);
    ui_navigation_push(ui_settings_screen_create());
  }
}

/* Khôi phục lại phiên làm việc cũ khi click nhanh 3 lần (Triple-tap) lên Watchface */
static void watchface_restore_last_cb(lv_event_t *e) {
  (void)e;
  static uint8_t tap_count = 0;
  static uint32_t first_tap_ms = 0;
  uint32_t now_ms = lv_tick_get();

  /* Kiểm tra cửa sổ thời gian gõ */
  if (tap_count == 0 ||
      (uint32_t)(now_ms - first_tap_ms) > WATCHFACE_RESTORE_TAP_WINDOW_MS) {
    first_tap_ms = now_ms;
    tap_count = 1;
    return;
  }

  tap_count++;
  if (tap_count < WATCHFACE_RESTORE_TAP_COUNT) {
    return;
  }

  /* Kích hoạt khôi phục */
  tap_count = 0;
  first_tap_ms = 0;
  if (ui_navigation_restore_last_screen()) {
    ui_statusbar_set_time_visible(true);
  }
}

/* Xóa bỏ màn hình dọn dẹp tài nguyên */
static void watchface_screen_delete_cb(lv_event_t *e) {
  if (s_watchface_time_timer) { 
    lv_timer_del(s_watchface_time_timer); 
    s_watchface_time_timer = NULL; 
  }
  s_watchface_time_label = NULL;
  s_watchface_weekday_label = NULL;
  s_watchface_date_label = NULL;
  s_watchface_battery_info_label = NULL;
  s_watchface_steps_label = NULL;
  s_watchface_battery_arc = NULL;
  s_watchface_second_hand = NULL;
  s_watchface_minute_hand = NULL;
  s_watchface_hour_hand = NULL;
  s_watchface_second_angle = -1;
  s_watchface_battery_arc_value = -1;
  s_watchface_battery_arc_color = 0;
  s_watchface_scr = NULL;
}

/* Khởi tạo đối tượng màn hình Watchface và các widget con theo style đã chọn */
lv_obj_t *ui_watchface_screen_create(void) {
  lv_obj_t *scr = ui_navigation_create_screen_with_id(UI_SCREEN_WATCHFACE);
  lv_obj_add_event_cb(scr, watchface_screen_delete_cb, LV_EVENT_DELETE, NULL);
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
  lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);

  if (s_watchface_style == 0) { /* Style 1: SỐ ĐƠN GIẢN */
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    /* Nhãn giờ lớn đặt ở chính giữa */
    s_watchface_weekday_label = lv_label_create(scr);
    lv_obj_set_style_text_font(s_watchface_weekday_label, UI_FONT_14, 0);
    lv_obj_set_style_text_color(s_watchface_weekday_label, COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_text_align(s_watchface_weekday_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_watchface_weekday_label, LV_ALIGN_CENTER, 0, -82);

    s_watchface_time_label = lv_label_create(scr);
    lv_obj_set_style_text_font(s_watchface_time_label, UI_FONT_48, 0);
    lv_obj_set_style_text_color(s_watchface_time_label, lv_color_white(), 0);
    lv_obj_set_width(s_watchface_time_label, 200);
    lv_obj_set_style_text_align(s_watchface_time_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_watchface_time_label, LV_ALIGN_CENTER, 0, -42);

    lv_obj_t *accent = lv_obj_create(scr);
    lv_obj_set_size(accent, 60, 3);
    lv_obj_set_style_bg_color(accent, lv_color_hex(0x3B82F6), 0);
    lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(accent, 2, 0);
    lv_obj_set_style_border_width(accent, 0, 0);
    lv_obj_clear_flag(accent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(accent, LV_ALIGN_CENTER, 0, 2);

    /* Nhãn ngày tháng phụ ở dưới nhãn giờ */
    s_watchface_date_label = lv_label_create(scr);
    lv_obj_set_style_text_font(s_watchface_date_label, UI_FONT_14, 0);
    lv_obj_set_style_text_color(s_watchface_date_label, COLOR_TEXT_MUTED, 0);
    lv_obj_align(s_watchface_date_label, LV_ALIGN_CENTER, 0, 28);

    /* Số bước chân và chỉ số pin đã được gỡ bỏ để tối giản màn hình */
    s_watchface_steps_label = NULL;
    s_watchface_battery_info_label = NULL;
  } 
  else if (s_watchface_style == 1) { /* Style 2: SỐ HIỆN ĐẠI GRADIENT */
    /* Đặt nền chuyển sắc Gradient (từ xanh ngọc sang tím hồng) */
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x6D5BD0), 0);
    lv_obj_set_style_bg_grad_color(scr, lv_color_hex(0x3B82F6), 0);
    lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);

    /* Nhãn giờ và phút xếp dọc chồng lên nhau */
    s_watchface_time_label = lv_label_create(scr);
    lv_obj_set_style_text_font(s_watchface_time_label, UI_FONT_48, 0);
    lv_obj_set_style_text_color(s_watchface_time_label, lv_color_white(), 0);
    lv_obj_set_style_text_line_space(s_watchface_time_label, -10, 0); // Thu hẹp khoảng cách dòng
    lv_obj_set_style_text_align(s_watchface_time_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_watchface_time_label, LV_ALIGN_CENTER, 0, -15);

    s_watchface_battery_info_label = lv_label_create(scr);
    lv_obj_set_style_text_font(s_watchface_battery_info_label, UI_FONT_12, 0);
    lv_obj_set_style_text_color(s_watchface_battery_info_label, lv_color_white(), 0);
    lv_obj_set_style_text_opa(s_watchface_battery_info_label, LV_OPA_80, 0);
    lv_obj_align(s_watchface_battery_info_label, LV_ALIGN_CENTER, 0, 54);

    /* Vòng tròn cung chỉ báo dung lượng Pin bao quanh đã được gỡ bỏ theo yêu cầu */
    s_watchface_battery_arc = NULL;

    s_watchface_date_label = lv_label_create(scr);
    lv_obj_set_style_text_font(s_watchface_date_label, UI_FONT_12, 0);
    lv_obj_set_style_text_color(s_watchface_date_label, lv_color_white(), 0);
    lv_obj_align(s_watchface_date_label, LV_ALIGN_BOTTOM_MID, 0, -45);
  }
  else if (s_watchface_style == 2) { /* Style 3: KIM CỔ ĐIỂN (Analog Minimal) */
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    /* Khung tròn mặt đồng hồ (Dial Container) */
    lv_obj_t *dial = lv_obj_create(scr);
    lv_obj_set_size(dial, 210, 210);
    lv_obj_set_style_bg_opa(dial, 0, 0);
    lv_obj_set_style_radius(dial, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dial, 2, 0);
    lv_obj_set_style_border_color(dial, COLOR_SURFACE_LT, 0);
    lv_obj_set_style_border_opa(dial, LV_OPA_70, 0);
    lv_obj_center(dial);
    lv_obj_clear_flag(dial, LV_OBJ_FLAG_SCROLLABLE);

    /* Vẽ các vạch chia nhỏ tại 4 góc chính (12, 3, 6, 9 giờ) */
    for (int i = 0; i < 12; i++) {
      watchface_add_dial_tick(dial, i * 30, true);
    }
    char num_str[4];
    for (int i = 1; i <= 12; i++) {
      snprintf(num_str, sizeof(num_str), "%d", i);
      watchface_add_dial_number(dial, num_str, (i % 12) * 30);
    }

    /* Thiết lập Kim Phút (Màu trắng, dài và thanh mảnh) */
    s_watchface_minute_hand = lv_line_create(dial);
    lv_obj_set_style_line_width(s_watchface_minute_hand, 4, 0);
    lv_obj_set_style_line_color(s_watchface_minute_hand, lv_color_white(), 0);
    lv_obj_set_style_line_rounded(s_watchface_minute_hand, true, 0);

    /* Thiết lập Kim Giờ (Màu trắng, ngắn và dày hơn) */
    s_watchface_hour_hand = lv_line_create(dial);
    lv_obj_set_style_line_width(s_watchface_hour_hand, 6, 0);
    lv_obj_set_style_line_color(s_watchface_hour_hand, lv_color_white(), 0);
    lv_obj_set_style_line_rounded(s_watchface_hour_hand, true, 0);

    /* Thiết lập Kim Giây (Màu đỏ làm điểm nhấn, chạy trôi trơn) */
    s_watchface_second_hand = lv_line_create(dial);
    lv_obj_set_style_line_width(s_watchface_second_hand, 2, 0);
    lv_obj_set_style_line_color(s_watchface_second_hand, COLOR_RED, 0);
    lv_obj_set_style_line_rounded(s_watchface_second_hand, true, 0);

    /* Nắp chốt kim đồng hồ ở trung tâm (Premium Center Center Cap) */
    lv_obj_t *dot_bg = lv_obj_create(dial);
    lv_obj_set_size(dot_bg, 12, 12);
    lv_obj_set_style_radius(dot_bg, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot_bg, lv_color_black(), 0);
    lv_obj_set_style_border_width(dot_bg, 2, 0);
    lv_obj_set_style_border_color(dot_bg, lv_color_white(), 0);
    lv_obj_set_style_pad_all(dot_bg, 0, 0);
    lv_obj_clear_flag(dot_bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(dot_bg);

    lv_obj_t *dot_accent = lv_obj_create(dial);
    lv_obj_set_size(dot_accent, 4, 4);
    lv_obj_set_style_radius(dot_accent, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot_accent, COLOR_RED, 0);
    lv_obj_set_style_border_width(dot_accent, 0, 0);
    lv_obj_set_style_pad_all(dot_accent, 0, 0);
    lv_obj_clear_flag(dot_accent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(dot_accent);

    s_watchface_date_label = NULL;
  }

  /* Đăng ký sự kiện điều phối cử chỉ và gõ */
  lv_obj_add_event_cb(scr, watchface_gesture_cb, LV_EVENT_GESTURE, NULL);
  lv_obj_add_event_cb(scr, watchface_restore_last_cb, LV_EVENT_CLICKED, NULL);
  
  /* Ẩn nhãn giờ hệ thống trên Status Bar do watchface đã có hiển thị thời gian chính */
  ui_statusbar_set_time_visible(false);
  
  s_watchface_scr = scr;
  s_watchface_time_timer = lv_timer_create(watchface_update_time_cb, watchface_timer_period_ms(), NULL);
  watchface_update_time_cb(NULL); // Làm tươi nội dung hiển thị ngay lập tức

  return scr;
}

/* Thiết lập kiểu giao diện hiển thị cho mặt đồng hồ */
void ui_watchface_set_style(int style_id) {
  int new_style = watchface_normalize_style(style_id);
  if (new_style == s_watchface_style) return;

  s_watchface_style = new_style;
  watchface_sync_timer_period();
  
  if (s_watchface_scr) {
    /* Dựng lại màn hình gốc theo phong cách mới */
    ui_navigation_recreate_root(ui_watchface_screen_create);
  }
}

/* Truy vấn kiểu mặt đồng hồ hiện tại đang chạy */
int ui_watchface_get_style(void) { 
  return s_watchface_style; 
}
