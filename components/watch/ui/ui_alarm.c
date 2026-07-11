/* Định nghĩa phân hệ quản lý và báo rung chuông báo thức (Alarm Manager Service & GUI)
   Phân hệ quản lý tối đa 8 bộ báo thức độc lập, hỗ trợ lưu trữ trạng thái xuống bộ nhớ
   flash không bay hơi (NVS) dạng nhị phân thông qua dịch vụ `watch_settings`. Cung cấp giao diện
   cho phép người dùng tùy chọn giờ/phút bằng thanh lăn chọn (lv_roller), thiết lập chế độ lặp lại
   theo thứ trong tuần, bật/tắt rung báo thức. Tiến trình giám sát chạy ngầm dưới dạng lv_timer
   kiểm tra trùng khớp thời gian mỗi giây, điều khiển motor rung (PWM) và hiển thị giao diện báo lại (Snooze)/dừng (Stop). */

#include "board_config.h"
#include "ui.h"
#include "ui_navigation.h"
#include "ui_screens.h"
#include "ui_utils.h"
#include "vibration_motor.h"
#include "watch_settings.h"
#include "esp_err.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

LV_FONT_DECLARE(menu_icon_2);
LV_FONT_DECLARE(signal_icon);
LV_FONT_DECLARE(angle_icon);

#define ICON_ALARM "\xef\x8d\x8e"
#define ICON_OK    "\xef\x80\x8c"
#define ICON_CLOSE "\xef\x80\x8d"
#define ICON_ANGLE_UP   "\xef\x84\x86"
#define ICON_ANGLE_DOWN "\xef\x84\x87"

#define MAX_ALARMS 8                       // Hỗ trợ tối đa 8 mốc báo thức
#define ALARM_CHECK_MS 1000                 // Chu kỳ kiểm tra khớp giờ (1 giây)
#define ALARM_SNOOZE_SECONDS (5 * 60)       // Thời gian báo lại mặc định là 5 phút
#define ALARM_STORE_MAGIC 0x414C524DU       // Từ khóa xác thực tính hợp lệ vùng lưu trữ "ALRM"
#define ALARM_STORE_VERSION 1U              // Phiên bản dữ liệu lưu trữ

/* Cấu trúc định nghĩa một đối tượng báo thức (Alarm Item) */
typedef struct {
  bool used;             // Cờ đánh dấu ô nhớ này đã được cấu hình hay chưa
  int hh;                // Giờ cài đặt (0-23)
  int mm;                // Phút cài đặt (0-59)
  uint8_t repeat_mask;   // Mặt nạ bit lặp lại theo thứ (Bit 0: Chủ nhật, Bit 1: Thứ hai,..., Bit 6: Thứ bảy)
  bool vibrate;          // Cờ bật/tắt phản hồi rung motor
  bool enabled;          // Cờ kích hoạt hoạt động
  int last_fire_yday;    // Ghi nhận ngày (tm_yday) báo thức kêu gần nhất để tránh kêu liên tục trong cùng 1 phút
  int last_fire_minute;  // Ghi nhận phút báo thức kêu gần nhất
} alarm_t;

static alarm_t s_alarms[MAX_ALARMS];
static alarm_t s_draft_alarm; // Đối tượng bản nháp trung gian phục vụ sửa/thêm mới
static lv_obj_t *s_alarm_list = NULL;
static int s_edit_alarm_index = -1;
static lv_timer_t *s_alarm_timer = NULL;
static bool s_alarm_ringing = false;
static bool s_snooze_active = false;
static bool s_alarms_loaded = false;
static bool s_ring_vibrate = false;
static time_t s_snooze_at = 0;
static int s_ring_hh = 0;
static int s_ring_mm = 0;
static lv_obj_t *s_list_btn_cancel = NULL;
static lv_obj_t *s_list_btn_ok = NULL;
static int s_selected_alarm_index = -1;

/* Các con trỏ tham chiếu phục vụ dựng màn hình chỉnh sửa */
static lv_obj_t *s_hour_roller = NULL;
static lv_obj_t *s_minute_roller = NULL;
static lv_obj_t *s_repeat_label = NULL;
static lv_obj_t *s_alarm_time_row = NULL;
static lv_obj_t *s_alarm_options_list = NULL;
static lv_obj_t *s_alarm_action_bar = NULL;
static lv_obj_t *s_alarm_next_button = NULL;
static lv_obj_t *s_time_row_val = NULL;
static lv_obj_t *s_alarm_hdr = NULL;

/* Khai báo các nguyên mẫu hàm nội bộ */
static void alarm_list_render(void);
static void alarm_edit_screen_open(int idx);
static void alarm_repeat_screen_open(void);
static void alarm_custom_days_screen_open(void);
static void alarm_edit_screen_refresh(void);
static void alarm_make_hour_min_strings(char *hhbuf, size_t hhbuf_len, char *mmbuf, size_t mmbuf_len);
static void alarm_engine_ensure(void);
static void alarm_trigger_now(int hh, int mm, bool vibrate);
lv_obj_t *ui_alarm_ring_screen_create(void);

/* Cấu trúc đóng gói lưu trữ xuống NVS Flash */
typedef struct {
  uint32_t magic;
  uint8_t version;
  alarm_t alarms[MAX_ALARMS];
} alarm_store_t;

/* Nạp danh sách báo thức từ NVS Flash (Chạy 1 lần duy nhất lúc khởi động) */
static void alarm_storage_load_once(void) {
  if (s_alarms_loaded) return;
  s_alarms_loaded = true;

  alarm_store_t store = {0};
  esp_err_t err = watch_settings_load_alarm_blob(&store, sizeof(store));
  if (err == ESP_OK && store.magic == ALARM_STORE_MAGIC &&
      store.version == ALARM_STORE_VERSION) {
    memcpy(s_alarms, store.alarms, sizeof(s_alarms));
    return;
  }
}

/* Đồng bộ lưu giữ danh sách báo thức xuống NVS Flash */
static void alarm_storage_save(void) {
  alarm_store_t store = {
      .magic = ALARM_STORE_MAGIC,
      .version = ALARM_STORE_VERSION,
  };
  memcpy(store.alarms, s_alarms, sizeof(s_alarms));
  ESP_ERROR_CHECK_WITHOUT_ABORT(watch_settings_save_alarm_blob(&store, sizeof(store)));
}

static void alarm_list_screen_delete_cb(lv_event_t *e) {
  (void)e;
  s_alarm_list = NULL;
  s_list_btn_cancel = NULL;
  s_list_btn_ok = NULL;
  s_selected_alarm_index = -1;
}

static void alarm_edit_screen_delete_cb(lv_event_t *e) {
  (void)e;
  s_hour_roller = NULL;
  s_minute_roller = NULL;
  s_repeat_label = NULL;
  s_alarm_time_row = NULL;
  s_alarm_options_list = NULL;
  s_alarm_action_bar = NULL;
  s_alarm_next_button = NULL;
  s_time_row_val = NULL;
  s_alarm_hdr = NULL;
  s_edit_alarm_index = -1;
}

/* Trả về tên viết tắt của thứ trong tuần */
static const char *alarm_day_name_short(int day) {
  static const char *const en[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  static const char *const vi[] = {"CN", "T2", "T3", "T4", "T5", "T6", "T7"};
  if (day < 0 || day > 6) return "";
  return ui_language_get() == UI_LANG_VI ? vi[day] : en[day];
}

/* Trả về tên đầy đủ của thứ trong tuần */
static const char *alarm_day_name_full(int day) {
  static const char *const en[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  static const char *const vi[] = {"Chủ Nhật", "Thứ Hai", "Thứ Ba", "Thứ Tư", "Thứ Năm", "Thứ Sáu", "Thứ Bảy"};
  if (day < 0 || day > 6) return "";
  return ui_language_get() == UI_LANG_VI ? vi[day] : en[day];
}

/* Định dạng hiển thị chuỗi thông báo chế độ lặp lại */
static void alarm_get_repeat_text(uint8_t mask, char *buf, size_t len) {
  if (mask == 0) {
    snprintf(buf, len, "%s", ui_text("Once", "Một lần"));
  } else if (mask == 0x7F) {
    snprintf(buf, len, "%s", ui_text("Every day", "Hàng ngày"));
  } else if (mask == 0x3E) {
    snprintf(buf, len, "%s", ui_text("Mon - Fri", "Thứ 2 - Thứ 6"));
  } else {
    buf[0] = '\0';
    bool first = true;
    for (int i = 1; i <= 7; i++) {
      int day = i % 7; // Thứ hai (1) -> Thứ bảy (6), Chủ nhật (0)
      if (mask & (1 << day)) {
        if (!first) strncat(buf, " ", len - strlen(buf) - 1);
        strncat(buf, alarm_day_name_short(day), len - strlen(buf) - 1);
        first = false;
      }
    }
  }
}

/* Kiểm tra xem báo thức có khớp lịch lặp lại của ngày hôm nay hay không */
static bool alarm_matches_today(const alarm_t *alarm, const struct tm *now_tm) {
  if (!alarm || !now_tm || !alarm->used || !alarm->enabled) return false;
  if (alarm->repeat_mask == 0) return true;
  return (alarm->repeat_mask & (1 << now_tm->tm_wday)) != 0;
}

/* Hàm Callback xử lý định kỳ kiểm tra so khớp thời gian báo thức
   Quá trình chạy bao gồm:
   1. Nếu đang rung chuông s_alarm_ringing = true, bỏ qua đợt kiểm tra mới.
   2. Đọc thời gian thực UNIX hiện tại.
   3. Nếu đang trong trạng thái tạm tắt (Snooze) và đã qua thời gian báo lại, kích hoạt báo rung lại.
   4. Duyệt qua 8 khe báo thức, kiểm tra xem báo thức có hợp lệ ngày hôm nay (alarm_matches_today) và khớp Giờ/Phút cài đặt.
   5. Nếu khớp và chưa từng báo thức trong phút hiện tại, ghi nhận vết last_fire để chống lặp và gọi alarm_trigger_now(). */
static void alarm_check_timer_cb(lv_timer_t *t) {
  (void)t;
  if (s_alarm_ringing) return;

  time_t now = time(NULL);
  if (now <= 0) return;

  struct tm now_tm;
  localtime_r(&now, &now_tm);

  /* Xử lý điều kiện kích hoạt lại sau khi tạm dừng (Snooze) */
  if (s_snooze_active && now >= s_snooze_at) {
    s_snooze_active = false;
    alarm_trigger_now(s_ring_hh, s_ring_mm, s_ring_vibrate);
    return;
  }

  const int minute_of_day = now_tm.tm_hour * 60 + now_tm.tm_min;
  for (int i = 0; i < MAX_ALARMS; i++) {
    alarm_t *alarm = &s_alarms[i];
    if (!alarm_matches_today(alarm, &now_tm)) continue;
    if (alarm->hh != now_tm.tm_hour || alarm->mm != now_tm.tm_min) continue;
    
    /* Chống lặp lại báo thức nhiều lần trong cùng một phút */
    if (alarm->last_fire_yday == now_tm.tm_yday &&
        alarm->last_fire_minute == minute_of_day) {
      continue;
    }

    alarm->last_fire_yday = now_tm.tm_yday;
    alarm->last_fire_minute = minute_of_day;
    
    /* Nếu báo thức một lần (Once), tự động hủy kích hoạt sau khi kêu */
    if (alarm->repeat_mask == 0) alarm->enabled = false;
    
    alarm_storage_save();
    alarm_trigger_now(alarm->hh, alarm->mm, alarm->vibrate);
    alarm_list_render(); // Vẽ lại giao diện danh sách để cập nhật trạng thái Switch tắt/bật
    break;
  }
}

/* Đảm bảo tiến trình kiểm tra báo thức đã được khởi chạy */
static void alarm_engine_ensure(void) {
  alarm_storage_load_once();
  if (!s_alarm_timer) {
    s_alarm_timer = lv_timer_create(alarm_check_timer_cb, ALARM_CHECK_MS, NULL);
  }
}

void ui_alarm_engine_start(void) {
  alarm_engine_ensure();
}

/* Kích hoạt báo thức thức thời, rung động phần cứng và đẩy màn hình chuông kêu */
static void alarm_trigger_now(int hh, int mm, bool vibrate) {
  s_alarm_ringing = true;
  s_ring_hh = hh;
  s_ring_mm = mm;
  s_ring_vibrate = vibrate;
  
  /* Phát xung PWM điều khiển motor rung nếu được cho phép trong Settings */
  if (vibrate && watch_settings_get()->vibration_enabled) {
    watch_vibration_alarm_start(watch_settings_get()->vibration_strength);
  }
  
  /* Đẩy màn hình hiển thị chuông reo lên trên cùng */
  ui_navigation_push(ui_alarm_ring_screen_create());
}

/* Callback xử lý sự kiện gạt nút Switch bật/tắt nhanh báo thức trong danh sách */
static void alarm_toggle_enabled_cb(lv_event_t *e) {
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  lv_obj_t *sw = lv_event_get_target(e);
  bool checked = lv_obj_has_state(sw, LV_STATE_CHECKED);
  if (idx >= 0 && idx < MAX_ALARMS) {
    s_alarms[idx].enabled = checked;
    alarm_storage_save();
  }
}

/* Lưu trữ các thay đổi chỉnh sửa báo thức từ giao diện Roller */
static void alarm_save_cb(lv_event_t *e) {
  (void)e;
  if (!s_hour_roller || !s_minute_roller ||
      !lv_obj_is_valid(s_hour_roller) || !lv_obj_is_valid(s_minute_roller)) {
    return;
  }

  /* Trích xuất giờ/phút được cuộn chọn */
  s_draft_alarm.hh = lv_roller_get_selected(s_hour_roller);
  s_draft_alarm.mm = lv_roller_get_selected(s_minute_roller);
  s_draft_alarm.used = true;

  if (s_edit_alarm_index >= 0) {
    /* Cập nhật báo thức cũ */
    s_alarms[s_edit_alarm_index] = s_draft_alarm;
  } else {
    /* Thêm mới vào vị trí trống đầu tiên tìm thấy */
    for (int i = 0; i < MAX_ALARMS; i++) {
      if (!s_alarms[i].used) {
        s_alarms[i] = s_draft_alarm;
        break;
      }
    }
  }
  ui_navigation_pop();        // Quay lại màn hình danh sách
  alarm_list_render();  // Vẽ lại danh sách báo thức mới nhất
  alarm_engine_ensure();
  alarm_storage_save();
}

/* Xóa bỏ báo thức đang sửa đổi */
static void alarm_delete_cb(lv_event_t *e) {
  (void)e;
  if (s_edit_alarm_index >= 0) {
    s_alarms[s_edit_alarm_index].used = false; // Đánh dấu ô nhớ chưa sử dụng
  }
  ui_navigation_pop();
  alarm_list_render();
  alarm_storage_save();
}


static void alarm_show_time_stage(lv_event_t *e) {
  (void)e;
  if (s_alarm_time_row) lv_obj_clear_flag(s_alarm_time_row, LV_OBJ_FLAG_HIDDEN);
  if (s_alarm_next_button) lv_obj_clear_flag(s_alarm_next_button, LV_OBJ_FLAG_HIDDEN);
  if (s_alarm_options_list) lv_obj_add_flag(s_alarm_options_list, LV_OBJ_FLAG_HIDDEN);
  if (s_alarm_action_bar) lv_obj_add_flag(s_alarm_action_bar, LV_OBJ_FLAG_HIDDEN);
  if (s_alarm_hdr) {
    lv_label_set_text(s_alarm_hdr, s_edit_alarm_index >= 0 ? ui_text("Edit Alarm", "Sửa báo thức") : ui_text("Set Alarm", "Đặt báo thức"));
  }
}

static void alarm_cancel_cb(lv_event_t *e) {
  (void)e;
  alarm_show_time_stage(NULL);
}

static void alarm_show_options_stage(void) {
  if (s_alarm_time_row) lv_obj_add_flag(s_alarm_time_row, LV_OBJ_FLAG_HIDDEN);
  if (s_alarm_next_button) lv_obj_add_flag(s_alarm_next_button, LV_OBJ_FLAG_HIDDEN);
  if (s_alarm_options_list) lv_obj_clear_flag(s_alarm_options_list, LV_OBJ_FLAG_HIDDEN);
  if (s_alarm_action_bar) {
    lv_obj_clear_flag(s_alarm_action_bar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_alarm_action_bar);
  }
  if (s_alarm_hdr) {
    lv_label_set_text(s_alarm_hdr, ui_text("Settings", "Thông số"));
    lv_obj_move_foreground(s_alarm_hdr);
  }

  if (s_hour_roller && s_minute_roller && s_time_row_val) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", 
             lv_roller_get_selected(s_hour_roller), 
             lv_roller_get_selected(s_minute_roller));
    lv_label_set_text(s_time_row_val, buf);
  }
}

static void alarm_next_cb(lv_event_t *e) {
  (void)e;
  alarm_show_options_stage();
}

static lv_obj_t *alarm_make_time_picker(lv_obj_t *parent, const char *opts, int selected, lv_coord_t width) {
  lv_obj_t *box = lv_obj_create(parent);
  lv_obj_set_size(box, width, 118);
  lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(box, 0, 0);
  lv_obj_set_style_pad_all(box, 0, 0);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *up = lv_label_create(box);
  lv_label_set_text(up, ICON_ANGLE_UP);
  lv_obj_set_style_text_font(up, &angle_icon, 0);
  lv_obj_set_style_text_color(up, COLOR_TEXT_MUTED, 0);
  lv_obj_align(up, LV_ALIGN_TOP_MID, 0, 0);

  lv_obj_t *roller = lv_roller_create(box);
  lv_roller_set_options(roller, opts, LV_ROLLER_MODE_NORMAL);
  lv_roller_set_visible_row_count(roller, 1);
  lv_roller_set_selected(roller, selected, LV_ANIM_OFF);
  lv_obj_set_width(roller, width);
  lv_obj_set_style_bg_opa(roller, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(roller, 0, 0);
  lv_obj_set_style_text_font(roller, UI_FONT_24, 0);
  lv_obj_set_style_bg_opa(roller, LV_OPA_TRANSP, LV_PART_SELECTED);
  lv_obj_align(roller, LV_ALIGN_CENTER, 0, 0);

  lv_obj_t *down = lv_label_create(box);
  lv_label_set_text(down, ICON_ANGLE_DOWN);
  lv_obj_set_style_text_font(down, &angle_icon, 0);
  lv_obj_set_style_text_color(down, COLOR_TEXT_MUTED, 0);
  lv_obj_align(down, LV_ALIGN_BOTTOM_MID, 0, 0);
  return roller;
}

static void alarm_edit_screen_refresh(void) {
  if (s_repeat_label && lv_obj_is_valid(s_repeat_label)) {
    char buf[64];
    alarm_get_repeat_text(s_draft_alarm.repeat_mask, buf, sizeof(buf));
    lv_label_set_text(s_repeat_label, buf);
  }
}

static lv_obj_t *alarm_make_row(lv_obj_t *par, const char *title, const char *val, bool is_sw) {
  lv_obj_t *row = lv_obj_create(par);
  lv_obj_set_size(row, lv_pct(100), 45);
  lv_obj_set_style_bg_color(row, COLOR_SURFACE, 0);
  lv_obj_set_style_radius(row, 18, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  
  lv_obj_t *lbl_t = lv_label_create(row);
  lv_label_set_text(lbl_t, title);
  lv_obj_set_style_text_font(lbl_t, UI_FONT_14, 0);
  lv_obj_set_style_text_color(lbl_t, lv_color_white(), 0);
  lv_obj_align(lbl_t, LV_ALIGN_LEFT_MID, 0, 0);

  if (is_sw) {
    lv_obj_t *sw = lv_switch_create(row);
    lv_obj_set_size(sw, 36, 20);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0x2B3038), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0x4A4F58), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sw, COLOR_GREEN, LV_PART_INDICATOR | LV_STATE_CHECKED);
    return (lv_obj_t *)sw;
  } else {
    lv_obj_t *lbl_v = lv_label_create(row);
    lv_label_set_text(lbl_v, val);
    lv_obj_set_style_text_color(lbl_v, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_v, UI_FONT_14, 0);
    lv_obj_align(lbl_v, LV_ALIGN_RIGHT_MID, 0, 0);
    return (lv_obj_t *)lbl_v;
  }
}

static void alarm_open_edit_screen_cb(lv_event_t *e) {
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  alarm_edit_screen_open(idx);
}

static void alarm_edit_screen_open(int idx) {
  s_edit_alarm_index = idx;
  if (idx >= 0) {
    s_draft_alarm = s_alarms[idx];
  } else {
    memset(&s_draft_alarm, 0, sizeof(alarm_t));
    s_draft_alarm.enabled = true;
    s_draft_alarm.vibrate = true;
    s_draft_alarm.repeat_mask = 0; // Once
    s_draft_alarm.last_fire_yday = -1;
    s_draft_alarm.last_fire_minute = -1;
  }

  lv_obj_t *scr = ui_navigation_create_screen();
  lv_obj_add_event_cb(scr, alarm_edit_screen_delete_cb, LV_EVENT_DELETE, NULL);

  lv_obj_t *hdr = lv_label_create(scr);
  s_alarm_hdr = hdr;
  lv_label_set_text(hdr, idx >= 0 ? ui_text("Edit Alarm", "Sửa báo thức") : ui_text("Set Alarm", "Đặt báo thức"));
  lv_obj_set_style_text_font(hdr, UI_FONT_20, 0);
  lv_obj_set_style_text_color(hdr, lv_color_white(), 0);
  lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 15);

  s_alarm_time_row = lv_obj_create(scr);
  lv_obj_set_size(s_alarm_time_row, WATCH_LCD_H_RES - 70, 130);
  lv_obj_align(s_alarm_time_row, LV_ALIGN_CENTER, 0, -10);
  lv_obj_set_style_bg_opa(s_alarm_time_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(s_alarm_time_row, 0, 0);
  lv_obj_set_flex_flow(s_alarm_time_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(s_alarm_time_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(s_alarm_time_row, 10, 0);
  lv_obj_clear_flag(s_alarm_time_row, LV_OBJ_FLAG_SCROLLABLE);

  char hhbuf[3*24], mmbuf[3*60];
  alarm_make_hour_min_strings(hhbuf, sizeof(hhbuf), mmbuf, sizeof(mmbuf));

  s_hour_roller = alarm_make_time_picker(s_alarm_time_row, hhbuf, s_draft_alarm.hh, 70);

  lv_obj_t *sep = lv_label_create(s_alarm_time_row);
  lv_label_set_text(sep, ":");
  lv_obj_set_style_text_font(sep, UI_FONT_24, 0);
  lv_obj_set_style_text_color(sep, COLOR_TEXT_MUTED, 0);

  s_minute_roller = alarm_make_time_picker(s_alarm_time_row, mmbuf, s_draft_alarm.mm, 70);

  s_alarm_next_button = lv_btn_create(scr);
  lv_obj_set_size(s_alarm_next_button, 150, 40);
  lv_obj_set_style_radius(s_alarm_next_button, 12, 0);
  lv_obj_set_style_bg_color(s_alarm_next_button, COLOR_PRIMARY, 0);
  lv_obj_set_style_shadow_width(s_alarm_next_button, 0, 0);
  lv_obj_align(s_alarm_next_button, LV_ALIGN_BOTTOM_MID, 0, -18);
  lv_obj_add_event_cb(s_alarm_next_button, alarm_next_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *next_lbl = lv_label_create(s_alarm_next_button);
  lv_label_set_text(next_lbl, ui_text("Next", "Tiếp"));
  lv_obj_set_style_text_font(next_lbl, UI_FONT_14, 0);
  lv_obj_center(next_lbl);

  s_alarm_action_bar = lv_obj_create(scr);
  lv_obj_set_size(s_alarm_action_bar, WATCH_LCD_H_RES, 42);
  lv_obj_align(s_alarm_action_bar, LV_ALIGN_TOP_MID, 0, 15);
  lv_obj_set_style_bg_opa(s_alarm_action_bar, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(s_alarm_action_bar, 0, 0);
  lv_obj_set_style_pad_all(s_alarm_action_bar, 0, 0);
  lv_obj_clear_flag(s_alarm_action_bar, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *btn_cancel = lv_btn_create(s_alarm_action_bar);
  lv_obj_set_size(btn_cancel, 36, 30);
  lv_obj_set_style_bg_opa(btn_cancel, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_width(btn_cancel, 0, 0);
  lv_obj_align(btn_cancel, LV_ALIGN_LEFT_MID, 24, 0);
  lv_obj_add_event_cb(btn_cancel, alarm_cancel_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *cancel_lbl = lv_label_create(btn_cancel);
  lv_label_set_text(cancel_lbl, ICON_CLOSE);
  lv_obj_set_style_text_font(cancel_lbl, &signal_icon, 0);
  lv_obj_set_style_text_color(cancel_lbl, COLOR_RED, 0);
  lv_obj_center(cancel_lbl);

  lv_obj_t *btn_save = lv_btn_create(s_alarm_action_bar);
  lv_obj_set_size(btn_save, 36, 30);
  lv_obj_set_style_bg_opa(btn_save, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_width(btn_save, 0, 0);
  lv_obj_align(btn_save, LV_ALIGN_RIGHT_MID, -24, 0);
  lv_obj_add_event_cb(btn_save, alarm_save_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *save_lbl = lv_label_create(btn_save);
  lv_label_set_text(save_lbl, ICON_OK);
  lv_obj_set_style_text_font(save_lbl, &signal_icon, 0);
  lv_obj_set_style_text_color(save_lbl, COLOR_GREEN, 0);
  lv_obj_center(save_lbl);

  s_alarm_options_list = lv_obj_create(scr);
  lv_obj_set_size(s_alarm_options_list, WATCH_LCD_H_RES - 20, WATCH_LCD_V_RES - 95);
  lv_obj_align(s_alarm_options_list, LV_ALIGN_BOTTOM_MID, 0, -5);
  lv_obj_set_style_bg_opa(s_alarm_options_list, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(s_alarm_options_list, 0, 0);
  lv_obj_set_flex_flow(s_alarm_options_list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(s_alarm_options_list, 8, 0);
  lv_obj_set_style_pad_all(s_alarm_options_list, 0, 0);
  lv_obj_add_flag(s_alarm_options_list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(s_alarm_options_list, LV_SCROLLBAR_MODE_OFF);

  /* 0. Thời gian */
  s_time_row_val = (lv_obj_t *)alarm_make_row(s_alarm_options_list, ui_text("Time", "Thời gian"), "", false);
  lv_obj_t *row_time_cont = lv_obj_get_parent(s_time_row_val);
  lv_obj_add_flag(row_time_cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(row_time_cont, (lv_event_cb_t)alarm_show_time_stage, LV_EVENT_CLICKED, NULL);

  /* 1. Lặp lại */
  lv_obj_t *row_rep = (lv_obj_t *)alarm_make_row(s_alarm_options_list, ui_text("Repeat", "Lặp lại"), "", false);
  lv_obj_t *row_rep_cont = lv_obj_get_parent(row_rep);
  lv_obj_add_flag(row_rep_cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(row_rep_cont, (lv_event_cb_t)alarm_repeat_screen_open, LV_EVENT_CLICKED, NULL);
  s_repeat_label = row_rep;
  alarm_edit_screen_refresh();

  /* 2. Nhãn */
  alarm_make_row(s_alarm_options_list, ui_text("Label", "Nhãn"), idx >= 0 ? ui_text("Morning", "Buổi sáng") : ui_tr(UI_TXT_ALARM), false);

  if (idx >= 0) {
    lv_obj_t *del = alarm_make_row(s_alarm_options_list, ui_text("Delete", "Xoá"), "", false);
    lv_obj_t *del_cont = lv_obj_get_parent(del);
    lv_obj_add_flag(del_cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(del_cont, alarm_delete_cb, LV_EVENT_CLICKED, NULL);
  }

  lv_obj_add_flag(s_alarm_options_list, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(s_alarm_action_bar, LV_OBJ_FLAG_HIDDEN);


  ui_navigation_add_gesture(scr);
  ui_navigation_push(scr);
}

static void alarm_list_delete_selected_cb(lv_event_t *e) {
  (void)e;
  if (s_selected_alarm_index >= 0 && s_selected_alarm_index < MAX_ALARMS) {
    s_alarms[s_selected_alarm_index].used = false;
    s_selected_alarm_index = -1;
    alarm_storage_save();
    alarm_list_render();
    if (s_list_btn_cancel) lv_obj_add_flag(s_list_btn_cancel, LV_OBJ_FLAG_HIDDEN);
    if (s_list_btn_ok) lv_obj_add_flag(s_list_btn_ok, LV_OBJ_FLAG_HIDDEN);
  }
}

static void alarm_list_update_selection_visuals(void) {
  if (!s_alarm_list || !lv_obj_is_valid(s_alarm_list)) return;
  uint32_t cnt = lv_obj_get_child_cnt(s_alarm_list);
  for (uint32_t i = 0; i < cnt; i++) {
    lv_obj_t *card = lv_obj_get_child(s_alarm_list, i);
    if (!card || !lv_obj_is_valid(card)) continue;
    int idx = (int)(intptr_t)lv_obj_get_user_data(card);
    const bool is_selected = (s_selected_alarm_index == idx);
    lv_obj_set_style_bg_color(card, is_selected ? COLOR_PRIMARY : COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(card, is_selected ? LV_OPA_20 : LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, is_selected ? 3 : 0, 0);
    lv_obj_set_style_border_color(card, COLOR_PRIMARY, 0);
  }
}

static void alarm_list_confirm_selected_cb(lv_event_t *e) {
  (void)e;
  s_selected_alarm_index = -1;
  alarm_list_update_selection_visuals();
  if (s_list_btn_cancel) lv_obj_add_flag(s_list_btn_cancel, LV_OBJ_FLAG_HIDDEN);
  if (s_list_btn_ok) lv_obj_add_flag(s_list_btn_ok, LV_OBJ_FLAG_HIDDEN);
}

static void alarm_edit_card_cb(lv_event_t *e) {
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  if (idx >= 0 && idx < MAX_ALARMS) {
    if (s_selected_alarm_index == idx) {
      /* Click lần 2 vào báo thức đang chọn -> Mở màn hình sửa */
      s_selected_alarm_index = -1;
      if (s_list_btn_cancel) lv_obj_add_flag(s_list_btn_cancel, LV_OBJ_FLAG_HIDDEN);
      if (s_list_btn_ok) lv_obj_add_flag(s_list_btn_ok, LV_OBJ_FLAG_HIDDEN);
      alarm_edit_screen_open(idx);
    } else {
      /* Chọn báo thức */
      s_selected_alarm_index = idx;
      alarm_list_update_selection_visuals();
      /* Hiện các nút ở trên đầu */
      if (s_list_btn_cancel) lv_obj_clear_flag(s_list_btn_cancel, LV_OBJ_FLAG_HIDDEN);
      if (s_list_btn_ok) lv_obj_clear_flag(s_list_btn_ok, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

static void alarm_list_render(void) {
  if (!s_alarm_list) return;
  lv_obj_clean(s_alarm_list);

  for (int i = 0; i < MAX_ALARMS; i++) {
    if (!s_alarms[i].used) continue;

    const bool is_selected = (s_selected_alarm_index == i);
    lv_obj_t *card = lv_obj_create(s_alarm_list);
    lv_obj_set_user_data(card, (void *)(intptr_t)i); // Store index for safe selection style updates
    lv_obj_set_size(card, lv_pct(100), 62);
    lv_obj_set_style_bg_color(card, is_selected ? COLOR_PRIMARY : COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(card, is_selected ? LV_OPA_20 : LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 18, 0);
    lv_obj_set_style_pad_all(card, 15, 0);
    lv_obj_set_style_border_width(card, is_selected ? 3 : 0, 0);
    lv_obj_set_style_border_color(card, COLOR_PRIMARY, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    /* Time Label - Very Large */
    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d", s_alarms[i].hh, s_alarms[i].mm);
    lv_obj_t *lbl_time = lv_label_create(card);
    lv_label_set_text(lbl_time, buf);
    lv_obj_set_style_text_font(lbl_time, UI_FONT_24, 0);
    lv_obj_set_style_text_color(lbl_time, COLOR_TEXT, 0);
    lv_obj_align(lbl_time, LV_ALIGN_LEFT_MID, 0, 0);

    /* Switch - GREEN in image */
    lv_obj_t *sw = lv_switch_create(card);
    lv_obj_set_size(sw, 44, 24);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0x2B3038), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0x4A4F58), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sw, COLOR_GREEN, LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (s_alarms[i].enabled) lv_obj_add_state(sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw, alarm_toggle_enabled_cb, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)i);

    /* Click to edit */
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, alarm_edit_card_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
  }
}

/* Helper to create roller options string 00\n01\n... */
static void alarm_make_hour_min_strings(char *hhbuf, size_t hhbuf_len,
                                  char *mmbuf, size_t mmbuf_len) {
  char *p = hhbuf;
  size_t rem = hhbuf_len;
  for (int i = 0; i < 24; i++) {
    int n = snprintf(p, rem, "%02d", i);
    p += n;
    rem -= n;
    if (i != 23) {
      *p++ = '\n';
      rem--;
    }
  }
  p = mmbuf;
  rem = mmbuf_len;
  for (int i = 0; i < 60; i++) {
    int n = snprintf(p, rem, "%02d", i);
    p += n;
    rem -= n;
    if (i != 59) {
      *p++ = '\n';
      rem--;
    }
  }
}

static void alarm_repeat_option_cb(lv_event_t *e) {
  uint8_t mask = (uint8_t)(intptr_t)lv_event_get_user_data(e);
  s_draft_alarm.repeat_mask = mask;
  ui_navigation_pop();
  alarm_edit_screen_refresh();
}

static void alarm_repeat_screen_open(void) {
  lv_obj_t *scr = ui_navigation_create_screen();
  lv_obj_t *hdr = lv_label_create(scr);
  lv_label_set_text(hdr, ui_text("Repeat", "Lặp lại"));
  lv_obj_set_style_text_font(hdr, UI_FONT_20, 0);
  lv_obj_set_style_text_color(hdr, lv_color_white(), 0);
  lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 15);

  lv_obj_t *list = lv_obj_create(scr);
  lv_obj_set_size(list, WATCH_LCD_H_RES - 20, WATCH_LCD_V_RES - 65);
  lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, -5);
  lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(list, 0, 0);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  ui_enable_list_scrolling(list);
  lv_obj_set_style_pad_row(list, 10, 0);

  struct {
    const char *txt;
    uint8_t mask;
  } opts[] = {
    {ui_text("Once", "Một lần"), 0x00},
    {ui_text("Every day", "Hàng ngày"), 0x7F},
    {ui_text("Mon - Fri", "Thứ 2 - Thứ 6"), 0x3E},
    {ui_text("Custom", "Tùy chỉnh"), 0xFF} // Special marker for custom
  };

  for (int i = 0; i < 4; i++) {
    lv_obj_t *btn = lv_btn_create(list);
    lv_obj_set_size(btn, lv_pct(100), 45);
    lv_obj_set_style_bg_color(btn, COLOR_SURFACE, 0);
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, opts[i].txt);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

    /* Checkmark if selected */
    bool selected = false;
    if (opts[i].mask == 0xFF) selected = false; // Custom marker
    else if (s_draft_alarm.repeat_mask == opts[i].mask) selected = true;

    if (selected) {
      lv_obj_t *tick = lv_label_create(btn);
      lv_label_set_text(tick, ICON_OK);
      lv_obj_set_style_text_font(tick, &signal_icon, 0);
      lv_obj_set_style_text_color(tick, COLOR_GREEN, 0);
      lv_obj_align(tick, LV_ALIGN_RIGHT_MID, 0, 0);
    }

    if (opts[i].mask == 0xFF) {
      lv_obj_add_event_cb(btn, (lv_event_cb_t)alarm_custom_days_screen_open, LV_EVENT_CLICKED, NULL);
    } else {
      lv_obj_add_event_cb(btn, alarm_repeat_option_cb, LV_EVENT_CLICKED, (void *)(intptr_t)opts[i].mask);
    }
  }

  ui_navigation_add_gesture(scr);
  ui_navigation_push(scr);
}

static void alarm_day_toggle_cb(lv_event_t *e) {
  int day = (int)(intptr_t)lv_event_get_user_data(e);
  lv_obj_t *row = lv_event_get_target(e);
  
  if (s_draft_alarm.repeat_mask & (1 << day)) {
    s_draft_alarm.repeat_mask &= ~(1 << day);
  } else {
    s_draft_alarm.repeat_mask |= (1 << day);
  }

  lv_obj_t *tick = lv_obj_get_child(row, 1);
  if (tick) {
    if (s_draft_alarm.repeat_mask & (1 << day)) {
      lv_obj_clear_flag(tick, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(tick, LV_OBJ_FLAG_HIDDEN);
    }
  }

  alarm_edit_screen_refresh();
}

static void alarm_custom_days_screen_open(void) {
  lv_obj_t *scr = ui_navigation_create_screen();
  lv_obj_t *hdr = lv_label_create(scr);
  lv_label_set_text(hdr, ui_text("Custom", "Tùy chỉnh"));
  lv_obj_set_style_text_font(hdr, UI_FONT_20, 0);
  lv_obj_set_style_text_color(hdr, lv_color_white(), 0);
  lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 15);

  lv_obj_t *list = lv_obj_create(scr);
  lv_obj_set_size(list, WATCH_LCD_H_RES - 20, WATCH_LCD_V_RES - 91);
  lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 51);
  lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(list, 0, 0);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  ui_enable_list_scrolling(list);
  lv_obj_set_style_pad_row(list, 8, 0);
  lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);

  for (int i = 1; i <= 7; i++) {
    int day = i % 7; // 1, 2, 3, 4, 5, 6, 0 (Mon-Sun)
    lv_obj_t *row = lv_obj_create(list);
    lv_obj_set_size(row, lv_pct(100), 45);
    lv_obj_set_style_bg_color(row, COLOR_SURFACE, 0);
    lv_obj_set_style_radius(row, 12, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text(lbl, alarm_day_name_full(day));
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

    /* Checkmark in image - always create, but hide if not checked */
    lv_obj_t *tick = lv_label_create(row);
    lv_label_set_text(tick, ICON_OK);
    lv_obj_set_style_text_font(tick, &signal_icon, 0);
    lv_obj_set_style_text_color(tick, COLOR_GREEN, 0);
    lv_obj_align(tick, LV_ALIGN_RIGHT_MID, 0, 0);
    if (!(s_draft_alarm.repeat_mask & (1 << day))) {
      lv_obj_add_flag(tick, LV_OBJ_FLAG_HIDDEN);
    }
    
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, alarm_day_toggle_cb, LV_EVENT_CLICKED, (void *)(intptr_t)day);
  }

  /* Done button */
  lv_obj_t *btn = lv_btn_create(scr);
  lv_obj_set_size(btn, 140, 36);
  lv_obj_set_style_bg_color(btn, COLOR_ORANGE, 0);
  lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -5);
  lv_obj_t *lbl_btn = lv_label_create(btn);
  lv_label_set_text(lbl_btn, ui_text("Done", "Xong"));
  lv_obj_center(lbl_btn);
  lv_obj_add_event_cb(btn, (lv_event_cb_t)ui_navigation_pop, LV_EVENT_CLICKED, NULL);

  ui_navigation_add_gesture(scr);
  ui_navigation_push(scr);
}

lv_obj_t *ui_alarm_list_screen_create(void) {
  lv_obj_t *scr = ui_navigation_create_screen_with_id(UI_SCREEN_ALARM);
  lv_obj_add_event_cb(scr, alarm_list_screen_delete_cb, LV_EVENT_DELETE, NULL);
  alarm_engine_ensure();
  s_selected_alarm_index = -1;
  
  lv_obj_t *hdr = lv_label_create(scr);
  lv_label_set_text(hdr, ui_tr(UI_TXT_ALARM));
  lv_obj_set_style_text_font(hdr, UI_FONT_20, 0);
  lv_obj_set_style_text_color(hdr, lv_color_white(), 0);
  lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 15);

  /* Cancel Button (X) at top left */
  s_list_btn_cancel = lv_btn_create(scr);
  lv_obj_set_size(s_list_btn_cancel, 36, 36);
  lv_obj_set_style_bg_opa(s_list_btn_cancel, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_width(s_list_btn_cancel, 0, 0);
  lv_obj_align(s_list_btn_cancel, LV_ALIGN_TOP_LEFT, 15, 10);
  lv_obj_add_flag(s_list_btn_cancel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_t *cancel_lbl = lv_label_create(s_list_btn_cancel);
  lv_label_set_text(cancel_lbl, ICON_CLOSE);
  lv_obj_set_style_text_font(cancel_lbl, &signal_icon, 0);
  lv_obj_set_style_text_color(cancel_lbl, COLOR_RED, 0);
  lv_obj_center(cancel_lbl);
  lv_obj_add_event_cb(s_list_btn_cancel, alarm_list_delete_selected_cb, LV_EVENT_CLICKED, NULL);

  /* Confirm Button (checkmark) at top right */
  s_list_btn_ok = lv_btn_create(scr);
  lv_obj_set_size(s_list_btn_ok, 36, 36);
  lv_obj_set_style_bg_opa(s_list_btn_ok, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_width(s_list_btn_ok, 0, 0);
  lv_obj_align(s_list_btn_ok, LV_ALIGN_TOP_RIGHT, -15, 10);
  lv_obj_add_flag(s_list_btn_ok, LV_OBJ_FLAG_HIDDEN);
  lv_obj_t *ok_lbl = lv_label_create(s_list_btn_ok);
  lv_label_set_text(ok_lbl, ICON_OK);
  lv_obj_set_style_text_font(ok_lbl, &signal_icon, 0);
  lv_obj_set_style_text_color(ok_lbl, COLOR_GREEN, 0);
  lv_obj_center(ok_lbl);
  lv_obj_add_event_cb(s_list_btn_ok, alarm_list_confirm_selected_cb, LV_EVENT_CLICKED, NULL);

  s_alarm_list = lv_obj_create(scr);
  lv_obj_set_size(s_alarm_list, WATCH_LCD_H_RES - 20, WATCH_LCD_V_RES - 115);
  lv_obj_align(s_alarm_list, LV_ALIGN_TOP_MID, 0, 51);
  lv_obj_set_style_bg_opa(s_alarm_list, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(s_alarm_list, 0, 0);
  lv_obj_set_flex_flow(s_alarm_list, LV_FLEX_FLOW_COLUMN);
  ui_enable_list_scrolling(s_alarm_list);
  lv_obj_set_style_pad_row(s_alarm_list, 12, 0);
  lv_obj_add_flag(s_alarm_list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(s_alarm_list, LV_SCROLLBAR_MODE_OFF);

  /* "+ Add Alarm" row instead of FAB */
  lv_obj_t *add_row = lv_obj_create(scr);
  lv_obj_set_size(add_row, WATCH_LCD_H_RES - 20, 50);
  lv_obj_align(add_row, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_obj_set_style_bg_opa(add_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(add_row, 0, 0);
  lv_obj_add_flag(add_row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(add_row, alarm_open_edit_screen_cb, LV_EVENT_CLICKED, (void *)(intptr_t)-1);

  lv_obj_t *add_lbl = lv_label_create(add_row);
  lv_label_set_text_fmt(add_lbl, "+ %s", ui_text("Add Alarm", "Thêm báo thức"));
  lv_obj_set_style_text_font(add_lbl, UI_FONT_16, 0);
  lv_obj_set_style_text_color(add_lbl, COLOR_TEXT_MUTED, 0);
  lv_obj_center(add_lbl);

  alarm_list_render();
  ui_navigation_add_gesture(scr);
  return scr;
}

/* Callback dọn dẹp bộ nhớ khi màn hình chuông reo bị giải phóng */
static void alarm_ring_screen_delete_cb(lv_event_t *e) {
  (void)e;
  s_alarm_ringing = false;
  watch_vibration_stop(); // Dừng cấp điện cho motor rung
}

/* Callback xử lý sự kiện bấm nút "Báo lại" (Snooze)
   Thiết lập trạng thái báo lại hoạt động và đặt mốc thời gian kêu chuông tiếp theo
   (sau ALARM_SNOOZE_SECONDS = 5 phút). Tắt rung tạm thời và quay lại màn hình trước. */
static void alarm_snooze_cb(lv_event_t *e) {
  (void)e;

  int target_mm = s_ring_mm + 5;
  int target_hh = s_ring_hh;
  if (target_mm >= 60) {
    target_mm -= 60;
    target_hh = (target_hh + 1) % 24;
  }

  int free_idx = -1;
  for (int k = 0; k < MAX_ALARMS; k++) {
    if (!s_alarms[k].used) {
      free_idx = k;
      break;
    }
  }
  if (free_idx == -1) {
    for (int k = 0; k < MAX_ALARMS; k++) {
      if (!s_alarms[k].enabled) {
        free_idx = k;
        break;
      }
    }
  }
  if (free_idx == -1) {
    free_idx = MAX_ALARMS - 1;
  }

  s_alarms[free_idx].used = true;
  s_alarms[free_idx].hh = target_hh;
  s_alarms[free_idx].mm = target_mm;
  s_alarms[free_idx].repeat_mask = 0; // Một lần
  s_alarms[free_idx].vibrate = true;
  s_alarms[free_idx].enabled = true;
  s_alarms[free_idx].last_fire_yday = -1;
  s_alarms[free_idx].last_fire_minute = -1;

  alarm_storage_save();

  s_snooze_active = false;
  s_alarm_ringing = false;
  watch_vibration_stop();
  ui_navigation_pop();
}

/* Callback xử lý sự kiện bấm nút "Dừng" (Stop) tắt chuông hẳn */
static void alarm_stop_cb(lv_event_t *e) {
  (void)e;
  s_snooze_active = false;
  s_alarm_ringing = false;
  watch_vibration_stop();
  ui_navigation_pop();
}

/* Khởi tạo đối tượng màn hình báo thức kêu chuông (Alarm Ringing Screen) */
lv_obj_t *ui_alarm_ring_screen_create(void) {
  lv_obj_t *scr = ui_navigation_create_screen();
  lv_obj_add_event_cb(scr, alarm_ring_screen_delete_cb, LV_EVENT_DELETE, NULL);

  /* Icon quả chuông báo thức lớn ở trên */
  lv_obj_t *icon = lv_label_create(scr);
  lv_label_set_text(icon, ICON_ALARM);
  lv_obj_set_style_text_font(icon, &menu_icon_2, 0);
  lv_obj_set_style_text_color(icon, COLOR_ORANGE, 0);
  lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 40);

  /* Giờ báo thức kích thước cực lớn ở trung tâm */
  lv_obj_t *time_lbl = lv_label_create(scr);
  char time_buf[8];
  snprintf(time_buf, sizeof(time_buf), "%02d:%02d", s_ring_hh, s_ring_mm);
  lv_label_set_text(time_lbl, time_buf);
  lv_obj_set_style_text_font(time_lbl, UI_FONT_48, 0);
  lv_obj_set_style_text_color(time_lbl, COLOR_TEXT, 0);
  lv_obj_align(time_lbl, LV_ALIGN_CENTER, 0, -20);

  /* Nhãn phụ "Báo thức" mờ bên dưới */
  lv_obj_t *sub_lbl = lv_label_create(scr);
  lv_label_set_text(sub_lbl, ui_tr(UI_TXT_ALARM));
  lv_obj_set_style_text_font(sub_lbl, UI_FONT_16, 0);
  lv_obj_set_style_text_color(sub_lbl, COLOR_TEXT_MUTED, 0);
  lv_obj_align(sub_lbl, LV_ALIGN_CENTER, 0, 20);

  /* Khung Container chứa 2 nút bấm thao tác (Buttons Row Layout) */
  lv_obj_t *btn_cont = lv_obj_create(scr);
  lv_obj_set_size(btn_cont, WATCH_LCD_H_RES, 50);
  lv_obj_align(btn_cont, LV_ALIGN_BOTTOM_MID, 0, -20);
  lv_obj_set_style_bg_opa(btn_cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(btn_cont, 0, 0);
  lv_obj_set_style_pad_all(btn_cont, 0, 0);
  lv_obj_clear_flag(btn_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(btn_cont, 16, 0);

  /* Thiết lập Nút bấm "Báo lại" (Snooze) màu cam */
  lv_obj_t *btn_snooze = lv_btn_create(btn_cont);
  lv_obj_set_size(btn_snooze, 90, 44);
  lv_obj_set_style_radius(btn_snooze, 22, 0);
  lv_obj_set_style_bg_color(btn_snooze, COLOR_ORANGE, 0);
  lv_obj_t *lbl_snooze = lv_label_create(btn_snooze);
  lv_label_set_text(lbl_snooze, ui_text("Snooze", "Báo lại"));
  lv_obj_set_style_text_font(lbl_snooze, UI_FONT_14, 0);
  lv_obj_center(lbl_snooze);
  lv_obj_add_event_cb(btn_snooze, alarm_snooze_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *btn_stop = lv_btn_create(btn_cont);
  lv_obj_set_size(btn_stop, 90, 44);
  lv_obj_set_style_radius(btn_stop, 22, 0);
  lv_obj_set_style_bg_color(btn_stop, COLOR_RED, 0);
  lv_obj_t *lbl_stop = lv_label_create(btn_stop);
  lv_label_set_text(lbl_stop, ui_text("Stop", "Dừng"));
  lv_obj_set_style_text_font(lbl_stop, UI_FONT_14, 0);
  lv_obj_center(lbl_stop);
  lv_obj_add_event_cb(btn_stop, alarm_stop_cb, LV_EVENT_CLICKED, NULL);

  return scr;
}

bool watch_alarm_is_ringing(void) {
  return s_alarm_ringing;
}
