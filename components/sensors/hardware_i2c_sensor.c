/* Đọc cảm biến I2C phần cứng thời gian thực bằng tác vụ nền FreeRTOS. */

#include "hardware_i2c_sensor.h"
#include "board_config.h"
#include "gps_tracker.h"
#include "esp_sleep.h"
#include "esp_timer.h"

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_lvgl_port.h"
#include "ui_shutdown_dialog.h"
#include "ui.h"
#include "ui_navigation.h"
#include "ui_screens.h"
#include "bluetooth_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <math.h>
#include <string.h>

static const char *TAG = "HW_MON";

// Chu kỳ lấy mẫu các cảm biến
#define BAT_POLL_MS     5000 // Chu kỳ đo pin (5 giây)
#define IMU_POLL_MS     50   // Chu kỳ đọc IMU (50ms)
#define HR_POLL_MS      100  // Chu kỳ đọc cảm biến nhịp tim (100ms)
#define TASK_LOOP_MS    50   // Chu kỳ vòng lặp chính (50ms)

// Cấu hình tham số xử lý an toàn I2C
#define HARDWARE_I2C_SENSOR_MUTEX_TIMEOUT_MS 120 // Chờ khóa Mutex tránh treo luồng khác
#define HARDWARE_I2C_SENSOR_I2C_RETRY_COUNT 2   // Số lần thử lại giao tiếp I2C khi lỗi
#define HARDWARE_I2C_SENSOR_I2C_TIMEOUT_MS 50   // Thời gian timeout bus I2C
#define HARDWARE_I2C_SENSOR_TASK_STACK_SIZE 4096 // Kích thước ngăn xếp của task
#define HARDWARE_I2C_SENSOR_I2C_STUCK_RECOVER_COOLDOWN_MS 30000 // Cooldown 30s giữa các lần gỡ stuck I2C
#define HARDWARE_I2C_SENSOR_IMU_REINIT_INTERVAL_MS 5000 // Tần suất thử kết nối lại IMU (5 giây)
#define HARDWARE_I2C_SENSOR_HR_REINIT_INTERVAL_MS 3000  // Tần suất thử kết nối lại cảm biến nhịp tim (3 giây)
#define HARDWARE_I2C_SENSOR_SENSOR_FAILURE_LIMIT 5      // Giới hạn số lần lỗi đọc liên tiếp để báo offline

static SemaphoreHandle_t s_mutex = NULL;
static SemaphoreHandle_t s_i2c_mutex = NULL;
static TaskHandle_t s_task_handle = NULL;
static bool s_i2c_ready = false;

static watch_battery_data_t s_battery = { .voltage_v = 0.0f, .soc_percent = 0.0f, .valid = false };
static watch_imu_data_t s_imu = {0};
static bool s_imu_found = false;
static uint32_t s_last_stuck_recover_ms = 0;
static bool s_quick_wake_enabled = false;
static int s_screen_timeout = 0;
static watch_heart_rate_data_t s_heart_rate = { .heart_rate = 0.0f, .spo2 = 0.0f, .valid = false };
static bool s_heart_rate_found = false;
static bool s_heart_rate_measure_enabled = false;
static bool s_heart_rate_disabled = false;
static int s_heart_rate_init_failures = 0;

// Giới hạn số lần lỗi khởi tạo cảm biến nhịp tim: 3.
// Lý do: Tránh trường hợp cảm biến lỗi liên tục gây khóa bus I2C làm ảnh hưởng đến các cảm biến khác (IMU, pin).
#define HARDWARE_I2C_SENSOR_HR_MAX_INIT_FAILURES 3

static esp_err_t hardware_i2c_sensor_i2c_init(void);

static bool hardware_i2c_sensor_lock(void) {
    return s_mutex && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(HARDWARE_I2C_SENSOR_MUTEX_TIMEOUT_MS)) == pdTRUE;
}

static void hardware_i2c_sensor_unlock(void) {
    if (s_mutex) xSemaphoreGive(s_mutex);
}

static esp_err_t hardware_i2c_sensor_i2c_read_reg_once(uint8_t addr, uint8_t reg, uint8_t *data, size_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (!cmd) return ESP_ERR_NO_MEM;
    
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    
    esp_err_t err = i2c_master_cmd_begin(WATCH_SENSOR_I2C_PORT, cmd, pdMS_TO_TICKS(HARDWARE_I2C_SENSOR_I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    return err;
}

static void hardware_i2c_sensor_i2c_recover_bus(void) {
    i2c_reset_tx_fifo(WATCH_SENSOR_I2C_PORT);
    i2c_reset_rx_fifo(WATCH_SENSOR_I2C_PORT);
}

// Giải vây bus I2C bị nghẽn (SDA kẹt LOW) bằng cách tạo xung clock ảo
static void hardware_i2c_sensor_i2c_recover_stuck_bus(void) {
    if (gpio_get_level(WATCH_PIN_IMU_I2C_SDA) != 0 && gpio_get_level(WATCH_PIN_IMU_I2C_SCL) != 0) {
        return;
    }

    uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    if (s_last_stuck_recover_ms != 0 && (uint32_t)(now_ms - s_last_stuck_recover_ms) < HARDWARE_I2C_SENSOR_I2C_STUCK_RECOVER_COOLDOWN_MS) {
        return;
    }
    s_last_stuck_recover_ms = now_ms;

    esp_err_t err = i2c_driver_delete(WATCH_SENSOR_I2C_PORT);
    s_i2c_ready = false;

    // Cấu hình tạm thời chân SDA và SCL thành Open-Drain GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << WATCH_PIN_IMU_I2C_SDA) | (1ULL << WATCH_PIN_IMU_I2C_SCL),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    gpio_set_level(WATCH_PIN_IMU_I2C_SDA, 1);
    gpio_set_level(WATCH_PIN_IMU_I2C_SCL, 1);
    esp_rom_delay_us(5); // Chờ 5us theo chu kỳ xung clock Standard Mode I2C

    // Phát tối đa 9 nhịp xung clock để Slave nhả đường SDA
    for (int i = 0; i < 9; i++) {
        gpio_set_level(WATCH_PIN_IMU_I2C_SCL, 0);
        esp_rom_delay_us(5);
        gpio_set_level(WATCH_PIN_IMU_I2C_SCL, 1);
        esp_rom_delay_us(5);
        if (gpio_get_level(WATCH_PIN_IMU_I2C_SDA)) break;
    }

    // Tạo nhịp STOP thủ công
    gpio_set_level(WATCH_PIN_IMU_I2C_SDA, 0);
    esp_rom_delay_us(5);
    gpio_set_level(WATCH_PIN_IMU_I2C_SCL, 1);
    esp_rom_delay_us(5);
    gpio_set_level(WATCH_PIN_IMU_I2C_SDA, 1);
    esp_rom_delay_us(5);

    err = hardware_i2c_sensor_i2c_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Reinit I2C fail: %s", esp_err_to_name(err));
    }
}

esp_err_t hardware_i2c_sensor_i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *data, size_t len) {
    if (!data || len == 0) return ESP_ERR_INVALID_ARG;
    if (!s_i2c_mutex || xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(HARDWARE_I2C_SENSOR_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t err = ESP_FAIL;
    for (int attempt = 0; attempt < HARDWARE_I2C_SENSOR_I2C_RETRY_COUNT; attempt++) {
        err = hardware_i2c_sensor_i2c_read_reg_once(addr, reg, data, len);
        if (err == ESP_OK) break;
        hardware_i2c_sensor_i2c_recover_bus();
        vTaskDelay(pdMS_TO_TICKS(10)); // Trễ ngắn 10ms để giảm nhiễu vật lý trên bus trước khi thử lại
    }
    if (err != ESP_OK) hardware_i2c_sensor_i2c_recover_stuck_bus();
    xSemaphoreGive(s_i2c_mutex);
    return err;
}

esp_err_t hardware_i2c_sensor_i2c_write_reg(uint8_t addr, uint8_t reg, const uint8_t *data, size_t len) {
    if (!data || len == 0) return ESP_ERR_INVALID_ARG;
    if (!s_i2c_mutex || xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(HARDWARE_I2C_SENSOR_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (!cmd) {
        xSemaphoreGive(s_i2c_mutex);
        return ESP_ERR_NO_MEM;
    }

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write(cmd, (uint8_t *)data, len, true);
    i2c_master_stop(cmd);

    esp_err_t err = i2c_master_cmd_begin(WATCH_SENSOR_I2C_PORT, cmd, pdMS_TO_TICKS(HARDWARE_I2C_SENSOR_I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    if (err != ESP_OK) {
        hardware_i2c_sensor_i2c_recover_bus();
        hardware_i2c_sensor_i2c_recover_stuck_bus();
    }
    xSemaphoreGive(s_i2c_mutex);
    return err;
}

static esp_err_t hardware_i2c_sensor_i2c_init(void) {
    if (s_i2c_ready) return ESP_OK;

    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = WATCH_PIN_IMU_I2C_SDA,
        .scl_io_num = WATCH_PIN_IMU_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = WATCH_IMU_I2C_CLK_HZ,
    };

    esp_err_t err = i2c_param_config(WATCH_SENSOR_I2C_PORT, &i2c_cfg);
    if (err != ESP_OK) return err;

    err = i2c_driver_install(WATCH_SENSOR_I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK) return err;

    s_i2c_ready = true;
    return ESP_OK;
}

static bool hardware_i2c_sensor_is_imu_needed(void) {
    return true; // Giữ IMU luôn bật để thực hiện thuật toán đếm bước chân ngầm (Background step counter)
}

static bool hardware_i2c_sensor_is_hr_needed(void) {
    if (ui_activity_is_active()) return true;
    if (ui_navigation_is_backlight_off()) return false;
    
    bool enabled = s_heart_rate_measure_enabled;
    if (hardware_i2c_sensor_lock()) {
        enabled = s_heart_rate_measure_enabled;
        hardware_i2c_sensor_unlock();
    }
    return enabled;
}

bool hardware_i2c_sensor_is_quick_wake_enabled(void) {
    bool enabled = false;
    if (hardware_i2c_sensor_lock()) {
        enabled = s_quick_wake_enabled;
        hardware_i2c_sensor_unlock();
    } else {
        enabled = s_quick_wake_enabled;
    }
    return enabled;
}

void hardware_i2c_sensor_set_quick_wake_enabled(bool enabled) {
    if (hardware_i2c_sensor_lock()) {
        s_quick_wake_enabled = enabled;
        hardware_i2c_sensor_unlock();
    } else {
        s_quick_wake_enabled = enabled;
    }
    ESP_LOGI(TAG, "Quick wake: %s", enabled ? "ON" : "OFF");
}

int hardware_i2c_sensor_get_screen_timeout(void) {
    int val = 15;
    if (hardware_i2c_sensor_lock()) {
        val = s_screen_timeout;
        hardware_i2c_sensor_unlock();
    } else {
        val = s_screen_timeout;
    }
    return val;
}

void hardware_i2c_sensor_set_screen_timeout(int seconds) {
    if (hardware_i2c_sensor_lock()) {
        s_screen_timeout = seconds;
        hardware_i2c_sensor_unlock();
    } else {
        s_screen_timeout = seconds;
    }
    ESP_LOGI(TAG, "Screen timeout: %d s", seconds);
}

static void watch_hardware_i2c_sensor_task(void *arg) {
    (void)arg;

    uint32_t last_bat_poll_ms = 0;
    uint32_t last_imu_poll_ms = 0;
    uint32_t last_hr_poll_ms = 0;
    uint32_t last_imu_init_attempt_ms = 0;
    uint32_t last_hr_init_attempt_ms = 0;
    uint8_t imu_read_failures = 0;
    uint8_t hr_read_failures = 0;
    uint32_t btn_press_duration_ms = 0;

    while (1) {
        uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        uint32_t imu_poll_interval = IMU_POLL_MS;
        uint32_t loop_delay_ms = TASK_LOOP_MS;
        
        // Khi màn hình tắt, Bluetooth đang kết nối và không có phiên thể thao chạy, 
        // tăng chu kỳ thăm dò IMU lên 200ms để tối ưu hóa tiết kiệm pin tối đa.
        if (ui_navigation_is_backlight_off() && watch_bluetooth_is_connected() && !ui_navigation_has_active_session()) {
            imu_poll_interval = 200;
            loop_delay_ms = 200;
        }

        // 1. Đo mức pin định kỳ (5000ms)
        if (last_bat_poll_ms == 0 || (now_ms - last_bat_poll_ms) >= BAT_POLL_MS) {
            watch_battery_data_t bat;
            if (battery_read(&bat)) {
                if (hardware_i2c_sensor_lock()) {
                    s_battery = bat;
                    hardware_i2c_sensor_unlock();
                }
                
                // Tự động tắt nguồn nếu pin cực yếu (<= 1.0%) và không cắm sạc
                if (bat.valid && bat.soc_percent <= 1.0f && bat.charge_rate <= 0.0f) {
                    ESP_LOGW(TAG, "Battery level critical (%.1f%%). Shutting down immediately to protect battery...", bat.soc_percent);
                    ui_navigation_enter_deep_sleep();
                }
            }
            last_bat_poll_ms = now_ms;
        }

        // 2. Quét IMU đếm bước chân định kỳ (50ms)
        if (hardware_i2c_sensor_is_imu_needed() && (last_imu_poll_ms == 0 || (now_ms - last_imu_poll_ms) >= imu_poll_interval)) {
            bool imu_found = false;
            if (hardware_i2c_sensor_lock()) {
                imu_found = s_imu_found;
                hardware_i2c_sensor_unlock();
            }

            if (!imu_found) {
                if (last_imu_init_attempt_ms == 0 || (now_ms - last_imu_init_attempt_ms) >= HARDWARE_I2C_SENSOR_IMU_REINIT_INTERVAL_MS) {
                    last_imu_init_attempt_ms = now_ms;
                    if (imu_init()) {
                        if (hardware_i2c_sensor_lock()) {
                            s_imu_found = true;
                            s_imu.valid = false;
                            imu_read_failures = 0;
                            hardware_i2c_sensor_unlock();
                        }
                    }
                }
            } else {
                watch_imu_data_t imu;
                if (imu_read(&imu)) {
                    imu_read_failures = 0;
                    if (hardware_i2c_sensor_lock()) {
                        s_imu = imu;
                        hardware_i2c_sensor_unlock();
                    }
                } else if (++imu_read_failures >= HARDWARE_I2C_SENSOR_SENSOR_FAILURE_LIMIT) {
                    imu_read_failures = 0;
                    if (hardware_i2c_sensor_lock()) {
                        s_imu_found = false;
                        s_imu.valid = false;
                        hardware_i2c_sensor_unlock();
                    }
                }
            }
            last_imu_poll_ms = now_ms;
        }

        // 3. Đo nhịp tim và SpO2 định kỳ (100ms)
        bool hr_needed = hardware_i2c_sensor_is_hr_needed();
        if (hr_needed && !s_heart_rate_disabled && (last_hr_poll_ms == 0 || (now_ms - last_hr_poll_ms) >= HR_POLL_MS)) {
            bool hr_found = false;
            if (hardware_i2c_sensor_lock()) {
                hr_found = s_heart_rate_found;
                hardware_i2c_sensor_unlock();
            }

            if (!hr_found) {
                if (last_hr_init_attempt_ms == 0 || (now_ms - last_hr_init_attempt_ms) >= HARDWARE_I2C_SENSOR_HR_REINIT_INTERVAL_MS) {
                    last_hr_init_attempt_ms = now_ms;
                    if (heart_rate_init()) {
                        hr_read_failures = 0;
                        s_heart_rate_init_failures = 0;
                        if (hardware_i2c_sensor_lock()) {
                            s_heart_rate_found = true;
                            s_heart_rate.valid = false;
                            hardware_i2c_sensor_unlock();
                        }
                    } else {
                        s_heart_rate_init_failures++;
                        if (s_heart_rate_init_failures >= HARDWARE_I2C_SENSOR_HR_MAX_INIT_FAILURES) {
                            s_heart_rate_disabled = true;
                            ESP_LOGW(TAG, "Không tìm thấy cảm biến nhịp tim sau %d lần thử. Khóa tính năng HR để tránh lỗi bus I2C.", HARDWARE_I2C_SENSOR_HR_MAX_INIT_FAILURES);
                        }
                    }
                }
            } else {
                watch_heart_rate_data_t hr = {0};
                bool ok = heart_rate_read(&hr);
                if (hardware_i2c_sensor_lock()) {
                    // Nếu người dùng đang chạy bộ hoặc hoạt động mạnh dựa vào IMU, vô hiệu hóa việc đo SpO2 
                    // vì nhiễu chuyển động cơ học làm sai lệch nghiêm trọng thuật toán ước lượng nồng độ Oxy.
                    if (ok && s_imu.valid && (s_imu.motion_type == WATCH_MOTION_RUNNING || s_imu.motion_type == WATCH_MOTION_ACTIVE)) {
                        hr.spo2 = 0.0f;
                        hr.spo2_valid = false;
                        if (hr.quality > 20) hr.quality -= 20; // Trừ điểm chất lượng tín hiệu khi có chuyển động cơ học
                    }
                    s_heart_rate = hr;
                    if (!ok) s_heart_rate.valid = false;
                    hardware_i2c_sensor_unlock();
                }
                if (ok) {
                    hr_read_failures = 0;
                } else if (++hr_read_failures >= HARDWARE_I2C_SENSOR_SENSOR_FAILURE_LIMIT) {
                    hr_read_failures = 0;
                    if (hardware_i2c_sensor_lock()) {
                        s_heart_rate_found = false;
                        s_heart_rate.valid = false;
                        hardware_i2c_sensor_unlock();
                    }
                }
            }
            last_hr_poll_ms = now_ms;
        } else if (!hr_needed && s_heart_rate_found) {
            heart_rate_shutdown();
            if (hardware_i2c_sensor_lock()) {
                s_heart_rate_found = false;
                s_heart_rate.valid = false;
                hardware_i2c_sensor_unlock();
            }
            hr_read_failures = 0;
        }

        // 4. Giám sát phím nguồn vật lý (Active LOW)
        static bool s_btn_handled = false;
        static bool s_last_btn_state = false;
        static int64_t s_last_toggle_us = 0;
        bool btn_pressed = (gpio_get_level(WATCH_PIN_BUTTON_POWER) == 0);

        if (btn_pressed) {
            if (!s_last_btn_state) {
                ESP_LOGI(TAG, "Power button pressed");
            }
            btn_press_duration_ms += loop_delay_ms;
            
            // Nhấn giữ >= 1500 ms -> Hiện hộp thoại tắt nguồn
            if (btn_press_duration_ms >= 1500 && !ui_is_sleep_in_progress()) {
                btn_press_duration_ms = 0;
                s_btn_handled = true;
                if (lvgl_port_lock(1000)) {
                    if (!ui_is_sleep_in_progress() && !ui_shutdown_dialog_is_visible()) {
                        ui_shutdown_dialog_show();
                    }
                    lvgl_port_unlock();
                }
            }
        } else {
            if (s_last_btn_state) {
                // Nhấn ngắn (< 1500 ms) -> Bật/Tắt màn hình với cooldown 500ms (500000 us) chống dội (bounce)
                if (btn_press_duration_ms >= 50 && btn_press_duration_ms < 1500 && !s_btn_handled) {
                    int64_t now_us = esp_timer_get_time();
                    if (now_us - s_last_toggle_us >= 500000) {
                        s_last_toggle_us = now_us;
                        if (lvgl_port_lock(1000)) {
                            ui_navigation_toggle_backlight();
                            lvgl_port_unlock();
                        }
                    }
                }
            }
            btn_press_duration_ms = 0;
            s_btn_handled = false;
        }
        s_last_btn_state = btn_pressed;

        vTaskDelay(pdMS_TO_TICKS(loop_delay_ms));
    }
}

esp_err_t hardware_i2c_sensor_start(void) {
    if (s_task_handle) return ESP_OK;

    if (!s_i2c_mutex) {
        s_i2c_mutex = xSemaphoreCreateMutex();
        if (!s_i2c_mutex) return ESP_ERR_NO_MEM;
    }

    esp_err_t err = hardware_i2c_sensor_i2c_init();
    if (err != ESP_OK) return err;

    gpio_config_t btn_cfg = {
        .pin_bit_mask = (1ULL << WATCH_PIN_BUTTON_POWER),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&btn_cfg);
    gpio_wakeup_enable(WATCH_PIN_BUTTON_POWER, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();

    if (!s_mutex) {
        s_mutex = xSemaphoreCreateMutex();
        if (!s_mutex) return ESP_ERR_NO_MEM;
    }

    // Khởi chạy task nền FreeRTOS với độ ưu tiên 2 (Mức ưu tiên thấp hơn luồng đồ họa UI chính để tránh chiếm giữ CPU làm giật khung hình)
    BaseType_t task_ok = xTaskCreatePinnedToCore(watch_hardware_i2c_sensor_task, "hw_sens_task", HARDWARE_I2C_SENSOR_TASK_STACK_SIZE, NULL, 2, &s_task_handle, 0);
    if (task_ok != pdPASS) {
        s_task_handle = NULL;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

bool hardware_i2c_sensor_get_battery(watch_battery_data_t *out) {
    if (!out) return false;
    if (!hardware_i2c_sensor_lock()) return false;
    *out = s_battery;
    bool valid = s_battery.valid;
    hardware_i2c_sensor_unlock();
    return valid;
}

bool hardware_i2c_sensor_get_imu(watch_imu_data_t *out) {
    if (!out) return false;
    if (!hardware_i2c_sensor_lock()) return false;
    *out = s_imu;
    bool found = s_imu_found;
    hardware_i2c_sensor_unlock();
    return found;
}

bool hardware_i2c_sensor_get_heart_rate(watch_heart_rate_data_t *out) {
    if (!out) return false;
    if (!hardware_i2c_sensor_lock()) return false;
    *out = s_heart_rate;
    bool valid = s_heart_rate.valid;
    hardware_i2c_sensor_unlock();
    return valid;
}

void hardware_i2c_sensor_set_heart_rate_enabled(bool enabled) {
    if (hardware_i2c_sensor_lock()) {
        s_heart_rate_measure_enabled = enabled;
        if (enabled) {
            s_heart_rate.valid = false;
            s_heart_rate.heart_rate = 0.0f;
            s_heart_rate.spo2 = 0.0f;
            s_heart_rate.quality = 0;
            s_heart_rate_disabled = false;
            s_heart_rate_init_failures = 0;
        }
        hardware_i2c_sensor_unlock();
    } else {
        s_heart_rate_measure_enabled = enabled;
    }
    ESP_LOGI(TAG, "Manual HR: %s", enabled ? "ON" : "OFF");
}

void hardware_i2c_sensor_reset_steps(void) {
    imu_reset_steps();
    if (hardware_i2c_sensor_lock()) {
        s_imu.step_count = 0;
        hardware_i2c_sensor_unlock();
    }
}
