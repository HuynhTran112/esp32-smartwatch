/* Đọc dữ liệu từ IMU và thực hiện đếm bước chân. */

#include "imu.h"
#include "board_config.h"
#include "hardware_i2c_sensor.h"
#include "bmi270_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>

#define BMI270_I2C_ADDR          WATCH_IMU_I2C_ADDR
#define BMI270_REG_CHIP_ID       0x00 // Đọc Chip ID
#define BMI270_REG_ACC_DATA      0x0C // Dữ liệu gia tốc 3 trục
#define BMI270_REG_ACC_CONF      0x40 // Cấu hình bộ lọc gia tốc
#define BMI270_REG_ACC_RANGE     0x41 // Cấu hình dải đo gia tốc
#define BMI270_REG_PWR_CONF      0x7C // Cấu hình quản lý điện năng
#define BMI270_REG_PWR_CTRL      0x7D // Bật/tắt khối chức năng gia tốc và gyro
#define BMI270_REG_CMD           0x7E // Lệnh soft reset

#define BMI270_CHIP_ID_VAL       0x24 // ID mặc định của cảm biến
#define BMI270_CMD_SOFT_RESET    0xB6 // Lệnh reset mềm

// Trọng số chuyển đổi gia tốc kế sang đơn vị G.
// Lý do: Cấu hình dải đo ±4g, độ nhạy đạt được là 4096 LSB/g.
#define BMI270_ACCEL_LSB_PER_G   4096.0f

// Các hằng số cho thuật toán đếm bước chân
// Lý do chọn 0.28f: Ngưỡng biên độ gia tốc tối thiểu để nhận biết bước chân (tránh nhiễu vung tay nhỏ).
#define BMI270_STEP_THRESHOLD_G  0.28f
// Lý do chọn 0.15f: Ngưỡng trễ gia tốc để giải phóng bước chân trước khi nhận bước mới.
#define BMI270_STEP_RELEASE_G    0.15f
// Lý do chọn 300U: Khoảng cách tối thiểu giữa 2 bước (300ms) để loại bỏ nhiễu rung cơ học.
#define BMI270_STEP_MIN_MS       300U
// Lý do chọn 2000U: Thời gian dừng đi bộ (2 giây) để đưa trạng thái về đứng yên.
#define BMI270_STEP_MAX_MS       2000U
// Lý do chọn 1500U: Thời gian tối đa để 2 bước liên tiếp được tính là bước đi liên tiếp.
#define BMI270_STEP_CADENCE_MAX_MS 1500U
// Số bước làm ấm tối thiểu để bắt đầu đếm (lọc vung tay đơn lẻ).
#define BMI270_STEP_WARMUP_COUNT   2U

static const char *TAG = "IMU";

static bool s_initialized;
static uint32_t s_step_count;
static uint32_t s_last_step_ms;
static bool s_step_armed = true;
// Hệ số lọc thông thấp để loại bỏ gia tốc tĩnh của trọng lực (hệ số 0.08f).
static float s_mag_lp = 1.0f;
static uint32_t s_cadence_run;
static uint16_t s_cadence_spm;
static watch_motion_type_t s_motion_type = WATCH_MOTION_STATIONARY;

static esp_err_t bmi270_write_u8(uint8_t reg, uint8_t value) {
    return hardware_i2c_sensor_i2c_write_reg(BMI270_I2C_ADDR, reg, &value, 1);
}

static esp_err_t bmi270_read_u8(uint8_t reg, uint8_t *value) {
    return hardware_i2c_sensor_i2c_read_reg(BMI270_I2C_ADDR, reg, value, 1);
}

static int16_t bmi270_i16_le(const uint8_t *p) {
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

/* Thuật toán đếm bước chân thời gian thực (Pedometer) sử dụng lọc thông thấp động */
static void imu_update_steps(float ax, float ay, float az) {
    float mag = sqrtf((ax * ax) + (ay * ay) + (az * az));
    // Bộ lọc thông thấp trượt tách trọng lực DC tĩnh
    s_mag_lp += 0.08f * (mag - s_mag_lp);
    float dyn = mag - s_mag_lp; // Gia tốc động thực tế (AC component) sau khi khử trọng lực tĩnh
    
    uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    uint32_t dt_ms = (uint32_t)(now_ms - s_last_step_ms);

    if (s_step_armed && dyn > BMI270_STEP_THRESHOLD_G && dt_ms >= BMI270_STEP_MIN_MS) {
        bool rhythmic = (s_last_step_ms != 0) && (dt_ms <= BMI270_STEP_CADENCE_MAX_MS);
        if (rhythmic) {
            s_cadence_run++;
            if (s_cadence_run == BMI270_STEP_WARMUP_COUNT) {
                // Đã xác nhận nhịp điệu đi bộ thật sự, cộng dồn các bước warmup bị bỏ qua trước đó
                s_step_count += BMI270_STEP_WARMUP_COUNT;
            } else if (s_cadence_run > BMI270_STEP_WARMUP_COUNT) {
                s_step_count++;
            }
            // Quy đổi tần số bước chân SPM = 60000ms / delta_time
            s_cadence_spm = dt_ms > 0 ? (uint16_t)(60000U / dt_ms) : 0;
            // Nhận diện trạng thái Chạy bộ (Running) nếu SPM vượt quá ngưỡng tốc độ nhanh 145 bước/phút
            s_motion_type = s_cadence_spm >= 145 ? WATCH_MOTION_RUNNING : WATCH_MOTION_WALKING;
        } else {
            s_cadence_run = 1;
            s_cadence_spm = 0;
            s_motion_type = WATCH_MOTION_ACTIVE;
        }
        s_last_step_ms = now_ms;
        s_step_armed = false; // Khóa bộ vũ trang bước, chờ tín hiệu giảm xuống dưới ngưỡng release
        return;
    }

    if (!s_step_armed && (dyn < BMI270_STEP_RELEASE_G || dt_ms > BMI270_STEP_MAX_MS)) {
        s_step_armed = true;
    }

    if (dt_ms > BMI270_STEP_MAX_MS) {
        s_cadence_run = 0;
        s_cadence_spm = 0;
        s_motion_type = fabsf(dyn) < BMI270_STEP_RELEASE_G ? WATCH_MOTION_STATIONARY
                                                           : WATCH_MOTION_ACTIVE;
    }
}

bool imu_init(void) {
    uint8_t chip_id = 0;
    esp_err_t err = bmi270_read_u8(BMI270_REG_CHIP_ID, &chip_id);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Lỗi đọc CHIP_ID: %s", esp_err_to_name(err));
        return false;
    }

    if (chip_id != BMI270_CHIP_ID_VAL) {
        ESP_LOGE(TAG, "Sai CHIP_ID: %02X", chip_id);
        return false;
    }

    // Reset mềm cảm biến
    err = bmi270_write_u8(BMI270_REG_CMD, BMI270_CMD_SOFT_RESET);
    if (err != ESP_OK) return false;
    vTaskDelay(pdMS_TO_TICKS(20)); // Trễ bắt buộc 20ms sau khi soft reset theo datasheet

    // Tắt tiết kiệm điện tạm thời ghi 0x00 để nạp cấu hình firmware
    err = bmi270_write_u8(BMI270_REG_PWR_CONF, 0x00);
    if (err != ESP_OK) return false;
    vTaskDelay(pdMS_TO_TICKS(1));

    // Bắt đầu quy trình nạp cấu hình Bosch ASIC (ghi 0x00 vào thanh ghi 0x59 để mở khóa tải)
    err = bmi270_write_u8(0x59, 0x00);
    if (err != ESP_OK) return false;

    // Nạp file cấu hình nhị phân 8KB của Bosch theo từng mảnh nhỏ 64 byte để tránh đầy bộ đệm I2C
    const size_t config_size = sizeof(bmi270_config_file);
    const size_t chunk_size = 64;
    for (size_t offset = 0; offset < config_size; offset += chunk_size) {
        uint16_t word_offset = (uint16_t)(offset / 2);
        uint8_t addr_buf[2];
        addr_buf[0] = (uint8_t)(word_offset & 0x0F);
        addr_buf[1] = (uint8_t)((word_offset >> 4) & 0xFF);

        // Ghi địa chỉ từ byte offset nạp cấu hình (đích 0x5B)
        err = hardware_i2c_sensor_i2c_write_reg(BMI270_I2C_ADDR, 0x5B, addr_buf, 2);
        if (err != ESP_OK) return false;

        // Ghi khối dữ liệu firmware vào thanh ghi 0x5E
        err = hardware_i2c_sensor_i2c_write_reg(BMI270_I2C_ADDR, 0x5E, &bmi270_config_file[offset], chunk_size);
        if (err != ESP_OK) return false;
    }

    // Hoàn tất nạp cấu hình (ghi 0x01 vào thanh ghi 0x59 để kích hoạt firmware ASIC)
    err = bmi270_write_u8(0x59, 0x01);
    if (err != ESP_OK) return false;

    // Kiểm tra trạng thái khởi động ASIC thành công
    uint8_t status = 0;
    for (int retry = 0; retry < 12; retry++) {
        vTaskDelay(pdMS_TO_TICKS(25));
        status = 0;
        err = bmi270_read_u8(0x21, &status); // Đọc thanh ghi nội bộ 0x21 chứa trạng thái nạp cấu hình
        if (err == ESP_OK) {
            if ((status & 0x0F) == 0x01) break;   // ASIC khởi động thành công (0x01)
            if ((status & 0x0F) == 0x02) return false; // Lỗi nạp cấu hình
        }
    }

    if ((status & 0x0F) != 0x01) return false;

    // Cấu hình hoạt động:
    // - ACC_CONF = 0xA6: Đặt tần số lấy mẫu ODR = 50Hz (đủ nhạy cho đếm bước), bộ lọc thông thấp chế độ Normal
    // - ACC_RANGE = 0x02: Cấu hình dải đo gia tốc ±4g (tối ưu nhất cho hoạt động đeo tay của con người)
    // - PWR_CTRL = 0x04: Chỉ bật khối gia tốc kế, tắt con quay hồi chuyển gyro để giảm thiểu tiêu thụ dòng
    if (bmi270_write_u8(BMI270_REG_ACC_CONF, 0xA6) != ESP_OK ||
        bmi270_write_u8(BMI270_REG_ACC_RANGE, 0x02) != ESP_OK ||
        bmi270_write_u8(BMI270_REG_PWR_CTRL, 0x04) != ESP_OK) {
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(50)); // Trễ ổn định nguồn phát sau khi cấu hình
    s_initialized = true;
    return true;
}

bool imu_read(watch_imu_data_t *out) {
    if (!out || !s_initialized) return false;

    uint8_t raw[6] = {0};
    esp_err_t err = hardware_i2c_sensor_i2c_read_reg(BMI270_I2C_ADDR, BMI270_REG_ACC_DATA, raw, sizeof(raw));
    if (err != ESP_OK) return false;

    int16_t ax_raw = bmi270_i16_le(&raw[0]);
    int16_t ay_raw = bmi270_i16_le(&raw[2]);
    int16_t az_raw = bmi270_i16_le(&raw[4]);
    memset(out, 0, sizeof(*out));
    
    out->accel_x = (float)ax_raw / BMI270_ACCEL_LSB_PER_G;
    out->accel_y = (float)ay_raw / BMI270_ACCEL_LSB_PER_G;
    out->accel_z = (float)az_raw / BMI270_ACCEL_LSB_PER_G;

    imu_update_steps(out->accel_x, out->accel_y, out->accel_z);
    
    out->step_count = s_step_count;
    out->cadence_spm = s_cadence_spm;
    out->motion_type = s_motion_type;
    out->valid = true;
    return true;
}

void imu_reset_steps(void) {
    s_step_count = 0;
    s_last_step_ms = 0;
    s_step_armed = true;
    s_mag_lp = 1.0f;
    s_cadence_run = 0;
    s_cadence_spm = 0;
    s_motion_type = WATCH_MOTION_STATIONARY;
}

void imu_sleep(void) {
    if (!s_initialized) return;

    // 1. Đưa IMU về chế độ suspend
    (void)bmi270_write_u8(BMI270_REG_PWR_CTRL, 0x00);
    vTaskDelay(pdMS_TO_TICKS(1));

    // 2. Kích hoạt chế độ tiết kiệm năng lượng nâng cao
    (void)bmi270_write_u8(BMI270_REG_PWR_CONF, 0x03);

    s_initialized = false;
    ESP_LOGI(TAG, "IMU has entered suspend mode");
}
