/* Thư viện giao tiếp Bluetooth Low Energy (BLE) GATT Server.
   Quản lý thông báo, đồng bộ RTC, nhận chỉ đường bản đồ và truyền vết GPS lên điện thoại. */

#ifndef BLUETOOTH_MANAGER_H
#define BLUETOOTH_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- CẤU HÌNH BỘ NHỚ THÔNG BÁO ---
// Giới hạn lưu tối đa 10 thông báo trong bộ nhớ tạm
#define WATCH_BLUETOOTH_NOTIF_MAX           10

// Chiều dài tối đa các trường thông tin tin nhắn
#define WATCH_BLUETOOTH_NOTIF_APP_LEN       32
#define WATCH_BLUETOOTH_NOTIF_TITLE_LEN     64
#define WATCH_BLUETOOTH_NOTIF_CONTENT_LEN   256

/* Cấu trúc thông tin của một thông báo nhận được từ điện thoại qua BLE */
typedef struct {
    char     app_name[WATCH_BLUETOOTH_NOTIF_APP_LEN];
    char     title[WATCH_BLUETOOTH_NOTIF_TITLE_LEN];
    char     content[WATCH_BLUETOOTH_NOTIF_CONTENT_LEN];
    uint32_t timestamp;                            // Mốc thời gian hệ thống lúc nhận được thông báo (ms)
    uint32_t seq_id;                               // Mã ID tăng dần duy nhất để phân loại thông báo
    bool     is_new;                               // Cờ báo thông báo mới chưa được người dùng đọc
} watch_bluetooth_notification_t;

/* Con trỏ hàm callback xử lý sự kiện có thông báo mới đổ chuông */
typedef void (*watch_bluetooth_notif_new_cb_t)(const watch_bluetooth_notification_t *notif);

/* Danh mục các hình mũi tên chỉ đường điều hướng (Navigation Turn Type) */
typedef enum {
    WATCH_BLUETOOTH_NAV_TURN_UNKNOWN = 0,      // Không xác định
    WATCH_BLUETOOTH_NAV_TURN_STRAIGHT,         // Đi thẳng
    WATCH_BLUETOOTH_NAV_TURN_LEFT,             // Rẽ trái
    WATCH_BLUETOOTH_NAV_TURN_RIGHT,            // Rẽ phải
    WATCH_BLUETOOTH_NAV_TURN_SLIGHT_LEFT,      // Chếch trái nhẹ
    WATCH_BLUETOOTH_NAV_TURN_SLIGHT_RIGHT,     // Chếch phải nhẹ
    WATCH_BLUETOOTH_NAV_TURN_KEEP_LEFT,        // Đi bám bên trái
    WATCH_BLUETOOTH_NAV_TURN_KEEP_RIGHT,       // Đi bám bên phải
    WATCH_BLUETOOTH_NAV_TURN_UTURN,            // Quay đầu xe
    WATCH_BLUETOOTH_NAV_TURN_ROUNDABOUT,       // Đi vào vòng xuyến
    WATCH_BLUETOOTH_NAV_TURN_ARRIVE            // Đã tới điểm đến
} watch_bluetooth_nav_turn_t;

// Độ dài tối đa tên con đường hiển thị (64 byte)
#define WATCH_BLUETOOTH_NAV_ROAD_LEN 64

/* Cấu trúc lưu trữ dữ liệu chỉ đường đồng bộ từ ứng dụng điện thoại */
typedef struct {
    bool active;                                   // Trạng thái điều hướng bản đồ đang hoạt động
    watch_bluetooth_nav_turn_t current_turn;       // Hướng rẽ hiện hành
    char primary_road[WATCH_BLUETOOTH_NAV_ROAD_LEN]; // Tên con đường hiện tại đang di chuyển
    char eta_to_turn[48];                          // Khoảng cách hoặc thời gian chờ rẽ tiếp theo (ví dụ: "200m", "2 phút")
    watch_bluetooth_nav_turn_t next_turn;          // Hướng rẽ của chặng kế tiếp
    char next_road[WATCH_BLUETOOTH_NAV_ROAD_LEN];  // Tên đường của chặng kế tiếp
    char total_remaining[48];                      // Tổng quãng đường/thời gian còn lại tới đích
} watch_bluetooth_nav_data_t;

/* === CÁC API PHẦN CỨNG VÀ KẾT NỐI BLUETOOTH === */

/* Khởi tạo phần cứng Bluetooth Controller, Bluedroid, đăng ký GAP/GATTS và thiết lập dịch vụ BLE
   Trả về: esp_err_t Trạng thái thực hiện (ESP_OK nếu thành công) */
esp_err_t watch_bluetooth_init(void);

/* Hủy đăng ký và tắt nguồn hoàn toàn bộ phát sóng Bluetooth để tiết kiệm pin */
esp_err_t watch_bluetooth_deinit(void);

/* Đếm tổng số thông báo đang có trong bộ nhớ tạm
   Trả về: int Số lượng thông báo (0 đến WATCH_BLUETOOTH_NOTIF_MAX) */
int watch_bluetooth_get_count(void);

/* Lấy số lượng thông báo cũ đã bị đẩy ghi đè do đầy bộ đệm xoay vòng */
uint32_t watch_bluetooth_get_overflow_count(void);

/* Lấy thông báo theo vị trí xếp hạng (0: thông báo mới nhất)
   - index: Vị trí chỉ mục
   - out_notif: Con trỏ cấu trúc sao chép dữ liệu ra
   Trả về: true Sao chép thành công */
bool watch_bluetooth_get(int index, watch_bluetooth_notification_t *out_notif);

/* Kiểm tra xem đồng hồ có tin nhắn/thông báo nào chưa đọc hay không */
bool watch_bluetooth_has_new(void);

/* Đánh dấu một thông báo là đã đọc */
void watch_bluetooth_mark_read(int index);

/* Xóa một thông báo cụ thể dựa vào mã ID duy nhất seq_id
   Trả về: true Xóa thành công */
bool watch_bluetooth_delete_by_seq_id(uint32_t seq_id);

/* Xóa toàn bộ thông báo đã lưu trong bộ đệm RAM của đồng hồ */
void watch_bluetooth_clear_all(void);

/* Kiểm tra xem đồng hồ hiện tại có liên kết BLE thành công với điện thoại không */
bool watch_bluetooth_is_connected(void);

/* Thiết lập hàm callback khi có thông báo cuộc gọi/tin nhắn mới đổ xuống */
void watch_bluetooth_set_new_callback(watch_bluetooth_notif_new_cb_t callback);

/* Đọc thông tin hướng dẫn chỉ đường điều hướng hiện hành (Thread-safe)
   - out_nav: Con trỏ cấu trúc dữ liệu xuất ra
   Trả về: true Đang chạy chỉ đường */
bool watch_bluetooth_get_navigation_data(watch_bluetooth_nav_data_t *out_nav);

/* Xóa trạng thái chỉ đường điều hướng khi kết thúc hành trình */
void watch_bluetooth_clear_navigation_data(void);

/* Gửi lệnh hoặc chuỗi dữ liệu ngược từ đồng hồ lên app điện thoại qua cơ chế GATTS Notify
   - command: Chuỗi ký tự lệnh cần truyền đi */
esp_err_t watch_bluetooth_send_command(const char *command);

/* Tách chuỗi vết tọa độ hành trình thành từng mảnh nhỏ để truyền tải BLE an toàn */
void watch_bluetooth_send_track_chunks(const char *track);

/* Gửi lịch sử tọa độ tập luyện tại chỉ mục cụ thể qua BLE */
void watch_bluetooth_send_history_track(size_t index);

/* Bật hoặc tắt trạng thái Bluetooth của hệ thống */
esp_err_t watch_bluetooth_set_enabled(bool enabled);

esp_err_t watch_bluetooth_control_init(void);
esp_err_t watch_bluetooth_request_enabled(bool enabled);
bool watch_bluetooth_is_transitioning(void);

/* Kiểm tra trạng thái Bluetooth có đang bật hay không */
bool watch_bluetooth_is_enabled(void);

/* Lấy thế hệ/thứ tự kết nối Bluetooth hiện hành để quản lý trạng thái */
uint32_t watch_bluetooth_get_connection_generation(void);

#ifdef __cplusplus
}
#endif

#endif // BLUETOOTH_MANAGER_H
