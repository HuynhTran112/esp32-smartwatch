/* Định nghĩa giao diện đồng hồ bấm giờ thể thao (Stopwatch UI Component)
   Module này triển khai chức năng đếm giây thể thao với độ chính xác đến phần trăm giây (Centiseconds - 10ms).
   Cung cấp các nút điều khiển tròn gồm:
   1. Bắt đầu / Tạm dừng (Start/Pause): Điều khiển luồng đếm thời gian.
   2. Vòng (Lap/Split): Ghi lại mốc thời gian hiện tại và tính toán thời gian chênh lệch (Split Time) so với vòng trước.
   3. Đặt lại (Reset): Xóa trắng các thông số đo lường.
   Danh sách các vòng được hiển thị dạng danh sách cuộn trượt mượt mà (using Flexbox List) tự động đẩy xuống cuối. */

#include "board_config.h"
#include "ui.h"
#include "ui_navigation.h"
#include "ui_screens.h"
#include "ui_utils.h"
#include <stdio.h>

/* Các con trỏ lưu trữ trạng thái hiển thị của đồng hồ bấm giờ */
static lv_obj_t *s_stopwatch_time_label = NULL;
static lv_obj_t *s_stopwatch_sub_label = NULL;
static lv_obj_t *s_stopwatch_start_label = NULL;
static lv_obj_t *s_stopwatch_lap_list = NULL;
static lv_timer_t *s_stopwatch_timer = NULL;

static bool s_stopwatch_running = false;
static uint32_t s_stopwatch_start_tick = 0;    // Tick bắt đầu đếm của đợt hiện hành
static uint32_t s_stopwatch_accum_ms = 0;       // Thời gian tích lũy từ các đợt chạy trước
static uint32_t s_stopwatch_last_lap_ms = 0;    // Mốc thời gian của vòng ghi nhận trước đó
static int s_stopwatch_lap_count = 0;           // Bộ đếm số lượng vòng đo

// Số lượng vòng ghi nhận tối đa
#define STOPWATCH_MAX_VISIBLE_LAPS 40

// Tần suất làm tươi màn hình đồng hồ bấm giờ (50ms ~ 20 FPS)
#define STOPWATCH_REFRESH_MS 50

/* Trích xuất tick hiện tại của hệ thống dưới dạng mili-giây */
static uint32_t stopwatch_now_ms(void) {
  return (uint32_t)(lv_tick_get());
}

/* Tính toán tổng thời gian đã trôi qua */
static uint32_t stopwatch_elapsed_ms(void) {
  if (!s_stopwatch_running) return s_stopwatch_accum_ms;
  return s_stopwatch_accum_ms + (stopwatch_now_ms() - s_stopwatch_start_tick);
}

/* Định dạng dữ liệu mili-giây thành chuỗi hiển thị HH:MM:SS.CS hoặc MM:SS.CS */
static void stopwatch_format_time(char *buf, size_t buf_size, uint32_t elapsed_ms) {
  uint32_t total_cs = elapsed_ms / 10;      // Đổi sang phần trăm giây (10ms)
  uint32_t cs = total_cs % 100;
  uint32_t total_sec = total_cs / 100;
  uint32_t sec = total_sec % 60;
  uint32_t min = (total_sec / 60) % 60;
  uint32_t hour = total_sec / 3600;

  if (hour > 0) {
    snprintf(buf, buf_size, "%lu:%02lu:%02lu.%02lu",
             (unsigned long)hour, (unsigned long)min,
             (unsigned long)sec, (unsigned long)cs);
  } else {
    snprintf(buf, buf_size, "%02lu:%02lu.%02lu",
             (unsigned long)min, (unsigned long)sec, (unsigned long)cs);
  }
}

/* Làm mới toàn bộ nhãn chữ và nút nhấn trên giao diện */
static void stopwatch_refresh_ui(void) {
  if (!s_stopwatch_time_label) return;

  uint32_t elapsed_ms = stopwatch_elapsed_ms();
  char time_buf[24];
  stopwatch_format_time(time_buf, sizeof(time_buf), elapsed_ms);
  ui_label_set_text_if_changed(s_stopwatch_time_label, time_buf);

  /* Cập nhật nhãn phụ chỉ thị số lượng vòng hoặc trạng thái */
  if (s_stopwatch_sub_label) {
    if (s_stopwatch_lap_count == 0) {
      ui_label_set_text_if_changed(s_stopwatch_sub_label,
                                   s_stopwatch_running ? ui_tr(UI_TXT_TIMING) : ui_tr(UI_TXT_READY));
    } else {
      char lap_buf[32];
      snprintf(lap_buf, sizeof(lap_buf), "%d %s", s_stopwatch_lap_count, ui_tr(UI_TXT_LAP));
      ui_label_set_text_if_changed(s_stopwatch_sub_label, lap_buf);
    }
  }

  /* Cập nhật nhãn của nút Bắt đầu (">") / Tạm dừng ("||") */
  if (s_stopwatch_start_label) {
    ui_label_set_text_if_changed(s_stopwatch_start_label,
                                 s_stopwatch_running ? "||" : ">");
  }
}

/* Callback gọi theo chu kỳ của bộ định thời đếm bấm giờ */
static void stopwatch_timer_cb(lv_timer_t *t) {
  (void)t;
  stopwatch_refresh_ui();
}

/* Xử lý sự kiện bấm nút Bắt đầu / Tạm dừng */
static void stopwatch_start_pause_cb(lv_event_t *e) {
  (void)e;
  if (s_stopwatch_running) {
    s_stopwatch_accum_ms = stopwatch_elapsed_ms();
    s_stopwatch_running = false;
  } else {
    s_stopwatch_start_tick = stopwatch_now_ms();
    s_stopwatch_running = true;
  }
  stopwatch_refresh_ui();
}

/* Xử lý sự kiện bấm nút Đặt lại (Reset) */
static void stopwatch_reset_cb(lv_event_t *e) {
  (void)e;
  s_stopwatch_running = false;
  s_stopwatch_start_tick = 0;
  s_stopwatch_accum_ms = 0;
  s_stopwatch_last_lap_ms = 0;
  s_stopwatch_lap_count = 0;
  if (s_stopwatch_lap_list) {
    lv_obj_clean(s_stopwatch_lap_list); // Xóa sạch các dòng danh sách vòng
  }
  stopwatch_refresh_ui();
}

/* Xử lý sự kiện bấm nút ghi nhận Vòng (Lap/Split) */
static void stopwatch_lap_cb(lv_event_t *e) {
  (void)e;
  if (!s_stopwatch_lap_list) return;

  uint32_t elapsed_ms = stopwatch_elapsed_ms();
  if (elapsed_ms == 0) return;

  /* Tính toán thời gian chênh lệch giữa 2 vòng kề nhau (Split time) */
  uint32_t split_ms = elapsed_ms - s_stopwatch_last_lap_ms;
  s_stopwatch_last_lap_ms = elapsed_ms;
  s_stopwatch_lap_count++;

  if (lv_obj_get_child_cnt(s_stopwatch_lap_list) >= STOPWATCH_MAX_VISIBLE_LAPS) {
    lv_obj_t *oldest = lv_obj_get_child(s_stopwatch_lap_list, 0);
    if (oldest) lv_obj_del(oldest);
  }

  char elapsed_buf[24];
  char split_buf[24];
  stopwatch_format_time(elapsed_buf, sizeof(elapsed_buf), elapsed_ms);
  stopwatch_format_time(split_buf, sizeof(split_buf), split_ms);

  /* Tạo một dòng item vòng mới trong list */
  lv_obj_t *row = lv_obj_create(s_stopwatch_lap_list);
  lv_obj_set_size(row, lv_pct(100), 38);
  lv_obj_set_style_bg_color(row, COLOR_SURFACE, 0);
  lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(row, 12, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_hor(row, 10, 0);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

  /* Cột 1: Chỉ số thứ tự vòng (#01, #02,...) */
  lv_obj_t *idx = lv_label_create(row);
  lv_label_set_text_fmt(idx, "#%02d", s_stopwatch_lap_count);
  lv_obj_set_style_text_color(idx, COLOR_TEXT_MUTED, 0);
  lv_obj_set_style_text_font(idx, UI_FONT_12, 0);
  lv_obj_align(idx, LV_ALIGN_LEFT_MID, 0, 0);

  /* Cột 2: Tổng thời gian tích lũy hiện hành (Split Time) */
  lv_obj_t *elapsed = lv_label_create(row);
  lv_label_set_text(elapsed, elapsed_buf);
  lv_obj_set_style_text_color(elapsed, COLOR_TEXT, 0);
  lv_obj_set_style_text_font(elapsed, UI_FONT_14, 0);
  lv_obj_align(elapsed, LV_ALIGN_CENTER, 8, 0);

  /* Cột 3: Hiệu thời gian riêng lẻ của vòng hiện hành (Lap Time) */
  lv_obj_t *split = lv_label_create(row);
  lv_label_set_text(split, split_buf);
  lv_obj_set_style_text_color(split, COLOR_PRIMARY, 0);
  lv_obj_set_style_text_font(split, UI_FONT_12, 0);
  lv_obj_align(split, LV_ALIGN_RIGHT_MID, 0, 0);

  /* Cuộn danh sách trượt xuống dòng cuối cùng vừa thêm mới */
  lv_obj_scroll_to_y(s_stopwatch_lap_list, LV_COORD_MAX, LV_ANIM_ON);
  stopwatch_refresh_ui();
}

/* Tiện ích tạo nút bấm hình tròn */
static lv_obj_t *stopwatch_make_circle_button(lv_obj_t *parent, const char *text,
                                              lv_color_t color, lv_coord_t size) {
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_set_size(btn, size, size);
  lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(btn, color, 0);
  lv_obj_set_style_border_width(btn, 0, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);

  lv_obj_t *label = lv_label_create(btn);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label, lv_color_white(), 0);
  lv_obj_set_style_text_font(label, UI_FONT_16, 0);
  lv_obj_center(label);
  return btn;
}

static void stopwatch_screen_delete_cb(lv_event_t *e) {
  (void)e;
  if (s_stopwatch_timer) {
    lv_timer_del(s_stopwatch_timer);
    s_stopwatch_timer = NULL;
  }
  s_stopwatch_time_label = NULL;
  s_stopwatch_sub_label = NULL;
  s_stopwatch_start_label = NULL;
  s_stopwatch_lap_list = NULL;
}

/* Khởi tạo đối tượng màn hình bấm giờ thể thao (Stopwatch Screen) */
lv_obj_t *ui_stopwatch_screen_create(void) {
  lv_obj_t *scr = ui_navigation_create_screen_with_id(UI_SCREEN_STOPWATCH);
  lv_obj_add_event_cb(scr, stopwatch_screen_delete_cb, LV_EVENT_DELETE, NULL);

  /* Tiêu đề góc trên */
  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, ui_tr(UI_TXT_STOPWATCH));
  lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
  lv_obj_set_style_text_font(title, UI_FONT_16, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 35);

  /* Số đếm thời gian chính */
  s_stopwatch_time_label = lv_label_create(scr);
  lv_label_set_text(s_stopwatch_time_label, "00:00.00");
  lv_obj_set_style_text_color(s_stopwatch_time_label, COLOR_TEXT, 0);
  lv_obj_set_style_text_font(s_stopwatch_time_label, UI_FONT_36, 0);
  lv_obj_align(s_stopwatch_time_label, LV_ALIGN_TOP_MID, 0, 68);

  /* Trạng thái đếm phụ bên dưới số đếm chính */
  s_stopwatch_sub_label = lv_label_create(scr);
  lv_label_set_text(s_stopwatch_sub_label, ui_tr(UI_TXT_READY));
  lv_obj_set_style_text_color(s_stopwatch_sub_label, COLOR_TEXT_MUTED, 0);
  lv_obj_set_style_text_font(s_stopwatch_sub_label, UI_FONT_12, 0);
  lv_obj_align(s_stopwatch_sub_label, LV_ALIGN_TOP_MID, 0, 112);

  /* Cụm điều khiển 3 nút nhấn (Flexbox layout) */
  lv_obj_t *controls = lv_obj_create(scr);
  lv_obj_set_size(controls, WATCH_LCD_H_RES - 24, 62);
  lv_obj_align(controls, LV_ALIGN_TOP_MID, 0, 132);
  lv_obj_set_style_bg_opa(controls, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(controls, 0, 0);
  lv_obj_set_style_pad_all(controls, 0, 0);
  lv_obj_clear_flag(controls, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(controls, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  /* Nút bấm Vòng (Lap) */
  lv_obj_t *lap_btn = stopwatch_make_circle_button(controls, ui_tr(UI_TXT_LAP), COLOR_CYAN, 54);
  lv_obj_add_event_cb(lap_btn, stopwatch_lap_cb, LV_EVENT_CLICKED, NULL);

  /* Nút bấm Chạy/Tạm dừng (Start/Pause) */
  lv_obj_t *start_btn = stopwatch_make_circle_button(controls, ">", COLOR_GREEN, 58);
  s_stopwatch_start_label = lv_obj_get_child(start_btn, 0);
  lv_obj_add_event_cb(start_btn, stopwatch_start_pause_cb, LV_EVENT_CLICKED, NULL);

  /* Nút bấm Đặt lại (Reset) */
  lv_obj_t *reset_btn = stopwatch_make_circle_button(controls, "R", COLOR_ORANGE, 54);
  lv_obj_add_event_cb(reset_btn, stopwatch_reset_cb, LV_EVENT_CLICKED, NULL);

  /* Danh sách cuộn trượt hiển thị lịch sử vòng chạy */
  s_stopwatch_lap_list = lv_obj_create(scr);
  lv_obj_set_size(s_stopwatch_lap_list, WATCH_LCD_H_RES - 24, WATCH_LCD_V_RES - 205);
  lv_obj_align(s_stopwatch_lap_list, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_obj_set_style_bg_opa(s_stopwatch_lap_list, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(s_stopwatch_lap_list, 0, 0);
  lv_obj_set_style_pad_all(s_stopwatch_lap_list, 4, 0);
  lv_obj_set_style_pad_row(s_stopwatch_lap_list, 6, 0);
  lv_obj_set_flex_flow(s_stopwatch_lap_list, LV_FLEX_FLOW_COLUMN);
  lv_obj_add_flag(s_stopwatch_lap_list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(s_stopwatch_lap_list, LV_SCROLLBAR_MODE_OFF);

  /* 20 Hz keeps the stopwatch smooth without continuously loading the LVGL task. */
  s_stopwatch_timer = lv_timer_create(stopwatch_timer_cb, STOPWATCH_REFRESH_MS, NULL);
  
  ui_navigation_add_gesture(scr);
  stopwatch_refresh_ui();
  return scr;
}
