/* Quản lý cảm biến nhịp tim & SpO2. */

#ifndef HEART_RATE_H
#define HEART_RATE_H

#include <stdbool.h>
#include <stdint.h>

/* Cấu trúc lưu trữ dữ liệu đo sinh học từ cảm biến */
typedef struct {
    float heart_rate;  // Nhịp tim ước tính (BPM)
    float spo2;        // Nồng độ oxy trong máu (%)
    uint32_t ir_raw;   // Giá trị ánh sáng hồng ngoại thô
    uint32_t red_raw;  // Giá trị ánh sáng đỏ thô
    uint8_t quality;   // Chỉ số chất lượng tín hiệu (0-100)
    bool spo2_valid;   // Cờ báo hiệu chỉ số SpO2 đủ tin cậy
    bool valid;        // Cờ báo hiệu chỉ số nhịp tim đủ tin cậy
} watch_heart_rate_data_t;

/* Khởi tạo phần cứng cảm biến nhịp tim
   Trả về: true nếu thành công */
bool heart_rate_init(void);

/* Đọc dữ liệu từ FIFO cảm biến, tính toán nhịp tim & SpO2 qua bộ lọc DSP và bộ ước lượng
   - out: Con trỏ cấu trúc dữ liệu lưu kết quả đo
   Trả về: true nếu quá trình đọc FIFO và tính toán diễn ra thành công */
bool heart_rate_read(watch_heart_rate_data_t *out);

/* Tắt đèn LED hồng ngoại/đỏ và chuyển chế độ mode về Standby để tiết kiệm điện năng tiêu thụ */
void heart_rate_shutdown(void);

#endif /* HEART_RATE_H */
