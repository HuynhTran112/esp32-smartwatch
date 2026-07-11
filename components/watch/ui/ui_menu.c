/* Định nghĩa giao diện Menu ứng dụng dạng lưới trượt ngang (Horizontal Grid Menu)
   File này xây dựng màn hình Menu chính hiển thị danh sách tất cả các ứng dụng có sẵn trên đồng hồ.
   Giao diện sử dụng đối tượng `lv_tileview` chia làm 3 trang trượt ngang độc lập. Mỗi trang là một lưới 2x2
   hiển thị tối đa 4 icon tròn nhiều màu sắc kèm theo nhãn tên ứng dụng tương ứng bên dưới.
   Phía dưới đáy tích hợp 3 chấm tròn chỉ báo vị trí trang hiện tại (Page Indicator dots).
   Hỗ trợ chuyển đổi chủ đề đơn sắc (monochrome) theo màu sắc hệ thống người dùng tự cấu hình. */

#include "board_config.h"
#include "ui.h"
#include "ui_navigation.h"
#include "ui_screens.h"
#include "ui_icons.h"
#include "ui_utils.h"
#include <stdint.h>

/* Khai báo ngoại vi các phông chữ biểu tượng chứa Glyph icon */
LV_FONT_DECLARE(menu_icon);
LV_FONT_DECLARE(workout_health_icon);
LV_FONT_DECLARE(menu_icon_2);
LV_FONT_DECLARE(ble);

static lv_obj_t *scr_menu = NULL;
static lv_obj_t *s_tileview = NULL;
static lv_obj_t *s_pages[3] = {NULL, NULL, NULL};     // Mảng quản lý 3 trang Tileview
static lv_obj_t *s_page_dots[3] = {NULL, NULL, NULL}; // Mảng quản lý 3 chấm chỉ báo trang

/* Cấu trúc định nghĩa một mục ứng dụng trong Menu */
typedef struct {
  const char *symbol;              // Mã glyph của icon ứng dụng
  const lv_font_t *font;           // Bộ font chứa glyph tương ứng
  ui_text_id_t text_id;            // ID văn bản đa ngôn ngữ hiển thị nhãn tên ứng dụng
  const char *name;                // Tên chuỗi (phục vụ mục đích mở rộng)
  lv_color_t color;                // Cấu trúc màu nền icon
  void (*on_click)(lv_event_t *);  // Con trỏ hàm callback xử lý sự kiện click mở app
} app_item_t;

/* Khai báo các hàm callback chuyển đổi màn hình tương ứng khi bấm icon */
static void menu_open_activity_cb(lv_event_t *e) { (void)e; ui_navigation_push(ui_activity_screen_create()); }
static void menu_open_settings_cb(lv_event_t *e) { (void)e; ui_navigation_push(ui_settings_screen_create()); }
static void menu_open_connect_cb(lv_event_t *e) { (void)e; ui_navigation_push(ui_connect_screen_create()); }
static void menu_open_health_cb(lv_event_t *e) { (void)e; ui_navigation_push(ui_health_screen_create()); }
static void menu_open_alarm_cb(lv_event_t *e) { (void)e; ui_navigation_push(ui_alarm_list_screen_create()); }
static void menu_open_wifi_cb(lv_event_t *e) { (void)e; ui_navigation_push(ui_wifi_screen_create()); }
static void menu_open_gps_cb(lv_event_t *e) { (void)e; ui_navigation_push(ui_gps_screen_create()); }
static void menu_open_notifications_cb(lv_event_t *e) { (void)e; ui_navigation_push(ui_notifications_screen_create()); }
static void menu_open_update_cb(lv_event_t *e) { (void)e; ui_navigation_push(ui_update_screen_create()); }

/* Định nghĩa mã hex của các Glyph Icon */
#define ICON_MENU_HEALTH   "\xef\x80\x84"
#define ICON_MENU_SETTING  "\xef\x80\x93"
#define ICON_MENU_UPDATE   "\xef\x80\xa1"
#define ICON_MENU_GPS      "\xef\x8f\x85"
#define ICON_MENU_WIFI     "\xef\x87\xab"
#define ICON_MENU_BT       "\xef\x8a\x94"
#define ICON_MENU_ALARM    "\xef\x8d\x8e"
#define ICON_MENU_WORKOUT  "\xef\x95\x94"
#define ICON_MENU_NOTIF    "\xef\x83\xb3"

/* Mảng tĩnh danh sách ứng dụng trong menu chính */
static const app_item_t s_menu_apps[] = {
  {ICON_MENU_WORKOUT, &menu_icon_2, UI_TXT_WORKOUT, NULL, {.full = 0}, menu_open_activity_cb},
  {ICON_MENU_GPS, &menu_icon_2, UI_TXT_GPS, NULL, {.full = 0}, menu_open_gps_cb},
  {ICON_MENU_HEALTH, &menu_icon, UI_TXT_HEALTH, NULL, {.full = 0}, menu_open_health_cb},
  {ICON_MENU_NOTIF, &menu_icon_2, UI_TXT_NOTIFICATIONS, NULL, {.full = 0}, menu_open_notifications_cb},
  {ICON_MENU_ALARM, &menu_icon_2, UI_TXT_ALARM, NULL, {.full = 0}, menu_open_alarm_cb},
  {ICON_MENU_WIFI, &menu_icon_2, UI_TXT_WIFI, NULL, {.full = 0}, menu_open_wifi_cb},
  {ICON_MENU_BT, &ble, UI_TXT_CONNECT, NULL, {.full = 0}, menu_open_connect_cb},
  {ICON_MENU_SETTING, &menu_icon, UI_TXT_SETTINGS, NULL, {.full = 0}, menu_open_settings_cb},
  {ICON_MENU_UPDATE, &menu_icon, UI_TXT_UPDATE, NULL, {.full = 0}, menu_open_update_cb},
};

static lv_color_t s_menu_app_colors[sizeof(s_menu_apps) / sizeof(s_menu_apps[0])];
static bool s_icons_monochrome = false;
static lv_color_t s_system_icon_color;

/* Khởi tạo bảng màu sắc sặc sỡ mặc định cho các ứng dụng */
static void menu_init_colors(void) {
  s_system_icon_color = COLOR_PRIMARY;
  size_t n = sizeof(s_menu_app_colors) / sizeof(s_menu_app_colors[0]);
  lv_color_t palette[] = {COLOR_CYAN, COLOR_RED, COLOR_PRIMARY, COLOR_CYAN, COLOR_ORANGE,
                          COLOR_FB_BLUE, COLOR_GREEN, COLOR_PURPLE, COLOR_ORANGE, COLOR_GRAY,
                          COLOR_GREEN, COLOR_PURPLE, COLOR_CYAN, COLOR_PRIMARY, COLOR_BATTERY, COLOR_GRAY};
  size_t pcount = sizeof(palette) / sizeof(palette[0]);
  for (size_t i = 0; i < n; i++) {
    s_menu_app_colors[i] = palette[i % pcount];
  }
}

/* Thay đổi hệ màu sắc hiển thị của toàn bộ Menu (Đơn sắc / Đa sắc) */
void ui_menu_set_system_color(bool mono, lv_color_t color) {
  s_icons_monochrome = mono;
  s_system_icon_color = color;
  if (mono) {
    for (size_t i = 0; i < (sizeof(s_menu_app_colors) / sizeof(s_menu_app_colors[0])); i++) {
      s_menu_app_colors[i] = s_system_icon_color;
    }
  } else {
    menu_init_colors();
  }

  if (!s_tileview) return;
  for (int p = 0; p < 3; p++) {
    lv_obj_t *page = s_pages[p];
    if (!page) continue;
    int child_cnt = lv_obj_get_child_cnt(page);
    for (int i = 0; i < child_cnt; i++) {
      lv_obj_t *cont = lv_obj_get_child(page, i);
      lv_obj_t *icon_bg = lv_obj_get_child(cont, 0);
      int app_idx = p * 4 + i;
      if (icon_bg && app_idx < (int)(sizeof(s_menu_apps) / sizeof(s_menu_apps[0]))) {
        lv_obj_set_style_bg_color(icon_bg, s_menu_app_colors[app_idx], 0);
      }
    }
  }
}

/* Callback bọc xử lý chuyển đổi trang ứng dụng click */
static void menu_app_click_wrapper_cb(lv_event_t *e) {
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  if (idx >= 0 && idx < (int)(sizeof(s_menu_apps) / sizeof(s_menu_apps[0])) && s_menu_apps[idx].on_click) {
    s_menu_apps[idx].on_click(e);
  }
}

/* Khởi tạo một widget Container đóng vai trò nút bấm ứng dụng */
static lv_obj_t *menu_create_app_button(lv_obj_t *parent, int idx) {
  /* Tạo container bọc ngoài */
  lv_obj_t *cont = lv_obj_create(parent);
  lv_obj_set_size(cont, 100, 110);
  lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(cont, 0, 0);
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(cont, 8, 0);

  /* Tạo icon tròn */
  lv_obj_t *icon_bg = ui_icon_create(cont, s_menu_apps[idx].symbol, s_menu_app_colors[idx], s_icons_monochrome, s_system_icon_color);
  lv_obj_t *ic_label = lv_obj_get_child(icon_bg, 0);
  if (s_menu_apps[idx].font) {
    lv_obj_set_style_text_font(ic_label, s_menu_apps[idx].font, 0);
  }

  /* Tạo nhãn chữ hiển thị tên dưới icon */
  lv_obj_t *name = lv_label_create(cont);
  lv_label_set_text(name, ui_tr(s_menu_apps[idx].text_id));
  lv_obj_set_style_text_color(name, COLOR_TEXT_MUTED, 0);
  lv_obj_set_style_text_font(name, UI_FONT_14, 0);
  lv_obj_set_width(name, lv_pct(100));
  lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);

  /* Đăng ký phản hồi chạm Click nhạy và ổn định trên tất cả thành phần con */
  lv_obj_add_flag(cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(cont, menu_app_click_wrapper_cb, LV_EVENT_CLICKED, (void *)(intptr_t)idx);
  lv_obj_add_flag(icon_bg, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(icon_bg, menu_app_click_wrapper_cb, LV_EVENT_CLICKED, (void *)(intptr_t)idx);
  lv_obj_add_flag(name, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(name, menu_app_click_wrapper_cb, LV_EVENT_CLICKED, (void *)(intptr_t)idx);
  
  /* Tạo hiệu ứng nhấp nháy mờ nhẹ 70% khi nhấn giữ (Visual Feedback) */
  lv_obj_set_style_opa(icon_bg, LV_OPA_70, LV_STATE_PRESSED);

  /* Tạo dấu chấm nhỏ báo hiệu đỏ góc trên icon đối với ứng dụng Báo thức */
  if (s_menu_apps[idx].text_id == UI_TXT_ALARM) {
    lv_obj_t *dot = lv_obj_create(icon_bg);
    lv_obj_set_size(dot, 12, 12);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, COLOR_RED, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_align(dot, LV_ALIGN_TOP_RIGHT, -6, 6);
  }

  return cont;
}

/* Callback lắng nghe sự kiện trượt thay đổi trang để cập nhật các chấm tròn chỉ báo */
static void menu_page_changed_cb(lv_event_t *e) {
  lv_obj_t *tile = lv_event_get_target(e);
  lv_obj_t *active = lv_tileview_get_tile_act(tile);
  int col = 0;
  if (active == s_pages[1]) col = 1;
  else if (active == s_pages[2]) col = 2;

  /* Cập nhật độ mờ và kích thước phóng to của chấm chỉ vị trí trang hiện hành */
  for (int i = 0; i < 3; i++) {
    if (s_page_dots[i]) {
      lv_obj_set_style_bg_opa(s_page_dots[i], (i == col) ? LV_OPA_COVER : LV_OPA_30, 0);
      lv_obj_set_size(s_page_dots[i], (i == col) ? 14 : 8, 8); // Phóng to chấm trang đang đứng
    }
  }
}

/* Xóa bỏ màn hình menu giải phóng tài nguyên tĩnh */
static void menu_screen_delete_cb(lv_event_t *e) {
  (void)e;
  s_tileview = NULL;
  s_pages[0] = s_pages[1] = s_pages[2] = NULL;
  s_page_dots[0] = s_page_dots[1] = s_page_dots[2] = NULL;
  scr_menu = NULL;
}

/* Lấy con trỏ màn hình menu chính (Tạo mới nếu chưa tồn tại) */
lv_obj_t *ui_menu_get_screen(void) {
  if (!scr_menu) {
    scr_menu = ui_menu_screen_create();
  }
  return scr_menu;
}

/* Khởi tạo đối tượng màn hình menu lưới ứng dụng 3 trang vuốt ngang */
lv_obj_t *ui_menu_screen_create(void) {
  menu_init_colors();
  lv_obj_t *scr = ui_navigation_create_screen_with_id(UI_SCREEN_MENU);
  lv_obj_add_event_cb(scr, menu_screen_delete_cb, LV_EVENT_DELETE, NULL);

  /* Tạo Tileview chứa lưới menu trượt ngang */
  s_tileview = lv_tileview_create(scr);
  lv_obj_set_size(s_tileview, WATCH_LCD_H_RES, WATCH_LCD_V_RES - 30); // Giới hạn chiều cao nhường phần đỉnh cho status bar
  lv_obj_align(s_tileview, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_opa(s_tileview, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(s_tileview, 0, 0);
  lv_obj_set_style_pad_all(s_tileview, 0, 0);
  lv_obj_set_style_pad_row(s_tileview, 0, 0);
  lv_obj_set_style_pad_column(s_tileview, 0, 0);
  lv_obj_set_scrollbar_mode(s_tileview, LV_SCROLLBAR_MODE_OFF);
  lv_obj_add_flag(s_tileview, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(s_tileview, LV_OBJ_FLAG_SCROLL_ELASTIC); // Ngăn cuộn quá đà (Elastic scroll)
  lv_obj_add_event_cb(s_tileview, menu_page_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_set_scroll_dir(s_tileview, LV_DIR_HOR);            // Chỉ cho phép cuộn ngang (Horizontal scroll)

  /* Tạo 3 trang Tile chứa danh sách icon ứng dụng */
  for (int p = 0; p < 3; p++) {
    lv_obj_t *page = lv_tileview_add_tile(s_tileview, p, 0, LV_DIR_HOR);
    lv_obj_set_style_bg_color(page, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_set_scrollbar_mode(page, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    
    /* Sử dụng bố cục Flexbox tự động bọc dòng Wrap tạo lưới 2x2 */
    lv_obj_set_flex_flow(page, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(page, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_top(page, 16, 0);
    lv_obj_set_style_pad_bottom(page, 18, 0);
    lv_obj_set_style_pad_row(page, 10, 0);
    s_pages[p] = page;
  }

  /* Tạo và xếp các nút bấm ứng dụng vào từng trang (Mỗi trang chứa tối đa 4 icon) */
  int app_count = (int)(sizeof(s_menu_apps) / sizeof(s_menu_apps[0]));
  for (int i = 0; i < app_count; i++) {
    int page_idx = i / 4;
    if (page_idx > 2) page_idx = 2;
    menu_create_app_button(s_pages[page_idx], i);
  }

  /* Định vị căn trái trang thứ 3 (Do chỉ chứa số lượng icon lẻ tránh bị dàn đều ở giữa) */
  lv_obj_set_flex_align(s_pages[2], LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_left(s_pages[2], 20, 0);

  /* Tạo cụm chấm chỉ báo vị trí trang (Page Indicators dots) */
  lv_obj_t *dots = lv_obj_create(scr);
  lv_obj_set_size(dots, 64, 10);
  lv_obj_align(dots, LV_ALIGN_BOTTOM_MID, 0, -4);
  lv_obj_set_style_bg_opa(dots, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(dots, 0, 0);
  lv_obj_clear_flag(dots, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(dots, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(dots, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(dots, 6, 0);
  
  for (int i = 0; i < 3; i++) {
    s_page_dots[i] = lv_obj_create(dots);
    lv_obj_set_size(s_page_dots[i], (i == 0) ? 14 : 8, 8);
    lv_obj_set_style_radius(s_page_dots[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_page_dots[i], COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_bg_opa(s_page_dots[i], (i == 0) ? LV_OPA_COVER : LV_OPA_30, 0);
    lv_obj_set_style_border_width(s_page_dots[i], 0, 0);
  }

  /* Tải trang số 0 mặc định */
  lv_obj_set_tile_id(s_tileview, 0, 0, LV_ANIM_OFF);
  
  /* Đăng ký vuốt phải quay lại */
  ui_navigation_add_gesture(scr);
  
  scr_menu = scr;
  return scr;
}

