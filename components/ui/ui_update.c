/* Thiết kế giao diện và quản lý tiến trình nâng cấp hệ thống qua mạng (OTA)
   Phân hệ OTA cho phép cập nhật phần sụn (firmware) trực tuyến qua kết nối Wi-Fi.
   Giao diện hiển thị động linh hoạt theo 2 kịch bản phần cứng:
   1. Chưa kết nối Wi-Fi: Hướng dẫn người dùng và chuyển tiếp sang màn hình cấu hình Wi-Fi.
   2. Đã có Wi-Fi: Đọc thông tin phiên bản phần sụn hiện tại qua esp_app_get_description(),
   thăm dò trạng thái tải từ ota_service, cập nhật thanh tiến trình (progress bar) và
   yêu cầu người dùng khởi động lại để kích hoạt phân vùng ảo (OTA partition) mới. */
#include "board_config.h"
#include "ui.h"
#include "ui_navigation.h"
#include "ui_screens.h"
#include "ui_utils.h"
#include "lvgl.h"
#include <stdbool.h>
#include "esp_app_desc.h"
#include "esp_system.h"
#include "network_state.h"
#include "ota_service.h"

LV_FONT_DECLARE(signal_icon);
LV_FONT_DECLARE(menu_icon_2);
LV_FONT_DECLARE(menu_icon);

#define ICON_SIGNAL_OK    "\xef\x80\x8c"
#define ICON_SIGNAL_ERROR "\xef\x81\xb1"
#define ICON_WIFI         "\xef\x87\xab"
#define ICON_MENU_UPDATE  "\xef\x80\xa1"

static lv_obj_t *s_update_icon_bg = NULL;
static lv_obj_t *s_update_icon_label = NULL;
static lv_obj_t *s_update_title_label = NULL;
static lv_obj_t *s_update_subtitle_label = NULL;
static lv_obj_t *s_update_action_button = NULL;
static lv_obj_t *s_update_button_label = NULL;
static lv_obj_t *s_update_progress_bar = NULL;
static lv_timer_t *s_update_timer = NULL;
static bool s_update_ready_to_restart = false;

/* Định dạng hiển thị dung lượng dữ liệu đã tải xuống (KB / Tổng dung lượng) */
static void update_format_bytes(char *buf, size_t size, int bytes_read, int image_size) {
  if (!buf || size == 0) return;
  if (image_size > 0) {
    snprintf(buf, size, "%d KB / %d KB", bytes_read / 1024, image_size / 1024);
  } else if (bytes_read > 0) {
    snprintf(buf, size, "%d KB", bytes_read / 1024);
  } else {
    snprintf(buf, size, "%s", ui_tr(UI_TXT_CONNECTING_SERVER));
  }
}

/* Cập nhật trạng thái tiến trình OTA lên giao diện đồ họa người dùng
   - Trạng thái RUNNING: Cập nhật tỷ lệ phần trăm hiển thị trên Progress Bar.
   - Trạng thái SUCCEEDED: Chuyển màu nền vòng tròn sang Xanh lá, đổi nút bấm sang KHỞI ĐỘNG LẠI.
   - Trạng thái FAILED: Chuyển màu nền vòng tròn sang Đỏ, báo lỗi và đổi nút sang THỬ LẠI. */
static void update_apply_ota_status(const watch_ota_status_t *st) {
  if (!st) return;

  if (st->state == WATCH_OTA_RUNNING) {
    int progress = st->progress_percent >= 0 ? st->progress_percent : 0;
    if (s_update_icon_bg) lv_obj_set_style_bg_color(s_update_icon_bg, COLOR_PRIMARY, 0);
    if (s_update_icon_label) {
      lv_label_set_text(s_update_icon_label, ICON_MENU_UPDATE);
      lv_obj_set_style_text_font(s_update_icon_label, &menu_icon, 0);
    }
    if (s_update_progress_bar) {
      lv_obj_clear_flag(s_update_progress_bar, LV_OBJ_FLAG_HIDDEN);
      lv_bar_set_value(s_update_progress_bar, progress, LV_ANIM_ON);
    }
    if (s_update_title_label) {
      char buf[16];
      if (st->progress_percent >= 0) snprintf(buf, sizeof(buf), "%d%%", progress);
      else snprintf(buf, sizeof(buf), "OTA");
      lv_label_set_text(s_update_title_label, buf);
      lv_obj_set_style_text_font(s_update_title_label, UI_FONT_36, 0);
    }
    if (s_update_subtitle_label) {
      char buf[40];
      update_format_bytes(buf, sizeof(buf), st->bytes_read, st->image_size);
      lv_label_set_text(s_update_subtitle_label, buf);
    }
    return;
  }

  if (st->state == WATCH_OTA_SUCCEEDED) {
    s_update_ready_to_restart = true;
    if (s_update_icon_bg) lv_obj_set_style_bg_color(s_update_icon_bg, COLOR_GREEN, 0);
    if (s_update_icon_label) lv_label_set_text(s_update_icon_label, ICON_SIGNAL_OK);
    if (s_update_icon_label) lv_obj_set_style_text_font(s_update_icon_label, &signal_icon, 0);
    if (s_update_title_label) {
      lv_label_set_text(s_update_title_label, ui_tr(UI_TXT_OTA_SUCCESS));
      lv_obj_set_style_text_font(s_update_title_label, UI_FONT_14, 0);
    }
    if (s_update_subtitle_label) {
      if (st->new_version[0]) lv_label_set_text_fmt(s_update_subtitle_label, "%s %s", ui_tr(UI_TXT_VERSION), st->new_version);
      else lv_label_set_text(s_update_subtitle_label, ui_tr(UI_TXT_READY_RESTART));
    }
    if (s_update_progress_bar) lv_obj_add_flag(s_update_progress_bar, LV_OBJ_FLAG_HIDDEN);
    if (s_update_action_button) {
      lv_obj_add_flag(s_update_action_button, LV_OBJ_FLAG_HIDDEN);
    }
    return;
  }

  if (st->state == WATCH_OTA_FAILED) {
    if (s_update_icon_bg) lv_obj_set_style_bg_color(s_update_icon_bg, COLOR_RED, 0);
    if (s_update_icon_label) lv_label_set_text(s_update_icon_label, ICON_SIGNAL_ERROR);
    if (s_update_icon_label) lv_obj_set_style_text_font(s_update_icon_label, &signal_icon, 0);
    if (s_update_title_label) {
      lv_label_set_text(s_update_title_label, ui_tr(UI_TXT_OTA_FAILED));
      lv_obj_set_style_text_font(s_update_title_label, UI_FONT_14, 0);
    }
    if (s_update_subtitle_label) lv_label_set_text(s_update_subtitle_label, st->message[0] ? st->message : ui_tr(UI_TXT_OTA_FAILED));
    if (s_update_progress_bar) lv_obj_add_flag(s_update_progress_bar, LV_OBJ_FLAG_HIDDEN);
    if (s_update_action_button) {
      lv_obj_clear_flag(s_update_action_button, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_style_bg_color(s_update_action_button, COLOR_PRIMARY, 0);
      if (s_update_button_label) lv_label_set_text(s_update_button_label, ui_tr(UI_TXT_RETRY));
    }
  }
}

/* Callback định thời thăm dò (polling) đọc trạng thái OTA từ ota_service */
static void update_poll_timer_cb(lv_timer_t *t) {
  watch_ota_status_t st;
  watch_ota_get_status(&st);
  update_apply_ota_status(&st);

  if (st.state == WATCH_OTA_SUCCEEDED || st.state == WATCH_OTA_FAILED) {
    lv_timer_del(t);
    s_update_timer = NULL;
  }
}

/* Callback nút bấm thực hiện hành động (Bắt đầu OTA / Khởi động lại / Thử lại) */
static void update_action_cb(lv_event_t *e) {
  (void)e;
  if (s_update_ready_to_restart) {
    esp_restart();
    return;
  }

  if (s_update_action_button) lv_obj_add_flag(s_update_action_button, LV_OBJ_FLAG_HIDDEN);
  if (s_update_title_label) {
    lv_label_set_text(s_update_title_label, "0%");
    lv_obj_set_style_text_font(s_update_title_label, UI_FONT_36, 0);
  }
  if (s_update_subtitle_label) lv_label_set_text(s_update_subtitle_label, ui_tr(UI_TXT_STARTING_OTA));
  if (s_update_progress_bar) {
    lv_obj_clear_flag(s_update_progress_bar, LV_OBJ_FLAG_HIDDEN);
    lv_bar_set_value(s_update_progress_bar, 0, LV_ANIM_OFF);
  }

  s_update_ready_to_restart = false;
  watch_ota_start();
  if (!s_update_timer) s_update_timer = lv_timer_create(update_poll_timer_cb, 300, NULL);
}

/* Callback điều hướng sang màn hình cài đặt mạng Wi-Fi */
static void update_open_wifi_cb(lv_event_t *e) {
  (void)e;
  ui_navigation_push(ui_wifi_screen_create());
}

/* Callback giải phóng bộ định thời thăm dò khi hủy màn hình cập nhật */
static void update_screen_delete_cb(lv_event_t *e) {
  (void)e;
  if (s_update_timer) {
    lv_timer_del(s_update_timer);
    s_update_timer = NULL;
  }
  s_update_icon_bg = NULL;
  s_update_icon_label = NULL;
  s_update_title_label = NULL;
  s_update_subtitle_label = NULL;
  s_update_action_button = NULL;
  s_update_button_label = NULL;
  s_update_progress_bar = NULL;
  s_update_ready_to_restart = false;
}

/* Khởi tạo màn hình kiểm tra cập nhật hệ thống OTA
   Trả về: lv_obj_t* Con trỏ đối tượng màn hình OTA */
lv_obj_t *ui_update_screen_create(void) {
  lv_obj_t *scr = ui_navigation_create_screen_with_id(UI_SCREEN_UPDATE);
  lv_obj_add_event_cb(scr, update_screen_delete_cb, LV_EVENT_DELETE, NULL);

  bool wifi_connected = watch_network_is_wifi_connected();

  if (!wifi_connected) {
    /* --- WAITING FOR WIFI SCREEN (Matching the image) --- */
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(scr, 15, 0);

    /* 1. Title */
    s_update_title_label = lv_label_create(scr);
    lv_label_set_text(s_update_title_label, ui_tr(UI_TXT_CONNECT_WIFI_TITLE));
    lv_obj_set_style_text_font(s_update_title_label, UI_FONT_16, 0);
    lv_obj_set_style_text_color(s_update_title_label, lv_color_white(), 0);

    lv_obj_t *ic_wf = lv_label_create(scr);
    lv_label_set_text(ic_wf, ICON_WIFI);
    lv_obj_set_style_text_font(ic_wf, &menu_icon_2, 0);
    lv_obj_set_style_text_color(ic_wf, COLOR_PRIMARY, 0);

    /* 3. Description */
    s_update_subtitle_label = lv_label_create(scr);
    lv_label_set_text(s_update_subtitle_label, ui_tr(UI_TXT_CONNECT_WIFI_OTA_DESC));
    lv_obj_set_style_text_align(s_update_subtitle_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(s_update_subtitle_label, UI_FONT_14, 0);
    lv_obj_set_style_text_color(s_update_subtitle_label, COLOR_TEXT_MUTED, 0);

    /* 4. Action Button */
    s_update_action_button = lv_btn_create(scr);
    lv_obj_set_size(s_update_action_button, 160, 44);
    lv_obj_set_style_radius(s_update_action_button, 12, 0);
    lv_obj_set_style_bg_color(s_update_action_button, lv_color_hex(0x003366), 0); /* Dark Blue */
    lv_obj_set_style_shadow_width(s_update_action_button, 0, 0);
    
    s_update_button_label = lv_label_create(s_update_action_button);
    lv_label_set_text(s_update_button_label, ui_tr(UI_TXT_CONNECT_WIFI));
    lv_obj_set_style_text_font(s_update_button_label, UI_FONT_14, 0);
    lv_obj_center(s_update_button_label);
    lv_obj_add_event_cb(s_update_action_button, update_open_wifi_cb, LV_EVENT_CLICKED, NULL);

  } else {
    const esp_app_desc_t *app_desc = esp_app_get_description();
    bool ota_configured = watch_ota_url_configured();

    s_update_icon_bg = lv_obj_create(scr);
    lv_obj_set_size(s_update_icon_bg, 80, 80);
    lv_obj_set_style_radius(s_update_icon_bg, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_update_icon_bg, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(s_update_icon_bg, 0, 0);
    lv_obj_clear_flag(s_update_icon_bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(s_update_icon_bg, LV_ALIGN_TOP_MID, 0, 20);

    s_update_icon_label = lv_label_create(s_update_icon_bg);
    lv_label_set_text(s_update_icon_label, ICON_MENU_UPDATE);
    lv_obj_set_style_text_font(s_update_icon_label, &menu_icon, 0);
    lv_obj_set_style_text_color(s_update_icon_label, COLOR_TEXT, 0);
    lv_obj_center(s_update_icon_label);

    s_update_title_label = lv_label_create(scr);
    if (ota_configured) {
      lv_label_set_text(s_update_title_label, ui_tr(UI_TXT_READY_OTA));
    } else {
      lv_label_set_text(s_update_title_label, ui_tr(UI_TXT_OTA_NOT_CONFIGURED));
    }
    lv_obj_set_style_text_align(s_update_title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(s_update_title_label, UI_FONT_16, 0);
    lv_obj_set_style_text_color(s_update_title_label, COLOR_TEXT, 0);
    lv_obj_align(s_update_title_label, LV_ALIGN_TOP_MID, 0, 110);

    s_update_subtitle_label = lv_label_create(scr);
    lv_label_set_text_fmt(s_update_subtitle_label, ui_tr(UI_TXT_CURRENT_VERSION), app_desc ? app_desc->version : "unknown");
    lv_obj_set_style_text_font(s_update_subtitle_label, UI_FONT_14, 0);
    lv_obj_set_style_text_color(s_update_subtitle_label, COLOR_TEXT_MUTED, 0);
    lv_obj_align(s_update_subtitle_label, LV_ALIGN_TOP_MID, 0, 150);

    s_update_progress_bar = lv_bar_create(scr);
    lv_obj_set_size(s_update_progress_bar, 170, 8);
    lv_bar_set_range(s_update_progress_bar, 0, 100);
    lv_bar_set_value(s_update_progress_bar, 0, LV_ANIM_OFF);
    lv_obj_align(s_update_progress_bar, LV_ALIGN_TOP_MID, 0, 180);
    lv_obj_add_flag(s_update_progress_bar, LV_OBJ_FLAG_HIDDEN);

    s_update_action_button = lv_btn_create(scr);
    lv_obj_set_size(s_update_action_button, 160, 44);
    lv_obj_set_style_radius(s_update_action_button, 22, 0);
    lv_obj_set_style_bg_color(s_update_action_button, COLOR_PRIMARY, 0);
    lv_obj_add_event_cb(s_update_action_button, update_action_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(s_update_action_button, LV_ALIGN_BOTTOM_MID, 0, -20);
    if (!ota_configured) {
      lv_obj_add_state(s_update_action_button, LV_STATE_DISABLED);
      lv_obj_set_style_bg_color(s_update_action_button, COLOR_TEXT_MUTED, LV_STATE_DISABLED);
    }
    
    s_update_button_label = lv_label_create(s_update_action_button);
    lv_label_set_text(s_update_button_label, ota_configured ? ui_tr(UI_TXT_UPDATE) : ui_tr(UI_TXT_MISSING_URL));
    lv_obj_set_style_text_font(s_update_button_label, UI_FONT_14, 0);
    lv_obj_center(s_update_button_label);
  }

  ui_navigation_add_gesture(scr);
  return scr;
}
