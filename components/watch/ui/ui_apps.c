/* Thiết kế giao diện và quản lý logic các ứng dụng phụ trên Smartwatch
   (Hoạt động thể thao, Hộp thư thông báo, Chỉ đường GPS, Cài đặt hệ thống). */
#include "board_config.h"
#include "ui.h"
#include "ui_navigation.h"
#include "ui_screens.h"
#include "ui_utils.h"
#include "bluetooth_manager.h"
#include "gps_tracker.h"
#include "hardware_i2c_sensor.h"
#include "vibration_motor.h"
#include "watch_settings.h"
#include "watch_activity_log.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include "driver/ledc.h"
#include "esp_app_desc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

LV_FONT_DECLARE(sub_icon);
LV_FONT_DECLARE(language_icon);
LV_FONT_DECLARE(angle_icon);
LV_FONT_DECLARE(workout_health_icon);
LV_FONT_DECLARE(menu_icon_2);
LV_FONT_DECLARE(signal_icon);
LV_FONT_DECLARE(arrow_1);
LV_FONT_DECLARE(arrow_2);
LV_FONT_DECLARE(ble);

/* Định nghĩa mã Unicode các biểu tượng đồ họa từ font fontawesome custom */
#define ICON_SUB_ABOUT       "\xef\x8f\x8f"
#define ICON_SUB_VIBRATION   "\xee\xa0\x96"
#define ICON_SUB_PERSONAL    "\xef\x95\x9d"
#define ICON_SUB_DISPLAY     "\xef\x86\x85"
#define ICON_SUB_LANGUAGE    "\xef\x86\xab"
#define ICON_SUB_SET_TIME    "\xef\x80\x97"
#define ICON_BLE             "\xef\x8a\x94"
#define ICON_WORK_HEARTRATE  "\xef\x88\x9e"
#define ICON_WORK_SPO2       "\xef\x81\x83"
#define ICON_WORK_CYCLING    "\xef\x88\x86"
#define ICON_WORK_RUNNING    "\xef\x9c\x8c"
#define ICON_SIGNAL_OK       "\xef\x80\x8c"
#define ICON_SIGNAL_CLOSE    "\xef\x80\x8d"
#define ICON_SIGNAL_TRASH    "\xef\x8b\xad"
#define ICON_ARROW_UP        "\xef\x97\x9f"
#define ICON_ARROW_DOWN      "\xef\x97\x91"
#define ICON_ARROW_LEFT      "\xef\x8c\x95"
#define ICON_ARROW_RIGHT     "\xef\x8c\x99"
#define ICON_ARROW_SL_LEFT   "\xef\x97\x97"
#define ICON_ARROW_SL_RIGHT  "\xef\x97\x9d"
#define ICON_ARROW_SPIN      "\xee\x92\xbb"
#define ICON_NAV_ARRIVE      "\xee\x92\xbc" // Checkered flag icon
#define ICON_ANGLE_UP        "\xef\x84\x86"
#define ICON_ANGLE_DOWN      "\xef\x84\x87"

/* Khai báo trước các hàm callback xử lý sự kiện trong mục cài đặt */
static void settings_open_color_picker_cb(lv_event_t *e);
static void settings_open_display_cb(lv_event_t *e);
static void settings_open_vibration_cb(lv_event_t *e);
static void settings_open_personality_cb(lv_event_t *e);
static void settings_open_language_cb(lv_event_t *e);
static void settings_open_set_time_cb(lv_event_t *e);
static void settings_open_system_cb(lv_event_t *e) __attribute__((unused));
static void settings_open_about_cb(lv_event_t *e);

/* Khai báo trước các hàm khởi tạo màn hình thuộc phân hệ Cài đặt */
lv_obj_t *ui_display_settings_screen_create(void);
lv_obj_t *ui_vibration_settings_screen_create(void);
lv_obj_t *ui_personality_settings_screen_create(void);
lv_obj_t *ui_language_settings_screen_create(void);
lv_obj_t *ui_system_settings_screen_create(void);
lv_obj_t *ui_about_screen_create(void);

/* Khởi tạo một phần tử danh sách (List Item) tiêu chuẩn
   - par: Con trỏ trỏ tới container cha (thường là list)
   - sym: Ký tự biểu tượng icon đại diện ở đầu dòng (nếu có)
   - c: Màu sắc biểu tượng hiển thị
   - txt: Chuỗi nhãn văn bản hiển thị cho dòng lựa chọn
   Trả về: lv_obj_t* Con trỏ đối tượng phần tử danh sách được tạo */
static lv_obj_t *apps_make_list_item(lv_obj_t *par, const char *sym, lv_color_t c,
                                const char *txt) {
  lv_obj_t *item = lv_obj_create(par);
  lv_obj_set_size(item, lv_pct(100), 55);
  lv_obj_set_style_bg_color(item, COLOR_SURFACE, 0);
  lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(item, 18, 0);
  lv_obj_set_style_border_width(item, 0, 0);
  lv_obj_set_style_pad_hor(item, 12, 0);
  lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(item, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(item, 12, 0);

  if (sym) {
    lv_obj_t *icon_bg = lv_obj_create(item);
    lv_obj_set_size(icon_bg, 36, 36);
    lv_obj_set_style_radius(icon_bg, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(icon_bg, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(icon_bg, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(icon_bg, 0, 0);
    lv_obj_clear_flag(icon_bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *ic = lv_label_create(icon_bg);
    char itxt[4] = "?";
    if (sym[0]) {
      strncpy(itxt, sym, sizeof(itxt) - 1);
      itxt[sizeof(itxt) - 1] = '\0';
    } else if (txt && txt[0]) {
      itxt[0] = (char)toupper((unsigned char)txt[0]);
      itxt[1] = '\0';
    }
    lv_label_set_text(ic, itxt);
    lv_obj_set_style_text_color(ic, c, 0);
    const lv_font_t *icon_font = UI_FONT_16;
    if (sym[0]) {
      icon_font = (strcmp(sym, ICON_SUB_LANGUAGE) == 0) ? &language_icon : &sub_icon;
    }
    lv_obj_set_style_text_font(ic, icon_font, 0);
    lv_obj_center(ic);
  }

  lv_obj_t *lb = lv_label_create(item);
  lv_label_set_text(lb, txt);
  lv_obj_set_style_text_color(lb, COLOR_TEXT, 0);
  lv_obj_set_style_text_font(lb, UI_FONT_16, 0);
  return item;
}

/* Khởi tạo một màn hình danh sách cuộn tiêu chuẩn có tiêu đề
   - title: Nhãn tiêu đề hiển thị ở phía trên màn hình
   Trả về: lv_obj_t* Con trỏ đối tượng danh sách cuộn (list container) được tạo bên trong màn hình mới */
static lv_obj_t *apps_make_list_screen(const char *title) {
  lv_obj_t *scr = ui_navigation_create_screen();
  lv_obj_t *hdr = lv_label_create(scr);
  lv_label_set_text(hdr, title);
  lv_obj_set_style_text_font(hdr, UI_FONT_16, 0);
  lv_obj_set_style_text_color(hdr, COLOR_TEXT, 0);
  lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 30);

  lv_obj_t *list = lv_obj_create(scr);
  lv_obj_set_size(list, WATCH_LCD_H_RES - 20, WATCH_LCD_V_RES - 85);
  lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, -5);
  lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(list, 0, 0);
  lv_obj_set_style_pad_all(list, 5, 0);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  ui_enable_list_scrolling(list);
  lv_obj_set_style_pad_row(list, 10, 0);
  lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
  ui_navigation_add_gesture(scr);
  return list;
}

/* ============ PHÂN HỆ QUẢN LÝ HOẠT ĐỘNG THỂ THAO (ACTIVITY) ============ */

static lv_obj_t *ui_activity_gps_wait_screen_create(ui_screen_id_t activity_screen_id);

/* Callback chuyển tiếp sang màn hình chạy bộ/đi bộ (Running/Walking) */
static void activity_open_running_cb(lv_event_t *e) { 
  (void)e;
  ui_navigation_push(ui_activity_gps_wait_screen_create(UI_SCREEN_RUNNING));
}

/* Callback chuyển tiếp sang màn hình đạp xe (Cycling) */
static void activity_open_cycling_cb(lv_event_t *e) { 
  (void)e;
  ui_navigation_push(ui_activity_gps_wait_screen_create(UI_SCREEN_CYCLING));
}

/* Khởi tạo màn hình Menu lựa chọn bài tập thể thao
   Cho phép người dùng bấm chọn giữa 2 chế độ: Chạy bộ (Running) và Đạp xe (Cycling).
   Thiết kế giao diện theo dạng Card phẳng có màu chủ đạo riêng và bo góc hiện đại.
   Trả về: lv_obj_t* Con trỏ đối tượng màn hình Activity được tạo */
lv_obj_t *ui_activity_screen_create(void) {
  lv_obj_t *scr = ui_navigation_create_screen_with_id(UI_SCREEN_ACTIVITY);

  lv_obj_t *cont = lv_obj_create(scr);
  lv_obj_set_size(cont, WATCH_LCD_H_RES, WATCH_LCD_V_RES);
  lv_obj_align(cont, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(cont, 0, 0);
  lv_obj_set_style_pad_all(cont, 12, 0);
  lv_obj_set_style_pad_row(cont, 10, 0);
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *hdr = lv_label_create(cont);
  lv_label_set_text(hdr, ui_text("Choose exercise", "Chọn bài tập"));
  lv_obj_set_style_text_font(hdr, UI_FONT_16, 0);
  lv_obj_set_style_text_color(hdr, COLOR_TEXT, 0);

  /* === Lựa chọn 1: Chạy bộ / Đi bộ (Running) === */
  lv_obj_t *card1 = lv_obj_create(cont);
  lv_obj_set_size(card1, lv_pct(100), 62);
  lv_obj_set_style_bg_color(card1, COLOR_SURFACE, 0);
  lv_obj_set_style_bg_opa(card1, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card1, 18, 0);
  lv_obj_set_style_border_width(card1, 0, 0);
  lv_obj_clear_flag(card1, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(card1, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(card1, activity_open_running_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *ic1 = lv_obj_create(card1);
  lv_obj_set_size(ic1, 60, 60);
  lv_obj_set_style_radius(ic1, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(ic1, COLOR_GREEN, 0);
  lv_obj_set_style_bg_opa(ic1, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(ic1, 0, 0);
  lv_obj_clear_flag(ic1, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(ic1, LV_ALIGN_LEFT_MID, 10, 0);
  
  lv_obj_t *ic1_lbl = lv_label_create(ic1);
  lv_label_set_text(ic1_lbl, ICON_WORK_RUNNING);
  lv_obj_set_style_text_color(ic1_lbl, lv_color_white(), 0);
  lv_obj_set_style_text_font(ic1_lbl, &workout_health_icon, 0);
  lv_obj_center(ic1_lbl);

  lv_obj_t *t1 = lv_label_create(card1);
  lv_label_set_text(t1, ui_tr(UI_TXT_RUNNING));
  lv_obj_set_style_text_font(t1, UI_FONT_20, 0);
  lv_obj_set_style_text_color(t1, COLOR_TEXT, 0);
  lv_obj_align(t1, LV_ALIGN_LEFT_MID, 85, 0);

  /* === Card 2: Cycling === */
  lv_obj_t *card2 = lv_obj_create(cont);
  lv_obj_set_size(card2, lv_pct(100), 62);
  lv_obj_set_style_bg_color(card2, COLOR_SURFACE, 0);
  lv_obj_set_style_bg_opa(card2, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card2, 18, 0);
  lv_obj_set_style_border_width(card2, 0, 0);
  lv_obj_clear_flag(card2, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(card2, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(card2, activity_open_cycling_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *ic2 = lv_obj_create(card2);
  lv_obj_set_size(ic2, 60, 60);
  lv_obj_set_style_radius(ic2, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(ic2, COLOR_CYAN, 0);
  lv_obj_set_style_bg_opa(ic2, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(ic2, 0, 0);
  lv_obj_clear_flag(ic2, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(ic2, LV_ALIGN_LEFT_MID, 10, 0);
  
  lv_obj_t *ic2_lbl = lv_label_create(ic2);
  lv_label_set_text(ic2_lbl, ICON_WORK_CYCLING);
  lv_obj_set_style_text_color(ic2_lbl, lv_color_white(), 0);
  lv_obj_set_style_text_font(ic2_lbl, &workout_health_icon, 0);
  lv_obj_center(ic2_lbl);

  lv_obj_t *t2 = lv_label_create(card2);
  lv_label_set_text(t2, ui_tr(UI_TXT_CYCLING));
  lv_obj_set_style_text_font(t2, UI_FONT_20, 0);
  lv_obj_set_style_text_color(t2, COLOR_TEXT, 0);
  lv_obj_align(t2, LV_ALIGN_LEFT_MID, 85, 0);

  ui_navigation_add_gesture(scr);
  return scr;
}

/* Định dạng chuỗi hiển thị quãng đường di chuyển
   - out: Con trỏ đệm nhận chuỗi đầu ra
   - out_size: Kích thước tối đa của bộ đệm đầu ra
   - km: Số kilomet đo được */
static void activity_format_distance(char *out, size_t out_size, float km) {
  snprintf(out, out_size, "%.3f", km);
}

/* Định dạng chuỗi hiển thị vận tốc di chuyển từ thông tin GPS
   - out: Con trỏ đệm nhận chuỗi đầu ra
   - out_size: Kích thước tối đa của bộ đệm đầu ra
   - m: Con trỏ cấu trúc dữ liệu GPS đo đạc */
static void activity_format_speed(char *out, size_t out_size,
                                  const watch_gps_metrics_t *m) {
  if (!m || !m->fix_valid) {
    snprintf(out, out_size, "--.-");
    return;
  }
  snprintf(out, out_size, "%.1f", m->speed_kmh);
}

/* ============ PHÂN HỆ KHỞI TẠO VÀ QUẢN LÝ PHIÊN LUYỆN TẬP ============ */

/* Cấu hình bố cục giao diện của từng chế độ thể thao cụ thể */
typedef struct {
  ui_screen_id_t screen_id;       /* ID định danh màn hình */
  const char *icon;               /* Biểu tượng đại diện */
  uint32_t color_hex;             /* Màu sắc chủ đạo (RGB Hex) */
  bool show_steps;                /* Cho phép hiển thị đếm bước chân */
  bool show_pace;                 /* Cho phép hiển thị nhịp độ (Pace) */
  bool show_speed;                /* Cho phép hiển thị vận tốc di chuyển */
  int stats_y;                    /* Tọa độ Y bắt đầu vùng vẽ chỉ số */
} activity_screen_config_t;

/* Ngữ cảnh lưu trữ trạng thái phiên hoạt động thể thao hiện tại */
typedef struct {
  const activity_screen_config_t *cfg; /* Con trỏ trỏ tới cấu hình màn hình */
  lv_obj_t *screen;                   /* Con trỏ đối tượng màn hình LVGL */
  lv_obj_t *time_label;               /* Nhãn hiển thị thời gian tập */
  lv_obj_t *distance_label;           /* Nhãn hiển thị khoảng cách km */
  lv_obj_t *steps_label;              /* Nhãn hiển thị số bước chân */
  lv_obj_t *pace_label;               /* Nhãn hiển thị nhịp độ chạy */
  lv_obj_t *speed_label;              /* Nhãn hiển thị vận tốc chạy/đạp */
  lv_obj_t *bpm_label;                /* Nhãn hiển thị nhịp tim (Heart Rate) */
  lv_obj_t *gps_quality_label;        /* Nhãn chất lượng tín hiệu GPS */
  lv_obj_t *btn_pause;                /* Nút tạm dừng / tiếp tục */
  lv_obj_t *lbl_pause;                /* Văn bản hiển thị trên nút tạm dừng */
  lv_obj_t *btn_stop;                 /* Nút dừng hẳn và lưu/xóa */
  lv_obj_t *lbl_stop;                 /* Văn bản hiển thị trên nút dừng */
  bool active;                        /* Cờ trạng thái đang hoạt động (chạy timer) */
  bool ever_started;                  /* Đã từng bấm nút bắt đầu luyện tập hay chưa */
  lv_timer_t *timer;                  /* Bộ định thời làm tươi giao diện mỗi giây */
  uint32_t start_tick;                /* Tick khởi đầu phiên để tính mốc thời gian */
  uint32_t elapsed_sec;               /* Tổng số giây đã trôi qua */
  uint32_t session_id;                /* Mã phiên luyện tập (thời gian unix) */
  bool saving;                        /* Worker đang hoàn tất route và lịch sử */
  uint32_t exit_timestamp;            /* Thời điểm thoát màn hình ra nền (ms) */
} activity_session_ctx_t;

/* Khởi tạo biến ngữ cảnh thể thao tĩnh toàn cục trong file */
static activity_session_ctx_t s_activity = {0};

/* Lấy mã định danh phiên hoạt động hiện tại (đồng bộ qua BLE)
   Trả về: uint32_t Mã định danh phiên hoạt động (0 nếu không có) */
uint32_t ui_activity_get_current_session_id(void) {
  return (s_activity.ever_started && s_activity.session_id != 0) ? s_activity.session_id : 0;
}

/* Lấy tên chế độ thể thao hiện tại đang thực hiện
   Trả về: const char* Chuỗi tên chế độ: "walking", "cycling", "none" */
const char *ui_activity_get_current_sport_mode(void) {
  if (!s_activity.ever_started || !s_activity.cfg) return "none";
  switch (s_activity.cfg->screen_id) {
    case UI_SCREEN_RUNNING: return "walking";
    case UI_SCREEN_CYCLING: return "cycling";
    default: return "active";
  }
}

/* Kiểm tra xem phiên hoạt động thể thao hiện tại có đang chạy hay không
   Trả về: bool true nếu đang hoạt động (không bị tạm dừng hay dừng hẳn) */
bool ui_activity_is_active(void) {
  return s_activity.active;
}

/* Cấu hình chi tiết cho màn hình Chạy bộ (Running - Xanh lá) */
static const activity_screen_config_t s_running_cfg = {
  .screen_id = UI_SCREEN_RUNNING,
  .icon = ICON_WORK_RUNNING,
  .color_hex = 0x30D158,
  .show_steps = true,
  .show_pace = true,
  .show_speed = false,
  .stats_y = 125,
};

static const activity_screen_config_t s_cycling_cfg = {
  .screen_id = UI_SCREEN_CYCLING,
  .icon = ICON_WORK_CYCLING,
  .color_hex = 0x00BFFF,
  .show_steps = false,
  .show_pace = false,
  .show_speed = true,
  .stats_y = 130,
};

typedef struct {
  lv_obj_t *screen;
  lv_obj_t *lock_arc;
  lv_obj_t *lock_icon;
  lv_obj_t *status_label;
  lv_obj_t *quality_label;
  lv_obj_t *quality_bar;
  lv_obj_t *sat_value;
  lv_obj_t *hdop_value;
  lv_timer_t *timer;
  const activity_screen_config_t *cfg;
  uint8_t ready_ticks;
  uint16_t wait_ticks;
  bool transitioning;
} activity_gps_wait_ctx_t;

#define ACTIVITY_GPS_WAIT_FALLBACK_TICKS 60U

static void activity_gps_wait_delete_cb(lv_event_t *e) {
  activity_gps_wait_ctx_t *ctx = lv_event_get_user_data(e);
  if (!ctx) return;
  if (ctx->timer) {
    lv_timer_del(ctx->timer);
    ctx->timer = NULL;
  }
  if (!ctx->transitioning) {
    watch_gps_set_activity_mode(WATCH_ACTIVITY_MODE_NONE);
    watch_gps_request_stop();
  }
  free(ctx);
}

static void activity_gps_wait_open_workout(activity_gps_wait_ctx_t *ctx, lv_timer_t *timer) {
  if (!ctx || ctx->transitioning) return;
  ctx->transitioning = true;
  ctx->timer = NULL;
  if (timer) lv_timer_del(timer);
  ui_navigation_replace(ctx->cfg->screen_id == UI_SCREEN_RUNNING
                     ? ui_running_screen_create()
                     : ui_cycling_screen_create());
}

static void activity_gps_wait_timer_cb(lv_timer_t *t) {
  activity_gps_wait_ctx_t *ctx = t ? t->user_data : NULL;
  if (!ctx || !ctx->screen || !lv_obj_is_valid(ctx->screen)) {
    if (t) lv_timer_del(t);
    return;
  }

  watch_gps_metrics_t gps = {0};
  bool available = watch_gps_get_metrics(&gps);
  int quality = 0;
  if (available) {
    quality += gps.satellites_in_use >= 8 ? 55 :
               gps.satellites_in_use >= 5 ? 40 :
               gps.satellites_in_use >= 4 ? 25 :
               (int)gps.satellites_in_use * 5;
    quality += gps.hdop > 0.0f && gps.hdop <= 1.5f ? 45 :
               gps.hdop > 0.0f && gps.hdop <= 3.0f ? 30 :
               gps.hdop > 0.0f && gps.hdop <= 5.0f ? 15 : 0;
  }
  if (quality > 100) quality = 100;

  lv_color_t signal_color = quality >= 70 ? lv_color_hex(ctx->cfg->color_hex) :
                            quality >= 40 ? COLOR_ORANGE : COLOR_RED;
  lv_obj_set_style_arc_color(ctx->lock_arc, signal_color, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(ctx->quality_bar, signal_color, LV_PART_INDICATOR);
  lv_obj_set_style_text_color(ctx->lock_icon, signal_color, 0);
  lv_arc_set_value(ctx->lock_arc, quality);
  lv_bar_set_value(ctx->quality_bar, quality, LV_ANIM_ON);

  char sat[20];
  snprintf(sat, sizeof(sat), "%lu / %lu",
           (unsigned long)gps.satellites_in_use,
           (unsigned long)gps.satellites_in_view);
  ui_label_set_text_if_changed(ctx->sat_value, sat);

  char hdop[16];
  if (gps.hdop > 0.0f) {
    snprintf(hdop, sizeof(hdop), "%.1f", (double)gps.hdop);
  } else {
    snprintf(hdop, sizeof(hdop), "--");
  }
  ui_label_set_text_if_changed(ctx->hdop_value, hdop);

  if (!available || !gps.fix_valid) {
    ui_label_set_text_if_changed(ctx->status_label,
                                 ui_text("Waiting for accurate GPS...",
                                         "Đang chờ GPS đủ chính xác..."));
    ui_label_set_text_if_changed(ctx->quality_label,
                                 quality >= 70 ? ui_text("Almost ready", "Gần sẵn sàng") :
                                 quality >= 40 ? ui_text("Improving signal", "Tín hiệu đang tốt lên") :
                                                 ui_text("Move to an open area", "Hãy ra khu vực thoáng"));
    ctx->ready_ticks = 0;
    /* Disable automatic fallback transition; wait for stable GPS fix.
    if (++ctx->wait_ticks >= ACTIVITY_GPS_WAIT_FALLBACK_TICKS) {
      ui_label_set_text_if_changed(ctx->status_label,
                                   ui_text("Starting without GPS fix",
                                           "Bắt đầu khi GPS còn yếu"));
      ui_label_set_text_if_changed(ctx->quality_label,
                                   ui_text("GPS keeps searching in workout",
                                           "GPS vẫn tiếp tục dò trong bài tập"));
      activity_gps_wait_open_workout(ctx, t);
    }
    */
    return;
  }

  lv_color_t accent = lv_color_hex(ctx->cfg->color_hex);
  lv_obj_set_style_arc_color(ctx->lock_arc, accent, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(ctx->quality_bar, accent, LV_PART_INDICATOR);
  lv_obj_set_style_text_color(ctx->lock_icon, accent, 0);
  lv_arc_set_value(ctx->lock_arc, 100);
  lv_bar_set_value(ctx->quality_bar, 100, LV_ANIM_ON);
  lv_label_set_text(ctx->lock_icon, ICON_SIGNAL_OK);
  lv_obj_set_style_text_font(ctx->lock_icon, &signal_icon, 0);
  ui_label_set_text_if_changed(ctx->status_label,
                               ui_text("GPS ready", "GPS đã sẵn sàng"));
  ui_label_set_text_if_changed(ctx->quality_label,
                               ui_text("Opening workout...", "Đang mở bài tập..."));
  if (++ctx->ready_ticks < 2) return;

  activity_gps_wait_open_workout(ctx, t);
}

static lv_obj_t *ui_activity_gps_wait_screen_create(ui_screen_id_t activity_screen_id) {
  const activity_screen_config_t *cfg =
      activity_screen_id == UI_SCREEN_RUNNING ? &s_running_cfg : &s_cycling_cfg;
  lv_obj_t *scr = ui_navigation_create_screen();
  activity_gps_wait_ctx_t *ctx = calloc(1, sizeof(*ctx));
  if (!ctx) {
    lv_obj_del(scr);
    return activity_screen_id == UI_SCREEN_RUNNING
               ? ui_running_screen_create()
               : ui_cycling_screen_create();
  }

  ctx->screen = scr;
  ctx->cfg = cfg;
  lv_obj_add_event_cb(scr, activity_gps_wait_delete_cb, LV_EVENT_DELETE, ctx);

  lv_color_t accent = lv_color_hex(cfg->color_hex);

  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, cfg->screen_id == UI_SCREEN_RUNNING
                               ? ui_text("Walking workout", "Bài tập đi bộ")
                               : ui_text("Cycling workout", "Bài tập đạp xe"));
  lv_obj_set_style_text_font(title, UI_FONT_20, 0);
  lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

  lv_obj_t *subtitle = lv_label_create(scr);
  lv_label_set_text(subtitle, ui_text("Preparing route tracking", "Đang chuẩn bị theo dõi quãng đường"));
  lv_obj_set_style_text_font(subtitle, UI_FONT_12, 0);
  lv_obj_set_style_text_color(subtitle, COLOR_TEXT_MUTED, 0);
  lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 30);

  ctx->lock_arc = lv_arc_create(scr);
  lv_obj_set_size(ctx->lock_arc, 92, 92);
  lv_arc_set_range(ctx->lock_arc, 0, 100);
  lv_arc_set_value(ctx->lock_arc, 0);
  lv_arc_set_bg_angles(ctx->lock_arc, 135, 45);
  lv_obj_remove_style(ctx->lock_arc, NULL, LV_PART_KNOB);
  lv_obj_clear_flag(ctx->lock_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(ctx->lock_arc, 7, LV_PART_MAIN);
  lv_obj_set_style_arc_color(ctx->lock_arc, COLOR_SURFACE_LT, LV_PART_MAIN);
  lv_obj_set_style_arc_width(ctx->lock_arc, 7, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(ctx->lock_arc, accent, LV_PART_INDICATOR);
  lv_obj_align(ctx->lock_arc, LV_ALIGN_TOP_MID, 0, 48);

  ctx->lock_icon = lv_label_create(scr);
  lv_label_set_text(ctx->lock_icon, "GPS");
  lv_obj_set_style_text_font(ctx->lock_icon, UI_FONT_20, 0);
  lv_obj_set_style_text_color(ctx->lock_icon, accent, 0);
  lv_obj_align_to(ctx->lock_icon, ctx->lock_arc, LV_ALIGN_CENTER, 0, 0);

  ctx->status_label = lv_label_create(scr);
  lv_label_set_text(ctx->status_label,
                    ui_text("Starting GPS...", "Đang khởi động GPS..."));
  lv_obj_set_width(ctx->status_label, WATCH_LCD_H_RES - 30);
  lv_obj_set_style_text_align(ctx->status_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(ctx->status_label, UI_FONT_16, 0);
  lv_obj_set_style_text_color(ctx->status_label, COLOR_TEXT, 0);
  lv_obj_align(ctx->status_label, LV_ALIGN_TOP_MID, 0, 145);

  ctx->quality_label = lv_label_create(scr);
  lv_label_set_text(ctx->quality_label, ui_text("Move to an open area", "Hãy ra khu vực thoáng"));
  lv_obj_set_style_text_font(ctx->quality_label, UI_FONT_12, 0);
  lv_obj_set_style_text_color(ctx->quality_label, COLOR_TEXT_MUTED, 0);
  lv_obj_align(ctx->quality_label, LV_ALIGN_TOP_MID, 0, 167);

  ctx->quality_bar = lv_bar_create(scr);
  lv_obj_set_size(ctx->quality_bar, WATCH_LCD_H_RES - 48, 5);
  lv_bar_set_range(ctx->quality_bar, 0, 100);
  lv_bar_set_value(ctx->quality_bar, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(ctx->quality_bar, COLOR_SURFACE_LT, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ctx->quality_bar, accent, LV_PART_INDICATOR);
  lv_obj_set_style_radius(ctx->quality_bar, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_radius(ctx->quality_bar, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
  lv_obj_align(ctx->quality_bar, LV_ALIGN_TOP_MID, 0, 187);

  lv_obj_t *metrics = lv_obj_create(scr);
  lv_obj_set_size(metrics, WATCH_LCD_H_RES - 28, 44);
  lv_obj_align(metrics, LV_ALIGN_TOP_MID, 0, 198);
  lv_obj_set_style_bg_color(metrics, COLOR_SURFACE, 0);
  lv_obj_set_style_bg_opa(metrics, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(metrics, 12, 0);
  lv_obj_set_style_border_width(metrics, 1, 0);
  lv_obj_set_style_border_color(metrics, COLOR_SURFACE_LT, 0);
  lv_obj_set_style_pad_all(metrics, 0, 0);
  lv_obj_clear_flag(metrics, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *sat_name = lv_label_create(metrics);
  lv_label_set_text(sat_name, "SAT");
  lv_obj_set_style_text_font(sat_name, UI_FONT_12, 0);
  lv_obj_set_style_text_color(sat_name, COLOR_TEXT_MUTED, 0);
  lv_obj_align(sat_name, LV_ALIGN_LEFT_MID, 12, -8);
  ctx->sat_value = lv_label_create(metrics);
  lv_label_set_text(ctx->sat_value, "0 / 0");
  lv_obj_set_style_text_font(ctx->sat_value, UI_FONT_14, 0);
  lv_obj_set_style_text_color(ctx->sat_value, COLOR_TEXT, 0);
  lv_obj_align(ctx->sat_value, LV_ALIGN_LEFT_MID, 12, 9);

  lv_obj_t *hdop_name = lv_label_create(metrics);
  lv_label_set_text(hdop_name, "HDOP");
  lv_obj_set_style_text_font(hdop_name, UI_FONT_12, 0);
  lv_obj_set_style_text_color(hdop_name, COLOR_TEXT_MUTED, 0);
  lv_obj_align(hdop_name, LV_ALIGN_RIGHT_MID, -12, -8);
  ctx->hdop_value = lv_label_create(metrics);
  lv_label_set_text(ctx->hdop_value, "--");
  lv_obj_set_style_text_font(ctx->hdop_value, UI_FONT_14, 0);
  lv_obj_set_style_text_color(ctx->hdop_value, COLOR_TEXT, 0);
  lv_obj_align(ctx->hdop_value, LV_ALIGN_RIGHT_MID, -12, 9);

  watch_gps_set_activity_mode(activity_screen_id == UI_SCREEN_RUNNING
                                      ? WATCH_ACTIVITY_MODE_WALKING
                                      : WATCH_ACTIVITY_MODE_CYCLING);
  watch_gps_request_start();
  ctx->timer = lv_timer_create(activity_gps_wait_timer_cb, 500, ctx);
  activity_gps_wait_timer_cb(ctx->timer);
  ui_navigation_add_gesture(scr);
  return scr;
}

/* Định dạng số giây sang định dạng giờ:phút:giây hiển thị
   - buf: Bộ đệm đầu ra chứa chuỗi kết quả
   - sz: Kích thước bộ đệm đầu ra
   - sec: Tổng số giây trôi qua */
static void activity_format_time(char *buf, size_t sz, uint32_t sec) {
  snprintf(buf, sz, "%02lu:%02lu:%02lu",
           (unsigned long)(sec / 3600),
           (unsigned long)((sec % 3600) / 60),
           (unsigned long)(sec % 60));
}

/* Thêm một hàng hiển thị chỉ số đo đạc trên giao diện tập luyện
   - scr: Màn hình chứa đối tượng nhãn
   - label: Tên chỉ số (Ví dụ: "Khoảng cách", "Tốc độ")
   - value: Giá trị hiện tại của chỉ số
   - label_color: Màu chữ tiêu đề chỉ số
   - y: Tọa độ Y hiển thị của hàng
   Trả về: lv_obj_t* Nhãn hiển thị giá trị để cập nhật động sau này */
static lv_obj_t *activity_add_stat_row(lv_obj_t *scr, const char *label,
                                       const char *value, lv_color_t label_color,
                                       int y) {
  lv_obj_t *name = lv_label_create(scr);
  lv_label_set_text(name, label);
  lv_obj_set_style_text_font(name, UI_FONT_16, 0);
  lv_obj_set_style_text_color(name, label_color, 0);
  lv_obj_align(name, LV_ALIGN_TOP_LEFT, 12, y);

  lv_obj_t *val = lv_label_create(scr);
  lv_label_set_text(val, value);
  lv_obj_set_style_text_font(val, UI_FONT_16, 0);
  lv_obj_set_style_text_color(val, COLOR_TEXT, 0);
  lv_obj_align(val, LV_ALIGN_TOP_RIGHT, -12, y);
  return val;
}

/* Tính toán và làm tươi nhịp độ di chuyển (Pace)
   Công thức tính: Số phút cần để hoàn thành 1 km dựa trên khoảng cách GPS và thời gian trôi qua.
   - m: Con trỏ cấu trúc dữ liệu GPS chứa khoảng cách tích lũy */
static void activity_update_pace(const watch_gps_metrics_t *m) {
  if (!s_activity.pace_label) return;
  if (!m || m->total_distance_km <= 0.005f || s_activity.elapsed_sec == 0) {
    ui_label_set_text_if_changed(s_activity.pace_label, "--'--\"/km");
    return;
  }

  uint32_t sec_per_km = (uint32_t)((float)s_activity.elapsed_sec / m->total_distance_km);
  if (sec_per_km > 3600) { // Chậm hơn 1 km/h (60 phút/km) -> Coi như đứng yên
    ui_label_set_text_if_changed(s_activity.pace_label, "--'--\"/km");
    return;
  }

  char buf[16];
  snprintf(buf, sizeof(buf), "%lu'%02lu\"/km",
           (unsigned long)(sec_per_km / 60),
           (unsigned long)(sec_per_km % 60));
  ui_label_set_text_if_changed(s_activity.pace_label, buf);
}

/* Chuyển nút dừng sang trạng thái LƯU khi phiên tập đang tạm dừng */
static void activity_set_stop_save_state(void) {
  if (s_activity.lbl_stop) lv_label_set_text(s_activity.lbl_stop, ui_text("SAVE", "Lưu"));
  if (s_activity.btn_stop) lv_obj_set_style_bg_color(s_activity.btn_stop, COLOR_GREEN, 0);
}

/* Định thời 1 giây để đọc cảm biến và cập nhật giao diện phiên tập
   Hàm này lấy dữ liệu từ:
   - Bộ đếm bước chân của IMU.
   - Nhịp tim từ cảm biến.
   - Quãng đường và tốc độ thực tế từ GPS. */
static void activity_timer_cb(lv_timer_t *t) {
  (void)t;
  if (s_activity.active) {
    s_activity.elapsed_sec = (lv_tick_get() - s_activity.start_tick) / 1000;
  }

  char tb[16];
  activity_format_time(tb, sizeof(tb), s_activity.elapsed_sec);
  if (s_activity.time_label) ui_label_set_text_if_changed(s_activity.time_label, tb);

  if (s_activity.steps_label) {
    watch_imu_data_t imu;
    if (hardware_i2c_sensor_get_imu(&imu)) {
      char steps[24];
      snprintf(steps, sizeof(steps), "%lu steps", (unsigned long)imu.step_count);
      ui_label_set_text_if_changed(s_activity.steps_label, steps);
    }
  }

  if (s_activity.bpm_label) {
    watch_heart_rate_data_t hr;
    if (hardware_i2c_sensor_get_heart_rate(&hr) && hr.valid) {
      char hr_buf[16];
      snprintf(hr_buf, sizeof(hr_buf), "%.0f bpm", hr.heart_rate);
      ui_label_set_text_if_changed(s_activity.bpm_label, hr_buf);
    } else {
      ui_label_set_text_if_changed(s_activity.bpm_label, "-- bpm");
    }
  }

  watch_gps_metrics_t m;
  if (!watch_gps_get_metrics(&m)) {
    activity_update_pace(NULL);
    if (s_activity.gps_quality_label) {
      ui_label_set_text_if_changed(s_activity.gps_quality_label, ui_text("GPS: Offline", "GPS: Ngoại tuyến"));
      lv_obj_set_style_text_color(s_activity.gps_quality_label, COLOR_TEXT_MUTED, 0);
    }
    return;
  }

  /* Cập nhật chất lượng tín hiệu GPS góc trên bên phải */
  if (s_activity.gps_quality_label) {
    if (!m.fix_valid) {
      ui_label_set_text_if_changed(s_activity.gps_quality_label, ui_text("GPS: Searching...", "GPS: Đang tìm..."));
      lv_obj_set_style_text_color(s_activity.gps_quality_label, COLOR_TEXT_MUTED, 0);
    } else {
      int quality = 0;
      quality += m.satellites_in_use >= 8 ? 55 :
                 m.satellites_in_use >= 5 ? 40 :
                 m.satellites_in_use >= 4 ? 25 :
                 (int)m.satellites_in_use * 5;
      quality += m.hdop > 0.0f && m.hdop <= 1.5f ? 45 :
                 m.hdop > 0.0f && m.hdop <= 3.0f ? 30 :
                 m.hdop > 0.0f && m.hdop <= 5.0f ? 15 : 0;
      if (quality > 100) quality = 100;

      if (quality >= 70) {
        ui_label_set_text_if_changed(s_activity.gps_quality_label, ui_text("GPS: Excellent", "GPS: Mạnh"));
        lv_obj_set_style_text_color(s_activity.gps_quality_label, COLOR_GREEN, 0);
      } else if (quality >= 40) {
        ui_label_set_text_if_changed(s_activity.gps_quality_label, ui_text("GPS: Good", "GPS: Tốt"));
        lv_obj_set_style_text_color(s_activity.gps_quality_label, COLOR_PRIMARY, 0);
      } else {
        ui_label_set_text_if_changed(s_activity.gps_quality_label, ui_text("GPS: Weak", "GPS: Yếu"));
        lv_obj_set_style_text_color(s_activity.gps_quality_label, COLOR_ORANGE, 0);
      }
    }
  }

  char distance[16];
  activity_format_distance(distance, sizeof(distance), m.total_distance_km);
  if (s_activity.distance_label) ui_label_set_text_if_changed(s_activity.distance_label, distance);

  if (s_activity.speed_label) {
    char speed[16];
    activity_format_speed(speed, sizeof(speed), &m);
    ui_label_set_text_if_changed(s_activity.speed_label, speed);
  }
  activity_update_pace(&m);
}

/* Callback xử lý nút bấm Bắt đầu / Tạm dừng / Tiếp tục
   Quản lý khởi chạy/tạm dừng bộ định thời hiển thị và trạng thái đọc của các cảm biến phần cứng. */
static void activity_toggle_cb(lv_event_t *e) {
  (void)e;
  ui_haptic_feedback(UI_HAPTIC_CONFIRM);

  if (!s_activity.active) {
    if (s_activity.elapsed_sec == 0) {
      watch_gps_reset_track();
      ESP_ERROR_CHECK_WITHOUT_ABORT(watch_activity_log_capture_start());
      hardware_i2c_sensor_reset_steps();
      s_activity.session_id = (uint32_t)time(NULL);
      if (s_activity.session_id == 0) {
        s_activity.session_id = (uint32_t)lv_tick_get();
      }
    }
    s_activity.active = true;
    s_activity.ever_started = true;
    s_activity.start_tick = lv_tick_get() - s_activity.elapsed_sec * 1000;
    
    // Bật cờ cảm biến: Nhịp tim/SpO2, GPS/IMU
    hardware_i2c_sensor_set_heart_rate_enabled(true);
    watch_gps_set_activity_mode(
        s_activity.cfg && s_activity.cfg->screen_id == UI_SCREEN_RUNNING
            ? WATCH_ACTIVITY_MODE_WALKING
            : WATCH_ACTIVITY_MODE_CYCLING);
    watch_gps_request_start();
    
    if (s_activity.lbl_pause) lv_label_set_text(s_activity.lbl_pause, ui_text("PAUSE", "Tạm dừng"));
    if (s_activity.lbl_stop) lv_label_set_text(s_activity.lbl_stop, ui_text("SAVE", "Lưu"));
    if (s_activity.btn_stop) lv_obj_set_style_bg_color(s_activity.btn_stop, COLOR_GREEN, 0);
    if (!s_activity.timer) {
      s_activity.timer = lv_timer_create(activity_timer_cb, 1000, NULL);
    }
    activity_timer_cb(NULL);
  } else {
    s_activity.active = false;
    
    // Tắt cờ cảm biến: Nhịp tim/SpO2, GPS/IMU
    hardware_i2c_sensor_set_heart_rate_enabled(false);
    watch_gps_set_activity_mode(WATCH_ACTIVITY_MODE_NONE);
    watch_gps_request_stop();
    
    if (s_activity.lbl_pause) lv_label_set_text(s_activity.lbl_pause, ui_text("RESUME", "Tiếp tục"));
    if (s_activity.lbl_stop) lv_label_set_text(s_activity.lbl_stop, ui_text("DELETE", "Xóa"));
    if (s_activity.btn_stop) lv_obj_set_style_bg_color(s_activity.btn_stop, COLOR_RED, 0);
    activity_set_stop_save_state();
    if (s_activity.timer) {
      lv_timer_del(s_activity.timer);
      s_activity.timer = NULL;
    }
  }
}


/* Dọn dẹp tài nguyên và tắt các luồng cảm biến của phiên hoạt động */
static void activity_screen_cleanup(void) {
  ESP_LOGI("activity", "Running activity_screen_cleanup");
  if (s_activity.timer) {
    lv_timer_del(s_activity.timer);
    s_activity.timer = NULL;
  }
  hardware_i2c_sensor_set_heart_rate_enabled(false);
  watch_gps_set_activity_mode(WATCH_ACTIVITY_MODE_NONE);
  watch_gps_request_stop();
  if (s_activity.ever_started && !s_activity.saving) {
    watch_activity_log_capture_discard();
  }
  memset(&s_activity, 0, sizeof(s_activity));
}

/* Callback giải phóng tài nguyên khi màn hình phiên luyện tập bị hủy */
static void activity_screen_detach_widgets(void) {
  s_activity.screen = NULL;
  s_activity.time_label = NULL;
  s_activity.distance_label = NULL;
  s_activity.steps_label = NULL;
  s_activity.pace_label = NULL;
  s_activity.speed_label = NULL;
  s_activity.bpm_label = NULL;
  s_activity.gps_quality_label = NULL;
  s_activity.btn_pause = NULL;
  s_activity.lbl_pause = NULL;
  s_activity.btn_stop = NULL;
  s_activity.lbl_stop = NULL;
}

static void activity_screen_delete_cb(lv_event_t *e) {
  lv_obj_t *screen = lv_event_get_target(e);
  if (screen != s_activity.screen) return;

  ESP_LOGI("activity", "activity_screen_delete_cb triggered");
  if (s_activity.ever_started && !s_activity.saving) {
    // Session is still active in the background!
    // Just stop the UI timer and detach the screen pointer.
    if (s_activity.timer) {
      lv_timer_del(s_activity.timer);
      s_activity.timer = NULL;
    }
    activity_screen_detach_widgets();
    s_activity.exit_timestamp = lv_tick_get();
    ESP_LOGI("activity", "Workout session moved to background");
  } else {
    // Session is completed/saved or explicitly discarded, so perform full cleanup.
    activity_screen_cleanup();
  }
}

void ui_activity_check_background_timeout(void) {
  if (s_activity.ever_started && s_activity.screen == NULL) {
    uint32_t now_ms = lv_tick_get();
    if (s_activity.exit_timestamp != 0 && (now_ms - s_activity.exit_timestamp) >= 3600000U) {
      ESP_LOGW("activity", "Workout session background timeout (1 hour). Discarding.");
      activity_screen_cleanup();
    }
  }
}

/* Callback tắt hộp thông báo Toast tự động sau khoảng thời gian hiển thị */
static void activity_toast_timer_cb(lv_timer_t *t) {
  lv_obj_t *toast = t ? (lv_obj_t *)t->user_data : NULL;
  if (toast && lv_obj_is_valid(toast)) {
    lv_obj_del(toast);
  }
  lv_timer_del(t);
}

/* Định thời để thoát màn hình và đưa người dùng trở lại màn hình trước đó */
static void activity_exit_timer_cb(lv_timer_t *t) {
  ESP_LOGI("activity", "Running activity_exit_timer_cb");
  lv_obj_t *screen = t ? (lv_obj_t *)t->user_data : NULL;
  lv_timer_del(t);
  if (!screen || !lv_obj_is_valid(screen) || s_activity.screen != screen) {
    ESP_LOGW("activity", "activity_exit_timer_cb screen invalid or mismatch");
    return;
  }
  activity_screen_cleanup();
  ESP_LOGI("activity", "Calling ui_navigation_pop from activity_exit_timer_cb");
  ui_navigation_pop();
}

typedef struct {
  watch_activity_record_t record;
  uint32_t session_id;
  lv_obj_t *screen;
  esp_err_t result;
  bool route_saved;
} activity_save_job_t;

static void activity_save_complete_async(void *arg) {
  activity_save_job_t *job = (activity_save_job_t *)arg;
  if (!job) return;

  if (job->screen && lv_obj_is_valid(job->screen) && s_activity.screen == job->screen) {
    const bool stats_only = job->result == ESP_OK && !job->route_saved;
    const lv_color_t result_color =
        job->result != ESP_OK ? COLOR_RED : (stats_only ? COLOR_ORANGE : COLOR_GREEN);
    lv_obj_t *toast = lv_obj_create(s_activity.screen);
    lv_obj_set_size(toast, 190, 110);
    lv_obj_center(toast);
    lv_obj_set_style_bg_color(toast, COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(toast, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(toast, 16, 0);
    lv_obj_set_style_border_width(toast, 2, 0);
    lv_obj_set_style_border_color(toast, result_color, 0);
    lv_obj_clear_flag(toast, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ic = lv_label_create(toast);
    lv_label_set_text(ic, job->result == ESP_OK ? ICON_SIGNAL_OK : ICON_SIGNAL_CLOSE);
    lv_obj_set_style_text_font(ic, &signal_icon, 0);
    lv_obj_set_style_text_color(ic, result_color, 0);
    lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 15);

    lv_obj_t *txt = lv_label_create(toast);
    lv_label_set_text(txt, job->result != ESP_OK
                               ? ui_text("SAVE FAILED", "Lưu thất bại")
                               : (stats_only
                                      ? ui_text("STATS SAVED; NO ROUTE", "Chỉ lưu thống kê")
                                      : ui_text("SAVED SUCCESSFULLY", "Đã lưu thành công")));
    lv_obj_set_style_text_font(txt, UI_FONT_14, 0);
    lv_obj_set_style_text_color(txt, COLOR_TEXT, 0);
    lv_obj_align(txt, LV_ALIGN_BOTTOM_MID, 0, -15);

    if (job->result == ESP_OK) {
      s_activity.ever_started = false; // Mark as saved so we don't discard capture on exit
      lv_timer_create(activity_exit_timer_cb, 1200, s_activity.screen);
    } else {
      s_activity.saving = false; // Only allow clicks again if it failed
      s_activity.ever_started = true; // Cho phép thử lại lưu
      if (s_activity.btn_stop) lv_obj_clear_state(s_activity.btn_stop, LV_STATE_DISABLED);
      if (s_activity.lbl_stop) {
        lv_label_set_text(s_activity.lbl_stop, ui_text("RETRY SAVE", "Thử lưu lại"));
      }
      lv_timer_create(activity_toast_timer_cb, 1800, toast);
    }
  }
  free(job);
}

static void activity_save_worker(void *arg) {
  activity_save_job_t *job = (activity_save_job_t *)arg;
  hardware_i2c_sensor_set_heart_rate_enabled(false);
  watch_gps_stop();

  job->result = watch_activity_log_capture_finish(&job->record);
  job->route_saved = job->result == ESP_OK;
  if (job->result != ESP_OK) {
    ESP_LOGW("activity", "Route save failed (%s); saving activity statistics only",
             esp_err_to_name(job->result));
    watch_activity_log_capture_discard();
    job->result = watch_activity_log_add(&job->record, NULL);
  }
  if (job->result == ESP_OK) {
    char act_msg[160];
    snprintf(act_msg, sizeof(act_msg), "ACT|%lu|%lu|%u|%lu|%lu|%.3f|%.1f",
             (unsigned long)job->session_id,
             (unsigned long)job->record.timestamp,
             (unsigned int)job->record.sport_id,
             (unsigned long)job->record.duration_sec,
             (unsigned long)job->record.steps,
             job->record.distance_km,
             job->record.avg_speed_kmh);
    watch_bluetooth_send_command(act_msg);
    // Disabling proactive send to let the phone pull the track on-demand via TRACK_REQ.
    // This avoids BLE collisions and SPIFFS/VFS lock-ups during screen transition.
    // if (job->route_saved && watch_bluetooth_is_connected()) {
    //   watch_bluetooth_send_history_track(0);
    // }
  }

  lv_res_t async_res = LV_RES_INV;
  for (int attempt = 0; attempt < 5 && async_res != LV_RES_OK; ++attempt) {
    if (lvgl_port_lock(100)) {
      async_res = lv_async_call(activity_save_complete_async, job);
      lvgl_port_unlock();
    }
    if (async_res != LV_RES_OK) {
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }
  if (async_res != LV_RES_OK) {
    ESP_LOGE("activity", "Cannot enqueue activity save completion");
    free(job);
  }
  vTaskDelete(NULL);
}

static void activity_save_start_failed(void) {
  s_activity.saving = false;
  s_activity.ever_started = true; // Cho phép thử lại lưu
  hardware_i2c_sensor_set_heart_rate_enabled(false);
  watch_gps_set_activity_mode(WATCH_ACTIVITY_MODE_NONE);
  watch_gps_request_stop();
  if (s_activity.btn_stop) lv_obj_clear_state(s_activity.btn_stop, LV_STATE_DISABLED);
  if (s_activity.lbl_stop) {
    lv_label_set_text(s_activity.lbl_stop, ui_text("RETRY SAVE", "Thử lưu lại"));
  }
}

/* Callback xử lý nút bấm dừng hẳn phiên hoạt động (Lưu hoặc Hủy)
   Nếu đã bắt đầu luyện tập và có dữ liệu, thực hiện:
   1. Đọc và tính toán lại quãng đường tổng cộng (km), nhịp tim, số bước (IMU).
   2. Lưu trữ bản ghi hoạt động thể thao cục bộ vào bộ nhớ Flash NVS.
   3. Truyền tải gói dữ liệu định dạng JSON/Text qua BLE thông báo cho Flutter App.
   4. Hiển thị thông báo Toast "ĐÃ LƯU THÀNH CÔNG". */
static void activity_stop_cb(lv_event_t *e) {
  (void)e;
  if (s_activity.saving) return;
  ui_haptic_feedback(UI_HAPTIC_WARNING);
  if (s_activity.elapsed_sec == 0) {
    activity_screen_cleanup();
    ui_navigation_pop();
    return;
  }


  if (s_activity.ever_started) {
    s_activity.ever_started = false; // Ngăn chặn kích hoạt lưu nhiều lần khi đang xử lý
    watch_gps_metrics_t m;
    watch_imu_data_t imu = {0};
    float km = 0.0f;
    float avg_speed = 0.0f;
    if (watch_gps_get_metrics(&m)) {
      km = m.total_distance_km;
      avg_speed = s_activity.elapsed_sec > 0 ? (km * 3600.0f / (float)s_activity.elapsed_sec) : 0.0f;
    }
    hardware_i2c_sensor_get_imu(&imu);

    uint32_t activity_session_id = s_activity.session_id != 0 ? s_activity.session_id : (uint32_t)time(NULL);
    uint32_t activity_timestamp = (uint32_t)time(NULL);
    uint8_t activity_sport_id = (uint8_t)(s_activity.cfg ? s_activity.cfg->screen_id : 0);
    s_activity.active = false;
    s_activity.saving = true;
    if (s_activity.timer) {
      lv_timer_del(s_activity.timer);
      s_activity.timer = NULL;
    }

    if (s_activity.btn_pause) lv_obj_add_state(s_activity.btn_pause, LV_STATE_DISABLED);
    if (s_activity.btn_stop) lv_obj_add_state(s_activity.btn_stop, LV_STATE_DISABLED);
    if (s_activity.lbl_stop) lv_label_set_text(s_activity.lbl_stop, ui_text("SAVING...", "Đang lưu..."));

    activity_save_job_t *job = calloc(1, sizeof(*job));
    if (job) {
      job->record = (watch_activity_record_t) {
          .timestamp = activity_timestamp,
          .sport_id = activity_sport_id,
          .duration_sec = s_activity.elapsed_sec,
          .steps = imu.step_count,
          .distance_km = km,
          .avg_speed_kmh = avg_speed,
      };
      job->session_id = activity_session_id;
      job->screen = s_activity.screen;
      if (xTaskCreate(activity_save_worker, "activity_save", 4096, job, 2, NULL) != pdPASS) {
        activity_save_start_failed();
        free(job);
        ESP_LOGE("activity", "Cannot create activity save worker");
      }
    } else {
      activity_save_start_failed();
      ESP_LOGE("activity", "Cannot allocate activity save job");
    }
  } else {
    watch_gps_reset_track();
    watch_activity_log_capture_discard();
    printf("[Activity] Discarded workout\n");

    s_activity.elapsed_sec = 0;
    s_activity.active = false;
    s_activity.ever_started = false; // Mark session as stopped/discarded
    hardware_i2c_sensor_set_heart_rate_enabled(false);
    if (s_activity.timer) {
      lv_timer_del(s_activity.timer);
      s_activity.timer = NULL;
    }

    if (s_activity.time_label) lv_label_set_text(s_activity.time_label, "00:00:00");
    if (s_activity.distance_label) lv_label_set_text(s_activity.distance_label, "0.00 km");
    if (s_activity.steps_label) lv_label_set_text(s_activity.steps_label, "0 steps");
    if (s_activity.pace_label) lv_label_set_text(s_activity.pace_label, "--'--\"/km");
    if (s_activity.speed_label) lv_label_set_text(s_activity.speed_label, "--.-");
    if (s_activity.bpm_label) lv_label_set_text(s_activity.bpm_label, "-- bpm");

    if (s_activity.lbl_pause) lv_label_set_text(s_activity.lbl_pause, ui_text("START", "BẮT ĐẦU"));
    if (s_activity.btn_pause) lv_obj_set_style_bg_color(s_activity.btn_pause, COLOR_ORANGE, 0);

    if (s_activity.lbl_stop) lv_label_set_text(s_activity.lbl_stop, ui_text("EXIT", "THOÁT"));
    if (s_activity.btn_stop) lv_obj_set_style_bg_color(s_activity.btn_stop, COLOR_RED, 0);

    lv_obj_t *toast = lv_obj_create(s_activity.screen);
    lv_obj_set_size(toast, 190, 110);
    lv_obj_center(toast);
    lv_obj_set_style_bg_color(toast, COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(toast, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(toast, 16, 0);
    lv_obj_set_style_border_width(toast, 2, 0);
    lv_obj_set_style_border_color(toast, COLOR_RED, 0);
    lv_obj_clear_flag(toast, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ic = lv_label_create(toast);
    lv_label_set_text(ic, ICON_SIGNAL_TRASH);
    lv_obj_set_style_text_font(ic, &signal_icon, 0);
    lv_obj_set_style_text_color(ic, COLOR_RED, 0);
    lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 15);

    lv_obj_t *txt = lv_label_create(toast);
    lv_label_set_text(txt, ui_text("DELETED SESSION", "ĐÃ XÓA BÀI TẬP"));
    lv_obj_set_style_text_font(txt, UI_FONT_14, 0);
    lv_obj_set_style_text_color(txt, COLOR_TEXT, 0);
    lv_obj_align(txt, LV_ALIGN_BOTTOM_MID, 0, -15);

    lv_timer_create(activity_toast_timer_cb, 1200, toast);
  }
}
    /* Khởi tạo giao diện cấu trúc màn hình phiên tập thể thao
   Thiết kế các nhãn hiển thị thông số: Thời gian trôi qua, Quãng đường, Bước chân (nếu có),
   Nhịp độ (nếu có), Vận tốc (nếu có), và Nhịp tim đo trực tiếp.
   Bố trí 2 nút bấm điều khiển ở dưới cùng: Bắt đầu/Tạm dừng và Kết thúc/Xóa.
   - cfg: Con trỏ trỏ tới thông số cấu hình riêng biệt của môn thể thao đó (Running/Cycling)
   Trả về: lv_obj_t* Con trỏ đối tượng màn hình phiên luyện tập được tạo */
static lv_obj_t *ui_activity_session_screen_create(const activity_screen_config_t *cfg) {
  lv_obj_t *scr = ui_navigation_create_screen_with_id(cfg->screen_id);
  lv_obj_add_event_cb(scr, activity_screen_delete_cb, LV_EVENT_DELETE, NULL);

  bool resuming = false;
  if (s_activity.ever_started) {
    if (s_activity.cfg && s_activity.cfg->screen_id == cfg->screen_id) {
      resuming = true;
      s_activity.screen = scr;
      s_activity.exit_timestamp = 0;
      ESP_LOGI("activity", "Resuming background workout session");
    } else {
      activity_screen_cleanup();
      memset(&s_activity, 0, sizeof(s_activity));
      s_activity.screen = scr;
      s_activity.cfg = cfg;
    }
  } else {
    memset(&s_activity, 0, sizeof(s_activity));
    s_activity.screen = scr;
    s_activity.cfg = cfg;
  }
  lv_color_t activity_color = lv_color_hex(cfg->color_hex);

  lv_obj_t *ic_bg = lv_obj_create(scr);
  lv_obj_set_size(ic_bg, 36, 36);
  lv_obj_set_style_radius(ic_bg, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(ic_bg, activity_color, 0);
  lv_obj_set_style_border_width(ic_bg, 0, 0);
  lv_obj_clear_flag(ic_bg, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(ic_bg, LV_ALIGN_TOP_LEFT, 12, 36);

  lv_obj_t *ic_lbl = lv_label_create(ic_bg);
  lv_label_set_text(ic_lbl, cfg->icon);
  lv_obj_set_style_text_color(ic_lbl, lv_color_white(), 0);
  lv_obj_set_style_text_font(ic_lbl, &workout_health_icon, 0);
  lv_obj_center(ic_lbl);

  /* Thêm nhãn hiển thị chất lượng định vị GPS góc trên bên phải */
  s_activity.gps_quality_label = lv_label_create(scr);
  lv_label_set_text(s_activity.gps_quality_label, ui_text("GPS: Searching...", "GPS: Đang tìm..."));
  lv_obj_set_style_text_font(s_activity.gps_quality_label, UI_FONT_12, 0);
  lv_obj_set_style_text_color(s_activity.gps_quality_label, COLOR_TEXT_MUTED, 0);
  lv_obj_align(s_activity.gps_quality_label, LV_ALIGN_TOP_RIGHT, -12, 44);

  s_activity.time_label = lv_label_create(scr);
  lv_label_set_text(s_activity.time_label, "00:00:00");
  lv_obj_set_style_text_font(s_activity.time_label, UI_FONT_36, 0);
  lv_obj_set_style_text_color(s_activity.time_label, activity_color, 0);
  lv_obj_align(s_activity.time_label, LV_ALIGN_TOP_MID, 0, 78);

  const int row_h = 28;
  int row = 0;
  s_activity.distance_label = activity_add_stat_row(scr, ui_tr(UI_TXT_DISTANCE), "0.00 km",
                                                     COLOR_TEXT_MUTED,
                                                     cfg->stats_y + row_h * row++);
  if (cfg->show_steps) {
    s_activity.steps_label = activity_add_stat_row(scr, ui_tr(UI_TXT_STEPS), "0 steps",
                                                   COLOR_TEXT_MUTED,
                                                   cfg->stats_y + row_h * row++);
  }
  if (cfg->show_pace) {
    s_activity.pace_label = activity_add_stat_row(scr, ui_tr(UI_TXT_PACE), "--'--\"/km",
                                                   COLOR_TEXT_MUTED,
                                                   cfg->stats_y + row_h * row++);
  }
  if (cfg->show_speed) {
    s_activity.speed_label = activity_add_stat_row(scr, ui_tr(UI_TXT_SPEED), "--.-",
                                                   COLOR_TEXT_MUTED,
                                                   cfg->stats_y + row_h * row++);
  }
  s_activity.bpm_label = activity_add_stat_row(scr, "HR", "-- bpm", COLOR_RED,
                                               cfg->stats_y + row_h * row);

  lv_obj_t *btn_cont = lv_obj_create(scr);
  lv_obj_set_size(btn_cont, WATCH_LCD_H_RES, 46);
  lv_obj_align(btn_cont, LV_ALIGN_BOTTOM_MID, 0, -6);
  lv_obj_set_style_bg_opa(btn_cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(btn_cont, 0, 0);
  lv_obj_set_style_pad_all(btn_cont, 0, 0);
  lv_obj_clear_flag(btn_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(btn_cont, 12, 0);

  lv_obj_t *btn_pause = lv_btn_create(btn_cont);
  lv_obj_set_size(btn_pause, 100, 38);
  lv_obj_set_style_radius(btn_pause, 8, 0);
  lv_obj_set_style_bg_color(btn_pause, COLOR_ORANGE, 0);
  lv_obj_set_style_shadow_width(btn_pause, 0, 0);
  lv_obj_add_event_cb(btn_pause, activity_toggle_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *lbl_p = lv_label_create(btn_pause);
  lv_label_set_text(lbl_p, ui_text("START", "Bắt đầu"));
  lv_obj_set_style_text_font(lbl_p, UI_FONT_14, 0);
  lv_obj_center(lbl_p);

  lv_obj_t *btn_stop = lv_btn_create(btn_cont);
  lv_obj_set_size(btn_stop, 100, 38);
  lv_obj_set_style_radius(btn_stop, 8, 0);
  lv_obj_set_style_bg_color(btn_stop, COLOR_RED, 0);
  lv_obj_set_style_shadow_width(btn_stop, 0, 0);
  lv_obj_add_event_cb(btn_stop, activity_stop_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *lbl_s = lv_label_create(btn_stop);
  lv_label_set_text(lbl_s, ui_text("EXIT", "Thoát"));
  lv_obj_set_style_text_font(lbl_s, UI_FONT_14, 0);
  lv_obj_center(lbl_s);

  s_activity.btn_pause = btn_pause;
  s_activity.lbl_pause = lbl_p;
  s_activity.btn_stop = btn_stop;
  s_activity.lbl_stop = lbl_s;

  if (resuming) {
    if (s_activity.active) {
      lv_label_set_text(s_activity.lbl_pause, ui_text("PAUSE", "Tạm dừng"));
      lv_label_set_text(s_activity.lbl_stop, ui_text("SAVE", "Lưu"));
      lv_obj_set_style_bg_color(s_activity.btn_stop, COLOR_GREEN, 0);
      if (!s_activity.timer) {
        s_activity.timer = lv_timer_create(activity_timer_cb, 1000, NULL);
      }
    } else {
      lv_label_set_text(s_activity.lbl_pause, ui_text("RESUME", "Tiếp tục"));
      lv_label_set_text(s_activity.lbl_stop, ui_text("SAVE", "Lưu"));
      lv_obj_set_style_bg_color(s_activity.btn_stop, COLOR_GREEN, 0);
    }
    activity_timer_cb(NULL);
  }

  ui_navigation_add_gesture(scr);
  return scr;
}

/* Điểm vào để khởi tạo màn hình chạy bộ / đi bộ
   Trả về: lv_obj_t* Con trỏ đối tượng màn hình chạy bộ được tạo */
lv_obj_t *ui_running_screen_create(void) {
  return ui_activity_session_screen_create(&s_running_cfg);
}

/* Điểm vào để khởi tạo màn hình đạp xe (Cycling)
   Trả về: lv_obj_t* Con trỏ đối tượng màn hình đạp xe được tạo */
lv_obj_t *ui_cycling_screen_create(void) {
  return ui_activity_session_screen_create(&s_cycling_cfg);
}

/* ============ PHÂN HỆ QUẢN LÝ THÔNG BÁO (NOTIFICATIONS) ============ */

/* Cấu trúc dữ liệu chứa nội dung chi tiết của một thẻ thông báo */
typedef struct {
    char *app_name;         /* Tên ứng dụng gửi thông báo (Messenger, Zalo, SMS,...) */
    char *sender;           /* Tên người gửi */
    char *message;          /* Nội dung tin nhắn hiển thị */
    uint32_t seq_id;        /* Mã tuần tự (Sequence ID) để quản lý việc xóa/sửa */
    lv_obj_t *delete_btn;   /* Con trỏ nút bấm xóa thẻ thông báo này */
} notification_card_data_t;

/* Khai báo các biến trạng thái tĩnh phục vụ hộp thư thông báo */
static lv_obj_t *s_notif_list = NULL;
static int s_last_notif_count = -1;
static uint32_t s_last_notif_overflow_count = UINT32_MAX;
static lv_timer_t *s_notif_refresh_timer = NULL;

static void notifications_refresh_list_cb(lv_timer_t *t);

/* Hàm khởi tạo cấp phát động cho cấu trúc thông báo
   - app_name: Tên ứng dụng nguồn
   - sender: Tên người gửi
   - message: Nội dung thông điệp
   - seq_id: ID định danh gói tin nhận qua BLE
   Trả về: notification_card_data_t* Con trỏ cấu trúc được cấp phát bộ nhớ */
static notification_card_data_t *notifications_card_data_create(const char *app_name,
                                                                const char *sender,
                                                                const char *message,
                                                                uint32_t seq_id) {
    notification_card_data_t *data = malloc(sizeof(*data));
    if (!data) return NULL;
    data->app_name = ui_safe_strdup(app_name && app_name[0] ? app_name : "Other");
    data->sender = ui_safe_strdup(sender && sender[0] ? sender : ui_text("Message", "Tin nhắn"));
    data->message = ui_safe_strdup(message);
    data->seq_id = seq_id;
    data->delete_btn = NULL;
    if (!data->app_name || !data->sender || !data->message) {
        free(data->app_name);
        free(data->sender);
        free(data->message);
        free(data);
        return NULL;
    }
    return data;
}

/* Callback giải phóng bộ nhớ động của cấu trúc dữ liệu thông báo */
static void notifications_card_data_delete_cb(lv_event_t *e) {
    notification_card_data_t *data = lv_event_get_user_data(e);
    if (!data) return;
    free(data->app_name);
    free(data->sender);
    free(data->message);
    free(data);
}

#ifndef ICON_CLOSE
#define ICON_CLOSE "\xef\x80\x8d"
#endif

LV_FONT_DECLARE(signal_icon);

#define MAX_QUICK_REPLIES 32
static char *s_dynamic_quick_replies[MAX_QUICK_REPLIES] = {NULL};
static int s_dynamic_quick_reply_count = 0;

/* Danh sách các câu trả lời mẫu tiếng Việt định sẵn phục vụ trả lời nhanh */
static const char *s_default_quick_replies[] = {
    "OK",
    "Tôi đang bận",
    "Đang lái xe, gọi lại sau",
    "Đồng ý",
    "Không đồng ý",
    "Đã nhận thông tin",
    "Gọi lại cho tôi nhé"
};

/* Khởi tạo các câu trả lời nhanh mặc định vào bộ đệm động */
static void init_dynamic_quick_replies(void) {
    if (s_dynamic_quick_reply_count > 0) return;
    int default_count = sizeof(s_default_quick_replies) / sizeof(s_default_quick_replies[0]);
    for (int i = 0; i < default_count && i < MAX_QUICK_REPLIES; i++) {
        char *copy = ui_safe_strdup(s_default_quick_replies[i]);
        if (copy) {
            s_dynamic_quick_replies[s_dynamic_quick_reply_count++] = copy;
        }
    }
}

/* Cấu trúc dữ liệu liên kết cho mỗi lựa chọn trả lời nhanh */
typedef struct {
    char *app_name;
    char *sender;
    char *reply_text;
} quick_reply_item_data_t;

/* Callback gửi phản hồi nhanh đã chọn lên Mobile App qua kết nối BLE */
static void quick_reply_select_cb(lv_event_t *e) {
    quick_reply_item_data_t *data = lv_event_get_user_data(e);
    if (!data) return;

    // Định dạng gói tin BLE gửi lên điện thoại: REPLY|AppName|Sender|Text
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "REPLY|%s|%s|%s", data->app_name, data->sender, data->reply_text);
    if (watch_bluetooth_send_command(cmd) != ESP_OK) {
        ui_haptic_feedback(UI_HAPTIC_WARNING);
        return;
    }

    // Quay lại các màn hình trước an toàn
    ui_navigation_pop_multiple(2);
}

/* Giải phóng dữ liệu liên kết lựa chọn trả lời nhanh */
static void quick_reply_item_data_delete_cb(lv_event_t *e) {
    quick_reply_item_data_t *data = lv_event_get_user_data(e);
    if (!data) return;
    free(data->app_name);
    free(data->sender);
    free(data->reply_text);
    free(data);
}


/* ── BÀN PHÍM ẢO TỰ SOẠN TIN TRẢ LỜI NHANH (CUSTOM REPLY KEYBOARD) ── */
static char s_reply_text_buf[128];                                 /* Bộ đệm lưu trữ chuỗi văn bản đang soạn thảo */
static int s_reply_text_len = 0;                                    /* Độ dài hiện tại của chuỗi văn bản */
static lv_obj_t *s_reply_text_label = NULL;                         /* Nhãn hiển thị chuỗi văn bản đang nhập trên LCD */
static lv_obj_t *s_reply_keyboard_container = NULL;                 /* Container chứa lưới các phím bấm */
static lv_obj_t *s_reply_caps_button = NULL;                        /* Nút bấm bật/tắt viết hoa (Shift/Caps Lock) */
static lv_obj_t *s_reply_mode_button = NULL;                        /* Nút bấm chuyển trang bàn phím (Chữ / Số / Ký tự) */
static lv_obj_t *s_reply_delete_button = NULL;                      /* Nút xóa lùi ký tự (Backspace) */
static lv_obj_t *s_reply_enter_button = NULL;                       /* Nút xác nhận hoàn tất (OK/Enter) */
static int s_reply_key_page = 0;                                    /* Trang bàn phím hiện tại: 0 = Số, 1..3 = Các trang chữ cái */
static bool s_reply_key_upper = false;                              /* Cờ trạng thái chữ hoa */

static char *s_reply_target_app = NULL;                             /* Tên ứng dụng nhận phản hồi */
static char *s_reply_target_sender = NULL;                          /* Tên người nhận phản hồi */
static lv_obj_t *s_quick_reply_list_obj = NULL;                     /* Con trỏ danh sách cha để cập nhật câu mẫu động */
static char *s_quick_reply_context_app = NULL;                      
static char *s_quick_reply_context_sender = NULL;                   

/* Làm tươi nhãn văn bản hiển thị nội dung đang soạn thảo trên LCD */
static void reply_keyboard_update_label(void) {
    if (!s_reply_text_label) return;
    if (s_reply_text_len <= 0) {
        lv_label_set_text(s_reply_text_label, "");
    } else {
        lv_label_set_text(s_reply_text_label, s_reply_text_buf);
    }
}

/* Hàm khởi tạo nhanh một phím bấm trên bàn phím ảo
   - parent: Container chứa phím bấm
   - width: Chiều rộng phím
   - height: Chiều cao phím
   - bg: Màu nền phím bấm
   - text: Nhãn ký tự in trên phím
   Trả về: lv_obj_t* Con trỏ đối tượng phím bấm được tạo */
static lv_obj_t *reply_keyboard_make_button(lv_obj_t *parent, lv_coord_t width, lv_coord_t height, lv_color_t bg, const char *text) {
    lv_obj_t *btn = lv_btn_create(parent);
    if (!btn) return NULL;

    lv_obj_set_size(btn, width, height);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_outline_width(btn, 0, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(btn);
    if (!lbl) {
        lv_obj_del(btn);
        return NULL;
    }
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_14, 0);
    lv_obj_center(lbl);

    return btn;
}


static void reply_update_keyboard_control_labels(void) {
    if (s_reply_mode_button) {
        lv_obj_t *lbl = lv_obj_get_child(s_reply_mode_button, 0);
        if (lbl) {
            lv_label_set_text(lbl, s_reply_key_page == 0 ? "@" : "1");
        }
    }

    if (s_reply_caps_button) {
        lv_obj_t *lbl = lv_obj_get_child(s_reply_caps_button, 0);
        if (lbl) {
            lv_label_set_text(lbl, s_reply_key_upper ? "a" : "A");
        }
        lv_obj_set_style_bg_color(s_reply_caps_button, s_reply_key_upper ? COLOR_ORANGE : COLOR_PURPLE, 0);
    }

    if (s_reply_delete_button) {
        lv_obj_t *lbl = lv_obj_get_child(s_reply_delete_button, 0);
        if (lbl) lv_label_set_text(lbl, "<-");
    }

    if (s_reply_enter_button) {
        lv_obj_t *lbl = lv_obj_get_child(s_reply_enter_button, 0);
        if (lbl) lv_label_set_text(lbl, "OK");
    }
}

/* Callback khi người dùng chạm vào một phím ký tự thông thường
   Lấy nhãn ký tự trên phím bấm và thêm vào bộ đệm soạn thảo s_reply_text_buf. */
static void reply_keyboard_key_cb(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *lbl = lv_obj_get_child(btn, 0);
    const char *t = lbl ? lv_label_get_text(lbl) : NULL;
    if (!t) return;
    size_t L = strlen(t);
    for (size_t i = 0; i < L; i++) {
        if (s_reply_text_len < (int)sizeof(s_reply_text_buf) - 1) {
            s_reply_text_buf[s_reply_text_len++] = t[i];
        }
    }
    s_reply_text_buf[s_reply_text_len] = '\0';
    reply_keyboard_update_label();
}

/* Callback nút xóa lùi (Backspace / Delete) */
static void reply_keyboard_delete_cb(lv_event_t *e) {
    (void)e;
    if (s_reply_text_len > 0) {
        s_reply_text_len--;
        s_reply_text_buf[s_reply_text_len] = '\0';
        reply_keyboard_update_label();
    }
}

/* Callback chuyển chế độ viết hoa / viết thường */
static void reply_keyboard_caps_cb(lv_event_t *e) {
    (void)e;
    s_reply_key_upper = !s_reply_key_upper;
    reply_update_keyboard_control_labels();
    if (s_reply_key_page > 0) {
        // Rebuild lại bàn phím để đổi chữ hoa/thường
        void reply_rebuild_keyboard(void);
        reply_rebuild_keyboard();
    }
}

/* Callback chuyển đổi giữa các trang ký tự (chữ cái / số) */
static void reply_keyboard_mode_cb(lv_event_t *e) {
    (void)e;
    s_reply_key_page = (s_reply_key_page + 1) % 4;
    reply_update_keyboard_control_labels();
    void reply_rebuild_keyboard(void);
    reply_rebuild_keyboard();
}

static void ui_quick_reply_list_build(lv_obj_t *list, const char *app_name, const char *sender);

typedef struct {
    bool clear;
    char reply[96];
} quick_reply_sync_msg_t;

static void quick_reply_sync_apply_cb(void *arg) {
    quick_reply_sync_msg_t *msg = (quick_reply_sync_msg_t *)arg;
    if (!msg) return;

    if (msg->clear) {
        for (int i = 0; i < s_dynamic_quick_reply_count; i++) {
            free(s_dynamic_quick_replies[i]);
            s_dynamic_quick_replies[i] = NULL;
        }
        s_dynamic_quick_reply_count = 0;
    } else if (msg->reply[0] != '\0' && s_dynamic_quick_reply_count < MAX_QUICK_REPLIES) {
        char *copy = ui_safe_strdup(msg->reply);
        if (copy) s_dynamic_quick_replies[s_dynamic_quick_reply_count++] = copy;
    }

    if (s_quick_reply_list_obj && lv_obj_is_valid(s_quick_reply_list_obj)) {
        ui_quick_reply_list_build(s_quick_reply_list_obj,
                                  s_quick_reply_context_app,
                                  s_quick_reply_context_sender);
    }
    free(msg);
}

void ui_quick_replies_clear_from_ble(void) {
    quick_reply_sync_msg_t *msg = calloc(1, sizeof(*msg));
    if (!msg) return;
    msg->clear = true;
    lv_res_t res = LV_RES_INV;
    if (!ui_is_sleep_in_progress() && lvgl_port_lock(100)) {
        res = lv_async_call(quick_reply_sync_apply_cb, msg);
        lvgl_port_unlock();
    }
    if (res != LV_RES_OK) free(msg);
}

void ui_quick_replies_add_from_ble(const char *reply) {
    if (!reply || reply[0] == '\0') return;
    quick_reply_sync_msg_t *msg = calloc(1, sizeof(*msg));
    if (!msg) return;
    snprintf(msg->reply, sizeof(msg->reply), "%s", reply);
    lv_res_t res = LV_RES_INV;
    if (!ui_is_sleep_in_progress() && lvgl_port_lock(100)) {
        res = lv_async_call(quick_reply_sync_apply_cb, msg);
        lvgl_port_unlock();
    }
    if (res != LV_RES_OK) free(msg);
}

/* Callback nút Enter (OK) hoàn tất việc soạn thảo
   Nếu đang soạn thảo phản hồi cho một tin nhắn cụ thể:
   - Gửi chuỗi tin nhắn vừa soạn qua BLE bằng định dạng lệnh "REPLY|...".
   Nếu đang thêm một câu trả lời mẫu mới:
   - Thêm chuỗi vừa soạn vào danh sách câu mẫu động cục bộ s_dynamic_quick_replies. */
static void reply_keyboard_enter_cb(lv_event_t *e) {
    (void)e;
    if (s_reply_text_len > 0) {
        if (s_reply_target_app && s_reply_target_sender) {
            // Gửi trực tiếp qua BLE
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "REPLY|%s|%s|%s", s_reply_target_app, s_reply_target_sender, s_reply_text_buf);
            if (watch_bluetooth_send_command(cmd) != ESP_OK) {
                ui_haptic_feedback(UI_HAPTIC_WARNING);
                return;
            }
            ui_navigation_pop_multiple(3);
        } else {
            // Thêm vào danh sách câu mẫu động
            init_dynamic_quick_replies();
            if (s_dynamic_quick_reply_count < MAX_QUICK_REPLIES) {
                char *copy = ui_safe_strdup(s_reply_text_buf);
                if (copy) s_dynamic_quick_replies[s_dynamic_quick_reply_count++] = copy;
            }
            if (s_quick_reply_list_obj) {
                ui_quick_reply_list_build(s_quick_reply_list_obj,
                                          s_quick_reply_context_app,
                                          s_quick_reply_context_sender);
            }
            ui_navigation_pop(); // Đóng bàn phím
        }
    }
}

/* Callback dọn dẹp biến trạng thái khi đóng màn hình bàn phím ảo */
static void reply_keyboard_screen_delete_cb(lv_event_t *e) {
    (void)e;
    s_reply_text_label = NULL;
    s_reply_keyboard_container = NULL;
    s_reply_caps_button = NULL;
    s_reply_mode_button = NULL;
    s_reply_delete_button = NULL;
    s_reply_enter_button = NULL;
    s_reply_text_len = 0;
    s_reply_text_buf[0] = '\0';
    free(s_reply_target_app);
    s_reply_target_app = NULL;
    free(s_reply_target_sender);
    s_reply_target_sender = NULL;
    s_quick_reply_list_obj = NULL;
}


/* Tái cấu trúc lưới phím bấm (Rebuild Keyboard Grid)
   Dựa trên trang bàn phím hiện tại (s_reply_key_page) và trạng thái viết hoa (s_reply_key_upper)
   để tạo các nút bấm chữ/số/ký tự và các nút chức năng đặc biệt ở cột thứ tư. */
void reply_rebuild_keyboard(void) {
    if (!s_reply_keyboard_container) return;
    const lv_coord_t key_w = 52;
    const lv_coord_t key_h = 36;
    const lv_coord_t key_gap = 2;

    lv_obj_clean(s_reply_keyboard_container);
    s_reply_mode_button = NULL;
    s_reply_caps_button = NULL;
    s_reply_delete_button = NULL;
    s_reply_enter_button = NULL;
    lv_obj_set_style_pad_all(s_reply_keyboard_container, 0, 0);

    static const char *num_keys[12] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "@", "0", "."};
    static const char *alpha_pages[3][12] = {
        {"a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l"},
        {"m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x"},
        {"y", "z", " ", ",", "!", "?", "-", "_", "(", ")", NULL, NULL},
    };
    const char *const *keys = (s_reply_key_page == 0) ? num_keys : alpha_pages[s_reply_key_page - 1];

    for (int row_idx = 0; row_idx < 4; row_idx++) {
        for (int col_idx = 0; col_idx < 3; col_idx++) {
            int key_idx = row_idx * 3 + col_idx;
            const char *key_text = keys[key_idx];
            if (!key_text) continue;

            lv_obj_t *b = reply_keyboard_make_button(s_reply_keyboard_container, key_w, key_h, COLOR_PRIMARY, key_text);
            if (!b) continue;
            lv_obj_set_pos(b, col_idx * (key_w + key_gap), row_idx * (key_h + key_gap));
            lv_obj_t *l = lv_obj_get_child(b, 0);
            if (!l) continue;

            char key_buf[2] = {0};
            if (s_reply_key_page > 0) {
                key_buf[0] = s_reply_key_upper ? (char)toupper((unsigned char)key_text[0]) : key_text[0];
                lv_label_set_text(l, key_buf);
            }
            lv_obj_add_event_cb(b, reply_keyboard_key_cb, LV_EVENT_CLICKED, NULL);
        }

        lv_color_t special_color = COLOR_CYAN;
        if (row_idx == 1) special_color = COLOR_PURPLE;
        else if (row_idx == 2) special_color = COLOR_RED;
        else if (row_idx == 3) special_color = COLOR_GREEN;

        lv_obj_t *special = reply_keyboard_make_button(s_reply_keyboard_container, key_w, key_h, special_color, "");
        if (!special) continue;
        lv_obj_set_pos(special, 3 * (key_w + key_gap), row_idx * (key_h + key_gap));
        lv_obj_t *sl = lv_obj_get_child(special, 0);
        if (sl) lv_obj_set_style_text_font(sl, UI_FONT_14, 0);

        if (row_idx == 0) {
            s_reply_mode_button = special;
            lv_obj_add_event_cb(s_reply_mode_button, reply_keyboard_mode_cb, LV_EVENT_CLICKED, NULL);
        } else if (row_idx == 1) {
            s_reply_caps_button = special;
            lv_obj_add_event_cb(s_reply_caps_button, reply_keyboard_caps_cb, LV_EVENT_CLICKED, NULL);
        } else if (row_idx == 2) {
            s_reply_delete_button = special;
            lv_obj_add_event_cb(s_reply_delete_button, reply_keyboard_delete_cb, LV_EVENT_CLICKED, NULL);
        } else {
            s_reply_enter_button = special;
            lv_obj_add_event_cb(s_reply_enter_button, reply_keyboard_enter_cb, LV_EVENT_CLICKED, NULL);
        }
    }

    reply_update_keyboard_control_labels();
}

/* Khởi tạo màn hình bàn phím ảo tự soạn tin nhắn phản hồi
   Thiết kế gồm nhãn tiêu đề, nhãn hiển thị nội dung đang gõ và lưới bàn phím 4x4.
   - app_name: Tên ứng dụng nhận phản hồi (NULL nếu thêm câu mẫu mới)
   - sender: Tên người nhận (NULL nếu thêm câu mẫu mới)
   - list_obj: Con trỏ danh sách câu mẫu cần cập nhật sau khi thêm
   Trả về: lv_obj_t* Con trỏ đối tượng màn hình bàn phím ảo được tạo */
static lv_obj_t *ui_custom_reply_keyboard_create(const char *app_name, const char *sender, lv_obj_t *list_obj) {

    lv_obj_t *scr = ui_navigation_create_screen();

    lv_obj_t *hdr = lv_label_create(scr);
    if (app_name && sender) {
        lv_label_set_text(hdr, ui_text("Type Reply", "Nhập câu trả lời"));
    } else {
        lv_label_set_text(hdr, ui_text("Add New Reply", "Thêm câu trả lời"));
    }
    lv_obj_set_style_text_font(hdr, UI_FONT_16, 0);
    lv_obj_set_style_text_color(hdr, COLOR_TEXT, 0);
    lv_obj_set_style_text_align(hdr, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 38);

    s_reply_text_label = lv_label_create(scr);
    lv_label_set_text(s_reply_text_label, "");
    lv_obj_set_style_text_font(s_reply_text_label, UI_FONT_14, 0);
    lv_obj_set_style_text_color(s_reply_text_label, COLOR_TEXT_MUTED, 0);
    lv_obj_set_width(s_reply_text_label, WATCH_LCD_H_RES - 20);
    lv_obj_set_style_text_align(s_reply_text_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_reply_text_label, LV_ALIGN_CENTER, 0, -28);

    s_reply_keyboard_container = lv_obj_create(scr);
    lv_obj_set_size(s_reply_keyboard_container, 214, 150);
    lv_obj_set_style_bg_opa(s_reply_keyboard_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_reply_keyboard_container, 0, 0);
    lv_obj_clear_flag(s_reply_keyboard_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(s_reply_keyboard_container, LV_ALIGN_BOTTOM_MID, 0, -5);

    s_reply_text_len = 0;
    s_reply_text_buf[0] = '\0';
    s_reply_key_page = 0;
    s_reply_key_upper = false;
    s_reply_target_app = app_name ? ui_safe_strdup(app_name) : NULL;
    s_reply_target_sender = sender ? ui_safe_strdup(sender) : NULL;
    s_quick_reply_list_obj = list_obj;

    lv_obj_add_event_cb(scr, reply_keyboard_screen_delete_cb, LV_EVENT_DELETE, NULL);

    reply_rebuild_keyboard();
    ui_navigation_push(scr);
    return scr;
}

/* Callback khi chọn tùy chọn "Tự soạn tin..."
   Chuyển tiếp người dùng sang giao diện bàn phím ảo. */
static void quick_reply_custom_click_cb(lv_event_t *e) {
    const quick_reply_item_data_t *data = lv_event_get_user_data(e);
    if (!data) return;

    ui_custom_reply_keyboard_create(data->app_name, data->sender, NULL);
}

/* Callback khi nhấn giữ một câu trả lời mẫu
   Hiển thị nút thùng rác (xóa câu mẫu) tương ứng trên phần tử được nhấn giữ. */
static void quick_reply_item_long_press_cb(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *trash = lv_obj_get_child(btn, 1);
    if (trash) {
        lv_obj_clear_flag(trash, LV_OBJ_FLAG_HIDDEN);
    }
}

/* Callback khi người dùng nhấn nút thùng rác xóa một câu mẫu động */
static void quick_reply_delete_item_cb(lv_event_t *e) {
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    init_dynamic_quick_replies();
    if (index >= 0 && index < s_dynamic_quick_reply_count) {
        free(s_dynamic_quick_replies[index]);
        for (int i = index; i < s_dynamic_quick_reply_count - 1; i++) {
            s_dynamic_quick_replies[i] = s_dynamic_quick_replies[i + 1];
        }
        s_dynamic_quick_replies[s_dynamic_quick_reply_count - 1] = NULL;
        s_dynamic_quick_reply_count--;
    }

    lv_obj_t *trash_btn = lv_event_get_target(e);
    lv_obj_t *card = lv_obj_get_parent(trash_btn);
    lv_obj_t *list = lv_obj_get_parent(card);
    
    ui_quick_reply_list_build(list, s_quick_reply_context_app, s_quick_reply_context_sender);
}

/* Xây dựng danh sách các câu trả lời nhanh hiển thị lên màn hình
   Bao gồm nút "Tự soạn tin..." ở dòng đầu và danh sách các câu trả lời mẫu động phía dưới. */
static void ui_quick_reply_list_build(lv_obj_t *list, const char *app_name, const char *sender) {

    if (!list || !lv_obj_is_valid(list)) return;
    lv_obj_clean(list);

    init_dynamic_quick_replies();

    // 1. Nút "Tự soạn tin..." ở đầu danh sách
    lv_obj_t *btn_custom = lv_btn_create(list);
    lv_obj_set_size(btn_custom, lv_pct(100), 40);
    lv_obj_set_style_bg_color(btn_custom, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_custom, 8, 0);
    lv_obj_set_style_shadow_width(btn_custom, 0, 0);

    lv_obj_t *lbl_custom = lv_label_create(btn_custom);
    lv_label_set_text(lbl_custom, ui_text("Custom...", "Tự soạn tin..."));
    lv_obj_set_style_text_font(lbl_custom, UI_FONT_14, 0);
    lv_obj_set_style_text_color(lbl_custom, lv_color_white(), 0);
    lv_obj_center(lbl_custom);

    quick_reply_item_data_t *custom_data = malloc(sizeof(*custom_data));
    if (custom_data) {
        custom_data->app_name = ui_safe_strdup(app_name);
        custom_data->sender = ui_safe_strdup(sender);
        custom_data->reply_text = NULL;
        if (custom_data->app_name && custom_data->sender) {
            lv_obj_add_event_cb(btn_custom, quick_reply_custom_click_cb, LV_EVENT_CLICKED, custom_data);
            lv_obj_add_event_cb(btn_custom, quick_reply_item_data_delete_cb, LV_EVENT_DELETE, custom_data);
        } else {
            free(custom_data->app_name);
            free(custom_data->sender);
            free(custom_data);
        }
    }

    // 2. Danh sách các câu mẫu động
    for (int i = 0; i < s_dynamic_quick_reply_count; i++) {
        lv_obj_t *btn = lv_btn_create(list);
        lv_obj_set_size(btn, lv_pct(100), 40);
        lv_obj_set_style_bg_color(btn, COLOR_SURFACE, 0);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, s_dynamic_quick_replies[i]);
        lv_obj_set_style_text_font(lbl, UI_FONT_14, 0);
        lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
        lv_obj_center(lbl);

        // Nút thùng rác ở góc phải card (Ban đầu bị ẩn)
        lv_obj_t *trash = lv_btn_create(btn);
        lv_obj_set_size(trash, 32, 32);
        lv_obj_align(trash, LV_ALIGN_RIGHT_MID, 5, 0);
        lv_obj_set_style_bg_color(trash, COLOR_RED, 0);
        lv_obj_set_style_radius(trash, 6, 0);
        lv_obj_set_style_shadow_width(trash, 0, 0);
        lv_obj_add_flag(trash, LV_OBJ_FLAG_HIDDEN);

        lv_obj_t *trash_lbl = lv_label_create(trash);
        lv_label_set_text(trash_lbl, ICON_CLOSE);
        lv_obj_set_style_text_font(trash_lbl, &signal_icon, 0);
        lv_obj_set_style_text_color(trash_lbl, lv_color_white(), 0);
        lv_obj_center(trash_lbl);

        quick_reply_item_data_t *data = malloc(sizeof(*data));
        if (data) {
            data->app_name = ui_safe_strdup(app_name);
            data->sender = ui_safe_strdup(sender);
            data->reply_text = ui_safe_strdup(s_dynamic_quick_replies[i]);
            if (data->app_name && data->sender && data->reply_text) {
                lv_obj_add_event_cb(btn, quick_reply_select_cb, LV_EVENT_CLICKED, data);
                lv_obj_add_event_cb(btn, quick_reply_item_long_press_cb, LV_EVENT_LONG_PRESSED, NULL);
                lv_obj_add_event_cb(btn, quick_reply_item_data_delete_cb, LV_EVENT_DELETE, data);
                lv_obj_add_event_cb(trash, quick_reply_delete_item_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
            } else {
                free(data->app_name);
                free(data->sender);
                free(data->reply_text);
                free(data);
            }
        }
    }
}

static void quick_reply_add_click_cb(lv_event_t *e) {
    lv_obj_t *list = lv_event_get_user_data(e);
    ui_custom_reply_keyboard_create(NULL, NULL, list);
}

static void quick_reply_screen_delete_cb(lv_event_t *e) {
    (void)e;
    free(s_quick_reply_context_app);
    s_quick_reply_context_app = NULL;
    free(s_quick_reply_context_sender);
    s_quick_reply_context_sender = NULL;
    s_quick_reply_list_obj = NULL;
}

/* Khởi tạo màn hình danh sách lựa chọn trả lời nhanh
   Hiển thị tiêu đề, nút thêm nhanh câu trả lời mẫu "+", và gọi danh sách các câu mẫu.
   - app_name: Tên ứng dụng nhận phản hồi
   - sender: Tên người nhận
   Trả về: lv_obj_t* Con trỏ đối tượng màn hình vừa tạo */
static lv_obj_t *ui_quick_reply_screen_create(const char *app_name, const char *sender) {
    lv_obj_t *list = apps_make_list_screen(ui_text("Quick Reply", "Trả lời nhanh"));
    lv_obj_t *scr = lv_obj_get_parent(list);
    free(s_quick_reply_context_app);
    s_quick_reply_context_app = ui_safe_strdup(app_name);
    free(s_quick_reply_context_sender);
    s_quick_reply_context_sender = ui_safe_strdup(sender);
    s_quick_reply_list_obj = list;
    lv_obj_add_event_cb(scr, quick_reply_screen_delete_cb, LV_EVENT_DELETE, NULL);

    // Nút "+" ở góc trên bên phải header để thêm mẫu mới
    lv_obj_t *btn_add = lv_btn_create(scr);
    lv_obj_set_size(btn_add, 32, 32);
    lv_obj_align(btn_add, LV_ALIGN_TOP_RIGHT, -10, 32);
    lv_obj_set_style_radius(btn_add, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn_add, COLOR_PRIMARY, 0);
    lv_obj_set_style_shadow_width(btn_add, 0, 0);
    
    lv_obj_t *lbl_add = lv_label_create(btn_add);
    lv_label_set_text(lbl_add, "+");
    lv_obj_set_style_text_font(lbl_add, UI_FONT_16, 0);
    lv_obj_set_style_text_color(lbl_add, lv_color_white(), 0);
    lv_obj_center(lbl_add);

    lv_obj_add_event_cb(btn_add, quick_reply_add_click_cb, LV_EVENT_CLICKED, list);

    ui_quick_reply_list_build(list, s_quick_reply_context_app, s_quick_reply_context_sender);

    ui_navigation_add_gesture(scr);
    return scr;
}

/* Callback khi click nút "Trả lời" (Reply) trên màn hình chi tiết thông báo */
static void notifications_reply_click_cb(lv_event_t *e) {
    const notification_card_data_t *data = lv_event_get_user_data(e);
    if (!data) return;

    ui_navigation_push(ui_quick_reply_screen_create(data->app_name, data->sender));
}

/* Khởi tạo màn hình chi tiết hiển thị tin nhắn / thông báo
   Thiết kế giao diện hiển thị tên người gửi, nội dung tin nhắn và nút "Trả lời".
   Cho phép văn bản cuộn tròn tự động (circular scroll) nếu nội dung tin nhắn dài vượt màn hình. */
void ui_notifications_show_detail(const char *app_name, const char *sender, const char *message, uint32_t seq_id) {
    notification_card_data_t *data = notifications_card_data_create(app_name, sender, message, seq_id);
    if (!data) return;

    const char *title_text = data->sender ? data->sender : ui_text("Message", "Tin nhắn");
    const char *body_text = data->message ? data->message : "";

    lv_obj_t *scr = ui_navigation_create_screen();
    ui_navigation_set_screen_id(scr, UI_SCREEN_INVALID);
    lv_obj_add_event_cb(scr, notifications_card_data_delete_cb, LV_EVENT_DELETE, data);

    lv_obj_t *hdr = lv_label_create(scr);
    lv_label_set_text(hdr, title_text);
    lv_obj_set_style_text_font(hdr, UI_FONT_16, 0);
    lv_obj_set_style_text_color(hdr, COLOR_TEXT_MUTED, 0);
    lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 38);

    lv_obj_t *body = lv_label_create(scr);
    lv_label_set_text(body, body_text);
    lv_obj_set_style_text_font(body, UI_FONT_14, 0);
    lv_obj_set_style_text_color(body, COLOR_TEXT, 0);
    lv_obj_set_width(body, WATCH_LCD_H_RES - 30);
    lv_obj_set_height(body, 120); // Giới hạn chiều cao nhường chỗ cho nút bấm Trả lời
    lv_label_set_long_mode(body, LV_LABEL_LONG_SCROLL_CIRCULAR); // Cuộn chữ tròn tự động nếu tin nhắn dài
    lv_obj_align(body, LV_ALIGN_TOP_MID, 0, 68);

    // Thêm nút Trả lời cực kì sang trọng ở dưới cùng
    lv_obj_t *btn_reply = lv_btn_create(scr);
    lv_obj_set_size(btn_reply, 130, 38);
    lv_obj_set_style_radius(btn_reply, 10, 0);
    lv_obj_set_style_bg_color(btn_reply, COLOR_PRIMARY, 0);
    lv_obj_set_style_shadow_width(btn_reply, 0, 0);
    lv_obj_align(btn_reply, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_obj_add_event_cb(btn_reply, notifications_reply_click_cb, LV_EVENT_CLICKED, (void *)data);

    lv_obj_t *lbl_reply = lv_label_create(btn_reply);
    lv_label_set_text(lbl_reply, ui_text("Reply", "Trả lời"));
    lv_obj_set_style_text_font(lbl_reply, UI_FONT_14, 0);
    lv_obj_set_style_text_color(lbl_reply, lv_color_white(), 0);
    lv_obj_center(lbl_reply);

    ui_navigation_add_gesture(scr);
    ui_navigation_push(scr);
}

/* Khởi tạo màn hình chi tiết hiển thị tin nhắn / thông báo
   Thiết kế giao diện hiển thị tên người gửi, nội dung tin nhắn và nút "Trả lời".
   Cho phép văn bản cuộn tròn tự động (circular scroll) nếu nội dung tin nhắn dài vượt màn hình. */
static void notifications_show_message_cb(lv_event_t *e) {
    const notification_card_data_t *data = lv_event_get_user_data(e);
    if (!data) return;
    ui_notifications_show_detail(data->app_name, data->sender, data->message, data->seq_id);
}


/* Callback hiển thị nút xóa (thùng rác) khi người dùng nhấn giữ thẻ thông báo */
static void notifications_show_delete_cb(lv_event_t *e) {
    notification_card_data_t *data = lv_event_get_user_data(e);
    if (data && data->delete_btn && lv_obj_is_valid(data->delete_btn)) {
        lv_obj_clear_flag(data->delete_btn, LV_OBJ_FLAG_HIDDEN);
    }
}

/* Callback xử lý sự kiện xóa một thông báo cụ thể khỏi danh sách hiển thị */
static void notifications_delete_one_cb(lv_event_t *e) {
    notification_card_data_t *data = lv_event_get_user_data(e);
    if (!data) return;

    watch_bluetooth_delete_by_seq_id(data->seq_id);
    s_last_notif_count = -1;
    s_last_notif_overflow_count = UINT32_MAX;
    notifications_refresh_list_cb(NULL);
}

/* Khởi tạo đối tượng thẻ thông báo (Notification Card)
   Thiết kế giao diện thẻ bao gồm hình đại diện (avatar) lấy chữ cái đầu của người gửi,
   nhãn tên người gửi, nhãn xem trước tin nhắn, và nút xóa ẩn góc phải.
   - par: Container cha chứa thẻ thông báo
   - c: Màu sắc chủ đạo đại diện của thẻ
   - app_name: Tên ứng dụng nguồn gửi
   - name: Tên người gửi
   - msg: Nội dung tin nhắn
   - seq_id: ID định vị gói tin thông báo
   Trả về: lv_obj_t* Con trỏ đối tượng thẻ thông báo được tạo */
static lv_obj_t *notifications_make_card(lv_obj_t *par, lv_color_t c, const char *app_name, const char *name,
                           const char *msg, uint32_t seq_id) {

    lv_obj_t *card = lv_obj_create(par);
    lv_obj_set_size(card, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(card, COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, COLOR_SURFACE_LT, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 4, 0);

    /* Row: avatar + column(title, body) */
    lv_obj_t *row = lv_obj_create(card);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(row, 6, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *avatar = lv_obj_create(row);
    lv_obj_set_size(avatar, 40, 40);
    lv_obj_set_style_radius(avatar, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(avatar, c, 0);
    lv_obj_set_style_border_width(avatar, 0, 0);
    lv_obj_clear_flag(avatar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(avatar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *av_lbl = lv_label_create(avatar);
    char avc[2] = {'?', 0};
    if (name && name[0]) avc[0] = toupper((unsigned char)name[0]);
    lv_label_set_text(av_lbl, avc);
    lv_obj_set_style_text_color(av_lbl, lv_color_white(), 0);
    lv_obj_center(av_lbl);

    lv_obj_t *col = lv_obj_create(row);
    lv_obj_set_size(col, lv_pct(80), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(col, 0, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *title = lv_label_create(col);
    lv_label_set_text(title, name);
    lv_obj_set_style_text_font(title, UI_FONT_14, 0);
    lv_obj_set_style_text_color(title, c, 0);

    lv_obj_t *body = lv_label_create(col);
    lv_label_set_text(body, msg);
    lv_obj_set_style_text_font(body, UI_FONT_14, 0);
    lv_obj_set_style_text_color(body, COLOR_TEXT_MUTED, 0);
    lv_obj_set_width(body, lv_pct(100));
    lv_label_set_long_mode(body, LV_LABEL_LONG_DOT);

    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    notification_card_data_t *data = notifications_card_data_create(app_name, name, msg, seq_id);
    lv_obj_add_event_cb(card, notifications_show_message_cb, LV_EVENT_CLICKED, data);
    lv_obj_add_event_cb(card, notifications_show_delete_cb, LV_EVENT_LONG_PRESSED, data);
    lv_obj_add_event_cb(card, notifications_card_data_delete_cb, LV_EVENT_DELETE, data);

    lv_obj_t *delete_btn = lv_btn_create(card);
    lv_obj_set_size(delete_btn, 36, 30);
    lv_obj_set_style_radius(delete_btn, 8, 0);
    lv_obj_set_style_bg_color(delete_btn, COLOR_RED, 0);
    lv_obj_set_style_shadow_width(delete_btn, 0, 0);
    lv_obj_align(delete_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_add_flag(delete_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(delete_btn, notifications_delete_one_cb, LV_EVENT_CLICKED, data);

    lv_obj_t *delete_lbl = lv_label_create(delete_btn);
    lv_label_set_text(delete_lbl, ICON_SIGNAL_TRASH);
    lv_obj_set_style_text_font(delete_lbl, &signal_icon, 0);
    lv_obj_center(delete_lbl);
    if (data) data->delete_btn = delete_btn;
    return card;
}

/* Lấy màu theo tên app - dùng cho cả notifications_create */
/* Timer cập nhật danh sách thông báo */
static void notifications_make_limit_warning(lv_obj_t *par, int count, uint32_t overflow_count) {
    lv_obj_t *card = lv_obj_create(par);
    lv_obj_set_size(card, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(card, COLOR_SURFACE_LT, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, COLOR_ORANGE, 0);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_set_style_pad_row(card, 8, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);

    char msg[128];
    if (overflow_count > 0) {
        snprintf(msg, sizeof(msg),
                 ui_text("Full %d/%d messages.\n%lu oldest messages were deleted.",
                         "Đã đầy %d/%d tin.\n%lu tin cũ nhất đã bị xóa."),
                 count, WATCH_BLUETOOTH_NOTIF_MAX, (unsigned long)overflow_count);
    } else {
        snprintf(msg, sizeof(msg),
                 ui_text("Full %d/%d messages.\nLong press a message to delete it.",
                         "Đã đầy %d/%d tin.\nNhấn giữ tin nhắn để xóa từng tin."),
                 count, WATCH_BLUETOOTH_NOTIF_MAX);
    }

    lv_obj_t *label = lv_label_create(card);
    lv_label_set_text(label, msg);
    lv_obj_set_width(label, lv_pct(100));
    lv_obj_set_style_text_font(label, UI_FONT_12, 0);
    lv_obj_set_style_text_color(label, COLOR_ORANGE, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);

}

/* Callback dọn dẹp biến trạng thái và giải phóng timer khi đóng hộp thư thông báo */
static void notifications_screen_delete_cb(lv_event_t *e) {
    (void)e;
    if (s_notif_refresh_timer) {
        lv_timer_del(s_notif_refresh_timer);
        s_notif_refresh_timer = NULL;
    }
    s_notif_list = NULL;
    s_last_notif_count = -1;
    s_last_notif_overflow_count = UINT32_MAX;
}

/* Định thời 2 giây để quét và cập nhật danh sách thông báo mới nhận qua BLE
   Kiểm tra xem số lượng thông báo có thay đổi hay không. Nếu có:
   1. Làm sạch container hiển thị cũ.
   2. Đọc tuần tự các thông báo trong bộ đệm BLE.
   3. Tạo thẻ thông báo card tương ứng và hiển thị cảnh báo nếu hộp thư đầy. */
static void notifications_refresh_list_cb(lv_timer_t *t) {
    (void)t;
    int count = watch_bluetooth_get_count();
    uint32_t overflow_count = watch_bluetooth_get_overflow_count();
    if (count == s_last_notif_count && overflow_count == s_last_notif_overflow_count) return;  /* Không có gì mới */
    if (!s_notif_list) return;

    /* Xóa tất cả children cũ */
    lv_obj_clean(s_notif_list);
    s_last_notif_count = count;
    s_last_notif_overflow_count = overflow_count;

    if (count == 0) {
        /* Hiển thị trạng thái trống */
        lv_obj_t *empty = lv_label_create(s_notif_list);
        lv_label_set_text(empty, ui_tr(UI_TXT_NO_NOTIFICATIONS));
        lv_obj_set_style_text_font(empty, UI_FONT_14, 0);
        lv_obj_set_style_text_color(empty, COLOR_TEXT_MUTED, 0);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(empty, lv_pct(100));
        return;
    }

    /* Tạo card cho mỗi thông báo */
    if (count >= WATCH_BLUETOOTH_NOTIF_MAX) {
        notifications_make_limit_warning(s_notif_list, count, overflow_count);
    }

    for (int i = 0; i < count; i++) {
        watch_bluetooth_notification_t notif;
        if (!watch_bluetooth_get(i, &notif)) continue;

        lv_color_t c = ui_app_color(notif.app_name);
        notifications_make_card(s_notif_list, c, notif.app_name, notif.title, notif.content, notif.seq_id);

        /* Đánh dấu đã đọc khi user mở danh sách */
        watch_bluetooth_mark_read(i);
    }
}

/* Khởi tạo màn hình Hộp thư thông báo (Notification Center)
   Trả về: lv_obj_t* Con trỏ đối tượng màn hình thông báo được tạo */
lv_obj_t *ui_notifications_screen_create(void) {
    s_notif_list = apps_make_list_screen(ui_tr(UI_TXT_NOTIFICATIONS));
    lv_obj_t *scr = lv_obj_get_parent(s_notif_list);
    ui_navigation_set_screen_id(scr, UI_SCREEN_NOTIFICATIONS);
    lv_obj_add_event_cb(scr, notifications_screen_delete_cb, LV_EVENT_DELETE, NULL);

    /* Populate lần đầu */
    s_last_notif_count = -1;
    s_last_notif_overflow_count = UINT32_MAX;
    notifications_refresh_list_cb(NULL);

    /* Timer cập nhật mỗi 2 giây */
    s_notif_refresh_timer = lv_timer_create(notifications_refresh_list_cb, 2000, NULL);

    return scr;
}

/* ============ GPS ============ */
static lv_obj_t *s_gps_wait_view = NULL;
static lv_obj_t *s_gps_wait_title = NULL;
static lv_obj_t *s_gps_wait_desc = NULL;
static lv_obj_t *s_gps_wait_btn = NULL;
static lv_obj_t *s_gps_nav_view = NULL;
static lv_obj_t *s_gps_primary_bar = NULL;
static lv_obj_t *s_gps_secondary_bar = NULL;
static lv_obj_t *s_gps_primary_arrow = NULL;
static lv_obj_t *s_gps_primary_distance = NULL;
static lv_obj_t *s_gps_primary_road = NULL;
static lv_obj_t *s_gps_secondary_arrow = NULL;
static lv_obj_t *s_gps_secondary_text = NULL;
static lv_obj_t *s_gps_total_distance = NULL;
static lv_timer_t *s_gps_timer = NULL;
static lv_timer_t *s_gps_start_sync_timer = NULL;

/* Chỉ bật chế độ 2 bước khi rất gần ngã rẽ (đoạn ngắn/hẹp). */
// static const int GPS_SHOW_SECOND_STEP_UNDER_M = 80;

/* Lấy biểu tượng mũi tên rẽ tương ứng cho hướng đi GPS */
static const char *gps_nav_turn_symbol(watch_bluetooth_nav_turn_t turn) {
  switch (turn) {
    case WATCH_BLUETOOTH_NAV_TURN_LEFT: return ICON_ARROW_LEFT;
    case WATCH_BLUETOOTH_NAV_TURN_SLIGHT_LEFT: return ICON_ARROW_SL_LEFT;
    case WATCH_BLUETOOTH_NAV_TURN_KEEP_LEFT: return ICON_ARROW_LEFT;
    case WATCH_BLUETOOTH_NAV_TURN_RIGHT: return ICON_ARROW_RIGHT;
    case WATCH_BLUETOOTH_NAV_TURN_SLIGHT_RIGHT: return ICON_ARROW_SL_RIGHT;
    case WATCH_BLUETOOTH_NAV_TURN_KEEP_RIGHT: return ICON_ARROW_RIGHT;
    case WATCH_BLUETOOTH_NAV_TURN_UTURN: return ICON_ARROW_DOWN;
    case WATCH_BLUETOOTH_NAV_TURN_ROUNDABOUT: return ICON_ARROW_SPIN;
    case WATCH_BLUETOOTH_NAV_TURN_ARRIVE: return ICON_NAV_ARRIVE;
    case WATCH_BLUETOOTH_NAV_TURN_STRAIGHT:
    case WATCH_BLUETOOTH_NAV_TURN_UNKNOWN:
    default: return ICON_ARROW_UP;
  }
}

/* Lấy font chữ tương ứng cho ký tự rẽ */
static const lv_font_t *gps_nav_turn_font(watch_bluetooth_nav_turn_t turn) {
  (void)turn;
  return &arrow_2;
}

/* Lấy chuỗi văn bản hướng dẫn chỉ đường bằng tiếng Anh / Việt */
static const char *gps_nav_turn_text(watch_bluetooth_nav_turn_t turn) {
  switch (turn) {
    case WATCH_BLUETOOTH_NAV_TURN_STRAIGHT: return ui_text("Go straight", "Đi thẳng");
    case WATCH_BLUETOOTH_NAV_TURN_LEFT: return ui_text("Turn left", "Rẽ trái");
    case WATCH_BLUETOOTH_NAV_TURN_SLIGHT_LEFT: return ui_text("Slight left", "Chếch trái");
    case WATCH_BLUETOOTH_NAV_TURN_KEEP_LEFT: return ui_text("Keep left", "Bám trái");
    case WATCH_BLUETOOTH_NAV_TURN_RIGHT: return ui_text("Turn right", "Rẽ phải");
    case WATCH_BLUETOOTH_NAV_TURN_SLIGHT_RIGHT: return ui_text("Slight right", "Chếch phải");
    case WATCH_BLUETOOTH_NAV_TURN_KEEP_RIGHT: return ui_text("Keep right", "Bám phải");
    case WATCH_BLUETOOTH_NAV_TURN_UTURN: return ui_text("U-turn", "Quay đầu");
    case WATCH_BLUETOOTH_NAV_TURN_ARRIVE: return ui_text("You have arrived", "Đã đến nơi");
    case WATCH_BLUETOOTH_NAV_TURN_ROUNDABOUT: return ui_text("Enter roundabout", "Vào vòng xuyến");
    default: return ui_text("Continue", "Tiếp tục");
  }
}

/* Lấy màu sắc biểu thị cho từng trạng thái rẽ của chỉ đường GPS */
static lv_color_t gps_nav_turn_color(watch_bluetooth_nav_turn_t turn) {
  switch (turn) {
    case WATCH_BLUETOOTH_NAV_TURN_STRAIGHT:
      return COLOR_PRIMARY; /* Xanh dương */
    case WATCH_BLUETOOTH_NAV_TURN_LEFT:
    case WATCH_BLUETOOTH_NAV_TURN_SLIGHT_LEFT:
    case WATCH_BLUETOOTH_NAV_TURN_KEEP_LEFT:
      return COLOR_GREEN; /* Xanh lá */
    case WATCH_BLUETOOTH_NAV_TURN_RIGHT:
    case WATCH_BLUETOOTH_NAV_TURN_SLIGHT_RIGHT:
    case WATCH_BLUETOOTH_NAV_TURN_KEEP_RIGHT:
      return COLOR_PURPLE; /* Tím */
    case WATCH_BLUETOOTH_NAV_TURN_UTURN:
      return COLOR_ORANGE; /* Vàng */
    case WATCH_BLUETOOTH_NAV_TURN_ROUNDABOUT:
      return COLOR_RED; /* Đỏ */
    case WATCH_BLUETOOTH_NAV_TURN_ARRIVE:
      return COLOR_GREEN;
    default:
      return COLOR_TEXT;
  }
}

/*
static void gps_nav_format_distance(char *out, size_t out_size, int meters) {
  if (meters < 0) meters = 0;
  if (meters >= 1000) {
    float km = (float)meters / 1000.0f;
    snprintf(out, out_size, "%.1fkm", km);
  } else {
    snprintf(out, out_size, "%dm", meters);
  }
}
*/

/* Bật/tắt chế độ chờ kết nối GPS hoặc màn hình chỉ đường */
static void gps_nav_set_waiting_mode(bool waiting) {
  if (!s_gps_wait_view || !s_gps_nav_view) return;
  if (waiting) {
    lv_obj_clear_flag(s_gps_wait_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_gps_nav_view, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(s_gps_wait_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_gps_nav_view, LV_OBJ_FLAG_HIDDEN);
  }
}

static void gps_nav_update_wait_content(bool connected) {
  if (!s_gps_wait_title || !s_gps_wait_desc || !s_gps_wait_btn) return;

  if (connected) {
    ui_label_set_text_if_changed(s_gps_wait_title, ui_text("Waiting for navigation", "Chờ dẫn đường"));
    ui_label_set_text_if_changed(
        s_gps_wait_desc,
        ui_text("Open Google Maps navigation on your phone.",
                "Mở dẫn đường Google Maps trên điện thoại."));
    lv_obj_add_flag(s_gps_wait_btn, LV_OBJ_FLAG_HIDDEN);
  } else {
    ui_label_set_text_if_changed(s_gps_wait_title, ui_tr(UI_TXT_NEED_BLE));
    ui_label_set_text_if_changed(s_gps_wait_desc, ui_tr(UI_TXT_BLE_GPS_DESC));
    lv_obj_clear_flag(s_gps_wait_btn, LV_OBJ_FLAG_HIDDEN);
  }
}

/* Gửi yêu cầu bắt đầu đồng bộ GPS (GPS_START) sang điện thoại qua BLE */
static void gps_nav_start_sync_cmd_cb(lv_timer_t *t) {
  if (t == s_gps_start_sync_timer) {
    s_gps_start_sync_timer = NULL;
  }
  watch_bluetooth_send_command("GPS_START");
  lv_timer_del(t);
}

/* Callback mở màn hình kết nối BLE khi chưa liên kết ứng dụng chỉ đường */
static void gps_open_ble_cb(lv_event_t *e) {
  (void)e;
  ui_navigation_push(ui_connect_screen_create());
}

/* Làm tươi giao diện chỉ đường GPS định kỳ mỗi 500ms
   Đọc dữ liệu chỉ đường (hướng rẽ, khoảng cách, tên đường) nhận được từ BLE
   để cập nhật trực quan lên màn hình. */
static void gps_nav_refresh_cb(lv_timer_t *t) {
  (void)t;
  watch_bluetooth_nav_data_t nav;
  bool has_nav = watch_bluetooth_get_navigation_data(&nav);
  bool connected = watch_bluetooth_is_connected();

  if (!has_nav || !nav.active) {
    gps_nav_update_wait_content(connected);
    gps_nav_set_waiting_mode(true);
    return;
  }

  gps_nav_set_waiting_mode(false);

  ui_label_set_text_if_changed(s_gps_primary_arrow, gps_nav_turn_symbol(nav.current_turn));
  lv_obj_set_style_text_font(s_gps_primary_arrow, gps_nav_turn_font(nav.current_turn), 0);
  lv_obj_set_style_text_color(s_gps_primary_arrow, gps_nav_turn_color(nav.current_turn), 0);
  
  /* Hiện ETA/Distance đến ngã rẽ (ví dụ: "200m") */
  ui_label_set_text_if_changed(s_gps_primary_distance, nav.eta_to_turn[0] ? nav.eta_to_turn : "--");
  
  char road_info[128];
  snprintf(road_info, sizeof(road_info), "%s%s%s",
           gps_nav_turn_text(nav.current_turn),
           nav.primary_road[0] ? "\n" : "",
           nav.primary_road[0] ? nav.primary_road : "");
  ui_label_set_text_if_changed(s_gps_primary_road, road_info);

  if (nav.next_turn != WATCH_BLUETOOTH_NAV_TURN_UNKNOWN || nav.next_road[0]) {
    ui_label_set_text_if_changed(s_gps_secondary_arrow, gps_nav_turn_symbol(nav.next_turn));
    lv_obj_set_style_text_font(s_gps_secondary_arrow, gps_nav_turn_font(nav.next_turn), 0);
    lv_obj_set_style_text_color(s_gps_secondary_arrow, gps_nav_turn_color(nav.next_turn), 0);

    char next_info[96];
    snprintf(next_info, sizeof(next_info), "%s%s%s",
             gps_nav_turn_text(nav.next_turn),
             nav.next_road[0] ? ": " : "",
             nav.next_road);
    ui_label_set_text_if_changed(s_gps_secondary_text, next_info);
    lv_obj_clear_flag(s_gps_secondary_bar, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(s_gps_secondary_bar, LV_OBJ_FLAG_HIDDEN);
  }

  if (nav.total_remaining[0]) {
    char remaining[72];
    snprintf(remaining, sizeof(remaining), "%s: %s",
             ui_tr(UI_TXT_REMAINING), nav.total_remaining);
    ui_label_set_text_if_changed(s_gps_total_distance, remaining);
    lv_obj_clear_flag(s_gps_total_distance, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(s_gps_total_distance, LV_OBJ_FLAG_HIDDEN);
  }
}

/* Callback dừng đồng bộ GPS (GPS_STOP) và dọn dẹp tài nguyên khi đóng màn hình */
static void gps_nav_screen_delete_cb(lv_event_t *e) {
  (void)e;
  watch_bluetooth_send_command("GPS_STOP");
  if (s_gps_timer) {
    lv_timer_del(s_gps_timer);
    s_gps_timer = NULL;
  }
  if (s_gps_start_sync_timer) {
    lv_timer_del(s_gps_start_sync_timer);
    s_gps_start_sync_timer = NULL;
  }
  s_gps_wait_view = NULL;
  s_gps_wait_title = NULL;
  s_gps_wait_desc = NULL;
  s_gps_wait_btn = NULL;
  s_gps_nav_view = NULL;
  s_gps_primary_bar = NULL;
  s_gps_secondary_bar = NULL;
  s_gps_primary_arrow = NULL;
  s_gps_primary_distance = NULL;
  s_gps_primary_road = NULL;
  s_gps_secondary_arrow = NULL;
  s_gps_secondary_text = NULL;
  s_gps_total_distance = NULL;
}

/* Khởi tạo màn hình bản đồ chỉ dẫn GPS đồng bộ từ điện thoại
   Bố trí 2 luồng giao diện chính:
   1. Chờ kết nối (s_gps_wait_view): Hiển thị quy trình Phone -> BLE -> Watch và nút bấm kết nối BLE.
   2. Đang dẫn đường (s_gps_nav_view): Hiển thị mũi tên hướng đi lớn, tên đường hiện tại, khoảng cách rẽ.
   Trả về: lv_obj_t* Con trỏ màn hình chỉ đường GPS */
lv_obj_t *ui_gps_screen_create(void) {
  lv_obj_t *scr = ui_navigation_create_screen_with_id(UI_SCREEN_GPS);
  lv_obj_add_event_cb(scr, gps_nav_screen_delete_cb, LV_EVENT_DELETE, NULL);
  /* Delay nhẹ để app kịp subscribe notify sau khi vừa connect BLE. */
  s_gps_start_sync_timer = lv_timer_create(gps_nav_start_sync_cmd_cb, 250, NULL);

  s_gps_wait_view = lv_obj_create(scr);
  lv_obj_set_size(s_gps_wait_view, WATCH_LCD_H_RES, 230);
  lv_obj_align(s_gps_wait_view, LV_ALIGN_CENTER, 0, 10);
  lv_obj_set_style_bg_opa(s_gps_wait_view, 0, 0);
  lv_obj_set_style_border_width(s_gps_wait_view, 0, 0);
  lv_obj_set_flex_flow(s_gps_wait_view, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(s_gps_wait_view, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(s_gps_wait_view, 0, 0);
  lv_obj_set_style_pad_row(s_gps_wait_view, 10, 0);
  lv_obj_clear_flag(s_gps_wait_view, LV_OBJ_FLAG_SCROLLABLE);

  /* 1. Title */
  s_gps_wait_title = lv_label_create(s_gps_wait_view);
  lv_label_set_text(s_gps_wait_title, ui_tr(UI_TXT_NEED_BLE));
  lv_obj_set_style_text_font(s_gps_wait_title, UI_FONT_16, 0);
  lv_obj_set_style_text_color(s_gps_wait_title, lv_color_white(), 0);


  /* 3. Description */
  s_gps_wait_desc = lv_label_create(s_gps_wait_view);
  lv_label_set_text(s_gps_wait_desc, ui_tr(UI_TXT_BLE_GPS_DESC));
  lv_obj_set_width(s_gps_wait_desc, WATCH_LCD_H_RES - 40);
  lv_obj_set_style_text_align(s_gps_wait_desc, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(s_gps_wait_desc, UI_FONT_12, 0);
  lv_obj_set_style_text_color(s_gps_wait_desc, COLOR_TEXT_MUTED, 0);

  /* 4. Connect Button */
  s_gps_wait_btn = lv_btn_create(s_gps_wait_view);
  lv_obj_set_size(s_gps_wait_btn, 140, 38);
  lv_obj_set_style_bg_color(s_gps_wait_btn, lv_color_hex(0x5D3A1A), 0); /* Brownish color from image */
  lv_obj_set_style_radius(s_gps_wait_btn, 10, 0);
  lv_obj_set_style_shadow_width(s_gps_wait_btn, 0, 0);
  
  lv_obj_t *btn_lbl = lv_label_create(s_gps_wait_btn);
  lv_label_set_text(btn_lbl, ui_text("Connect BLE", "Kết nối BLE"));
  lv_obj_set_style_text_font(btn_lbl, UI_FONT_14, 0);
  lv_obj_set_style_text_color(btn_lbl, lv_color_white(), 0);
  lv_obj_center(btn_lbl);
  lv_obj_add_event_cb(s_gps_wait_btn, gps_open_ble_cb, LV_EVENT_CLICKED, NULL);

  s_gps_nav_view = lv_obj_create(scr);
  lv_obj_set_size(s_gps_nav_view, WATCH_LCD_H_RES, WATCH_LCD_V_RES - 36);
  lv_obj_align(s_gps_nav_view, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_opa(s_gps_nav_view, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(s_gps_nav_view, 0, 0);
  lv_obj_set_style_pad_all(s_gps_nav_view, 0, 0);
  lv_obj_set_style_pad_row(s_gps_nav_view, 4, 0);
  lv_obj_set_flex_flow(s_gps_nav_view, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(s_gps_nav_view, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(s_gps_nav_view, LV_OBJ_FLAG_SCROLLABLE);

  s_gps_primary_bar = lv_obj_create(s_gps_nav_view);
  lv_obj_set_size(s_gps_primary_bar, lv_pct(100), 190);
  lv_obj_set_style_bg_opa(s_gps_primary_bar, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(s_gps_primary_bar, 0, 0);
  lv_obj_set_style_border_width(s_gps_primary_bar, 0, 0);
  lv_obj_set_style_pad_all(s_gps_primary_bar, 0, 0);
  lv_obj_clear_flag(s_gps_primary_bar, LV_OBJ_FLAG_SCROLLABLE);

  s_gps_primary_arrow = lv_label_create(s_gps_primary_bar);
  lv_label_set_text(s_gps_primary_arrow, ICON_ARROW_UP);
  lv_obj_set_style_text_font(s_gps_primary_arrow, &arrow_2, 0);
  lv_obj_set_style_text_color(s_gps_primary_arrow, COLOR_GREEN, 0);
  lv_obj_align(s_gps_primary_arrow, LV_ALIGN_TOP_MID, 0, 15);

  s_gps_primary_road = lv_label_create(s_gps_primary_bar);
  lv_label_set_text(s_gps_primary_road, ui_tr(UI_TXT_GPS_NAVIGATING));
  lv_obj_set_width(s_gps_primary_road, WATCH_LCD_H_RES - 24);
  lv_label_set_long_mode(s_gps_primary_road, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(s_gps_primary_road, UI_FONT_24, 0);
  lv_obj_set_style_text_color(s_gps_primary_road, COLOR_TEXT, 0);
  lv_obj_set_style_text_align(s_gps_primary_road, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(s_gps_primary_road, LV_ALIGN_TOP_MID, 0, 75);

  s_gps_primary_distance = lv_label_create(s_gps_primary_bar);
  lv_label_set_text(s_gps_primary_distance, "0m");
  lv_obj_set_style_text_font(s_gps_primary_distance, UI_FONT_20, 0);
  lv_obj_set_style_text_color(s_gps_primary_distance, COLOR_TEXT_MUTED, 0);
  lv_obj_align(s_gps_primary_distance, LV_ALIGN_BOTTOM_MID, 0, -10);

  s_gps_secondary_bar = lv_obj_create(s_gps_nav_view);
  lv_obj_set_size(s_gps_secondary_bar, lv_pct(100), 48);
  lv_obj_set_style_bg_color(s_gps_secondary_bar, COLOR_SURFACE, 0);
  lv_obj_set_style_radius(s_gps_secondary_bar, 12, 0);
  lv_obj_set_style_border_side(s_gps_secondary_bar, LV_BORDER_SIDE_TOP, 0);
  lv_obj_set_style_border_width(s_gps_secondary_bar, 1, 0);
  lv_obj_set_style_border_color(s_gps_secondary_bar, COLOR_SURFACE_LT, 0);
  lv_obj_set_style_pad_all(s_gps_secondary_bar, 8, 0);
  lv_obj_clear_flag(s_gps_secondary_bar, LV_OBJ_FLAG_SCROLLABLE);

  s_gps_secondary_arrow = lv_label_create(s_gps_secondary_bar);
  lv_label_set_text(s_gps_secondary_arrow, "<");
  lv_obj_set_style_text_font(s_gps_secondary_arrow, UI_FONT_20, 0);
  lv_obj_align(s_gps_secondary_arrow, LV_ALIGN_LEFT_MID, 0, 0);

  s_gps_secondary_text = lv_label_create(s_gps_secondary_bar);
  lv_label_set_text(s_gps_secondary_text, ui_tr(UI_TXT_NEXT_STEP));
  lv_obj_set_width(s_gps_secondary_text, WATCH_LCD_H_RES - 60);
  lv_label_set_long_mode(s_gps_secondary_text, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_font(s_gps_secondary_text, UI_FONT_14, 0);
  lv_obj_align(s_gps_secondary_text, LV_ALIGN_LEFT_MID, 30, 0);
  lv_obj_add_flag(s_gps_secondary_bar, LV_OBJ_FLAG_HIDDEN);

  s_gps_total_distance = lv_label_create(s_gps_nav_view);
  lv_label_set_text(s_gps_total_distance, ui_tr(UI_TXT_REMAINING));
  lv_obj_set_style_text_font(s_gps_total_distance, UI_FONT_20, 0);
  lv_obj_set_style_text_color(s_gps_total_distance, COLOR_PRIMARY, 0);
  lv_obj_align(s_gps_total_distance, LV_ALIGN_BOTTOM_MID, 0, -5);
  lv_obj_add_flag(s_gps_total_distance, LV_OBJ_FLAG_HIDDEN);

  s_gps_timer = lv_timer_create(gps_nav_refresh_cb, 500, NULL);
  gps_nav_refresh_cb(NULL);

  ui_navigation_add_gesture(scr);
  return scr;
}

/* Callback mở màn hình cấu hình Rung (Vibration) */
static void settings_open_vibration_cb(lv_event_t *e) {
  (void)e;
  ui_navigation_push(ui_vibration_settings_screen_create());
}

/* Callback mở màn hình cấu hình Cá nhân hóa (Personality) */
static void settings_open_personality_cb(lv_event_t *e) {
  (void)e;
  ui_navigation_push(ui_personality_settings_screen_create());
}

/* Callback mở màn hình cấu hình Ngôn ngữ (Language) */
static void settings_open_language_cb(lv_event_t *e) {
  (void)e;
  ui_navigation_push(ui_language_settings_screen_create());
}

/* Callback mở màn hình cấu hình Thời gian (Set Time) */
static void settings_open_set_time_cb(lv_event_t *e) {
  (void)e;
  ui_navigation_push(ui_set_time_screen_create());
}

/* Callback mở màn hình cấu hình Hệ thống (System) */
static void settings_open_system_cb(lv_event_t *e) {
  (void)e;
  ui_navigation_push(ui_system_settings_screen_create());
}

/* Callback đóng màn hình hiện tại (Pop) */
static void settings_close_action_screen_cb(lv_event_t *e) {
  (void)e;
  ui_navigation_pop();
}

/* Callback thực hiện khởi động lại thiết bị (Reboot) */
static void settings_restart_confirm_cb(lv_event_t *e) {
  (void)e;
  esp_restart();
}

static void __attribute__((unused)) settings_open_restart_confirm_cb(lv_event_t *e) {
  (void)e;
  lv_obj_t *scr = ui_navigation_create_screen();

  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, ui_tr(UI_TXT_RESTART));
  lv_obj_set_style_text_font(title, UI_FONT_20, 0);
  lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
  lv_obj_align(title, LV_ALIGN_CENTER, 0, -55);

  lv_obj_t *body = lv_label_create(scr);
  lv_label_set_text(body, ui_tr(UI_TXT_POWER_OFF_CONFIRM));
  lv_obj_set_width(body, WATCH_LCD_H_RES - 40);
  lv_obj_set_style_text_align(body, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(body, COLOR_TEXT_MUTED, 0);
  lv_obj_align(body, LV_ALIGN_CENTER, 0, -18);

  lv_obj_t *cancel = lv_btn_create(scr);
  lv_obj_set_size(cancel, 86, 42);
  lv_obj_set_style_bg_color(cancel, COLOR_SURFACE_LT, 0);
  lv_obj_align(cancel, LV_ALIGN_CENTER, -48, 45);
  lv_obj_add_event_cb(cancel, settings_close_action_screen_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *cancel_lbl = lv_label_create(cancel);
  lv_label_set_text(cancel_lbl, ui_tr(UI_TXT_CANCEL));
  lv_obj_center(cancel_lbl);

  lv_obj_t *ok = lv_btn_create(scr);
  lv_obj_set_size(ok, 86, 42);
  lv_obj_set_style_bg_color(ok, COLOR_ORANGE, 0);
  lv_obj_align(ok, LV_ALIGN_CENTER, 48, 45);
  lv_obj_add_event_cb(ok, settings_restart_confirm_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *ok_lbl = lv_label_create(ok);
  lv_label_set_text(ok_lbl, ui_tr(UI_TXT_RESTART));
  lv_obj_center(ok_lbl);

  ui_navigation_add_gesture(scr);
  ui_navigation_push(scr);
}

static void __attribute__((unused)) settings_open_unsupported_cb(lv_event_t *e) {
  const char *action = lv_event_get_user_data(e);
  lv_obj_t *scr = ui_navigation_create_screen();

  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, action ? action : ui_text("Feature", "Chức năng"));
  lv_obj_set_style_text_font(title, UI_FONT_20, 0);
  lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
  lv_obj_align(title, LV_ALIGN_CENTER, 0, -45);

  lv_obj_t *body = lv_label_create(scr);
  lv_label_set_text(body, ui_tr(UI_TXT_UNSUPPORTED));
  lv_obj_set_width(body, WATCH_LCD_H_RES - 40);
  lv_obj_set_style_text_align(body, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(body, COLOR_TEXT_MUTED, 0);
  lv_obj_align(body, LV_ALIGN_CENTER, 0, -5);

  lv_obj_t *ok = lv_btn_create(scr);
  lv_obj_set_size(ok, 110, 42);
  lv_obj_set_style_bg_color(ok, COLOR_PRIMARY, 0);
  lv_obj_align(ok, LV_ALIGN_CENTER, 0, 50);
  lv_obj_add_event_cb(ok, settings_close_action_screen_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *ok_lbl = lv_label_create(ok);
  lv_label_set_text(ok_lbl, ui_tr(UI_TXT_OK));
  lv_obj_center(ok_lbl);

  ui_navigation_add_gesture(scr);
  ui_navigation_push(scr);
}

static void settings_open_watchface_picker_cb(lv_event_t *e);
static void settings_open_display_cb(lv_event_t *e) {
  (void)e;
  ui_navigation_push(ui_display_settings_screen_create());
}

/* Khởi tạo màn hình Menu Cài đặt hệ thống (Settings Screen)
   Bố cục danh sách cuộn gồm các phần tử: Giới thiệu thiết bị, Rung,
   Cá nhân hóa (chọn mặt đồng hồ, màu chủ đề), Đèn nền hiển thị, Ngôn ngữ, và Cài đặt thời gian.
   Trả về: lv_obj_t* Con trỏ đối tượng màn hình Cài đặt */
lv_obj_t *ui_settings_screen_create(void) {
  lv_obj_t *list = apps_make_list_screen(ui_tr(UI_TXT_SETTINGS));
  lv_obj_t *scr = lv_obj_get_parent(list);
  ui_navigation_set_screen_id(scr, UI_SCREEN_SETTINGS);

  lv_obj_t *i1 = apps_make_list_item(list, ICON_SUB_ABOUT, COLOR_PRIMARY, ui_tr(UI_TXT_ABOUT));
  lv_obj_add_flag(i1, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(i1, settings_open_about_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *i2 = apps_make_list_item(list, ICON_SUB_VIBRATION, COLOR_CYAN, ui_tr(UI_TXT_VIBRATION));
  lv_obj_add_flag(i2, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(i2, settings_open_vibration_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *i3 = apps_make_list_item(list, ICON_SUB_PERSONAL, COLOR_PURPLE, ui_tr(UI_TXT_PERSONALIZATION));
  lv_obj_add_flag(i3, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(i3, settings_open_personality_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *i4 = apps_make_list_item(list, ICON_SUB_DISPLAY, COLOR_ORANGE, ui_text("Display", "Hiển thị"));
  lv_obj_add_flag(i4, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(i4, settings_open_display_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *i5 = apps_make_list_item(list, ICON_SUB_LANGUAGE, COLOR_GREEN, ui_tr(UI_TXT_LANGUAGE));
  lv_obj_add_flag(i5, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(i5, settings_open_language_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *i6 = apps_make_list_item(list, ICON_SUB_SET_TIME, COLOR_CYAN, ui_tr(UI_TXT_SET_TIME));
  lv_obj_add_flag(i6, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(i6, settings_open_set_time_cb, LV_EVENT_CLICKED, NULL);

  return scr;
}

/* ============ Watch Face Picker ============ */
/* Callback lưu cấu hình kiểu mặt đồng hồ mới chọn vào Flash NVS
   Nếu chọn trùng mặt đồng hồ hiện tại thì tiến hành vẽ lại (Recreate Root). */
static void settings_watchface_select_cb(lv_event_t *e) {
  int id = (int)(intptr_t)lv_event_get_user_data(e);
  int current_id = ui_watchface_get_style();
  ui_watchface_set_style(id);
  ESP_ERROR_CHECK_WITHOUT_ABORT(watch_settings_set_watchface_style((uint8_t)id));
  if (id == current_id) {
    ui_navigation_recreate_root(ui_watchface_screen_create);
  }
}

/* Callback mở danh sách lựa chọn phong cách Mặt đồng hồ (Watchface Picker) */
static void settings_open_watchface_picker_cb(lv_event_t *e) {
  (void)e;
  lv_obj_t *scr = ui_navigation_create_screen();
  lv_obj_t *hdr = lv_label_create(scr);
  lv_label_set_text(hdr, ui_tr(UI_TXT_WATCH_FACE));
  lv_obj_set_style_text_font(hdr, UI_FONT_16, 0);
  lv_obj_set_style_text_color(hdr, COLOR_TEXT_MUTED, 0);
  lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 38);

  int cur = ui_watchface_get_style();
  const char *names[] = {
    ui_tr(UI_TXT_SIMPLE_DIGITAL),
    ui_tr(UI_TXT_GRADIENT_MODERN),
    ui_tr(UI_TXT_CLASSIC_ANALOG),
  };

  lv_obj_t *list = lv_obj_create(scr);
  lv_obj_set_size(list, lv_pct(90), WATCH_LCD_V_RES - 80);
  lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 68);
  lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(list, 0, 0);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  ui_enable_list_scrolling(list);
  lv_obj_set_style_pad_row(list, 8, 0);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);

  for (int i = 0; i < 3; i++) {
    lv_obj_t *card = lv_obj_create(list);
    lv_obj_set_size(card, lv_pct(100), 52);
    lv_obj_set_style_bg_color(card, COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(card, (i == cur) ? 2 : 0, 0);
    lv_obj_set_style_border_color(card, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_left(card, 15, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, settings_watchface_select_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

    lv_obj_t *lbl = lv_label_create(card);
    lv_label_set_text(lbl, names[i]);
    lv_obj_set_style_text_color(lbl, (i == cur) ? COLOR_PRIMARY : COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_16, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

    if (i == cur) {
      lv_obj_t *check = lv_label_create(card);
      lv_label_set_text(check, ICON_SIGNAL_OK);
      lv_obj_set_style_text_font(check, &signal_icon, 0);
      lv_obj_set_style_text_color(check, COLOR_GREEN, 0);
      lv_obj_align(check, LV_ALIGN_RIGHT_MID, -10, 0);
    }
  }
  ui_navigation_add_gesture(scr);
  ui_navigation_push(scr);
}

/* Khởi tạo màn hình cài đặt Cá nhân hóa (Mặt đồng hồ & Màu chủ đề)
   Trả về: lv_obj_t* Con trỏ đối tượng màn hình vừa tạo */
lv_obj_t *ui_personality_settings_screen_create(void) {
  lv_obj_t *list = apps_make_list_screen(ui_tr(UI_TXT_PERSONALIZATION));
  lv_obj_t *scr = lv_obj_get_parent(list);
  ui_navigation_set_screen_id(scr, UI_SCREEN_PERSONALITY_SETTINGS);

  lv_obj_t *watchface = apps_make_list_item(list, NULL, COLOR_PRIMARY, ui_tr(UI_TXT_WATCH_FACE));
  lv_obj_add_flag(watchface, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(watchface, settings_open_watchface_picker_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *color = apps_make_list_item(list, NULL, COLOR_ORANGE, ui_tr(UI_TXT_SYSTEM_COLOR));
  lv_obj_add_flag(color, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(color, settings_open_color_picker_cb, LV_EVENT_CLICKED, NULL);

  return scr;
}

/* Callback xử lý thanh trượt (slider) thay đổi độ sáng màn hình
   Chuyển đổi tỉ lệ phần trăm [5%, 100%] sang chu kỳ làm việc của PWM LEDC [0, 255]
   để điều khiển độ rộng xung đèn nền Backlight LCD, sau đó lưu cấu hình vào Flash. */
static void settings_brightness_slider_cb(lv_event_t *e) {
  lv_obj_t *slider = lv_event_get_target(e);
  lv_obj_t *value_label = lv_event_get_user_data(e);
  int val = lv_slider_get_value(slider);
  /* Update duty: val 0-100 -> duty 0-255 */
  int duty = val * 255 / 100;

  void ui_backlight_set_active_duty(uint32_t duty);
  ui_backlight_set_active_duty(duty);

  ledc_set_duty(LEDC_LOW_SPEED_MODE, WATCH_LCD_BACKLIGHT_LEDC_CH, duty);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, WATCH_LCD_BACKLIGHT_LEDC_CH);
  if (value_label && lv_obj_is_valid(value_label)) {
    char buf[8];
    lv_snprintf(buf, sizeof(buf), "%d%%", val);
    lv_label_set_text(value_label, buf);
  }
}

static void settings_brightness_save_cb(lv_event_t *e) {
  lv_obj_t *slider = lv_event_get_target(e);
  ESP_ERROR_CHECK_WITHOUT_ABORT(
      watch_settings_set_brightness_pct((uint8_t)lv_slider_get_value(slider)));
}

static void settings_quick_wake_toggle_cb(lv_event_t *e) {
  lv_obj_t *sw = lv_event_get_target(e);
  bool val = lv_obj_has_state(sw, LV_STATE_CHECKED);
  hardware_i2c_sensor_set_quick_wake_enabled(val);
  ESP_ERROR_CHECK_WITHOUT_ABORT(watch_settings_set_quick_wake(val));
  if (val) {
    lv_obj_set_style_bg_color(sw, COLOR_GREEN, LV_PART_INDICATOR);
  } else {
    lv_obj_set_style_bg_color(sw, lv_color_hex(0x4A4F58), LV_PART_INDICATOR);
  }
}

static void settings_auto_sleep_toggle_cb(lv_event_t *e) {
  lv_obj_t *sw = lv_event_get_target(e);
  bool val = lv_obj_has_state(sw, LV_STATE_CHECKED);
  ESP_ERROR_CHECK_WITHOUT_ABORT(watch_settings_set_auto_light_sleep(val));
  if (val) {
    lv_obj_set_style_bg_color(sw, COLOR_GREEN, LV_PART_INDICATOR);
  } else {
    lv_obj_set_style_bg_color(sw, lv_color_hex(0x4A4F58), LV_PART_INDICATOR);
  }
}

static void settings_timeout_select_cb(lv_event_t *e) {
  int seconds = (int)(intptr_t)lv_event_get_user_data(e);
  hardware_i2c_sensor_set_screen_timeout(seconds);
  ESP_ERROR_CHECK_WITHOUT_ABORT(watch_settings_set_screen_timeout((uint16_t)seconds));
  ui_navigation_pop();
}

static void settings_open_display_timeout_picker(void) {
  lv_obj_t *scr = ui_navigation_create_screen();
  
  lv_obj_t *hdr = lv_label_create(scr);
  lv_label_set_text(hdr, ui_tr(UI_TXT_SCREEN_TIMEOUT));
  lv_obj_set_style_text_font(hdr, UI_FONT_20, 0);
  lv_obj_set_style_text_color(hdr, COLOR_TEXT, 0);
  lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 42);

  lv_obj_t *list = lv_obj_create(scr);
  lv_obj_set_size(list, WATCH_LCD_H_RES - 20, WATCH_LCD_V_RES - 85);
  lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, -5);
  lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(list, 0, 0);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  ui_enable_list_scrolling(list);
  lv_obj_set_style_pad_row(list, 8, 0);
  lv_obj_set_style_pad_all(list, 0, 0);
  lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);

  struct {
    const char *label;
    int seconds;
  } opts[] = {
    {ui_text("5 seconds", "5 giây"), 5},
    {ui_text("10 seconds", "10 giây"), 10},
    {ui_text("15 seconds", "15 giây"), 15},
    {ui_text("20 seconds", "20 giây"), 20},
    {ui_text("Never", "Không bao giờ"), 0}
  };

  int cur = hardware_i2c_sensor_get_screen_timeout();

  for (int i = 0; i < 5; i++) {
    lv_obj_t *btn = lv_btn_create(list);
    lv_obj_set_size(btn, lv_pct(100), 45);
    lv_obj_set_style_bg_color(btn, COLOR_SURFACE, 0);
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, opts[i].label);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 12, 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_14, 0);

    if (opts[i].seconds == cur) {
      lv_obj_t *tick = lv_label_create(btn);
      lv_label_set_text(tick, ICON_SIGNAL_OK);
      lv_obj_set_style_text_font(tick, &signal_icon, 0);
      lv_obj_set_style_text_color(tick, COLOR_GREEN, 0);
      lv_obj_align(tick, LV_ALIGN_RIGHT_MID, -12, 0);
    }

    lv_obj_add_event_cb(btn, settings_timeout_select_cb, LV_EVENT_CLICKED, (void *)(intptr_t)opts[i].seconds);
  }

  ui_navigation_add_gesture(scr);
  ui_navigation_push(scr);
}

static void settings_open_timeout_cb(lv_event_t *e) {
  (void)e;
  settings_open_display_timeout_picker();
}

/* Khởi tạo màn hình cài đặt Đèn nền & Đánh thức màn hình (Display Settings)
   Bố trí thanh trượt slider đổi độ sáng, nút chọn thời gian sáng màn hình (Screen Timeout),
   và công tắc Switch bật/tắt Đánh thức nhanh bằng cử chỉ lắc cổ tay (Quick Wake).
   Trả về: lv_obj_t* Con trỏ đối tượng màn hình Cài đặt hiển thị */
lv_obj_t *ui_display_settings_screen_create(void) {
  lv_obj_t *list = apps_make_list_screen(ui_text("Display", "Hiển thị"));
  lv_obj_t *scr = lv_obj_get_parent(list);
  ui_navigation_set_screen_id(scr, UI_SCREEN_DISPLAY_SETTINGS);

  lv_obj_t *brightness_card = lv_obj_create(list);
  lv_obj_set_size(brightness_card, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(brightness_card, COLOR_SURFACE, 0);
  lv_obj_set_style_radius(brightness_card, 18, 0);
  lv_obj_set_style_border_width(brightness_card, 0, 0);
  lv_obj_set_style_pad_all(brightness_card, 10, 0);
  lv_obj_clear_flag(brightness_card, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *b_icon = lv_label_create(brightness_card);
  lv_label_set_text(b_icon, ui_tr(UI_TXT_BRIGHTNESS));
  lv_obj_set_style_text_color(b_icon, COLOR_TEXT_MUTED, 0);
  lv_obj_set_style_text_font(b_icon, UI_FONT_14, 0);
  lv_obj_align(b_icon, LV_ALIGN_TOP_LEFT, 0, 0);

  lv_obj_t *b_val = lv_label_create(brightness_card);
  uint32_t cur_duty = ledc_get_duty(LEDC_LOW_SPEED_MODE, WATCH_LCD_BACKLIGHT_LEDC_CH);
  int cur_pct = (int)(cur_duty * 100 / 255);
  if (cur_pct < 5) cur_pct = 5;
  char b_buf[8];
  lv_snprintf(b_buf, sizeof(b_buf), "%d%%", cur_pct);
  lv_label_set_text(b_val, b_buf);
  lv_obj_set_style_text_color(b_val, COLOR_TEXT, 0);
  lv_obj_set_style_text_font(b_val, UI_FONT_14, 0);
  lv_obj_align(b_val, LV_ALIGN_TOP_RIGHT, 0, 0);

  lv_obj_t *slider = lv_slider_create(brightness_card);
  lv_obj_set_width(slider, lv_pct(100));
  lv_obj_align(slider, LV_ALIGN_TOP_MID, 0, 25);
  lv_slider_set_range(slider, 5, 100);
  lv_slider_set_value(slider, cur_pct, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(slider, COLOR_SURFACE_LT, LV_PART_MAIN);
  lv_obj_set_style_bg_color(slider, COLOR_PRIMARY, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider, COLOR_PRIMARY, LV_PART_KNOB);
  lv_obj_add_event_cb(slider, settings_brightness_slider_cb, LV_EVENT_VALUE_CHANGED, b_val);
  lv_obj_add_event_cb(slider, settings_brightness_save_cb, LV_EVENT_RELEASED, NULL);
  lv_obj_add_event_cb(slider, settings_brightness_save_cb, LV_EVENT_PRESS_LOST, NULL);

  lv_obj_t *timeout_btn = apps_make_list_item(list, NULL, COLOR_ORANGE, ui_tr(UI_TXT_SCREEN_TIMEOUT));
  lv_obj_add_flag(timeout_btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(timeout_btn, settings_open_timeout_cb, LV_EVENT_CLICKED, NULL);

  /* Quick Wake Toggle Card */
  lv_obj_t *qw_card = lv_obj_create(list);
  lv_obj_set_size(qw_card, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(qw_card, COLOR_SURFACE, 0);
  lv_obj_set_style_radius(qw_card, 18, 0);
  lv_obj_set_style_border_width(qw_card, 0, 0);
  lv_obj_set_style_pad_all(qw_card, 15, 0);
  lv_obj_clear_flag(qw_card, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *qw_lbl = lv_label_create(qw_card);
  lv_label_set_text(qw_lbl, ui_text("Quick Wake", "Đánh thức nhanh"));
  lv_obj_set_style_text_color(qw_lbl, COLOR_TEXT, 0);
  lv_obj_set_style_text_font(qw_lbl, UI_FONT_16, 0);
  lv_obj_align(qw_lbl, LV_ALIGN_LEFT_MID, 0, 0);

  lv_obj_t *qw_sw = lv_switch_create(qw_card);
  if (hardware_i2c_sensor_is_quick_wake_enabled()) {
    lv_obj_add_state(qw_sw, LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(qw_sw, COLOR_GREEN, LV_PART_INDICATOR);
  } else {
    lv_obj_set_style_bg_color(qw_sw, lv_color_hex(0x4A4F58), LV_PART_INDICATOR);
  }
  lv_obj_set_style_bg_color(qw_sw, lv_color_hex(0x2B3038), LV_PART_MAIN);
  lv_obj_align(qw_sw, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_add_event_cb(qw_sw, settings_quick_wake_toggle_cb, LV_EVENT_VALUE_CHANGED, NULL);
  
  /* Auto Sleep (Light Sleep) Toggle Card */
  lv_obj_t *as_card = lv_obj_create(list);
  lv_obj_set_size(as_card, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(as_card, COLOR_SURFACE, 0);
  lv_obj_set_style_radius(as_card, 18, 0);
  lv_obj_set_style_border_width(as_card, 0, 0);
  lv_obj_set_style_pad_all(as_card, 15, 0);
  lv_obj_clear_flag(as_card, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *as_lbl = lv_label_create(as_card);
  lv_label_set_text(as_lbl, ui_text("Auto Sleep", "Tự động ngủ"));
  lv_obj_set_style_text_color(as_lbl, COLOR_TEXT, 0);
  lv_obj_set_style_text_font(as_lbl, UI_FONT_16, 0);
  lv_obj_align(as_lbl, LV_ALIGN_LEFT_MID, 0, 0);

  lv_obj_t *as_sw = lv_switch_create(as_card);
  if (watch_settings_get()->auto_light_sleep_enabled) {
    lv_obj_add_state(as_sw, LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(as_sw, COLOR_GREEN, LV_PART_INDICATOR);
  } else {
    lv_obj_set_style_bg_color(as_sw, lv_color_hex(0x4A4F58), LV_PART_INDICATOR);
  }
  lv_obj_set_style_bg_color(as_sw, lv_color_hex(0x2B3038), LV_PART_MAIN);
  lv_obj_align(as_sw, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_add_event_cb(as_sw, settings_auto_sleep_toggle_cb, LV_EVENT_VALUE_CHANGED, NULL);

  return scr;
}

static void settings_toggle_event_cb(lv_event_t *e) {
  lv_obj_t *sw = lv_event_get_target(e);
  if (lv_obj_has_state(sw, LV_STATE_CHECKED)) {
    lv_obj_set_style_bg_color(sw, COLOR_GREEN, LV_PART_INDICATOR);
  } else {
    lv_obj_set_style_bg_color(sw, lv_color_hex(0x4A4F58), LV_PART_INDICATOR);
  }
}

/* Lấy chuỗi ngôn ngữ hiển thị tương ứng cho mức độ rung động cơ */
static const char *settings_vibration_strength_text(void) {
  int s_vibration_strength = watch_settings_get()->vibration_strength;
  switch (s_vibration_strength) {
    case 0: return ui_text("Light", "Nhẹ");
    case 2: return ui_text("Strong", "Mạnh");
    default: return ui_text("Medium", "Vừa");
  }
}

/* Callback chu kỳ thay đổi cường độ rung động cơ
   Vòng lặp xoay vòng cường độ: Nhẹ -> Vừa -> Mạnh và rung thử để kiểm tra. */
static void settings_vibration_strength_cb(lv_event_t *e) {
  lv_obj_t *label = lv_event_get_user_data(e);
  uint8_t s_vibration_strength = (watch_settings_get()->vibration_strength + 1) % 3;
  ESP_ERROR_CHECK_WITHOUT_ABORT(watch_settings_set_vibration_strength(s_vibration_strength));
  if (watch_settings_get()->vibration_enabled) {
    watch_vibration_pulse(s_vibration_strength, 180);
  }
  if (label && lv_obj_is_valid(label)) {
    lv_label_set_text(label, settings_vibration_strength_text());
  }
}

static void settings_vibration_enable_cb(lv_event_t *e) {
  settings_toggle_event_cb(e);
  lv_obj_t *sw = lv_event_get_target(e);
  bool enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
  ESP_ERROR_CHECK_WITHOUT_ABORT(watch_settings_set_vibration_enabled(enabled));
  if (enabled) {
    watch_vibration_pulse(watch_settings_get()->vibration_strength, 120);
  } else {
    watch_vibration_stop();
  }
}

lv_obj_t *ui_vibration_settings_screen_create(void) {
  lv_obj_t *list = apps_make_list_screen(ui_tr(UI_TXT_VIBRATION));
  lv_obj_t *scr = lv_obj_get_parent(list);
  ui_navigation_set_screen_id(scr, UI_SCREEN_VIBRATION_SETTINGS);

  lv_obj_t *card = lv_obj_create(list);
  lv_obj_set_size(card, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(card, COLOR_SURFACE, 0);
  lv_obj_set_style_radius(card, 18, 0);
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_set_style_pad_all(card, 15, 0);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *lbl = lv_label_create(card);
  lv_label_set_text(lbl, ui_tr(UI_TXT_VIBRATION));
  lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
  lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

  lv_obj_t *sw = lv_switch_create(card);
  if (watch_settings_get()->vibration_enabled) lv_obj_add_state(sw, LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(sw, lv_color_hex(0x2B3038), LV_PART_MAIN);
  lv_obj_set_style_bg_color(sw, lv_color_hex(0x4A4F58), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(sw, COLOR_GREEN, LV_PART_INDICATOR | LV_STATE_CHECKED);
  lv_obj_align(sw, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_add_event_cb(sw, settings_vibration_enable_cb, LV_EVENT_VALUE_CHANGED, NULL);

  lv_obj_t *strength = lv_obj_create(list);
  lv_obj_set_size(strength, lv_pct(100), 55);
  lv_obj_set_style_bg_color(strength, COLOR_SURFACE, 0);
  lv_obj_set_style_radius(strength, 18, 0);
  lv_obj_set_style_border_width(strength, 0, 0);
  lv_obj_set_style_pad_all(strength, 15, 0);
  lv_obj_clear_flag(strength, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(strength, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *strength_title = lv_label_create(strength);
  lv_label_set_text(strength_title, ui_text("Strength", "Độ mạnh"));
  lv_obj_set_style_text_font(strength_title, UI_FONT_16, 0);
  lv_obj_set_style_text_color(strength_title, COLOR_TEXT, 0);
  lv_obj_align(strength_title, LV_ALIGN_LEFT_MID, 0, 0);

  lv_obj_t *strength_val = lv_label_create(strength);
  lv_label_set_text(strength_val, settings_vibration_strength_text());
  lv_obj_set_style_text_font(strength_val, UI_FONT_14, 0);
  lv_obj_set_style_text_color(strength_val, COLOR_TEXT_MUTED, 0);
  lv_obj_align(strength_val, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_add_event_cb(strength, settings_vibration_strength_cb, LV_EVENT_CLICKED, strength_val);

  /* Messages and alarms */
  lv_obj_t *card2 = lv_obj_create(list);
  lv_obj_set_size(card2, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(card2, COLOR_SURFACE, 0);
  lv_obj_set_style_radius(card2, 18, 0);
  lv_obj_set_style_border_width(card2, 0, 0);
  lv_obj_set_style_pad_all(card2, 15, 0);
  lv_obj_clear_flag(card2, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *lbl2 = lv_label_create(card2);
  lv_label_set_text(lbl2, ui_tr(UI_TXT_CALLS));
  lv_obj_set_style_text_font(lbl2, UI_FONT_16, 0);
  lv_obj_set_style_text_color(lbl2, COLOR_TEXT, 0);
  lv_obj_align(lbl2, LV_ALIGN_LEFT_MID, 0, 0);

  lv_obj_t *sw2 = lv_switch_create(card2);
  lv_obj_add_state(sw2, LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(sw2, lv_color_hex(0x2B3038), LV_PART_MAIN);
  lv_obj_set_style_bg_color(sw2, lv_color_hex(0x4A4F58), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(sw2, COLOR_GREEN, LV_PART_INDICATOR | LV_STATE_CHECKED);
  lv_obj_align(sw2, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_add_event_cb(sw2, settings_toggle_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

  lv_obj_t *card3 = lv_obj_create(list);
  lv_obj_set_size(card3, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(card3, COLOR_SURFACE, 0);
  lv_obj_set_style_radius(card3, 18, 0);
  lv_obj_set_style_border_width(card3, 0, 0);
  lv_obj_set_style_pad_all(card3, 15, 0);
  lv_obj_clear_flag(card3, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *lbl3 = lv_label_create(card3);
  lv_label_set_text(lbl3, ui_tr(UI_TXT_ALARMS));
  lv_obj_set_style_text_font(lbl3, UI_FONT_16, 0);
  lv_obj_set_style_text_color(lbl3, COLOR_TEXT, 0);
  lv_obj_align(lbl3, LV_ALIGN_LEFT_MID, 0, 0);

  lv_obj_t *sw3 = lv_switch_create(card3);
  lv_obj_add_state(sw3, LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(sw3, lv_color_hex(0x2B3038), LV_PART_MAIN);
  lv_obj_set_style_bg_color(sw3, lv_color_hex(0x4A4F58), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(sw3, COLOR_GREEN, LV_PART_INDICATOR | LV_STATE_CHECKED);
  lv_obj_align(sw3, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_add_event_cb(sw3, settings_toggle_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

  return scr;
}

static void settings_open_about_cb(lv_event_t *e) {
  (void)e;
  ui_navigation_push(ui_about_screen_create());
}

static void __attribute__((unused)) settings_poweroff_cb(lv_event_t *e) {
  (void)e;
  ui_request_deep_sleep();
}

static void settings_language_select_cb(lv_event_t *e) {
  ui_language_t lang = (ui_language_t)(intptr_t)lv_event_get_user_data(e);
  ui_language_set(lang);
  ESP_ERROR_CHECK_WITHOUT_ABORT(watch_settings_set_language((uint8_t)lang));
  ui_navigation_pop_to_root();
  ui_navigation_push(ui_settings_screen_create());
}

lv_obj_t *ui_language_settings_screen_create(void) {
  lv_obj_t *list = apps_make_list_screen(ui_tr(UI_TXT_LANGUAGE));
  lv_obj_t *scr = lv_obj_get_parent(list);
  ui_navigation_set_screen_id(scr, UI_SCREEN_LANGUAGE_SETTINGS);

  ui_language_t cur = ui_language_get();
  const struct {
    ui_language_t lang;
    ui_text_id_t text_id;
  } rows[] = {
    {UI_LANG_EN, UI_TXT_ENGLISH},
    {UI_LANG_VI, UI_TXT_VIETNAMESE},
  };

  for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
    lv_obj_t *item = apps_make_list_item(list, NULL,
                                         rows[i].lang == cur ? COLOR_GREEN : COLOR_PRIMARY,
                                         ui_tr(rows[i].text_id));
    lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(item, settings_language_select_cb, LV_EVENT_CLICKED,
                        (void *)(intptr_t)rows[i].lang);
  }

  return scr;
}

static lv_obj_t *s_time_year_roller;
static lv_obj_t *s_time_month_roller;
static lv_obj_t *s_time_day_roller;
static lv_obj_t *s_time_hour_roller;
static lv_obj_t *s_time_min_roller;
static lv_obj_t *s_time_header_label;
static lv_obj_t *s_time_date_row;
static lv_obj_t *s_time_clock_row;
static lv_obj_t *s_time_action_label;
static int s_time_stage;

static void set_time_screen_delete_cb(lv_event_t *e) {
  (void)e;
  s_time_year_roller = NULL;
  s_time_month_roller = NULL;
  s_time_day_roller = NULL;
  s_time_hour_roller = NULL;
  s_time_min_roller = NULL;
  s_time_header_label = NULL;
  s_time_date_row = NULL;
  s_time_clock_row = NULL;
  s_time_action_label = NULL;
  s_time_stage = 0;
}

static void set_time_make_range(char *buf, size_t len, int first, int last, int width) {
  char *p = buf;
  size_t rem = len;
  for (int v = first; v <= last && rem > 1; v++) {
    int n = snprintf(p, rem, "%0*d", width, v);
    if (n < 0 || (size_t)n >= rem) break;
    p += n;
    rem -= n;
    if (v != last && rem > 1) {
      *p++ = '\n';
      rem--;
    }
  }
  *p = '\0';
}

static lv_obj_t *set_time_make_roller(lv_obj_t *parent, const char *opts, int selected, lv_coord_t width) {
  lv_obj_t *roller = lv_roller_create(parent);
  lv_roller_set_options(roller, opts, LV_ROLLER_MODE_NORMAL);
  lv_roller_set_visible_row_count(roller, 1);
  lv_roller_set_selected(roller, selected, LV_ANIM_OFF);
  lv_obj_set_width(roller, width);
  lv_obj_set_style_bg_opa(roller, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(roller, 0, 0);
  lv_obj_set_style_text_font(roller, UI_FONT_24, 0);
  lv_obj_set_style_bg_opa(roller, LV_OPA_TRANSP, LV_PART_SELECTED);
  return roller;
}

static lv_obj_t *set_time_make_picker(lv_obj_t *parent, const char *opts, int selected, lv_coord_t width) {
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

  lv_obj_t *roller = set_time_make_roller(box, opts, selected, width);
  lv_obj_align(roller, LV_ALIGN_CENTER, 0, 0);

  lv_obj_t *down = lv_label_create(box);
  lv_label_set_text(down, ICON_ANGLE_DOWN);
  lv_obj_set_style_text_font(down, &angle_icon, 0);
  lv_obj_set_style_text_color(down, COLOR_TEXT_MUTED, 0);
  lv_obj_align(down, LV_ALIGN_BOTTOM_MID, 0, 0);
  return roller;
}

/* Điều khiển các bước cấu hình thời gian (Stage 0: Đặt ngày, Stage 1: Đặt giờ) */
static void set_time_show_stage(int stage) {
  s_time_stage = stage;
  if (s_time_header_label) {
    lv_label_set_text(s_time_header_label, stage == 0 ? ui_text("Set Day", "Đặt ngày")
                                                       : ui_tr(UI_TXT_SET_TIME));
  }
  if (s_time_date_row && s_time_clock_row) {
    if (stage == 0) {
      lv_obj_clear_flag(s_time_date_row, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(s_time_clock_row, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(s_time_date_row, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(s_time_clock_row, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (s_time_action_label) {
    lv_label_set_text(s_time_action_label, stage == 0 ? ui_text("Next", "Tiếp")
                                                       : ui_text("Save", "Lưu"));
  }
}

/* Callback xác nhận lưu thời gian đã cấu hình từ các Roller
   Tập hợp thông số năm-tháng-ngày và giờ-phút để cấu hình lại thời gian hệ thống ESP32-S3
   thông qua hàm settimeofday và đồng bộ lại thanh trạng thái StatusBar. */
static void set_time_save_cb(lv_event_t *e) {
  (void)e;
  if (s_time_stage == 0) {
    set_time_show_stage(1);
    return;
  }
  if (!s_time_year_roller || !s_time_month_roller || !s_time_day_roller ||
      !s_time_hour_roller || !s_time_min_roller) {
    return;
  }

  struct tm tm_info = {0};
  tm_info.tm_year = 2024 + lv_roller_get_selected(s_time_year_roller) - 1900;
  tm_info.tm_mon = lv_roller_get_selected(s_time_month_roller);
  tm_info.tm_mday = 1 + lv_roller_get_selected(s_time_day_roller);
  tm_info.tm_hour = lv_roller_get_selected(s_time_hour_roller);
  tm_info.tm_min = lv_roller_get_selected(s_time_min_roller);
  tm_info.tm_sec = 0;
  time_t t = mktime(&tm_info);
  if (t > 0) {
    struct timeval tv = {.tv_sec = t, .tv_usec = 0};
    settimeofday(&tv, NULL);
    ui_statusbar_update_time();
  }
  ui_navigation_pop();
}

lv_obj_t *ui_set_time_screen_create(void) {
  lv_obj_t *scr = ui_navigation_create_screen_with_id(UI_SCREEN_SET_TIME);
  lv_obj_add_event_cb(scr, set_time_screen_delete_cb, LV_EVENT_DELETE, NULL);
  s_time_stage = 0;

  s_time_header_label = lv_label_create(scr);
  lv_label_set_text(s_time_header_label, ui_text("Set Day", "Đặt ngày"));
  lv_obj_set_style_text_font(s_time_header_label, UI_FONT_20, 0);
  lv_obj_set_style_text_color(s_time_header_label, COLOR_TEXT, 0);
  lv_obj_align(s_time_header_label, LV_ALIGN_TOP_MID, 0, 42);

  ui_time_snapshot_t now;
  ui_time_get_snapshot(&now);

  char years[80], months[36], days[96], hours[72], minutes[180];
  set_time_make_range(years, sizeof(years), 2024, 2035, 4);
  set_time_make_range(months, sizeof(months), 1, 12, 2);
  set_time_make_range(days, sizeof(days), 1, 31, 2);
  set_time_make_range(hours, sizeof(hours), 0, 23, 2);
  set_time_make_range(minutes, sizeof(minutes), 0, 59, 2);

  s_time_date_row = lv_obj_create(scr);
  lv_obj_set_size(s_time_date_row, WATCH_LCD_H_RES - 6, 130);
  lv_obj_align(s_time_date_row, LV_ALIGN_CENTER, 0, -8);
  lv_obj_set_style_bg_opa(s_time_date_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(s_time_date_row, 0, 0);
  lv_obj_set_style_pad_all(s_time_date_row, 0, 0);
  lv_obj_set_style_pad_column(s_time_date_row, 4, 0);
  lv_obj_set_flex_flow(s_time_date_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(s_time_date_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(s_time_date_row, LV_OBJ_FLAG_SCROLLABLE);

  time_t raw_now = time(NULL);
  struct tm ti;
  localtime_r(&raw_now, &ti);
  int year = ti.tm_year + 1900;
  if (year < 2024 || year > 2035) year = 2024;
  s_time_day_roller = set_time_make_picker(s_time_date_row, days, now.day > 0 ? now.day - 1 : 0, 54);
  s_time_month_roller = set_time_make_picker(s_time_date_row, months, now.month > 0 ? now.month - 1 : 0, 54);
  s_time_year_roller = set_time_make_picker(s_time_date_row, years, year - 2024, 82);

  s_time_clock_row = lv_obj_create(scr);
  lv_obj_set_size(s_time_clock_row, WATCH_LCD_H_RES - 60, 130);
  lv_obj_align(s_time_clock_row, LV_ALIGN_CENTER, 0, -8);
  lv_obj_set_style_bg_opa(s_time_clock_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(s_time_clock_row, 0, 0);
  lv_obj_set_style_pad_all(s_time_clock_row, 0, 0);
  lv_obj_set_style_pad_column(s_time_clock_row, 10, 0);
  lv_obj_set_flex_flow(s_time_clock_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(s_time_clock_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(s_time_clock_row, LV_OBJ_FLAG_SCROLLABLE);

  s_time_hour_roller = set_time_make_picker(s_time_clock_row, hours, now.hour, 70);
  s_time_min_roller = set_time_make_picker(s_time_clock_row, minutes, now.minute, 70);

  lv_obj_t *save = lv_btn_create(scr);
  lv_obj_set_size(save, 150, 40);
  lv_obj_set_style_radius(save, 12, 0);
  lv_obj_set_style_bg_color(save, COLOR_PRIMARY, 0);
  lv_obj_align(save, LV_ALIGN_BOTTOM_MID, 0, -14);
  lv_obj_add_event_cb(save, set_time_save_cb, LV_EVENT_CLICKED, NULL);
  s_time_action_label = lv_label_create(save);
  lv_label_set_text(s_time_action_label, ui_text("Next", "Tiếp"));
  lv_obj_set_style_text_font(s_time_action_label, UI_FONT_14, 0);
  lv_obj_center(s_time_action_label);

  set_time_show_stage(0);
  ui_navigation_add_gesture(scr);
  return scr;
}

/* Khởi tạo màn hình giới thiệu thiết bị (About Screen)
   Hiển thị thông tin phần sụn (Firmware version), phần cứng (Hardware target: ESP32-S3),
   và địa chỉ MAC Bluetooth để định danh kết nối cho ứng dụng Mobile.
   Trả về: lv_obj_t* Con trỏ đối tượng màn hình About vừa tạo */
lv_obj_t *ui_about_screen_create(void) {
  lv_obj_t *scr = ui_navigation_create_screen_with_id(UI_SCREEN_ABOUT);
  
  lv_obj_t *hdr = lv_label_create(scr);
  lv_label_set_text(hdr, ui_tr(UI_TXT_ABOUT));
  lv_obj_set_style_text_font(hdr, UI_FONT_16, 0);
  lv_obj_set_style_text_color(hdr, COLOR_TEXT_MUTED, 0);
  lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 38);

  lv_obj_t *card = lv_obj_create(scr);
  lv_obj_set_size(card, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(card, COLOR_SURFACE, 0);
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_set_style_pad_all(card, 15, 0);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(card, 10, 0);
  lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 70);

  const esp_app_desc_t *app_desc = esp_app_get_description();
  lv_obj_t *lbl1 = lv_label_create(card);
  lv_label_set_text_fmt(lbl1, "%s\n%s: %s",
                        ui_tr(UI_TXT_FIRMWARE),
                        ui_tr(UI_TXT_VERSION),
                        app_desc ? app_desc->version : "unknown");
  lv_obj_set_style_text_color(lbl1, COLOR_TEXT, 0);
  lv_obj_set_style_text_font(lbl1, UI_FONT_14, 0);

  lv_obj_t *lbl2 = lv_label_create(card);
  lv_label_set_text_fmt(lbl2, "%s: ESP32-S3", ui_tr(UI_TXT_HARDWARE));
  lv_obj_set_style_text_color(lbl2, COLOR_TEXT_MUTED, 0);
  lv_obj_set_style_text_font(lbl2, UI_FONT_14, 0);

  lv_obj_t *lbl3 = lv_label_create(card);
  uint8_t mac[6] = {0};
  if (esp_read_mac(mac, ESP_MAC_BT) == ESP_OK) {
    lv_label_set_text_fmt(lbl3, "BT MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  } else {
    lv_label_set_text(lbl3, "BT MAC: --");
  }
  lv_obj_set_style_text_color(lbl3, COLOR_TEXT_MUTED, 0);
  lv_obj_set_style_text_font(lbl3, UI_FONT_14, 0);

  ui_navigation_add_gesture(scr);
  return scr;
}

lv_obj_t *ui_system_settings_screen_create(void) {
  lv_obj_t *list = apps_make_list_screen(ui_tr(UI_TXT_SETTINGS));
  lv_obj_t *scr = lv_obj_get_parent(list);
  ui_navigation_set_screen_id(scr, UI_SCREEN_SYSTEM_SETTINGS);

  lv_obj_t *i1 = apps_make_list_item(list, ICON_SUB_ABOUT, COLOR_PRIMARY, ui_tr(UI_TXT_ABOUT));
  lv_obj_add_flag(i1, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(i1, settings_open_about_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *reset = apps_make_list_item(list, "\xef\x80\x94", COLOR_TEXT_MUTED, ui_text("Reset", "Đặt lại"));
  lv_obj_add_flag(reset, LV_OBJ_FLAG_HIDDEN);

  return scr;
}

/* ============ SpO2 ============ */
/* Khởi tạo giao diện màn hình đo Nồng độ Oxy trong máu (SpO2)
   Hiển thị chỉ số SpO2 đo từ cảm biến. */
lv_obj_t *ui_spo2_screen_create(void) {
  lv_obj_t *scr = ui_navigation_create_screen_with_id(UI_SCREEN_SPO2);
  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, "SpO2");
  lv_obj_set_style_text_font(title, UI_FONT_20, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);

  lv_obj_t *val = lv_label_create(scr);
  lv_label_set_text(val, "-- %");
  lv_obj_set_style_text_font(val, UI_FONT_48, 0);
  lv_obj_set_style_text_color(val, COLOR_CYAN, 0);
  lv_obj_align(val, LV_ALIGN_CENTER, 0, -10);

  lv_obj_t *hint = lv_label_create(scr);
  lv_label_set_text(hint, ui_tr(UI_TXT_WAITING_MAX30102));
  lv_obj_set_style_text_font(hint, UI_FONT_14, 0);
  lv_obj_set_style_text_color(hint, COLOR_TEXT_MUTED, 0);
  lv_obj_align(hint, LV_ALIGN_CENTER, 0, 30);
  ui_navigation_add_gesture(scr);
  return scr;
}

/* Open a simple color picker for system icon color */
static void settings_apply_icon_color(bool mono, lv_color_t c) {
  ui_menu_set_system_color(mono, c);
}

static void settings_color_button_cb(lv_event_t *e) {
  (void)e;
  int id = (int)(intptr_t)lv_event_get_user_data(e);
  switch (id) {
  case 0: /* default = multicolor */
    settings_apply_icon_color(false, COLOR_BG);
    ESP_ERROR_CHECK_WITHOUT_ABORT(watch_settings_set_icon_color(false, WATCH_COLOR_DEFAULT));
    break;
  case 1:
    settings_apply_icon_color(true, COLOR_GREEN);
    ESP_ERROR_CHECK_WITHOUT_ABORT(watch_settings_set_icon_color(true, WATCH_COLOR_GREEN));
    break;
  case 2:
    settings_apply_icon_color(true, COLOR_RED);
    ESP_ERROR_CHECK_WITHOUT_ABORT(watch_settings_set_icon_color(true, WATCH_COLOR_RED));
    break;
  case 3:
    settings_apply_icon_color(true, COLOR_PURPLE);
    ESP_ERROR_CHECK_WITHOUT_ABORT(watch_settings_set_icon_color(true, WATCH_COLOR_PURPLE));
    break;
  case 4:
    settings_apply_icon_color(true, COLOR_ORANGE);
    ESP_ERROR_CHECK_WITHOUT_ABORT(watch_settings_set_icon_color(true, WATCH_COLOR_ORANGE));
    break;
  }
  /* Close picker screen (pop) */
  ui_navigation_pop();
}

static void settings_open_color_picker_cb(lv_event_t *e) {
  (void)e;
  lv_obj_t *scr = ui_navigation_create_screen();
  lv_obj_t *hdr = lv_label_create(scr);
  lv_label_set_text(hdr, ui_tr(UI_TXT_SYSTEM_COLOR));
  lv_obj_set_style_text_font(hdr, UI_FONT_16, 0);
  lv_obj_set_style_text_color(hdr, COLOR_TEXT_MUTED, 0);
  lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 38);

  /* Nút Mac dinh (da phoi mau) */
  lv_obj_t *def_btn = lv_btn_create(scr);
  lv_obj_set_size(def_btn, 180, 44);
  lv_obj_set_style_radius(def_btn, 22, 0);
  lv_obj_set_style_bg_color(def_btn, COLOR_SURFACE, 0);
  lv_obj_align(def_btn, LV_ALIGN_CENTER, 0, -20);
  lv_obj_add_event_cb(def_btn, settings_color_button_cb, LV_EVENT_CLICKED, (void *)(intptr_t)0);
  lv_obj_t *dl = lv_label_create(def_btn);
  lv_label_set_text(dl, ui_tr(UI_TXT_DEFAULT));
  lv_obj_set_style_text_color(dl, COLOR_TEXT, 0);
  lv_obj_set_style_text_font(dl, UI_FONT_14, 0);
  lv_obj_center(dl);

  /* Label cho hàng color circles */
  lv_obj_t *lbl = lv_label_create(scr);
  lv_label_set_text(lbl, ui_tr(UI_TXT_CHOOSE_COLOR));
  lv_obj_set_style_text_font(lbl, UI_FONT_14, 0);
  lv_obj_set_style_text_color(lbl, COLOR_TEXT_MUTED, 0);
  lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 20);

  /* Hàng chứa 4 hình tròn màu */
  lv_obj_t *row = lv_obj_create(scr);
  lv_obj_set_size(row, 220, 60);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 55);

  /* Mảng màu + id tương ứng */
  const lv_color_t colors[] = {COLOR_GREEN, COLOR_RED, COLOR_PURPLE, COLOR_ORANGE};
  const int ids[] = {1, 2, 3, 4};

  for (int i = 0; i < 4; i++) {
    lv_obj_t *circle = lv_obj_create(row);
    lv_obj_set_size(circle, 44, 44);
    lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(circle, colors[i], 0);
    lv_obj_set_style_bg_opa(circle, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(circle, 2, 0);
    lv_obj_set_style_border_color(circle, COLOR_SURFACE_LT, 0);
    lv_obj_set_style_border_opa(circle, LV_OPA_60, 0);
    lv_obj_set_style_shadow_width(circle, 0, 0);
    lv_obj_clear_flag(circle, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(circle, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(circle, settings_color_button_cb, LV_EVENT_CLICKED,
                        (void *)(intptr_t)ids[i]);
  }

  ui_navigation_add_gesture(scr);
  ui_navigation_push(scr);
}
