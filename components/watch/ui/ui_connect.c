/* Thiết kế giao diện và quản lý trạng thái kết nối Bluetooth Low Energy (BLE)
   Phân hệ quản lý kết nối không dây BLE giữa đồng hồ thông minh và điện thoại đồng hành.
   Các tính năng chính bao gồm:
   1. Bật/Tắt BLE: Cho phép người dùng chủ động kiểm soát bộ thu phát RF nhằm tối ưu hóa năng lượng (tiết kiệm pin).
   2. Theo dõi trạng thái kết nối thời gian thực: Cập nhật trực quan trạng thái (Đã tắt, Đang chờ kết nối, Đã kết nối) thông qua màu sắc và nhãn văn bản.
   3. Đồng bộ hóa thông báo: Hiển thị số lượng thông báo chưa đọc nhận được từ điện thoại qua liên kết Bluetooth.
   4. Tự động cập nhật giao diện thông qua bộ hẹn giờ (LVGL Timer) tuần hoàn 500ms. */
#include "board_config.h"
#include "ui.h"
#include "ui_navigation.h"
#include "ui_screens.h"
#include "ui_utils.h"
#include "lvgl.h"
#include <stdbool.h>
#include "bluetooth_manager.h"

/* Khai báo phông chữ (Font) tùy chỉnh chứa biểu tượng Bluetooth */
LV_FONT_DECLARE(ble);

/* Mã Hex của các ký tự biểu tượng Bluetooth từ phông chữ custom FontAwesome */
#define ICON_BLE_BG "\xef\x8a\x93"
#define ICON_BLE    "\xef\x8a\x94"

/* Các biến tĩnh (static) quản lý các đối tượng giao diện LVGL và thông số trạng thái */
static lv_obj_t *s_connect_status_label = NULL;       /* Nhãn hiển thị chuỗi trạng thái kết nối */
static lv_obj_t *s_connect_bt_button = NULL;          /* Nút bấm chuyển đổi trạng thái bật/tắt Bluetooth */
static lv_obj_t *s_connect_notif_count_label = NULL;   /* Nhãn hiển thị số lượng thông báo BLE nhận được */
static lv_obj_t *s_connect_bt_icon = NULL;            /* Biểu tượng Bluetooth chính giữa màn hình */
static lv_timer_t *s_connect_timer = NULL;            /* Bộ hẹn giờ để quét cập nhật trạng thái định kỳ */
static uint32_t s_connect_connected_ms = 0;           /* Thời gian tích lũy kể từ khi kết nối thành công (ms) */
static uint32_t s_connect_connection_generation = 0;  /* Số thế hệ kết nối (để phát hiện kết nối mới) */

/* Hàm callback giải phóng tài nguyên khi màn hình kết nối bị xóa (Delete Event)
   - e: Đối tượng sự kiện của LVGL */
static void connect_screen_delete_cb(lv_event_t *e) {
    (void)e;
    /* Hủy bộ hẹn giờ cập nhật giao diện để tránh rò rỉ tài nguyên */
    if (s_connect_timer) {
        lv_timer_del(s_connect_timer);
        s_connect_timer = NULL;
    }
    /* Đặt lại các con trỏ giao diện về NULL để tránh con trỏ hoang (dangling pointers) */
    s_connect_status_label = NULL;
    s_connect_bt_button = NULL;
    s_connect_notif_count_label = NULL;
    s_connect_bt_icon = NULL;
    s_connect_connected_ms = 0;
    s_connect_connection_generation = 0;
}

/* Hàm cập nhật trạng thái hiển thị của giao diện kết nối Bluetooth
   Dựa trên trạng thái bật/tắt BLE và trạng thái kết nối để thay đổi nội dung văn bản,
   màu sắc biểu tượng và nút điều khiển một cách tương ứng. */
static void connect_refresh_status(void) {
    /* Đảm bảo các đối tượng giao diện cốt lõi đã được khởi tạo */
    if (!s_connect_status_label || !s_connect_bt_button || !s_connect_bt_icon) return;

    bool enabled = watch_bluetooth_is_enabled();     /* Kiểm tra xem Bluetooth có đang được bật */
    bool connected = watch_bluetooth_is_connected(); /* Kiểm tra xem đã kết nối với điện thoại */
    lv_obj_t *circle_bg = lv_obj_get_parent(s_connect_bt_icon); /* Lấy container hình tròn bao quanh icon */

    /* Kịch bản 1: Bluetooth đang bị TẮT */
    if (!enabled) {
        s_connect_connected_ms = 0;
        s_connect_connection_generation = 0;
        /* Cập nhật nhãn trạng thái và màu chữ xám (Muted) */
        ui_label_set_text_if_changed(s_connect_status_label, ui_tr(UI_TXT_BLUETOOTH_OFF));
        lv_obj_set_style_text_color(s_connect_status_label, COLOR_TEXT_MUTED, 0);
        /* Cập nhật nhãn nút bấm thành "Bật BLE" với màu nền chủ đạo */
        ui_label_set_text_if_changed(lv_obj_get_child(s_connect_bt_button, 0), ui_tr(UI_TXT_START_BLE));
        lv_obj_set_style_bg_color(s_connect_bt_button, COLOR_PRIMARY, 0);
        
        /* Đặt nền vòng tròn biểu tượng và màu biểu tượng về trạng thái ẩn/xám */
        if (circle_bg) lv_obj_set_style_bg_color(circle_bg, COLOR_SURFACE, 0);
        ui_label_set_text_if_changed(s_connect_bt_icon, ICON_BLE_BG);
        lv_obj_set_style_text_color(s_connect_bt_icon, COLOR_TEXT_MUTED, 0);
    } 
    /* Kịch bản 2: Bluetooth BẬT và ĐÃ KẾT NỐI thành công */
    else if (connected) {
        uint32_t generation = watch_bluetooth_get_connection_generation();
        /* Nếu phát hiện kết nối mới (thay đổi thế hệ kết nối), đặt lại bộ đếm thời gian */
        if (generation != s_connect_connection_generation) {
            s_connect_connection_generation = generation;
            s_connect_connected_ms = 0;
        }
        s_connect_connected_ms += 500; /* Tăng thời gian kết nối (chu kỳ timer 500ms) */
        /* Cập nhật nhãn trạng thái thành "Đã kết nối" với màu chính của hệ thống (màu xanh dương) */
        ui_label_set_text_if_changed(s_connect_status_label, ui_tr(UI_TXT_CONNECTED));
        lv_obj_set_style_text_color(s_connect_status_label, COLOR_PRIMARY, 0);
        /* Cập nhật nhãn nút bấm thành "Ngắt kết nối" với màu đỏ cảnh báo */
        ui_label_set_text_if_changed(lv_obj_get_child(s_connect_bt_button, 0), ui_tr(UI_TXT_DISCONNECT));
        lv_obj_set_style_bg_color(s_connect_bt_button, COLOR_RED, 0);
        
        /* Đổi màu nền vòng tròn biểu tượng sang màu thương hiệu (Primary - màu xanh dương) */
        if (circle_bg) lv_obj_set_style_bg_color(circle_bg, COLOR_PRIMARY, 0);
        
        /* Giữ biểu tượng Bluetooth màu trắng nổi bật trên nền xanh dương */
        ui_label_set_text_if_changed(s_connect_bt_icon, ICON_BLE_BG);
        lv_obj_set_style_text_color(s_connect_bt_icon, lv_color_white(), 0);
    } 
    /* Kịch bản 3: Bluetooth BẬT nhưng CHƯA KẾT NỐI (Đang quảng bá - Advertising) */
    else {
        s_connect_connected_ms = 0;
        s_connect_connection_generation = watch_bluetooth_get_connection_generation();
        /* Cập nhật nhãn trạng thái thành "Đang chờ kết nối..." với màu xám */
        ui_label_set_text_if_changed(s_connect_status_label, ui_tr(UI_TXT_CONNECTING));
        lv_obj_set_style_text_color(s_connect_status_label, COLOR_TEXT_MUTED, 0);
        /* Cập nhật nút điều khiển sang "Dừng BLE" với màu đỏ */
        ui_label_set_text_if_changed(lv_obj_get_child(s_connect_bt_button, 0), ui_tr(UI_TXT_STOP_BLE));
        lv_obj_set_style_bg_color(s_connect_bt_button, COLOR_RED, 0);
        
        /* Giữ vòng tròn biểu tượng màu xám ẩn */
        if (circle_bg) lv_obj_set_style_bg_color(circle_bg, COLOR_SURFACE, 0);
        ui_label_set_text_if_changed(s_connect_bt_icon, ICON_BLE_BG);
        lv_obj_set_style_text_color(s_connect_bt_icon, COLOR_TEXT_MUTED, 0);
    }

    /* Cập nhật số lượng thông báo BLE nhận được từ thiết bị di động */
    if (s_connect_notif_count_label) {
        int count = watch_bluetooth_get_count();
        if (count > 0) {
            char buf[32];
            snprintf(buf, sizeof(buf), ui_tr(UI_TXT_NOTIFICATIONS_COUNT), count);
            ui_label_set_text_if_changed(s_connect_notif_count_label, buf);
            lv_obj_clear_flag(s_connect_notif_count_label, LV_OBJ_FLAG_HIDDEN); /* Hiện nhãn thông báo */
        } else {
            lv_obj_add_flag(s_connect_notif_count_label, LV_OBJ_FLAG_HIDDEN);    /* Ẩn nhãn nếu không có thông báo */
        }
    }
}

/* Hàm callback định kỳ cho bộ hẹn giờ cập nhật giao diện kết nối
   - t: Đối tượng timer của LVGL */
static void connect_refresh_timer_cb(lv_timer_t *t) {
    (void)t;
    connect_refresh_status();
}

/* Hàm callback xử lý sự kiện click vào nút Bật/Tắt Bluetooth
   Đọc trạng thái Bluetooth hiện tại và thực hiện đảo ngược trạng thái (toggle).
   - e: Đối tượng sự kiện của LVGL */
static void connect_bt_toggle_cb(lv_event_t *e) {
    (void)e;
    if (watch_bluetooth_is_transitioning()) return;
    bool enabled = watch_bluetooth_is_enabled();
    watch_bluetooth_request_enabled(!enabled);
}

/* Hàm khởi tạo màn hình cấu hình kết nối Bluetooth
   Xây dựng cây phân cấp đối tượng đồ họa LVGL bao gồm biểu tượng Bluetooth trung tâm,
   tên thiết bị phát quảng bá, trạng thái kết nối hiện thời và nút nhấn thay đổi trạng thái.
   Trả về: lv_obj_t* Con trỏ trỏ tới đối tượng màn hình vừa tạo */
lv_obj_t *ui_connect_screen_create(void) {
    /* Khởi tạo màn hình cơ bản và liên kết ID màn hình kết nối */
    lv_obj_t *scr = ui_navigation_create_screen_with_id(UI_SCREEN_CONNECT);
    lv_obj_add_event_cb(scr, connect_screen_delete_cb, LV_EVENT_DELETE, NULL);
    s_connect_connected_ms = 0;
    s_connect_connection_generation = 0;

    /* Thiết kế vòng tròn lớn làm nền cho biểu tượng Bluetooth ở trung tâm */
    lv_obj_t *bt_card = lv_obj_create(scr);
    lv_obj_set_size(bt_card, 68, 68);
    lv_obj_set_style_radius(bt_card, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(bt_card, COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(bt_card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bt_card, 0, 0);
    lv_obj_clear_flag(bt_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(bt_card, LV_ALIGN_CENTER, 0, -48);

    /* Khởi tạo nhãn biểu tượng Bluetooth nằm chính giữa vòng tròn nền */
    s_connect_bt_icon = lv_label_create(bt_card);
    lv_label_set_text(s_connect_bt_icon, ICON_BLE_BG);
    lv_obj_set_style_text_font(s_connect_bt_icon, &ble, 0);
    lv_obj_set_style_text_color(s_connect_bt_icon, COLOR_PRIMARY, 0);
    lv_obj_center(s_connect_bt_icon);

    /* Hiển thị Tên thiết bị Bluetooth quảng bá cố định của Smartwatch */
    lv_obj_t *lbl_name = lv_label_create(scr);
    lv_label_set_text(lbl_name, "Smartwatch_DATN");
    lv_obj_set_style_text_font(lbl_name, UI_FONT_16, 0);
    lv_obj_set_style_text_color(lbl_name, COLOR_TEXT, 0);
    lv_obj_align(lbl_name, LV_ALIGN_CENTER, 0, 20);

    /* Khởi tạo nhãn trạng thái kết nối dạng văn bản bên dưới tên thiết bị */
    s_connect_status_label = lv_label_create(scr);
    lv_obj_set_style_text_font(s_connect_status_label, UI_FONT_14, 0);
    lv_obj_align(s_connect_status_label, LV_ALIGN_CENTER, 0, 42);

    /* Khởi tạo nhãn đếm số lượng thông báo chưa đọc */
    s_connect_notif_count_label = lv_label_create(scr);
    lv_obj_set_style_text_font(s_connect_notif_count_label, UI_FONT_12, 0);
    lv_obj_set_style_text_color(s_connect_notif_count_label, COLOR_TEXT_MUTED, 0);
    lv_obj_align(s_connect_notif_count_label, LV_ALIGN_CENTER, 0, 62);

    /* Thiết kế nút bấm tròn cạnh (Rounded Button) để bật/tắt kết nối */
    s_connect_bt_button = lv_btn_create(scr);
    lv_obj_set_size(s_connect_bt_button, 160, 44);
    lv_obj_set_style_radius(s_connect_bt_button, 22, 0);
    lv_obj_set_style_bg_color(s_connect_bt_button, COLOR_PRIMARY, 0);
    lv_obj_set_style_shadow_width(s_connect_bt_button, 0, 0);
    lv_obj_add_event_cb(s_connect_bt_button, connect_bt_toggle_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(s_connect_bt_button, LV_ALIGN_BOTTOM_MID, 0, -25);
    
    /* Thiết lập nhãn văn bản bên trong nút bấm */
    lv_obj_t *lb = lv_label_create(s_connect_bt_button);
    lv_label_set_text(lb, "");
    lv_obj_set_style_text_font(lb, UI_FONT_14, 0);
    lv_obj_center(lb);

    /* Thực hiện làm mới giao diện lần đầu dựa trên trạng thái thực tế */
    connect_refresh_status();
    
    /* Khởi động Timer chu kỳ 500ms để theo dõi thay đổi kết nối BLE bất đồng bộ */
    s_connect_timer = lv_timer_create(connect_refresh_timer_cb, 500, NULL);

    /* Đăng ký cử chỉ vuốt chuyển màn hình điều hướng */
    ui_navigation_add_gesture(scr);
    
    return scr;
}
