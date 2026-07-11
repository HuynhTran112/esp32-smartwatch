/* Định nghĩa popup hiển thị thông tin thông báo mới (Notification Overlay Popup)
   Module này xử lý hiển thị một popup trượt từ trên xuống (Slide-down Overlay) trên lớp
   trên cùng của giao diện (layer_top) bất cứ khi nào đồng hồ nhận được gói tin thông báo (SMS, Zalo,...)
   từ Smartphone thông qua BLE. Thiết lập bộ đếm thời gian tự động ẩn (5 giây) hoặc ẩn lập tức khi
   người dùng chạm vào. Đảm bảo tính an toàn đa luồng (Thread-safe) bằng cách đóng gói luồng BLE
   Task đẩy sang LVGL Task Context qua cơ chế khóa Lock/Unlock và gọi hàm lv_async_call. */

#include "ui_notification_popup.h"
#include "ui.h"
#include "ui_utils.h"
#include "board_config.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "notif_popup";

#define POPUP_SHOW_MS    5000   /* Thời gian hiển thị popup (5 giây) */
#define POPUP_ANIM_MS    300    /* Thời gian hiệu ứng trượt trượt (300ms) */
#define POPUP_HEIGHT     80     /* Chiều cao của khung popup (80px) */
#define POPUP_MARGIN     8      /* Lề biên trái/phải (8px) */

static lv_obj_t  *s_popup = NULL;

/* Wrapper callback thiết lập tọa độ Y phục vụ hiệu ứng Animation */
static void notif_popup_anim_set_y_cb(void *obj, int32_t v) {
    lv_obj_set_y((lv_obj_t *)obj, (lv_coord_t)v);
}
static lv_obj_t  *s_lbl_app = NULL;
static lv_obj_t  *s_lbl_title = NULL;
static lv_obj_t  *s_lbl_content = NULL;
static lv_timer_t *s_hide_timer = NULL;

static watch_bluetooth_notification_t s_current_notif;
static bool s_has_current_notif = false;


/* Kích hoạt hiệu ứng trượt ngược lên trên để ẩn popup */
static void notif_popup_hide(void) {
    if (!s_popup) return;
    
    /* Cấu hình hiệu ứng trượt lên trên (Slide-up Animation) */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_popup);
    lv_anim_set_values(&a, lv_obj_get_y(s_popup), -(POPUP_HEIGHT + 10)); // Vượt lên ngoài mép trên LCD
    lv_anim_set_time(&a, POPUP_ANIM_MS);
    lv_anim_set_exec_cb(&a, notif_popup_anim_set_y_cb);
    lv_anim_start(&a);
}

/* Callback của timer tự động ẩn sau 5 giây nhàn rỗi */
static void notif_popup_hide_timer_cb(lv_timer_t *t) {
    (void)t;
    notif_popup_hide();
    if (s_hide_timer) {
        lv_timer_del(s_hide_timer);
        s_hide_timer = NULL;
    }
}

/* Xử lý sự kiện nhấn chạm trực tiếp vào popup để tắt nhanh và mở chi tiết tin nhắn */
static void notif_popup_click_cb(lv_event_t *e) {
    (void)e;
    if (s_hide_timer) {
        lv_timer_del(s_hide_timer);
        s_hide_timer = NULL;
    }
    if (s_has_current_notif) {
        extern void ui_notifications_show_detail(const char *app_name, const char *sender, const char *message, uint32_t seq_id);
        ui_notifications_show_detail(s_current_notif.app_name, s_current_notif.title, s_current_notif.content, s_current_notif.seq_id);
        s_has_current_notif = false;
    }
    notif_popup_hide();
}

/* Khởi tạo các thành phần giao diện popup (Chỉ chạy một lần duy nhất lúc khởi động)
   Thiết lập các lớp Label hiển thị tên app, tiêu đề người gửi và nội dung rút gọn. */
void ui_notification_popup_init(void) {
    if (s_popup) return;

    /* 1. Tạo đối tượng Card Popup trên lớp hiển thị trên cùng (layer_top) */
    s_popup = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_popup, WATCH_LCD_H_RES - POPUP_MARGIN * 2, POPUP_HEIGHT);
    lv_obj_set_pos(s_popup, POPUP_MARGIN, -(POPUP_HEIGHT + 10)); /* Ẩn trên đầu màn hình */
    lv_obj_set_style_bg_color(s_popup, lv_color_hex(0x2C2C2E), 0); // Màu nền xám đậm phong cách iOS Dark mode
    lv_obj_set_style_bg_opa(s_popup, LV_OPA_90, 0);                 // Đặt độ mờ 90% (Glossy background)
    lv_obj_set_style_radius(s_popup, 16, 0);                        // Bo tròn 4 góc 16px
    lv_obj_set_style_border_width(s_popup, 0, 0);                  // Không viền mặc định
    lv_obj_set_style_pad_all(s_popup, 10, 0);                       // Cách lề đệm bên trong 10px
    lv_obj_set_style_shadow_width(s_popup, 0, 0);                  // Tắt đổ bóng để cải thiện tốc độ vẽ
    lv_obj_clear_flag(s_popup, LV_OBJ_FLAG_SCROLLABLE);            // Chặn kéo cuộn
    lv_obj_add_flag(s_popup, LV_OBJ_FLAG_CLICKABLE);                // Cho phép bắt sự kiện chạm
    lv_obj_add_event_cb(s_popup, notif_popup_click_cb, LV_EVENT_CLICKED, NULL);

    /* 2. Nhãn hiển thị Tên Ứng Dụng (phông chữ nhỏ 14px) */
    s_lbl_app = lv_label_create(s_popup);
    lv_obj_set_style_text_font(s_lbl_app, UI_FONT_14, 0);
    lv_obj_set_style_text_color(s_lbl_app, lv_color_hex(0x0A84FF), 0);
    lv_label_set_text(s_lbl_app, "");
    lv_obj_align(s_lbl_app, LV_ALIGN_TOP_LEFT, 0, 0);

    /* 3. Nhãn hiển thị Người Gửi (phông chữ trung bình 16px, in đậm) */
    s_lbl_title = lv_label_create(s_popup);
    lv_obj_set_style_text_font(s_lbl_title, UI_FONT_16, 0);
    lv_obj_set_style_text_color(s_lbl_title, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(s_lbl_title, "");
    lv_obj_align(s_lbl_title, LV_ALIGN_TOP_LEFT, 0, 18);

    /* 4. Nhãn hiển thị Nội dung tin nhắn rút gọn (1 dòng, tự động thêm dấu "...") */
    s_lbl_content = lv_label_create(s_popup);
    lv_obj_set_style_text_font(s_lbl_content, UI_FONT_14, 0);
    lv_obj_set_style_text_color(s_lbl_content, lv_color_hex(0x888888), 0);
    lv_label_set_text(s_lbl_content, "");
    lv_obj_set_width(s_lbl_content, WATCH_LCD_H_RES - POPUP_MARGIN * 2 - 20);
    lv_label_set_long_mode(s_lbl_content, LV_LABEL_LONG_DOT); // Cắt đuôi chuỗi dài bằng ký tự "..."
    lv_obj_align(s_lbl_content, LV_ALIGN_TOP_LEFT, 0, 40);

    ESP_LOGI(TAG, "Notification popup initialized");
}

/* Hàm Callback xử lý bất đồng bộ hiển thị Popup (Chạy trong LVGL Task Context) */
static void notif_popup_show_async_cb(void *arg) {
    watch_bluetooth_notification_t *notif = (watch_bluetooth_notification_t *)arg;
    if (!notif) return;
    lv_disp_trig_activity(NULL);

    s_current_notif = *notif;
    s_has_current_notif = true;

    if (!s_popup) {
        ESP_LOGW(TAG, "Notification popup missing; reinitializing");
        ui_notification_popup_init();
    }

    /* Đọc màu sắc chủ đề tương ứng của app nguồn gửi đến */
    lv_color_t app_color = ui_app_color(notif->app_name);

    /* Cập nhật nhãn */
    lv_label_set_text(s_lbl_app, notif->app_name);
    lv_obj_set_style_text_color(s_lbl_app, app_color, 0);

    lv_label_set_text(s_lbl_title, notif->title);
    lv_label_set_text(s_lbl_content, notif->content);

    /* Thiết lập đường viền sọc dọc màu sắc bên trái popup để nhấn mạnh */
    lv_obj_set_style_border_side(s_popup, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_width(s_popup, 3, 0);
    lv_obj_set_style_border_color(s_popup, app_color, 0);

    /* Tạo hiệu ứng chuyển động trượt từ đỉnh màn hình xuống (dừng dưới Status Bar là 35px) */
    lv_obj_set_y(s_popup, -(POPUP_HEIGHT + 10));
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_popup);
    lv_anim_set_values(&a, -(POPUP_HEIGHT + 10), 35);
    lv_anim_set_time(&a, POPUP_ANIM_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a, notif_popup_anim_set_y_cb);
    lv_anim_start(&a);

    /* Đăng ký bộ định thì tự động tắt sau 5 giây */
    if (s_hide_timer) {
        lv_timer_del(s_hide_timer);
    }
    s_hide_timer = lv_timer_create(notif_popup_hide_timer_cb, POPUP_SHOW_MS, NULL);

    /* Giải phóng con trỏ bản sao dữ liệu heap */
    free(notif);
}

/* API hiển thị Popup thông báo (Thread-Safe Entry Point)
   Có thể gọi tự do từ Task xử lý BLE của ESP-IDF mà không gây xung đột tài nguyên hiển thị. */
void ui_notification_popup_show(const watch_bluetooth_notification_t *notif) {
    if (!notif || ui_is_sleep_in_progress()) return;

    /* Sao chép dữ liệu thông báo sang vùng nhớ đệm heap mới của luồng BLE */
    watch_bluetooth_notification_t *copy = malloc(sizeof(watch_bluetooth_notification_t));
    if (!copy) {
        ESP_LOGW(TAG, "No memory for notification popup");
        return;
    }
    memcpy(copy, notif, sizeof(*copy));

    /* Lock luồng LVGL, đẩy tiến trình chạy bất đồng bộ và unlock nhanh để tránh nghẽn luồng BLE */
    if (lvgl_port_lock(100)) {
        if (ui_is_sleep_in_progress()) {
            lvgl_port_unlock();
            free(copy);
            return;
        }
        lv_res_t res = lv_async_call(notif_popup_show_async_cb, copy);
        lvgl_port_unlock();
        if (res != LV_RES_OK) {
            ESP_LOGW(TAG, "Drop notification popup: LVGL async queue full");
            free(copy);
        }
    } else {
        ESP_LOGW(TAG, "Drop notification popup: LVGL lock timeout");
        free(copy);
    }
}
