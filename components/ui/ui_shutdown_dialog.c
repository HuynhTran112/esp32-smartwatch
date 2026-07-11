/* Định nghĩa giao diện hộp thoại xác nhận tắt nguồn (Shutdown Dialog Overlay) */

#include "ui_shutdown_dialog.h"
#include "ui.h"
#include "ui_utils.h"
#include "board_config.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"

static const char *TAG = "shutdown_dialog";

/* Con trỏ toàn cục quản lý lớp phủ hộp thoại */
static lv_obj_t *s_dialog_overlay = NULL;

/* Xử lý sự kiện khi người dùng chọn nút "1. Có" (Đồng ý tắt nguồn) */
static void btn_yes_click_cb(lv_event_t *e) {
    (void)e;
    ESP_LOGI(TAG, "User confirmed shutdown. Requesting deep sleep...");
    ui_shutdown_dialog_close();
    ui_request_deep_sleep(); // Gửi yêu cầu tắt phần cứng và vào Deep Sleep
}

/* Xử lý sự kiện khi người dùng chọn nút "2. Không" (Hủy thao tác) */
static void btn_no_click_cb(lv_event_t *e) {
    (void)e;
    ESP_LOGI(TAG, "User cancelled shutdown.");
    ui_shutdown_dialog_close(); // Đóng hộp thoại quay về giao diện cũ
}

/* Hiển thị hộp thoại Modal xác nhận tắt nguồn */
void ui_shutdown_dialog_show(void) {
    if (s_dialog_overlay) {
        ESP_LOGW(TAG, "Shutdown dialog is already visible.");
        return;
    }

    ESP_LOGI(TAG, "Showing shutdown confirmation dialog...");

    /* 1. Tạo lớp nền che phủ tối mờ (Overlay) toàn màn hình trên layer_top */
    s_dialog_overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_dialog_overlay, WATCH_LCD_H_RES, WATCH_LCD_V_RES);
    lv_obj_align(s_dialog_overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(s_dialog_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_dialog_overlay, LV_OPA_70, 0); /* Nền mờ tối 70% để làm nổi bật hộp thoại */
    lv_obj_set_style_border_width(s_dialog_overlay, 0, 0);
    lv_obj_set_style_pad_all(s_dialog_overlay, 0, 0);
    lv_obj_clear_flag(s_dialog_overlay, LV_OBJ_FLAG_SCROLLABLE);

    /* 2. Tạo Card hộp thoại nằm ở chính giữa lớp phủ */
    lv_obj_t *card = lv_obj_create(s_dialog_overlay);
    lv_obj_set_size(card, 210, 120);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(card, COLOR_SURFACE, 0); // Sử dụng màu nền xám tối hệ thống
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 16, 0);              // Bo góc tròn 16px tạo nét thẩm mỹ hiện đại
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x3A3A3C), 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    /* 3. Tạo nhãn Tiêu đề hộp thoại */
    lv_obj_t *lbl_title = lv_label_create(card);
    lv_obj_set_style_text_font(lbl_title, UI_FONT_16, 0);
    lv_obj_set_style_text_color(lbl_title, COLOR_ORANGE, 0); // Đặt màu tiêu đề màu Cam để thu hút sự chú ý
    lv_label_set_text(lbl_title, "TẮT NGUỒN ?");
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 12);

    /* 5. Thiết lập Nút nhấn "Có" ở bên trái dưới cùng */
    lv_obj_t *btn_yes = lv_btn_create(card);
    lv_obj_set_size(btn_yes, 80, 42);
    lv_obj_align(btn_yes, LV_ALIGN_BOTTOM_LEFT, 4, -4);
    lv_obj_set_style_bg_color(btn_yes, COLOR_RED, 0); // Màu đỏ thể hiện hành động nguy hiểm/xóa dữ liệu
    lv_obj_set_style_radius(btn_yes, 12, 0);
    lv_obj_set_style_shadow_width(btn_yes, 0, 0);
    lv_obj_add_event_cb(btn_yes, btn_yes_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_yes = lv_label_create(btn_yes);
    lv_obj_set_style_text_font(lbl_yes, UI_FONT_14, 0);
    lv_obj_set_style_text_color(lbl_yes, COLOR_TEXT, 0);
    lv_label_set_text(lbl_yes, "Có");
    lv_obj_align(lbl_yes, LV_ALIGN_CENTER, 0, 0);

    /* 6. Thiết lập Nút nhấn "Không" ở bên phải dưới cùng */
    lv_obj_t *btn_no = lv_btn_create(card);
    lv_obj_set_size(btn_no, 80, 42);
    lv_obj_align(btn_no, LV_ALIGN_BOTTOM_RIGHT, -4, -4);
    lv_obj_set_style_bg_color(btn_no, COLOR_SURFACE_LT, 0);
    lv_obj_set_style_radius(btn_no, 12, 0);
    lv_obj_set_style_shadow_width(btn_no, 0, 0);
    lv_obj_add_event_cb(btn_no, btn_no_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_no = lv_label_create(btn_no);
    lv_obj_set_style_text_font(lbl_no, UI_FONT_14, 0);
    lv_obj_set_style_text_color(lbl_no, COLOR_TEXT, 0);
    lv_label_set_text(lbl_no, "Không");
    lv_obj_align(lbl_no, LV_ALIGN_CENTER, 0, 0);
}

/* Truy vấn trạng thái hiển thị của hộp thoại */
bool ui_shutdown_dialog_is_visible(void) {
    return s_dialog_overlay != NULL;
}

/* Đóng hộp thoại và giải phóng tài nguyên của các widget con bên trong */
void ui_shutdown_dialog_close(void) {
    if (s_dialog_overlay) {
        lv_obj_del(s_dialog_overlay); // Hàm này xóa overlay và toàn bộ các widget con nằm trên nó
        s_dialog_overlay = NULL;
        ESP_LOGI(TAG, "Shutdown confirmation dialog closed.");
    }
}

