/* Thư viện nâng cấp phần mềm qua mạng không dây OTA (Over-The-Air)
   Sử dụng thư viện bảo mật esp_https_ota để tải bản build phần mềm mới từ máy chủ HTTPS,
   ghi trực tiếp vào phân vùng ảo dự phòng của ESP32-S3 và tự động hoán đổi phân vùng khởi động khi thành công. */

#ifndef OTA_SERVICE_H
#define OTA_SERVICE_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* Định nghĩa các trạng thái của tiến trình OTA */
typedef enum {
    WATCH_OTA_IDLE = 0,      // Trạng thái rảnh
    WATCH_OTA_RUNNING,       // Đang tải và nạp phần mềm
    WATCH_OTA_SUCCEEDED,     // Nâng cấp thành công (đang đợi khởi động lại)
    WATCH_OTA_FAILED,        // Nâng cấp thất bại do lỗi mạng hoặc xác thực
} watch_ota_state_t;

/* Cấu trúc lưu trữ thông tin chi tiết tiến trình OTA phục vụ vẽ UI */
typedef struct {
    watch_ota_state_t state;     // Trạng thái hiện tại
    int progress_percent;        // Tiến độ tải về (% từ 0-100, hoặc -1 nếu đang kết nối)
    int image_size;              // Tổng kích thước file firmware (bytes)
    int bytes_read;              // Số lượng bytes thực tế đã tải về thành công
    esp_err_t last_error;        // Mã lỗi ESP-IDF gần nhất nếu thất bại
    char new_version[32];        // Dành cho tương thích UI; OTA hiện không yêu cầu version
    char message[96];            // Chuỗi ký tự trạng thái hiển thị trực quan lên màn hình
} watch_ota_status_t;

/* Khởi tạo các biến đồng bộ và Mutex bảo đảm an toàn dữ liệu OTA
   Trả về: esp_err_t Trạng thái khởi tạo */
esp_err_t watch_ota_init(void);

/* Tạo tác vụ FreeRTOS phụ chạy nền để thực hiện tải và nạp firmware qua HTTPS
   Trả về: esp_err_t Trạng thái bắt đầu tác vụ (ESP_OK nếu thành công) */
esp_err_t watch_ota_start(void);

/* Lấy trạng thái tiến độ OTA hiện hành để cập nhật thanh tiến trình hiển thị (Thread-safe)
   - out: Con trỏ cấu trúc chứa thông tin xuất */
void watch_ota_get_status(watch_ota_status_t *out);

/* Kiểm tra xem đã có đường link URL máy chủ nạp phần mềm hay chưa */
bool watch_ota_url_configured(void);

bool watch_ota_url_is_allowed(const char *url);

#endif // OTA_SERVICE_H
