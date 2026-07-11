/* Đọc dữ liệu từ cảm biến đo pin qua I2C. */

#include "battery.h"
#include "board_config.h"
#include "hardware_i2c_sensor.h"
#include "driver/gpio.h"
#include "esp_attr.h"

// Địa chỉ I2C và thanh ghi cảm biến đo pin
#define MAX17048_I2C_ADDR     0x36 // Địa chỉ I2C mặc định
#define MAX17048_REG_VCELL    0x02 // Thanh ghi lưu điện áp tức thời
#define MAX17048_REG_SOC      0x04 // Thanh ghi lưu phần trăm pin hiện tại (0% - 100%)
#define MAX17048_REG_CONFIG   0x0C // Thanh ghi cấu hình (ngưỡng báo pin yếu và cờ ngắt)
#define MAX17048_REG_CRATE    0x16 // Thanh ghi đo tốc độ sạc/xả (%/giờ)

// Trọng số LSB để quy đổi giá trị đo từ cảm biến
// Lý do chọn 0.000078125f: Độ phân giải đo điện áp là 78.125 uV mỗi LSB.
#define MAX17048_VCELL_LSB_V  0.000078125f
// Lý do chọn 0.208f: Trọng số để đổi ra phần trăm sạc/xả mỗi giờ (0.208% / LSB).
#define MAX17048_CRATE_LSB_PERCENT_H 0.208f

static volatile bool s_alert_pending;
static bool s_alert_handler_installed;

/* Hàm xử lý ngắt (ISR) khi pin giảm xuống dưới ngưỡng cài đặt */
static void IRAM_ATTR battery_alert_isr(void *arg) {
    (void)arg;
    s_alert_pending = true;
}

esp_err_t battery_init_alert(uint8_t threshold_percent) {
    // Ngưỡng báo pin yếu được giới hạn từ 1% đến 32% theo thiết kế phần cứng
    if (threshold_percent < 1) threshold_percent = 1;
    if (threshold_percent > 32) threshold_percent = 32;

    gpio_config_t alert_cfg = {
        .pin_bit_mask = 1ULL << WATCH_PIN_BATTERY_ALRT,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE, // Kích hoạt ngắt khi đường Alert kéo xuống mức thấp (Active LOW)
    };
    esp_err_t err = gpio_config(&alert_cfg);
    if (err != ESP_OK) return err;

    if (!s_alert_handler_installed) {
        err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
        err = gpio_isr_handler_add(WATCH_PIN_BATTERY_ALRT, battery_alert_isr, NULL);
        if (err != ESP_OK) return err;
        s_alert_handler_installed = true;
    }

    uint8_t config[2];
    err = hardware_i2c_sensor_i2c_read_reg(MAX17048_I2C_ADDR, MAX17048_REG_CONFIG, config, sizeof(config));
    if (err != ESP_OK) return err;

    // Cấu hình VALRT (5-bit thấp của byte thứ 2 của CONFIG): Ngưỡng kích hoạt ngắt = 32 - threshold_percent.
    config[1] = (config[1] & 0xC0U) | (uint8_t)(32U - threshold_percent);
    return hardware_i2c_sensor_i2c_write_reg(MAX17048_I2C_ADDR, MAX17048_REG_CONFIG, config, sizeof(config));
}

bool battery_take_alert(void) {
    bool pending = s_alert_pending;
    s_alert_pending = false;
    return pending;
}

esp_err_t battery_clear_alert(void) {
    uint8_t config[2];
    esp_err_t err = hardware_i2c_sensor_i2c_read_reg(MAX17048_I2C_ADDR, MAX17048_REG_CONFIG, config, sizeof(config));
    if (err != ESP_OK) return err;
    
    // Xóa cờ ALRT bằng cách đặt bit 5 (mặt nạ 0x20) của byte cấu hình thấp về 0
    config[1] &= (uint8_t)~0x20U;
    return hardware_i2c_sensor_i2c_write_reg(MAX17048_I2C_ADDR, MAX17048_REG_CONFIG, config, sizeof(config));
}

bool battery_read(watch_battery_data_t *out) {
    if (!out) return false;

    uint8_t vcell_buf[2] = {0};
    uint8_t soc_buf[2] = {0};
    uint8_t crate_buf[2] = {0};

    if (hardware_i2c_sensor_i2c_read_reg(MAX17048_I2C_ADDR, MAX17048_REG_VCELL, vcell_buf, sizeof(vcell_buf)) != ESP_OK ||
        hardware_i2c_sensor_i2c_read_reg(MAX17048_I2C_ADDR, MAX17048_REG_SOC, soc_buf, sizeof(soc_buf)) != ESP_OK ||
        hardware_i2c_sensor_i2c_read_reg(MAX17048_I2C_ADDR, MAX17048_REG_CRATE, crate_buf, sizeof(crate_buf)) != ESP_OK) {
        return false;
    }

    uint16_t vcell_raw = ((uint16_t)vcell_buf[0] << 8) | vcell_buf[1];
    uint16_t soc_raw = ((uint16_t)soc_buf[0] << 8) | soc_buf[1];
    int16_t crate_raw = (int16_t)(((uint16_t)crate_buf[0] << 8) | crate_buf[1]);

    // Tính điện áp thực tế bằng tích số raw đọc được nhân với hằng số LSB VCELL
    out->voltage_v = (float)vcell_raw * MAX17048_VCELL_LSB_V;
    
    // IC lưu trữ phần trăm pin (SOC) dưới dạng số chấm động có phần lẻ 8-bit (chia cho 256.0f để ra phần trăm 0-100%)
    out->soc_percent = (float)soc_raw / 256.0f;

    if (out->soc_percent < 0.0f) out->soc_percent = 0.0f;
    if (out->soc_percent > 100.0f) out->soc_percent = 100.0f;

    out->charge_rate = (float)crate_raw * MAX17048_CRATE_LSB_PERCENT_H;
    out->low_battery_alert = gpio_get_level(WATCH_PIN_BATTERY_ALRT) == 0;
    out->valid = true;
    
    return true;
}
