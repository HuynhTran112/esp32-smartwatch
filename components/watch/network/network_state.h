/* Thư viện quản lý kết nối và trạng thái mạng Wi-Fi
   Quản lý khởi tạo ngăn xếp TCP/IP, điều khiển kết nối mạng Station (STA),
   thực hiện quét điểm truy cập (Wi-Fi Scan) và nạp cấu hình Wi-Fi tự động để phục vụ nạp OTA. */

#ifndef NETWORK_STATE_H
#define NETWORK_STATE_H

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "esp_wifi_types.h"

// Tên tác vụ nền dùng để quét các điểm phát sóng Wi-Fi khả dụng
#define WATCH_NETWORK_WIFI_SCAN_TASK_NAME "wifi_scan"

/* Định nghĩa các trạng thái kết nối mạng Wi-Fi */
typedef enum {
    WATCH_NETWORK_WIFI_CONNECT_IDLE = 0,      // Trạng thái rảnh (chưa kết nối)
    WATCH_NETWORK_WIFI_CONNECT_CONNECTING,    // Đang trong tiến trình bắt tay kết nối
    WATCH_NETWORK_WIFI_CONNECT_SUCCESS,       // Kết nối thành công và đã nhận được IP
    WATCH_NETWORK_WIFI_CONNECT_FAILED,        // Kết nối thất bại (sai mật khẩu, ngoài vùng phủ sóng)
} watch_network_wifi_connect_state_t;

/* Khởi tạo hệ thống mạng TCP/IP và đăng ký các Event Handler Wi-Fi/IP
   Hàm này được gọi duy nhất 1 lần khi đồng hồ khởi động (app_main.c). */
void watch_network_init(void);

/* Bật sóng Wi-Fi (khởi động chế độ Station)
   Trả về: esp_err_t (ESP_OK nếu thành công) */
esp_err_t watch_network_wifi_start(void);

/* Tắt hoàn toàn sóng Wi-Fi để tiết kiệm điện
   Trả về: esp_err_t (ESP_OK nếu thành công) */
esp_err_t watch_network_wifi_stop(void);

/* Kiểm tra xem module Wi-Fi có đang được bật nguồn hay không */
bool watch_network_is_wifi_started(void);

/* Quét danh sách các điểm truy cập Wi-Fi khả dụng (Hàm chặn luồng)
   - out_records: Mảng chứa danh sách các AP quét được
   - out_num: Khai báo dung lượng mảng ban đầu, trả về số AP thực tế tìm thấy
   Trả về: esp_err_t Trạng thái quét (ESP_OK nếu thành công) */
esp_err_t watch_network_wifi_scan(wifi_ap_record_t *out_records, uint16_t *out_num);

/* Ra lệnh kết nối đến một mạng Wi-Fi chỉ định
   - ssid: Tên điểm phát Wi-Fi
   - password: Mật khẩu truy cập
   Trả về: esp_err_t Trạng thái gửi lệnh thành công */
esp_err_t watch_network_wifi_connect(const char *ssid, const char *password);

/* Tự động kết nối đến mạng Wi-Fi đã lưu lần cuối trong NVS Flash */
esp_err_t watch_network_wifi_connect_saved(void);

/* Ngắt kết nối Wi-Fi hiện hành và dọn dẹp các cờ trạng thái liên quan */
esp_err_t watch_network_wifi_disconnect(void);

/* Đọc thông tin mạng Wi-Fi đã kết nối lần cuối từ bộ nhớ đệm (Thread-safe)
   - ssid: Bộ đệm chứa SSID trả về
   - ssid_size: Kích thước bộ đệm SSID
   - password: Bộ đệm chứa mật khẩu trả về
   - password_size: Kích thước bộ đệm mật khẩu
   Trả về: true Có thông tin Wi-Fi đã lưu trước đó */
bool watch_network_get_last_wifi_credentials(char *ssid, size_t ssid_size,
                                             char *password, size_t password_size);

/* Bắt đầu ghi nhận tiến trình kết nối Wi-Fi mới */
void watch_network_begin_wifi_connect_attempt(const char *ssid, const char *password);

/* Kiểm tra xem đồng hồ hiện tại có đang kết nối mạng Wi-Fi ổn định hay không
   Trả về: true Đã có kết nối Wi-Fi và có IP hợp lệ */
bool watch_network_is_wifi_connected(void);

/* Lấy trạng thái kết nối Wi-Fi chi tiết hiện hành (Thread-safe) */
watch_network_wifi_connect_state_t watch_network_get_wifi_connect_state(void);

/* Dọn dẹp trạng thái kết nối Wi-Fi sau khi xử lý xong sự kiện */
void watch_network_clear_wifi_connect_state(void);

/* Lấy tên SSID của mạng Wi-Fi đang kết nối hiện hành
   Trả về: const char* Con trỏ chứa tên mạng SSID */
const char *watch_network_get_connected_ssid(void);

/* So sánh xem SSID hiện hành có khớp với SSID truyền vào không */
bool watch_network_is_connected_ssid(const char *ssid);

#endif // NETWORK_STATE_H
