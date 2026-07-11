/* Đọc cảm biến I2C phần cứng (Pin, IMU, Nhịp tim, Nút bấm). */

#ifndef HARDWARE_I2C_SENSOR_H
#define HARDWARE_I2C_SENSOR_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#include "battery.h"
#include "imu.h"
#include "heart_rate.h"

/* Đọc khối dữ liệu từ thanh ghi ngoại vi I2C
   - addr: Địa chỉ I2C thiết bị
   - reg: Địa chỉ thanh ghi cần đọc
   - data: Con trỏ tới bộ đệm lưu dữ liệu đọc được
   - len: Số lượng byte cần đọc
   Trả về: esp_err_t Trạng thái thực hiện (ESP_OK nếu thành công) */
esp_err_t hardware_i2c_sensor_i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *data, size_t len);

/* Ghi khối dữ liệu vào thanh ghi ngoại vi I2C
   - addr: Địa chỉ I2C thiết bị
   - reg: Địa chỉ thanh ghi cần ghi
   - data: Con trỏ tới bộ đệm chứa dữ liệu cần ghi
   - len: Số lượng byte cần ghi
   Trả về: esp_err_t Trạng thái thực hiện (ESP_OK nếu thành công) */
esp_err_t hardware_i2c_sensor_i2c_write_reg(uint8_t addr, uint8_t reg, const uint8_t *data, size_t len);

/* Khởi chạy luồng giám sát phần cứng nền FreeRTOS
   Trả về: esp_err_t Trạng thái khởi tạo nhiệm vụ (ESP_OK nếu thành công) */
esp_err_t hardware_i2c_sensor_start(void);

/* Lấy dữ liệu pin hiện tại (Thread-safe)
   - out: Con trỏ tới cấu trúc lưu trữ thông tin pin
   Trả về: true nếu dữ liệu pin hợp lệ */
bool hardware_i2c_sensor_get_battery(watch_battery_data_t *out);

/* Lấy dữ liệu gia tốc và số bước chân từ cảm biến IMU (Thread-safe)
   - out: Con trỏ tới cấu trúc lưu trữ thông tin IMU
   Trả về: true nếu dữ liệu IMU hợp lệ */
bool hardware_i2c_sensor_get_imu(watch_imu_data_t *out);

/* Lấy dữ liệu nhịp tim và SpO2 hiện tại (Thread-safe)
   - out: Con trỏ tới cấu trúc lưu trữ thông tin nhịp tim
   Trả về: true nếu dữ liệu nhịp tim hợp lệ */
bool hardware_i2c_sensor_get_heart_rate(watch_heart_rate_data_t *out);

/* Kích hoạt hoặc tắt chức năng đo nhịp tim liên tục của cảm biến
   - enabled: true để bật đo nhịp tim, false để chuyển sang chế độ chờ */
void hardware_i2c_sensor_set_heart_rate_enabled(bool enabled);

/* Thiết lập lại bộ đếm bước đi của IMU về 0 */
void hardware_i2c_sensor_reset_steps(void);

/* Truy vấn trạng thái tính năng Đánh thức màn hình nhanh (Quick Wake)
   Trả về: true nếu tính năng đánh thức nhanh được bật */
bool hardware_i2c_sensor_is_quick_wake_enabled(void);

/* Thiết lập bật/tắt tính năng Đánh thức nhanh khi xoay cổ tay
   - enabled: true để bật, false để tắt */
void hardware_i2c_sensor_set_quick_wake_enabled(bool enabled);

/* Lấy cấu hình thời gian chờ tắt màn hình tự động
   Trả về: số giây chờ (ví dụ: 10, 15, 30 giây) */
int hardware_i2c_sensor_get_screen_timeout(void);

/* Cấu hình thời gian chờ tắt màn hình tự động
   - seconds: số giây chờ trước khi tắt màn hình */
void hardware_i2c_sensor_set_screen_timeout(int seconds);

#endif /* HARDWARE_I2C_SENSOR_H */
