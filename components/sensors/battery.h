/* Quản lý IC đo pin MAX17048G. */

#ifndef BATTERY_H
#define BATTERY_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* Cấu trúc chứa dữ liệu trạng thái pin đọc được từ IC MAX17048G */
typedef struct {
    float voltage_v;         // Điện áp pin hiện tại (Volt)
    float soc_percent;       // Dung lượng pin ước tính theo phần trăm (%)
    float charge_rate;       // Tốc độ sạc/xả (%/h, giá trị dương là đang sạc, âm là đang xả)
    bool  low_battery_alert; // Cờ báo hiệu trạng thái pin yếu (ALRT được kích hoạt)
    bool  valid;             // Trạng thái dữ liệu đọc ra có hợp lệ hay không
} watch_battery_data_t;

/* Cấu hình chân GPIO nhận ngắt Alert pin yếu và thiết lập ngưỡng báo động trên IC MAX17048G
   - threshold_percent: Phần trăm pin yếu để kích hoạt cảnh báo (1% - 32%)
   Trả về: esp_err_t (ESP_OK nếu thành công) */
esp_err_t battery_init_alert(uint8_t threshold_percent);

/* Kiểm tra xem có ngắt cảnh báo pin yếu đang chờ xử lý hay không
   Trả về: true nếu có ngắt pin yếu xảy ra */
bool battery_take_alert(void);

/* Xóa cờ cảnh báo Alert trên thanh ghi IC để chuẩn bị nhận các cảnh báo tiếp theo
   Trả về: esp_err_t (ESP_OK nếu thành công) */
esp_err_t battery_clear_alert(void);

/* Đọc toàn bộ thông số điện áp, dung lượng % và tốc độ sạc/xả từ IC đo pin qua I2C
   - out: Con trỏ cấu trúc chứa dữ liệu pin xuất ra
   Trả về: true nếu đọc I2C thành công và dữ liệu hợp lệ */
bool battery_read(watch_battery_data_t *out);

#endif /* BATTERY_H */
