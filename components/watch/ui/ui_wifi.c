/* Thiết kế giao diện và quản lý logic quét, kết nối mạng Wi-Fi
   Phân hệ Wi-Fi được cấu hình tối ưu để chỉ hoạt động phục vụ quá trình nâng cấp hệ thống (OTA).
   Các tính năng chính bao gồm:
   1. Quét mạng Wi-Fi chạy nền: Sử dụng FreeRTOS Task độc lập để tránh gây nghẽn luồng xử lý giao diện LVGL chính.
   2. Đồng bộ bất đồng bộ (Asynchronous Callback): Truyền kết quả quét từ nhân mạng của ESP-IDF vào hàng đợi vẽ của LVGL qua lv_async_call được bảo vệ bởi port lock.
   3. Bàn phím ảo gõ mật khẩu tùy chỉnh (Custom Keyboard): Lưới phím 4x4 hỗ trợ Caps Lock và phím chuyển đổi số/ký tự đặc biệt.
   4. Trạng thái kết nối thời gian thực: Hiển thị tiến trình bắt tay, cấp phát địa chỉ IP tĩnh/động từ DHCP server. */
#include "board_config.h"
#include "ui.h"
#include "ui_utils.h"
#include "ui_navigation.h"
#include "ui_screens.h"
#include "lvgl.h"
#include "network_state.h"
#include "esp_log.h"

LV_FONT_DECLARE(menu_icon_2);
LV_FONT_DECLARE(wifi_24);
LV_FONT_DECLARE(signal_icon);

#define ICON_WIFI "\xef\x87\xab"
#define ICON_OK   "\xef\x80\x8c"
#define ICON_CLOSE "\xef\x80\x8d"
#include "esp_netif.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include "esp_err.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Độ dài mật khẩu Wi-Fi tối đa (theo chuẩn WPA2)
#define MAX_PASS 64

// Số lượng mạng hiển thị tối đa trên danh sách UI
#define MAX_WIFI_SCAN_RESULTS 8

// Dung lượng tối đa lưu danh sách quét Wi-Fi
#define WIFI_SCAN_RECORD_CAPACITY 32

#define WIFI_SCAN_ITEMS_PER_TICK 1

// Chu kỳ nhịp đếm vẽ danh sách quét
#define WIFI_SCAN_TIMER_MS 60

// Ngăn xếp tác vụ quét Wi-Fi nền
#define WIFI_SCAN_TASK_STACK_SIZE 4096

// Tần suất thăm dò trạng thái kết nối
#define WIFI_CONNECT_POLL_MS 250

// Thời gian giữ thông tin kết quả trên màn hình
#define WIFI_CONNECT_RESULT_HOLD_MS 1200

// Thời gian chờ kết nối Wi-Fi timeout
#define WIFI_CONNECT_TIMEOUT_MS 25000

/* Password entry state for the current password screen */
static char s_wifi_password_buf[MAX_PASS + 1];
static int s_wifi_password_len = 0;
static lv_obj_t *s_wifi_password_label = NULL;
static lv_obj_t *s_wifi_keyboard_container = NULL;
static lv_obj_t *s_wifi_caps_button = NULL;
static lv_obj_t *s_wifi_mode_button = NULL;
static lv_obj_t *s_wifi_delete_button = NULL;
static lv_obj_t *s_wifi_enter_button = NULL;
static int s_wifi_key_page = 0; /* 0=numbers, 1..3 alphabet pages */
static bool s_wifi_key_upper = false;
static char s_wifi_current_ssid[33] = {0};
static lv_obj_t *s_wifi_list_screen = NULL;
static lv_obj_t *s_wifi_list_area = NULL;
static bool s_wifi_scan_enabled = false;
static char *s_wifi_cached_ssids[MAX_WIFI_SCAN_RESULTS];
static int s_wifi_cached_count = 0;
static portMUX_TYPE s_wifi_task_lock = portMUX_INITIALIZER_UNLOCKED;
static bool s_scan_in_progress = false;
static bool s_connect_in_progress = false;

/* Shared styles to avoid per-object local style allocations (reduce heap usage) */
static lv_style_t s_wifi_card_style;
static lv_style_t s_wifi_title_style;
static lv_style_t s_wifi_button_style;
static bool s_wifi_styles_initialized = false;

/* Khởi tạo các Style hiển thị dùng chung cho giao diện Wi-Fi để giảm tải heap RAM */
static void wifi_init_styles(void)
{
  if (s_wifi_styles_initialized) return;
  s_wifi_styles_initialized = true;

  lv_style_init(&s_wifi_card_style);
  lv_style_set_bg_color(&s_wifi_card_style, COLOR_SURFACE);
  lv_style_set_bg_opa(&s_wifi_card_style, LV_OPA_COVER);
  lv_style_set_radius(&s_wifi_card_style, 12);
  lv_style_set_border_width(&s_wifi_card_style, 0);
  lv_style_set_shadow_width(&s_wifi_card_style, 0);
  lv_style_set_shadow_opa(&s_wifi_card_style, LV_OPA_TRANSP);
  lv_style_set_outline_width(&s_wifi_card_style, 0);
  lv_style_set_pad_all(&s_wifi_card_style, 10);
  lv_style_set_pad_column(&s_wifi_card_style, 8);

  lv_style_init(&s_wifi_title_style);
  lv_style_set_text_font(&s_wifi_title_style, UI_FONT_14);
  lv_style_set_text_color(&s_wifi_title_style, COLOR_TEXT);

  lv_style_init(&s_wifi_button_style);
  lv_style_set_bg_color(&s_wifi_button_style, COLOR_PRIMARY);
  lv_style_set_bg_opa(&s_wifi_button_style, LV_OPA_COVER);
  lv_style_set_radius(&s_wifi_button_style, 18);
  lv_style_set_border_width(&s_wifi_button_style, 0);
  lv_style_set_shadow_width(&s_wifi_button_style, 0);
  lv_style_set_shadow_opa(&s_wifi_button_style, LV_OPA_TRANSP);
  lv_style_set_outline_width(&s_wifi_button_style, 0);
}

/* Giải phóng bộ nhớ của các Style Wi-Fi khi tắt phân hệ để giải phóng RAM */
void ui_wifi_deinit(void)
{
  if (!s_wifi_styles_initialized) return;

  lv_style_reset(&s_wifi_card_style);
  lv_style_reset(&s_wifi_title_style);
  lv_style_reset(&s_wifi_button_style);
  s_wifi_styles_initialized = false;
}

/* Forward declarations for event callbacks (must appear before usage) */
static void wifi_keyboard_key_cb(lv_event_t *e);
static void wifi_open_password_cb(lv_event_t *e);
static void wifi_card_delete_cb(lv_event_t *e);
static void wifi_keyboard_delete_cb(lv_event_t *e);
static void wifi_keyboard_caps_cb(lv_event_t *e);
static void wifi_keyboard_mode_cb(lv_event_t *e);
static void wifi_keyboard_enter_cb(lv_event_t *e);
static void wifi_password_screen_open(const char *ssid);
static void wifi_status_screen_open(const char *ssid, const char *password);
static lv_obj_t *wifi_make_network_item(lv_obj_t *par, const char *ssid);
static void wifi_refresh_network_cards(void);

/* Thử kích hoạt cờ quét Wi-Fi độc quyền (Mutex Lock)
   Tránh trường hợp người dùng click quét liên tục tạo nhiều Task chồng chéo gây tràn RAM. */
static bool wifi_try_begin_scan(void) {
  bool started = false;
  taskENTER_CRITICAL(&s_wifi_task_lock);
  if (!s_scan_in_progress) {
    s_scan_in_progress = true;
    started = true;
  }
  taskEXIT_CRITICAL(&s_wifi_task_lock);
  return started;
}

static void wifi_end_scan(void) {
  taskENTER_CRITICAL(&s_wifi_task_lock);
  s_scan_in_progress = false;
  taskEXIT_CRITICAL(&s_wifi_task_lock);
}

/* Thử kích hoạt cờ kết nối Wi-Fi độc quyền (Mutex Lock) */
static bool wifi_try_begin_connect(void) {
  bool started = false;
  taskENTER_CRITICAL(&s_wifi_task_lock);
  if (!s_connect_in_progress) {
    s_connect_in_progress = true;
    started = true;
  }
  taskEXIT_CRITICAL(&s_wifi_task_lock);
  return started;
}

static void wifi_end_connect(void) {
  taskENTER_CRITICAL(&s_wifi_task_lock);
  s_connect_in_progress = false;
  taskEXIT_CRITICAL(&s_wifi_task_lock);
}

static void wifi_clear_cached_scan(void) {
  for (int i = 0; i < s_wifi_cached_count; i++) {
    free(s_wifi_cached_ssids[i]);
    s_wifi_cached_ssids[i] = NULL;
  }
  s_wifi_cached_count = 0;
}

static void wifi_cache_scan_result(char **ssids, int count) {
  wifi_clear_cached_scan();
  if (!ssids || count <= 0) return;
  if (count > MAX_WIFI_SCAN_RESULTS) count = MAX_WIFI_SCAN_RESULTS;
  for (int i = 0; i < count; i++) {
    if (!ssids[i]) continue;
    s_wifi_cached_ssids[s_wifi_cached_count] = ui_safe_strdup(ssids[i]);
    if (s_wifi_cached_ssids[s_wifi_cached_count]) s_wifi_cached_count++;
  }
}

/* Vẽ lại danh sách Wi-Fi từ bộ đệm đệm lưu trước đó */
static void wifi_render_cached_scan(lv_obj_t *list_area) {
  if (!list_area || !lv_obj_is_valid(list_area)) return;
  lv_obj_clean(list_area);
  for (int i = 0; i < s_wifi_cached_count; i++) {
    if (s_wifi_cached_ssids[i]) wifi_make_network_item(list_area, s_wifi_cached_ssids[i]);
  }
  wifi_refresh_network_cards();
}

static void wifi_list_screen_delete_cb(lv_event_t *e) {
  (void)e;
  s_wifi_list_screen = NULL;
  s_wifi_list_area = NULL;
  if (!watch_network_is_wifi_connected()) {
    ESP_ERROR_CHECK_WITHOUT_ABORT(watch_network_wifi_stop());
  }
}

static void wifi_password_screen_delete_cb(lv_event_t *e) {
  (void)e;
  s_wifi_password_label = NULL;
  s_wifi_keyboard_container = NULL;
  s_wifi_caps_button = NULL;
  s_wifi_mode_button = NULL;
  s_wifi_delete_button = NULL;
  s_wifi_enter_button = NULL;
  s_wifi_current_ssid[0] = '\0';
  s_wifi_password_len = 0;
  s_wifi_password_buf[0] = '\0';
}

/* Background scan helpers for non-blocking Wi-Fi list population */
typedef struct {
  lv_obj_t *screen;
  lv_obj_t *list_area;
  lv_obj_t *scanning_lbl;
} wifi_scan_request_t;

typedef struct {
  char *ssid;
  char *password;
} wifi_connect_request_t;

typedef struct {
  lv_obj_t *screen;
  lv_obj_t *list_area;
  char **ssids;
  int count;
  esp_err_t err;
} wifi_scan_result_t;

typedef struct {
  lv_obj_t *screen;
  lv_obj_t *list_area;
  char **ssids;
  int count;
  int idx;
} wifi_scan_iterator_t;

static void wifi_free_scan_result(char **ssids, int count);
static const char *wifi_get_card_ssid(lv_obj_t *target);

typedef struct {
  lv_obj_t *screen;
  lv_obj_t *status_label;
  lv_timer_t *timer;
  uint32_t elapsed_ms;
  uint32_t result_ms;
  bool result_shown;
  bool closed;
  bool connect_started;
  char ssid[33];
  char password[65];
} wifi_connect_status_ctx_t;

static char *wifi_make_safe_display_text(const char *ssid) {
  return ssid ? ui_safe_strdup(ssid) : NULL;
}

static esp_err_t wifi_connect_run_request(wifi_connect_request_t *param) {
  if (!param) {
    wifi_end_connect();
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t err = watch_network_wifi_connect(param->ssid, param->password);
  free(param->ssid);
  free(param->password);
  free(param);
  wifi_end_connect();
  return err;
}

static void wifi_connect_worker_task(void *arg) {
  (void)wifi_connect_run_request((wifi_connect_request_t *)arg);
  vTaskDelete(NULL);
}

/* Khởi tạo Task FreeRTOS chạy nền để thực hiện bắt tay kết nối AP Wi-Fi
   Giúp việc bắt tay WPA2 diễn ra ngầm mà không gây đơ giao diện đồng hồ. */
static esp_err_t wifi_start_connect_worker(const char *ssid, const char *password) {
  if (!wifi_try_begin_connect()) return ESP_ERR_INVALID_STATE;

  wifi_connect_request_t *param = malloc(sizeof(wifi_connect_request_t));
  if (!param) {
    wifi_end_connect();
    return ESP_ERR_NO_MEM;
  }

  param->ssid = ui_safe_strdup(ssid);
  param->password = password ? ui_safe_strdup(password) : NULL;
  if (!param->ssid || (password && !param->password)) {
    free(param->ssid);
    free(param->password);
    free(param);
    wifi_end_connect();
    return ESP_ERR_NO_MEM;
  }

  BaseType_t ok = xTaskCreate(wifi_connect_worker_task, "wifi_conn", 4096, param, 3, NULL);
  if (ok != pdPASS) {
    ESP_LOGW("UI_WIFI", "WiFi connect task create failed");
    free(param->ssid);
    free(param->password);
    free(param);
    wifi_end_connect();
    return ESP_ERR_NO_MEM;
  }

  return ESP_OK;
}

static void wifi_free_scan_result(char **ssids, int count) {
  if (!ssids) return;
  for (int i = 0; i < count; i++) {
    if (ssids[i]) free(ssids[i]);
  }
  free(ssids);
}

static const char *wifi_get_card_ssid(lv_obj_t *target) {
  if (!target || !lv_obj_is_valid(target)) return NULL;

  lv_obj_t *card = target;
  if (!lv_obj_check_type(card, &lv_btn_class) && lv_obj_get_parent(target)) {
    card = lv_obj_get_parent(target);
  }

  /* Children: wifi_icon(0), title(1), badge(2), raw(3) */
  lv_obj_t *raw = lv_obj_get_child(card, 3);
  if (raw && lv_obj_check_type(raw, &lv_label_class)) {
    return lv_label_get_text(raw);
  }

  lv_obj_t *title = lv_obj_get_child(card, 0);
  if (!title || !lv_obj_check_type(title, &lv_label_class)) return NULL;
  return lv_label_get_text(title);
}

/* Cập nhật màu sắc và trạng thái biểu tượng (vẽ dấu tích) cho phần tử mạng đang kết nối */
static void wifi_update_card_state(lv_obj_t *card) {
  if (!card || !lv_obj_is_valid(card)) return;

  const char *ssid = wifi_get_card_ssid(card);
  /* Children: wifi_icon(0), title(1), badge(2), raw(3) */
  lv_obj_t *wifi_icon = lv_obj_get_child(card, 0);
  lv_obj_t *title = lv_obj_get_child(card, 1);
  lv_obj_t *badge = lv_obj_get_child(card, 2);
  const bool connected = watch_network_is_connected_ssid(ssid);

  lv_obj_set_style_bg_color(card, connected ? COLOR_PRIMARY : COLOR_SURFACE, 0);
  lv_obj_set_style_bg_opa(card, connected ? LV_OPA_10 : LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(card, connected ? 1 : 0, 0);
  lv_obj_set_style_border_color(card, COLOR_PRIMARY, 0);

  if (wifi_icon) {
    lv_obj_set_style_text_color(wifi_icon, connected ? COLOR_PRIMARY : COLOR_TEXT_MUTED, 0);
  }

  if (title) {
    lv_obj_set_style_text_color(title, connected ? COLOR_PRIMARY : COLOR_TEXT, 0);
  }

  if (badge) {
    lv_label_set_text(badge, connected ? ICON_OK : "");
    lv_obj_set_style_text_color(badge, connected ? COLOR_PRIMARY : COLOR_TEXT_MUTED, 0);
  }
}

static void wifi_refresh_network_cards(void) {
  if (!s_wifi_list_area || !lv_obj_is_valid(s_wifi_list_area)) return;

  uint32_t child_count = lv_obj_get_child_cnt(s_wifi_list_area);
  for (uint32_t i = 0; i < child_count; i++) {
    lv_obj_t *child = lv_obj_get_child(s_wifi_list_area, i);
    if (child && lv_obj_check_type(child, &lv_btn_class)) {
      wifi_update_card_state(child);
    }
  }
}

static void wifi_status_ctx_delete(wifi_connect_status_ctx_t *ctx) {
  if (!ctx) return;
  lv_timer_t *timer = ctx->timer;
  ctx->timer = NULL;
  if (timer) lv_timer_del(timer);
  free(ctx);
}

static void wifi_status_screen_delete_cb(lv_event_t *e) {
  wifi_connect_status_ctx_t *ctx = lv_event_get_user_data(e);
  if (!ctx || ctx->closed) return;
  ctx->closed = true;
  wifi_status_ctx_delete(ctx);
}

/* Định thời định kỳ thăm dò trạng thái kết nối Wi-Fi
   Đọc trạng thái từ net_state. Nếu kết nối thành công, hiển thị địa chỉ IP lấy từ DHCP.
   Nếu thất bại hoặc hết thời gian chờ (Timeout), báo lỗi và quay lại. */
static void wifi_status_poll_timer_cb(lv_timer_t *t) {
  wifi_connect_status_ctx_t *ctx = (wifi_connect_status_ctx_t *)t->user_data;
  if (!ctx || !ctx->screen || !lv_obj_is_valid(ctx->screen) || !ctx->status_label || !lv_obj_is_valid(ctx->status_label)) {
    wifi_status_ctx_delete(ctx);
    return;
  }
  ctx->timer = t;

  if (!ctx->connect_started) {
    ctx->connect_started = true;
    esp_err_t err = wifi_start_connect_worker(ctx->ssid, ctx->password);
    if (err != ESP_OK) {
      ESP_LOGE("UI_WIFI", "Failed to start WiFi connection: %s", esp_err_to_name(err));
      lv_label_set_text(ctx->status_label, ui_text("Cannot start connection", "Không thể bắt đầu kết nối"));
      lv_obj_set_style_text_color(ctx->status_label, COLOR_RED, 0);
      ctx->result_shown = true;
      ctx->result_ms = 0;
    }
    return;
  }

  /* Layout: title(0), circle_bg(1), ssid_lbl(2), status_label(3) */
  lv_obj_t *circle_bg = lv_obj_get_child(ctx->screen, 1);
  lv_obj_t *wifi_icon = circle_bg ? lv_obj_get_child(circle_bg, 0) : NULL;

  if (!ctx->result_shown) {
    watch_network_wifi_connect_state_t state = watch_network_get_wifi_connect_state();
    if (state == WATCH_NETWORK_WIFI_CONNECT_SUCCESS) {
      lv_label_set_text(ctx->status_label, ui_tr(UI_TXT_CONNECTED));
      lv_obj_set_style_text_color(ctx->status_label, COLOR_TEXT, 0);
      if (circle_bg) lv_obj_set_style_bg_color(circle_bg, COLOR_GREEN, 0);
      if (wifi_icon) lv_label_set_text(wifi_icon, ICON_OK);
      if (wifi_icon) lv_obj_set_style_text_font(wifi_icon, &signal_icon, 0);
      
      /* Show IP address */
      lv_obj_t *ip_label = lv_label_create(ctx->screen);
      esp_netif_ip_info_t ip_info;
      esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
      if (sta && esp_netif_get_ip_info(sta, &ip_info) == ESP_OK) {
        char ip_buf[32];
        snprintf(ip_buf, sizeof(ip_buf), "IP: " IPSTR, IP2STR(&ip_info.ip));
        lv_label_set_text(ip_label, ip_buf);
      } else {
        lv_label_set_text(ip_label, "IP: --");
      }
      lv_obj_set_style_text_font(ip_label, UI_FONT_14, 0);
      lv_obj_set_style_text_color(ip_label, COLOR_TEXT_MUTED, 0);
      lv_obj_align(ip_label, LV_ALIGN_CENTER, 0, 60);

      wifi_refresh_network_cards();
      ctx->result_shown = true;
      ctx->result_ms = 0;
    } else if (state == WATCH_NETWORK_WIFI_CONNECT_FAILED) {
      lv_label_set_text(ctx->status_label, ui_text("Connection failed", "Kết nối thất bại"));
      lv_obj_set_style_text_color(ctx->status_label, COLOR_RED, 0);
      if (circle_bg) lv_obj_set_style_bg_color(circle_bg, COLOR_RED, 0);
      if (wifi_icon) lv_label_set_text(wifi_icon, ICON_CLOSE);
      if (wifi_icon) lv_obj_set_style_text_font(wifi_icon, &signal_icon, 0);
      ctx->result_shown = true;
      ctx->result_ms = 0;
    } else {
      ctx->elapsed_ms += WIFI_CONNECT_POLL_MS;
      if (ctx->elapsed_ms >= WIFI_CONNECT_TIMEOUT_MS) {
        lv_label_set_text(ctx->status_label, ui_text("Connection failed", "Kết nối thất bại"));
        lv_obj_set_style_text_color(ctx->status_label, COLOR_RED, 0);
        if (circle_bg) lv_obj_set_style_bg_color(circle_bg, COLOR_RED, 0);
        if (wifi_icon) lv_label_set_text(wifi_icon, ICON_CLOSE);
        if (wifi_icon) lv_obj_set_style_text_font(wifi_icon, &signal_icon, 0);
        watch_network_clear_wifi_connect_state();
        ctx->result_shown = true;
        ctx->result_ms = 0;
      }
    }
  } else {
    ctx->result_ms += WIFI_CONNECT_POLL_MS;
    if (ctx->result_ms >= WIFI_CONNECT_RESULT_HOLD_MS) {
      ctx->closed = true;
      ui_navigation_pop_multiple(2);
      wifi_status_ctx_delete(ctx);
    }
  }
}

/* Mở màn hình hiển thị trạng thái đang kết nối Wi-Fi */
static void wifi_status_screen_open(const char *ssid, const char *password) {
  if (!ssid) return;

  lv_obj_t *scr = ui_navigation_create_screen();

  /* Header */
  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, ui_tr(UI_TXT_CONNECT_WIFI));
  lv_obj_set_style_text_font(title, UI_FONT_16, 0);
  lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
  lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

  /* WiFi icon */
  lv_obj_t *wifi_icon = lv_label_create(scr);
  lv_label_set_text(wifi_icon, ICON_WIFI);
  lv_obj_set_style_text_font(wifi_icon, &wifi_24, 0);
  lv_obj_set_style_text_color(wifi_icon, COLOR_GREEN, 0);
  lv_obj_align(wifi_icon, LV_ALIGN_CENTER, 0, -40);

  /* SSID name */
  char *display_ssid = wifi_make_safe_display_text(ssid);
  lv_obj_t *ssid_lbl = lv_label_create(scr);
  lv_label_set_text(ssid_lbl, display_ssid ? display_ssid : ssid);
  lv_obj_set_style_text_font(ssid_lbl, UI_FONT_16, 0);
  lv_obj_set_style_text_color(ssid_lbl, COLOR_TEXT, 0);
  lv_obj_align(ssid_lbl, LV_ALIGN_CENTER, 0, -3);

  /* Status label (changes to ui_tr(UI_TXT_CONNECTED) or "That bai") */
  lv_obj_t *status_label = lv_label_create(scr);
  lv_label_set_text(status_label, ui_tr(UI_TXT_CONNECTING));
  lv_obj_set_style_text_font(status_label, UI_FONT_14, 0);
  lv_obj_set_style_text_color(status_label, COLOR_TEXT_MUTED, 0);
  lv_obj_align(status_label, LV_ALIGN_CENTER, 0, 20);

  ui_navigation_push(scr);

  wifi_connect_status_ctx_t *ctx = malloc(sizeof(wifi_connect_status_ctx_t));
  if (ctx) {
    ctx->screen = scr;
    ctx->status_label = status_label;
    ctx->timer = NULL;
    ctx->elapsed_ms = 0;
    ctx->result_ms = 0;
    ctx->result_shown = false;
    ctx->closed = false;
    ctx->connect_started = false;
    strncpy(ctx->ssid, ssid, sizeof(ctx->ssid) - 1);
    ctx->ssid[sizeof(ctx->ssid) - 1] = '\0';
    if (password) {
      strncpy(ctx->password, password, sizeof(ctx->password) - 1);
      ctx->password[sizeof(ctx->password) - 1] = '\0';
    } else {
      ctx->password[0] = '\0';
    }
    ctx->timer = lv_timer_create(wifi_status_poll_timer_cb, WIFI_CONNECT_POLL_MS, ctx);
    if (ctx->timer) {
      lv_obj_add_event_cb(scr, wifi_status_screen_delete_cb, LV_EVENT_DELETE, ctx);
    } else {
      free(ctx);
      ctx = NULL;
    }
  }

  free(display_ssid);
}

static void wifi_scan_item_timer_cb(lv_timer_t *t) {
  wifi_scan_iterator_t *it = (wifi_scan_iterator_t *)t->user_data;
  if (!it) {
    lv_timer_del(t);
    return;
  }

  if (!it->list_area || !lv_obj_is_valid(it->list_area) ||
      (it->screen && !lv_obj_is_valid(it->screen))) {
    wifi_free_scan_result(it->ssids, it->count);
    free(it);
    lv_timer_del(t);
    return;
  }

  for (int batch = 0; batch < WIFI_SCAN_ITEMS_PER_TICK && it->idx < it->count; batch++) {
    if (it->ssids && it->ssids[it->idx]) {
      wifi_make_network_item(it->list_area, it->ssids[it->idx]);
      free(it->ssids[it->idx]);
      it->ssids[it->idx] = NULL;
    }
    it->idx++;
  }

  if (it->idx >= it->count) {
    /* Free pending SSID strings and array, then delete iterator and timer */
    wifi_free_scan_result(it->ssids, it->count);
    free(it);
    lv_timer_del(t);
  }
}

static void wifi_scan_worker_task(void *arg);
static void wifi_connect_worker_task(void *arg);
static void wifi_scan_complete_async_cb(void *data);
static void wifi_update_keyboard_control_labels(void);
static lv_obj_t *wifi_keyboard_make_button(lv_obj_t *parent, lv_coord_t width, lv_coord_t height, lv_color_t bg, const char *text);

/* Cập nhật hiển thị chuỗi mật khẩu đang nhập với ký tự cuối hiện rõ, các ký tự trước dạng * */
static void wifi_password_update_label(void) {
  if (!s_wifi_password_label) return;
  char preview[MAX_PASS + 1];
  if (s_wifi_password_len <= 0) {
    preview[0] = '\0';
  } else {
    for (int i = 0; i < s_wifi_password_len - 1; i++) preview[i] = '*';
    preview[s_wifi_password_len - 1] = s_wifi_password_buf[s_wifi_password_len - 1];
    preview[s_wifi_password_len] = '\0';
  }
  lv_label_set_text(s_wifi_password_label, preview);
}

static lv_obj_t *wifi_keyboard_make_button(lv_obj_t *parent, lv_coord_t width, lv_coord_t height, lv_color_t bg, const char *text) {
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

static void wifi_update_keyboard_control_labels(void) {
  if (s_wifi_mode_button) {
    lv_obj_t *lbl = lv_obj_get_child(s_wifi_mode_button, 0);
    if (lbl) {
      if (s_wifi_key_page == 0) lv_label_set_text(lbl, "abc");
      else if (s_wifi_key_page == 4) lv_label_set_text(lbl, "123");
      else lv_label_set_text(lbl, "#");
    }
  }

  if (s_wifi_caps_button) {
    lv_obj_t *lbl = lv_obj_get_child(s_wifi_caps_button, 0);
    if (lbl) {
      lv_label_set_text(lbl, s_wifi_key_upper ? "a" : "A");
    }
    lv_obj_set_style_bg_color(s_wifi_caps_button, s_wifi_key_upper ? COLOR_ORANGE : COLOR_PURPLE, 0);
  }

  if (s_wifi_delete_button) {
    lv_obj_t *lbl = lv_obj_get_child(s_wifi_delete_button, 0);
    if (lbl) lv_label_set_text(lbl, "<-");
  }

  if (s_wifi_enter_button) {
    lv_obj_t *lbl = lv_obj_get_child(s_wifi_enter_button, 0);
    if (lbl) lv_label_set_text(lbl, "OK");
  }
}

/* Vẽ lại các phím bấm trên bàn phím ảo theo trang ký tự hiện tại */
static void wifi_rebuild_keyboard(void) {
  if (!s_wifi_keyboard_container) return;
  const lv_coord_t key_w = 52;  /* To hon (cu la 46) */
  const lv_coord_t key_h = 36;  /* To hon (cu la 31) */
  const lv_coord_t key_gap = 2; /* Hep lai de phim to hon */

  lv_obj_clean(s_wifi_keyboard_container);
  s_wifi_mode_button = NULL;
  s_wifi_caps_button = NULL;
  s_wifi_delete_button = NULL;
  s_wifi_enter_button = NULL;
  lv_obj_set_style_pad_all(s_wifi_keyboard_container, 0, 0);

  static const char *num_keys[12] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "@", "0", "."};
  static const char *alpha_pages[3][12] = {
      {"a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l"},
      {"m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x"},
      {"y", "z", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
  };
  static const char *symbol_keys[12] = {"#", "$", "!", "%", "-", "_", "&", "*", "+", "/", "?", ":"};
  const char *const *keys = num_keys;
  if (s_wifi_key_page >= 1 && s_wifi_key_page <= 3) {
    keys = alpha_pages[s_wifi_key_page - 1];
  } else if (s_wifi_key_page == 4) {
    keys = symbol_keys;
  }

  for (int row_idx = 0; row_idx < 4; row_idx++) {
    for (int col_idx = 0; col_idx < 3; col_idx++) {
      int key_idx = row_idx * 3 + col_idx;
      const char *key_text = keys[key_idx];
      if (!key_text) continue;

      lv_obj_t *b = wifi_keyboard_make_button(s_wifi_keyboard_container, key_w, key_h, COLOR_PRIMARY, key_text);
      if (!b) continue;
      lv_obj_set_pos(b, col_idx * (key_w + key_gap), row_idx * (key_h + key_gap));
      lv_obj_t *l = lv_obj_get_child(b, 0);
      if (!l) continue;

      char key_buf[2] = {0};
      if (s_wifi_key_page >= 1 && s_wifi_key_page <= 3) {
        key_buf[0] = s_wifi_key_upper ? (char)toupper((unsigned char)key_text[0]) : key_text[0];
        lv_label_set_text(l, key_buf);
      }
      lv_obj_add_event_cb(b, wifi_keyboard_key_cb, LV_EVENT_CLICKED, NULL);
    }

    lv_color_t special_color = COLOR_CYAN;
    if (row_idx == 1) special_color = COLOR_PURPLE;
    else if (row_idx == 2) special_color = COLOR_RED;
    else if (row_idx == 3) special_color = COLOR_GREEN;

    lv_obj_t *special = wifi_keyboard_make_button(s_wifi_keyboard_container, key_w, key_h, special_color, "");
    if (!special) continue;
    lv_obj_set_pos(special, 3 * (key_w + key_gap), row_idx * (key_h + key_gap));
    lv_obj_t *sl = lv_obj_get_child(special, 0);
    if (sl) lv_obj_set_style_text_font(sl, UI_FONT_14, 0);

    if (row_idx == 0) {
      s_wifi_mode_button = special;
      lv_obj_add_event_cb(s_wifi_mode_button, wifi_keyboard_mode_cb, LV_EVENT_CLICKED, NULL);
    } else if (row_idx == 1) {
      s_wifi_caps_button = special;
      lv_obj_add_event_cb(s_wifi_caps_button, wifi_keyboard_caps_cb, LV_EVENT_CLICKED, NULL);
    } else if (row_idx == 2) {
      s_wifi_delete_button = special;
      lv_obj_add_event_cb(s_wifi_delete_button, wifi_keyboard_delete_cb, LV_EVENT_CLICKED, NULL);
    } else {
      s_wifi_enter_button = special;
      lv_obj_add_event_cb(s_wifi_enter_button, wifi_keyboard_enter_cb, LV_EVENT_CLICKED, NULL);
    }
  }

  wifi_update_keyboard_control_labels();
}

/* ------ Event callback implementations ------ */
static void wifi_keyboard_key_cb(lv_event_t *e) {
  lv_obj_t *btn = lv_event_get_target(e);
  lv_obj_t *lbl = lv_obj_get_child(btn, 0);
  const char *t = lbl ? lv_label_get_text(lbl) : NULL;
  if (!t) return;
  size_t L = strlen(t);
  for (size_t i = 0; i < L; i++) {
    if (s_wifi_password_len < MAX_PASS) {
      s_wifi_password_buf[s_wifi_password_len++] = t[i];
    }
  }
  s_wifi_password_buf[s_wifi_password_len] = '\0';
  wifi_password_update_label();
}

static void wifi_open_password_cb(lv_event_t *e) {
  lv_obj_t *target = lv_event_get_target(e);
  const char *ssid = wifi_get_card_ssid(target);
  if (!ssid) return;
  if (watch_network_is_connected_ssid(ssid)) {
    return;
  } else {
    wifi_password_screen_open(ssid);
  }
}

static void wifi_keyboard_delete_cb(lv_event_t *e) {
  (void)e;
  if (s_wifi_password_len > 0) {
    s_wifi_password_len--;
    s_wifi_password_buf[s_wifi_password_len] = '\0';
    wifi_password_update_label();
  }
}

static void wifi_keyboard_caps_cb(lv_event_t *e) {
  (void)e;
  s_wifi_key_upper = !s_wifi_key_upper;
  wifi_update_keyboard_control_labels();
  if (s_wifi_key_page > 0) wifi_rebuild_keyboard();
}

static void wifi_keyboard_mode_cb(lv_event_t *e) {
  (void)e;
  s_wifi_key_page = (s_wifi_key_page + 1) % 5;
  wifi_update_keyboard_control_labels();
  wifi_rebuild_keyboard();
}

static void wifi_keyboard_enter_cb(lv_event_t *e) {
  (void)e;
  wifi_status_screen_open(s_wifi_current_ssid, s_wifi_password_buf);
}


/* Helper to create a WiFi list item */
/* Khởi tạo một phần tử Wi-Fi Card hiển thị tên SSID và nút bấm chọn */
static lv_obj_t *wifi_make_network_item(lv_obj_t *par, const char *ssid) {
  if (!par || !ssid || !lv_obj_is_valid(par)) return NULL;
  wifi_init_styles();

  lv_obj_t *card = lv_btn_create(par);
  if (!card) return NULL;
  lv_obj_set_width(card, lv_pct(100));
  lv_obj_set_height(card, 44);
  lv_obj_set_style_bg_color(card, COLOR_SURFACE, 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, 18, 0);
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_set_style_shadow_width(card, 0, 0);
  lv_obj_set_style_outline_width(card, 0, 0);
  lv_obj_set_style_pad_hor(card, 10, 0);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  char *display_ssid = wifi_make_safe_display_text(ssid);

  /* WiFi icon (simple label, no nested object) */
  lv_obj_t *wifi_icon = lv_label_create(card);
  lv_label_set_text(wifi_icon, ICON_WIFI);
  lv_obj_set_style_text_font(wifi_icon, &wifi_24, 0);
  lv_obj_set_style_text_color(wifi_icon, COLOR_TEXT_MUTED, 0);
  lv_obj_align(wifi_icon, LV_ALIGN_LEFT_MID, 0, 0);

  /* SSID title */
  lv_obj_t *title = lv_label_create(card);
  lv_label_set_text(title, display_ssid ? display_ssid : ssid);
  lv_obj_set_style_text_font(title, UI_FONT_14, 0);
  lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
  lv_label_set_long_mode(title, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(title, WATCH_LCD_H_RES - 110);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 28, 0);

  /* Checkmark badge */
  lv_obj_t *badge = lv_label_create(card);
  lv_label_set_text(badge, "");
  lv_obj_set_style_text_font(badge, &signal_icon, 0);
  lv_obj_set_style_text_color(badge, COLOR_GREEN, 0);
  lv_obj_align(badge, LV_ALIGN_RIGHT_MID, 0, 0);

  /* Hidden raw SSID */
  lv_obj_t *raw = lv_label_create(card);
  lv_label_set_text(raw, ssid);
  lv_obj_add_flag(raw, LV_OBJ_FLAG_HIDDEN);

  lv_obj_add_event_cb(card, wifi_open_password_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(card, wifi_card_delete_cb, LV_EVENT_DELETE, display_ssid);
  wifi_update_card_state(card);

  return card;
}

static void wifi_card_delete_cb(lv_event_t *e) {
  char *display_ssid = (char *)lv_event_get_user_data(e);
  free(display_ssid);
}

/* Mở giao diện nhập mật khẩu Wi-Fi với bàn phím ảo đi kèm */
static void wifi_password_screen_open(const char *ssid) {
  if (!ssid) return;
  strncpy(s_wifi_current_ssid, ssid, sizeof(s_wifi_current_ssid) - 1);
  s_wifi_current_ssid[sizeof(s_wifi_current_ssid) - 1] = '\0';
  s_wifi_password_len = 0;
  s_wifi_password_buf[0] = '\0';
  s_wifi_key_page = 0;
  s_wifi_key_upper = false;

  lv_obj_t *scr = ui_navigation_create_screen();
  lv_obj_add_event_cb(scr, wifi_password_screen_delete_cb, LV_EVENT_DELETE, NULL);
  char *display_ssid = wifi_make_safe_display_text(ssid);

  /* Header: WiFi icon + SSID name */
  lv_obj_t *hdr_icon = lv_label_create(scr);
  lv_label_set_text(hdr_icon, ICON_WIFI);
  lv_obj_set_style_text_font(hdr_icon, &wifi_24, 0);
  lv_obj_set_style_text_color(hdr_icon, COLOR_GREEN, 0);
  lv_obj_align(hdr_icon, LV_ALIGN_TOP_LEFT, 12, 15);

  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, display_ssid ? display_ssid : ssid);
  lv_obj_set_width(title, WATCH_LCD_H_RES - 60);
  lv_label_set_long_mode(title, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(title, UI_FONT_16, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 36, 15);

  /* Password label */
  lv_obj_t *pw_title = lv_label_create(scr);
  lv_label_set_text(pw_title, ui_text("Password", "Mật khẩu"));
  lv_obj_set_style_text_font(pw_title, UI_FONT_14, 0);
  lv_obj_set_style_text_color(pw_title, COLOR_TEXT_MUTED, 0);
  lv_obj_align(pw_title, LV_ALIGN_TOP_LEFT, 12, 35);

  /* Password display */
  s_wifi_password_label = lv_label_create(scr);
  lv_label_set_text(s_wifi_password_label, "");
  lv_obj_set_style_text_font(s_wifi_password_label, UI_FONT_20, 0);
  lv_obj_set_style_text_color(s_wifi_password_label, COLOR_TEXT, 0);
  lv_obj_align(s_wifi_password_label, LV_ALIGN_TOP_MID, 0, 47);

  /* Keyboard Container (Direct on screen to save RAM) */
  s_wifi_keyboard_container = lv_obj_create(scr);
  lv_obj_set_size(s_wifi_keyboard_container, 220, 160);
  lv_obj_align(s_wifi_keyboard_container, LV_ALIGN_TOP_MID, 0, 70);
  lv_obj_set_style_bg_opa(s_wifi_keyboard_container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(s_wifi_keyboard_container, 0, 0);
  lv_obj_set_style_pad_all(s_wifi_keyboard_container, 0, 0);
  lv_obj_clear_flag(s_wifi_keyboard_container, LV_OBJ_FLAG_SCROLLABLE);

  wifi_rebuild_keyboard();
  wifi_password_update_label();

  ui_navigation_add_gesture(scr);
  ui_navigation_push(scr);
  free(display_ssid);
}

/* Bắt đầu gửi yêu cầu và tạo Task quét Wi-Fi */
static void wifi_start_scan_ui(lv_obj_t *scr, lv_obj_t *list_area) {
  lv_obj_clean(list_area);

  if (!wifi_try_begin_scan()) {
    lv_obj_t *busy = lv_label_create(list_area);
    lv_label_set_text(busy, ui_text("Scanning Wi-Fi...", "Đang quét Wi-Fi..."));
    lv_obj_set_style_text_color(busy, COLOR_TEXT_MUTED, 0);
    return;
  }

  lv_obj_t *placeholder = lv_label_create(list_area);
  lv_label_set_text(placeholder, "");
  lv_obj_set_style_text_color(placeholder, COLOR_TEXT_MUTED, 0);

  wifi_scan_request_t *param = malloc(sizeof(wifi_scan_request_t));
  if (param) {
    param->screen = scr;
    param->list_area = list_area;
    param->scanning_lbl = placeholder;
    BaseType_t ok = xTaskCreate(wifi_scan_worker_task, WATCH_NETWORK_WIFI_SCAN_TASK_NAME,
                                WIFI_SCAN_TASK_STACK_SIZE, param, 3, NULL);
    if (ok != pdPASS) {
      lv_obj_clean(list_area);
      lv_obj_t *err_lbl = lv_label_create(list_area);
      lv_label_set_text(err_lbl, ui_text("Cannot scan Wi-Fi", "Không thể quét Wi-Fi"));
      lv_obj_set_style_text_color(err_lbl, COLOR_TEXT_MUTED, 0);
      free(param);
      wifi_end_scan();
    }
  } else {
    lv_obj_clean(list_area);
    lv_obj_t *err_lbl = lv_label_create(list_area);
    lv_label_set_text(err_lbl, ui_text("Cannot scan Wi-Fi", "Không thể quét Wi-Fi"));
    lv_obj_set_style_text_color(err_lbl, COLOR_TEXT_MUTED, 0);
    wifi_end_scan();
  }
}

/* Callback khi gạt công tắc bật/tắt Wi-Fi
   Nếu bật: Thử kết nối tự động cấu hình cũ hoặc bắt đầu quét mạng.
   Nếu tắt: Ngắt kết nối Wi-Fi hiện tại để tiết kiệm pin. */
static void wifi_scan_switch_cb(lv_event_t *e) {
  lv_obj_t *sw = lv_event_get_target(e);
  lv_obj_t *scr = lv_event_get_user_data(e);
  s_wifi_scan_enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
  if (!s_wifi_list_area || !lv_obj_is_valid(s_wifi_list_area)) return;
  if (s_wifi_scan_enabled) {
    ESP_ERROR_CHECK_WITHOUT_ABORT(watch_network_wifi_start());
    wifi_start_scan_ui(scr, s_wifi_list_area);
  } else {
    ESP_ERROR_CHECK_WITHOUT_ABORT(watch_network_wifi_stop());
    lv_obj_clean(s_wifi_list_area);
  }
}

/* Khởi tạo màn hình cấu hình Wi-Fi chính
   Gồm công tắc bật/tắt Wi-Fi ở trên cùng và danh sách các AP quét được hiển thị bên dưới. */
lv_obj_t *ui_wifi_screen_create(void) {
  ESP_ERROR_CHECK_WITHOUT_ABORT(watch_network_wifi_start());
  lv_obj_t *scr = ui_navigation_create_screen_with_id(UI_SCREEN_WIFI);
  lv_obj_add_event_cb(scr, wifi_list_screen_delete_cb, LV_EVENT_DELETE, NULL);
  s_wifi_list_screen = scr;

  lv_obj_t *top = lv_obj_create(scr);
  lv_obj_set_size(top, WATCH_LCD_H_RES - 28, 36);
  lv_obj_align(top, LV_ALIGN_TOP_MID, 0, 15);
  lv_obj_set_style_bg_opa(top, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(top, 0, 0);
  lv_obj_clear_flag(top, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *title = lv_label_create(top);
  lv_label_set_text(title, ui_tr(UI_TXT_WIFI));
  lv_obj_set_style_text_font(title, UI_FONT_20, 0);
  lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

  lv_obj_t *scan_sw = lv_switch_create(top);
  lv_obj_set_size(scan_sw, 44, 24);
  lv_obj_set_style_bg_color(scan_sw, lv_color_hex(0x2B3038), LV_PART_MAIN);
  lv_obj_set_style_bg_color(scan_sw, lv_color_hex(0x4A4F58), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(scan_sw, COLOR_GREEN, LV_PART_INDICATOR | LV_STATE_CHECKED);
  lv_obj_align(scan_sw, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_add_event_cb(scan_sw, wifi_scan_switch_cb, LV_EVENT_VALUE_CHANGED, scr);

  lv_obj_t *list_area = lv_obj_create(scr);
  lv_obj_set_size(list_area, WATCH_LCD_H_RES - 20, WATCH_LCD_V_RES - 56);
  lv_obj_align(list_area, LV_ALIGN_TOP_MID, 0, 51);
  lv_obj_set_style_bg_opa(list_area, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(list_area, 0, 0);
  lv_obj_set_style_shadow_width(list_area, 0, 0);
  lv_obj_set_style_outline_width(list_area, 0, 0);
  lv_obj_set_flex_flow(list_area, LV_FLEX_FLOW_COLUMN);
  ui_enable_list_scrolling(list_area);
  lv_obj_set_style_pad_row(list_area, 10, 0);
  lv_obj_add_flag(list_area, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(list_area, LV_SCROLLBAR_MODE_OFF);
  s_wifi_list_area = list_area;

  if (watch_network_is_wifi_connected() && s_wifi_cached_count > 0) {
    s_wifi_scan_enabled = true;
    lv_obj_add_state(scan_sw, LV_STATE_CHECKED);
    wifi_render_cached_scan(list_area);
    wifi_start_scan_ui(scr, list_area);
  } else if (watch_network_is_wifi_connected()) {
    s_wifi_scan_enabled = true;
    lv_obj_add_state(scan_sw, LV_STATE_CHECKED);
    const char *ssid = watch_network_get_connected_ssid();
    if (ssid && ssid[0]) wifi_make_network_item(list_area, ssid);
    wifi_refresh_network_cards();
    wifi_start_scan_ui(scr, list_area);
  } else {
    s_wifi_scan_enabled = false;
  }

  ui_navigation_add_gesture(scr);
  return scr;
}
/* Background task: perform blocking scan off the LVGL task, then post
 * results back to LVGL via lv_async_call. */
/* Task FreeRTOS chạy ngầm đảm nhận việc quét tín hiệu Wi-Fi
   Gọi hàm API của ESP-IDF để quét. Sau khi hoàn tất, đóng gói kết quả quét và đẩy
   ngược về giao diện chính thông qua hàm lv_async_call bảo vệ bởi Mutex Display Lock. */
static void wifi_scan_worker_task(void *arg) {
  wifi_scan_request_t *param = (wifi_scan_request_t *)arg;
  if (!param) {
    wifi_end_scan();
    vTaskDelete(NULL);
    return;
  }

  wifi_ap_record_t *records = calloc(WIFI_SCAN_RECORD_CAPACITY, sizeof(wifi_ap_record_t));
  uint16_t num = WIFI_SCAN_RECORD_CAPACITY;
  esp_err_t r = ESP_ERR_NO_MEM;
  if (records) {
    r = watch_network_wifi_scan(records, &num);
  }

  wifi_scan_result_t *res = malloc(sizeof(wifi_scan_result_t));
  if (!res) {
    free(records);
    free(param);
    wifi_end_scan();
    vTaskDelete(NULL);
    return;
  }

  res->screen = param->screen;
  res->list_area = param->list_area;
  res->err = r;
  if (r == ESP_OK && num > 0) {
    if (num > MAX_WIFI_SCAN_RESULTS) num = MAX_WIFI_SCAN_RESULTS;
    res->count = num;
    res->ssids = calloc(res->count, sizeof(char *));
    if (res->ssids) {
      for (int i = 0; i < res->count; i++) {
        size_t L = strnlen((char *)records[i].ssid, sizeof(records[i].ssid));
        if (L == 0) continue;
        res->ssids[i] = malloc(L + 1);
        if (res->ssids[i]) {
          memcpy(res->ssids[i], records[i].ssid, L);
          res->ssids[i][L] = '\0';
        }
      }
    } else {
      res->count = 0;
    }
  } else {
    res->count = 0;
    res->ssids = NULL;
  }

  free(records);
  free(param);

  if (!ui_is_sleep_in_progress() && lvgl_port_lock(200)) {
    if (ui_is_sleep_in_progress()) {
      lvgl_port_unlock();
      wifi_free_scan_result(res->ssids, res->count);
      free(res);
      wifi_end_scan();
      vTaskDelete(NULL);
      return;
    }
    lv_res_t async_res = lv_async_call(wifi_scan_complete_async_cb, res);
    lvgl_port_unlock();
    if (async_res != LV_RES_OK) {
      wifi_free_scan_result(res->ssids, res->count);
      free(res);
    }
  } else {
    wifi_free_scan_result(res->ssids, res->count);
    free(res);
  }
  wifi_end_scan();
  vTaskDelete(NULL);
}

/* Callback nhận dữ liệu Wi-Fi bất đồng bộ chạy trên luồng chính LVGL
   Nhận mảng SSID quét được, lưu vào bộ đệm và khởi chạy timer nạp dần các phần tử
   Wi-Fi Card lên LCD một cách từ từ để tránh gây đơ màn hình (giảm tải CPU). */
static void wifi_scan_complete_async_cb(void *data) {
  wifi_scan_result_t *res = (wifi_scan_result_t *)data;
  if (!res) return;

  /* Find the active screen and locate the Wi-Fi list area there. This avoids
   * using a list_area pointer captured earlier which may be stale if the
   * user navigated away. We identify the list area by searching for a child
   * with the scrollable flag (the list_area created in ui_wifi_screen_create).
   */
  lv_obj_t *list_area = res->list_area;
  if ((!list_area || !lv_obj_is_valid(list_area)) &&
      s_wifi_list_area && lv_obj_is_valid(s_wifi_list_area)) {
    list_area = s_wifi_list_area;
    res->screen = s_wifi_list_screen;
  }

  if (list_area && lv_obj_is_valid(list_area) &&
      (!res->screen || lv_obj_is_valid(res->screen))) {
    lv_obj_clean(list_area);

    if (res->count <= 0) {
      lv_obj_t *empty = lv_label_create(list_area);
      if (res->err == ESP_ERR_INVALID_STATE) {
        lv_label_set_text(empty, ui_text("Wi-Fi is busy, try again shortly.", "Wi-Fi đang bận, hãy thử lại sau."));
      } else {
        lv_label_set_text(empty, ui_text("No Wi-Fi networks found", "Không tìm thấy Wi-Fi"));
      }
      lv_obj_set_style_text_color(empty, COLOR_TEXT_MUTED, 0);
      wifi_free_scan_result(res->ssids, res->count);
      free(res);
      return;
    }

    wifi_cache_scan_result(res->ssids, res->count);

    /* Populate items incrementally via a short LVGL timer to avoid
     * blocking the LVGL task (prevents task watchdog triggers). */
    wifi_scan_iterator_t *it = malloc(sizeof(wifi_scan_iterator_t));
    if (it) {
      it->screen = res->screen;
      it->list_area = list_area;
      it->ssids = res->ssids; /* take ownership */
      it->count = res->count;
      it->idx = 0;
      if (!lv_timer_create(wifi_scan_item_timer_cb, WIFI_SCAN_TIMER_MS, it)) {
        wifi_free_scan_result(it->ssids, it->count);
        free(it);
      }
    } else {
      /* Fallback: synchronous creation if allocation fails */
      for (int i = 0; i < res->count; i++) {
        if (res->ssids && res->ssids[i]) wifi_make_network_item(list_area, res->ssids[i]);
      }
      wifi_refresh_network_cards();
      wifi_free_scan_result(res->ssids, res->count);
    }
  } else {
    /* No valid list area found; free the results */
    wifi_free_scan_result(res->ssids, res->count);
  }
  free(res);
}
