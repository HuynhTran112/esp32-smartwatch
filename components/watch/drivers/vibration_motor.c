/* Hiện thực các hàm điều khiển động cơ rung sử dụng bộ phát xung PWM LEDC
   Driver sử dụng tính năng phát xung PWM của bộ ngoại vi LEDC (LED Controller)
   kết hợp với cơ chế đa nhiệm FreeRTOS Task để tạo ra các kiểu rung không đồng bộ (Asynchronous),
   tránh block luồng xử lý đồ họa UI chính. */

#include "vibration_motor.h"
#include "board_config.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdbool.h>

static const char *TAG = "VIB_MOTOR";

static QueueHandle_t s_vibration_queue = NULL;
static bool s_initialized = false;           // Trạng thái khởi tạo driver

/* Định nghĩa các kiểu rung của đồng hồ */
typedef enum {
    VIBRATION_PATTERN_PULSE,   // Kiểu rung đơn ngắn (ví dụ: phản hồi nút nhấn)
    VIBRATION_PATTERN_NOTIFY,  // Kiểu rung kép ngắn (ví dụ: thông báo có tin nhắn)
    VIBRATION_PATTERN_ALARM,   // Kiểu rung lặp lại chu kỳ lâu (dùng cho báo thức)
    VIBRATION_PATTERN_STOP,    // Dừng rung
} vibration_pattern_t;

/* Cấu trúc truyền tải lệnh điều khiển cho tác vụ FreeRTOS */
typedef struct {
    vibration_pattern_t pattern; // Kiểu mẫu rung
    uint8_t strength;            // Cường độ rung (0: yếu, 1: trung bình, 2: mạnh)
    uint32_t duration_ms;        // Thời lượng rung (chỉ áp dụng cho kiểu PULSE)
} vibration_cmd_t;

/* Quy đổi mức cường độ (0, 1, 2) sang giá trị thanh ghi Duty của bộ Timer LEDC 10-bit (0-1023)
   - strength: Cường độ rung của người dùng cài đặt
   Trả về: uint32_t Giá trị độ rộng xung tương ứng */
static uint32_t vibration_duty_for_strength(uint8_t strength) {
    // LEDC PWM 10-bit (tối đa 1023)
    switch (strength) {
        // Tương đương ~35% điện áp mô-tơ, là ngưỡng tối thiểu để khởi động êm.
        case 0:  return 360;
        
        // Tương đương ~88% điện áp, mức rung mạnh nhất cho báo thức.
        case 2:  return 900;
        
        // Tương đương ~62% điện áp, mức trung bình cân bằng.
        case 1:
        default: return 640;
    }
}

/* Bật motor rung bằng cách xuất xung PWM */
static void vibration_set_on(uint8_t strength) {
    if (!s_initialized) return;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, WATCH_VIBRATOR_LEDC_CH,
                  vibration_duty_for_strength(strength));
    ledc_update_duty(LEDC_LOW_SPEED_MODE, WATCH_VIBRATOR_LEDC_CH);
}

/* Tắt động cơ rung (đặt Duty về 0) */
static void vibration_set_off(void) {
    if (!s_initialized) return;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, WATCH_VIBRATOR_LEDC_CH, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, WATCH_VIBRATOR_LEDC_CH);
}

/* Tác vụ FreeRTOS thực thi luồng rung tương tác phần cứng */
static bool vibration_wait_for_command(uint32_t delay_ms, vibration_cmd_t *next) {
    return xQueueReceive(s_vibration_queue, next, pdMS_TO_TICKS(delay_ms)) == pdTRUE;
}

static void vibration_task(void *arg) {
    (void)arg;
    vibration_cmd_t cmd;
    bool has_command = false;

    for (;;) {
        if (!has_command &&
            xQueueReceive(s_vibration_queue, &cmd, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        has_command = false;

        if (cmd.pattern == VIBRATION_PATTERN_STOP) {
            vibration_set_off();
            continue;
        }

        if (cmd.pattern == VIBRATION_PATTERN_NOTIFY) {
            // Chu kỳ rung thông báo kép (90ms ON - 60ms OFF - 90ms ON)
            vibration_set_on(cmd.strength);
            has_command = vibration_wait_for_command(90, &cmd);
            vibration_set_off();
            if (!has_command) has_command = vibration_wait_for_command(60, &cmd);
            if (!has_command) {
                vibration_set_on(cmd.strength);
                has_command = vibration_wait_for_command(90, &cmd);
                vibration_set_off();
            }
        } else if (cmd.pattern == VIBRATION_PATTERN_ALARM) {
            // Rung báo thức chu kỳ (650ms ON - 350ms OFF)
            while (!has_command) {
                vibration_set_on(cmd.strength);
                has_command = vibration_wait_for_command(650, &cmd);
                vibration_set_off();
                if (!has_command) has_command = vibration_wait_for_command(350, &cmd);
            }
        } else {
            // Rung đơn ngắn (mặc định 180ms)
            vibration_set_on(cmd.strength);
            has_command = vibration_wait_for_command(
                cmd.duration_ms ? cmd.duration_ms : 180, &cmd);
            vibration_set_off();
        }
    }
}

esp_err_t watch_vibration_init(void) {
    gpio_hold_dis(WATCH_PIN_VIBRATOR);

    if (s_initialized) return ESP_OK;

    /* Cấu hình bộ Timer phát xung PWM tần số 200 Hz, phân giải 10-bit */
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = WATCH_VIBRATOR_LEDC_TIMER,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = WATCH_VIBRATOR_LEDC_FREQ_HZ,
        .clk_cfg = LEDC_USE_RC_FAST_CLK,
    };
    esp_err_t err = ledc_timer_config(&timer_cfg);
    if (err != ESP_OK) return err;

    /* Cấu hình kênh đầu ra cho chân cắm GPIO động cơ rung */
    ledc_channel_config_t ch_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = WATCH_VIBRATOR_LEDC_CH,
        .timer_sel = WATCH_VIBRATOR_LEDC_TIMER,
        .gpio_num = WATCH_PIN_VIBRATOR,
        .duty = 0,
        .hpoint = 0,
    };
    err = ledc_channel_config(&ch_cfg);
    if (err != ESP_OK) return err;
    gpio_sleep_sel_dis(WATCH_PIN_VIBRATOR);

    s_vibration_queue = xQueueCreate(1, sizeof(vibration_cmd_t));
    if (!s_vibration_queue) return ESP_ERR_NO_MEM;
    s_initialized = true;

    // Tạo task rung với stack 2KB ở mức ưu tiên 3.
    if (xTaskCreate(vibration_task, "vibration_task", 2048, NULL, 3,
                    NULL) != pdPASS) {
        s_initialized = false;
        vQueueDelete(s_vibration_queue);
        s_vibration_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Động cơ rung đã cấu hình thành công trên GPIO %d", WATCH_PIN_VIBRATOR);
    return ESP_OK;
}

/* Khởi động một chu kỳ rung mới trong hệ thống */
static void vibration_start(vibration_pattern_t pattern, uint8_t strength, uint32_t duration_ms) {
    if (!s_initialized || !s_vibration_queue) return;
    vibration_cmd_t cmd = {
        .pattern = pattern,
        .strength = strength > 2 ? 1 : strength,
        .duration_ms = duration_ms,
    };
    xQueueOverwrite(s_vibration_queue, &cmd);
}

void watch_vibration_pulse(uint8_t strength, uint32_t duration_ms) {
    vibration_start(VIBRATION_PATTERN_PULSE, strength, duration_ms);
}

void watch_vibration_notify(uint8_t strength) {
    vibration_start(VIBRATION_PATTERN_NOTIFY, strength, 0);
}

void watch_vibration_alarm_start(uint8_t strength) {
    vibration_start(VIBRATION_PATTERN_ALARM, strength, 0);
}

void watch_vibration_stop(void) {
    if (!s_initialized || !s_vibration_queue) return;
    vibration_cmd_t cmd = {.pattern = VIBRATION_PATTERN_STOP};
    xQueueOverwrite(s_vibration_queue, &cmd);
    vibration_set_off();
}
