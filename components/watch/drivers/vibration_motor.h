/* Thư viện điều khiển động cơ rung (Vibration Motor)
   Sử dụng bộ phát xung PWM (LEDC) của vi điều khiển ESP32-S3 để thay đổi điện áp hiệu dụng
   cấp cho động cơ rung, qua đó tạo ra các mức rung mạnh yếu và kiểu rung khác nhau. */

#ifndef VIBRATION_MOTOR_H
#define VIBRATION_MOTOR_H

#include "esp_err.h"
#include <stdint.h>

/* Khởi tạo phần cứng PWM (LEDC) cho động cơ rung
   Trả về: esp_err_t Trạng thái thực hiện (ESP_OK nếu thành công) */
esp_err_t watch_vibration_init(void);

/* Phát một nhịp rung đơn (Pulse) với cường độ và thời gian chỉ định
   - strength: Cường độ rung (0: yếu, 1: trung bình, 2: mạnh)
   - duration_ms: Thời gian duy trì nhịp rung (miligiây) */
void watch_vibration_pulse(uint8_t strength, uint32_t duration_ms);

/* Phát rung thông báo (Notify) gồm 2 nhịp ngắn liên tiếp
   - strength: Cường độ rung (0, 1, 2) */
void watch_vibration_notify(uint8_t strength);

/* Khởi động luồng rung liên tục dành cho báo thức (Alarm)
   - strength: Cường độ rung (0, 1, 2) */
void watch_vibration_alarm_start(uint8_t strength);

/* Dừng động cơ rung ngay lập tức */
void watch_vibration_stop(void);

#endif /* VIBRATION_MOTOR_H */
