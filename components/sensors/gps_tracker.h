/* Quản lý ghi nhận hành trình GPS và chế độ hoạt động thể thao. */

#ifndef GPS_TRACKER_H
#define GPS_TRACKER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

/* Cấu trúc chứa dữ liệu trạng thái và sai số định vị GPS thời gian thực */
typedef struct {
    bool fix_valid;                 // Định vị GPS hợp lệ (đã khóa vệ tinh)
    bool uart_seen;                 // Đã nhận được dữ liệu vật lý UART từ module GPS
    bool nmea_seen;                 // Nhận dạng được bản tin cấu trúc NMEA hợp lệ
    double latitude_deg;            // Vĩ độ hiện hành (độ thập phân)
    double longitude_deg;           // Kinh độ hiện hành (độ thập phân)
    float speed_kmh;                // Tốc độ di chuyển tức thời (km/h)
    float total_distance_km;        // Tổng quãng đường đi được tích lũy (km)
    uint32_t last_rx_ms;            // Mốc thời gian hệ thống nhận được dữ liệu cuối (ms)
    uint32_t fix_age_ms;            // Thời gian kể từ khi có toạ độ hợp lệ cuối cùng (ms)
    uint32_t raw_bytes;             // Tổng lượng byte dữ liệu UART đã nhận
    uint32_t nmea_lines;            // Tổng số lượng dòng tin NMEA đã xử lý
    unsigned int satellites_in_use; // Số lượng vệ tinh đang được sử dụng để định vị
    unsigned int satellites_in_view;// Số lượng vệ tinh nằm trong tầm nhìn của anten
    float avg_cn0_dbhz;             // Cường độ tín hiệu vệ tinh trung bình (dB-Hz)
    unsigned int fix_quality;       // Chất lượng định vị (GGA fix quality)
    float pdop;                     // Sai số vị trí không gian 3D
    float hdop;                     // Sai số vị trí mặt ngang
    float vdop;                     // Sai số độ cao chiều dọc
    int current_baud;               // Tốc độ baudrate kết nối UART hiện hành
} watch_gps_metrics_t;

/* Cấu trúc lưu trữ dữ liệu toạ độ của từng điểm trên hành trình lưu trữ */
typedef struct {
    double latitude_deg;        // Vĩ độ điểm hành trình
    double longitude_deg;       // Kinh độ điểm hành trình
    float hdop;                 // Sai số mặt phẳng ngang HDOP tại thời điểm ghi mẫu
    uint8_t satellites_in_use;  // Số vệ tinh sử dụng khi ghi mẫu
    bool reconnect_segment;     // Cờ đánh dấu điểm bắt đầu đoạn mới (sau khi mất sóng dài)
} watch_gps_track_point_t;

/* Danh mục các chế độ hoạt động thể thao tích hợp trên đồng hồ */
typedef enum {
    WATCH_ACTIVITY_MODE_NONE = 0, // Không tập luyện (chế độ bình thường)
    WATCH_ACTIVITY_MODE_WALKING,  // Đi bộ thể thao
    WATCH_ACTIVITY_MODE_CYCLING,  // Đạp xe thể thao
} watch_activity_mode_t;

// Số lượng điểm tọa độ lưu trữ tối đa trong Ring buffer trên PSRAM (~10-16 km hành trình)
#define WATCH_GPS_TRACK_MAX_POINTS 2048

// Dung lượng tối đa của chuỗi văn bản vết hành trình được format (80KB)
#define WATCH_GPS_TRACK_TEXT_MAX   81920

/* Kích hoạt trực tiếp module GPS (bật UART driver, tạo tác vụ đọc NMEA nền) */
void watch_gps_start(void);

/* Tắt module GPS, xóa UART driver và giải phóng bộ nhớ để tiết kiệm pin */
void watch_gps_stop(void);

/* Khởi tạo hàng đợi điều khiển hoạt động của GPS (gọi tại app_main.c)
   Trả về: esp_err_t (ESP_OK nếu thành công) */
esp_err_t watch_gps_control_init(void);

/* Gửi yêu cầu khởi chạy GPS một cách an toàn thông qua hàng đợi điều khiển */
void watch_gps_request_start(void);

/* Gửi yêu cầu dừng hoạt động GPS thông qua hàng đợi điều khiển */
void watch_gps_request_stop(void);

/* Truy vấn các thông số đo đạc GPS thời gian thực (Thread-safe)
   - out: Con trỏ tới cấu trúc dữ liệu lưu thông số trả về
   Trả về: true nếu truy vấn thành công */
bool watch_gps_get_metrics(watch_gps_metrics_t *out);

/* Kiểm tra xem tác vụ GPS có đang hoạt động hay không */
bool watch_gps_is_active(void);

/* Thiết lập chế độ hoạt động thể thao hiện hành cho bộ theo dõi hành trình
   - mode: Chế độ thể thao chỉ định */
void watch_gps_set_activity_mode(watch_activity_mode_t mode);

/* Reset toàn bộ vết đường đi cũ, đặt quãng đường tích lũy về 0 */
void watch_gps_reset_track(void);

/* Lấy danh sách toàn bộ các điểm toạ độ hành trình đang lưu trữ trong bộ đệm xoay vòng (Thread-safe)
   - out: Bộ đệm chứa các điểm toạ độ trả về
   - max_points: Số điểm tối đa bộ đệm out có thể chứa
   Trả về: Số lượng điểm thực tế sao chép ra */
size_t watch_gps_get_track(watch_gps_track_point_t *out, size_t max_points);

/* Chuyển đổi danh sách vết toạ độ thành chuỗi văn bản phân cách bởi dấu chấm phẩy (;) phục vụ truyền tải qua Bluetooth
   - out: Con trỏ chuỗi lưu kết quả format
   - out_size: Dung lượng tối đa của chuỗi out
   Trả về: Độ dài chuỗi text thu được thực tế */
int watch_gps_format_track(char *out, size_t out_size);

#endif /* GPS_TRACKER_H */
