/* Chương trình chính khởi tạo phần cứng và môi trường LVGL. */

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_touch_cst816s.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "board_config.h"
#include "bluetooth_manager.h"
#include "hardware_i2c_sensor.h"
#include "network_state.h"
#include "ota_service.h"
#include "ui.h"
#include "ui_screens.h"
#include "ui_utils.h"
#include "ui_notification_popup.h"
#include "gps_tracker.h"
#include "vibration_motor.h"
#include "watch_settings.h"
#include "watch_activity_log.h"
#include "battery.h"
#include "imu.h"
#include "heart_rate.h"

static const char *TAG = "MAIN";
static esp_timer_handle_t s_ui_guard_timer;
static esp_lcd_panel_handle_t s_lcd_panel_handle = NULL;

// Chu kỳ kiểm tra tình trạng đứng hình của giao diện (UI Watchdog Guard): 10 giây (10,000,000 micro-giây).
// Lý do: Chu kỳ 10 giây đủ dài để phát hiện đơ hệ thống mà không tiêu hao nhiều tài nguyên CPU xử lý.
#define WATCH_UI_GUARD_PERIOD_US 10000000ULL

// Ngưỡng thời gian xác nhận giao diện bị treo hoàn toàn: 30 giây (30000ms).
// Lý do: Nếu sau 30 giây mà bộ đếm hoạt động đồ họa (heartbeat) không thay đổi, 
// hệ thống sẽ tự động khởi động lại chip (esp_restart) để tự sửa lỗi phần mềm đơ màn hình.
#define WATCH_UI_STALL_TIMEOUT_MS 30000U

static void watch_ui_guard_timer_cb(void *arg) {
    (void)arg;
    if (ui_is_sleep_in_progress()) return;

    uint32_t last_heartbeat_ms = ui_runtime_last_heartbeat_ms();
    uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    if (last_heartbeat_ms != 0 && (uint32_t)(now_ms - last_heartbeat_ms) > WATCH_UI_STALL_TIMEOUT_MS) {
        ESP_LOGE(TAG, "LVGL heartbeat stalled; restarting");
        esp_restart();
    }
}

static void watch_ui_guard_start(void) {
    const esp_timer_create_args_t args = {
        .callback = watch_ui_guard_timer_cb,
        .name = "ui_guard",
    };
    esp_err_t err = esp_timer_create(&args, &s_ui_guard_timer);
    if (err == ESP_OK) {
        err = esp_timer_start_periodic(s_ui_guard_timer, WATCH_UI_GUARD_PERIOD_US);
    }
}

static void watch_backlight_init(uint32_t duty) {
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = WATCH_LCD_BACKLIGHT_LEDC_TIMER,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = 5000,
        .clk_cfg = LEDC_USE_RC_FAST_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    ledc_channel_config_t ch_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = WATCH_LCD_BACKLIGHT_LEDC_CH,
        .timer_sel = WATCH_LCD_BACKLIGHT_LEDC_TIMER,
        .gpio_num = WATCH_PIN_LCD_BACKLIGHT,
        .duty = duty,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch_cfg));
    gpio_sleep_sel_dis(WATCH_PIN_LCD_BACKLIGHT);
}

static lv_color_t watch_color_from_id(watch_color_id_t color_id) {
    switch (color_id) {
        case WATCH_COLOR_GREEN:  return COLOR_GREEN;
        case WATCH_COLOR_RED:    return COLOR_RED;
        case WATCH_COLOR_PURPLE: return COLOR_PURPLE;
        case WATCH_COLOR_ORANGE: return COLOR_ORANGE;
        case WATCH_COLOR_DEFAULT:
        default:                 return COLOR_BG;
    }
}

static void watch_lcd_init(esp_lcd_panel_handle_t *panel_handle, esp_lcd_panel_io_handle_t *io_handle) {
    spi_bus_config_t bus_cfg = {
        .sclk_io_num = WATCH_PIN_LCD_SCLK,
        .mosi_io_num = WATCH_PIN_LCD_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = WATCH_LCD_H_RES * WATCH_LCD_V_RES * sizeof(lv_color_t) / 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(WATCH_LCD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = WATCH_PIN_LCD_DC,
        .cs_gpio_num = WATCH_PIN_LCD_CS,
        .pclk_hz = WATCH_LCD_PIXEL_CLK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)WATCH_LCD_SPI_HOST, &io_cfg, io_handle));

    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_direction(WATCH_PIN_LCD_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(WATCH_PIN_LCD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(WATCH_PIN_LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(150));

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = -1,
        .color_space = ESP_LCD_COLOR_SPACE_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(*io_handle, &panel_cfg, panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(*panel_handle));
    vTaskDelay(pdMS_TO_TICKS(150));
    ESP_ERROR_CHECK(esp_lcd_panel_init(*panel_handle));
    vTaskDelay(pdMS_TO_TICKS(150));

    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(*panel_handle, 0, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(*panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(*panel_handle, true));
}

void watch_lcd_enter_sleep(void) {
    if (s_lcd_panel_handle) {
        ESP_LOGI("LCD", "Tắt hiển thị màn hình LCD ST7789...");
        (void)esp_lcd_panel_disp_on_off(s_lcd_panel_handle, false);
    }
}

static void watch_touch_init(esp_lcd_touch_handle_t *touch_handle) {
    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = WATCH_PIN_TOUCH_SDA,
        .scl_io_num = WATCH_PIN_TOUCH_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = WATCH_TOUCH_I2C_CLK_HZ,
    };
    ESP_ERROR_CHECK(i2c_param_config(WATCH_TOUCH_I2C_PORT, &i2c_cfg));
    ESP_ERROR_CHECK(i2c_driver_install(WATCH_TOUCH_I2C_PORT, I2C_MODE_MASTER, 0, 0, 0));

    gpio_reset_pin(WATCH_PIN_TOUCH_RST);
    gpio_set_direction(WATCH_PIN_TOUCH_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(WATCH_PIN_TOUCH_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(WATCH_PIN_TOUCH_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Quét cổng I2C xác nhận thiết bị
    for (int addr = 1; addr < 127; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(WATCH_TOUCH_I2C_PORT, cmd, pdMS_TO_TICKS(20));
        i2c_cmd_link_delete(cmd);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Touch I2C address: 0x%02X", addr);
        }
    }

    esp_lcd_panel_io_handle_t tp_io_handle;
    esp_lcd_panel_io_i2c_config_t tp_io_cfg = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    tp_io_cfg.scl_speed_hz = 0;

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)WATCH_TOUCH_I2C_PORT, &tp_io_cfg, &tp_io_handle));

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = WATCH_LCD_H_RES,
        .y_max = WATCH_LCD_V_RES,
        .rst_gpio_num = WATCH_PIN_TOUCH_RST,
        .int_gpio_num = WATCH_PIN_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 1,
        },
    };
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst816s(tp_io_handle, &tp_cfg, touch_handle));
    gpio_set_pull_mode((gpio_num_t)WATCH_PIN_TOUCH_INT, GPIO_PULLUP_ONLY);
}

static void watch_lvgl_port_init(esp_lcd_panel_handle_t panel_handle, esp_lcd_panel_io_handle_t io_handle, esp_lcd_touch_handle_t touch_handle) {
    lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    const size_t pixel_bytes = sizeof(lv_color_t);
    const uint32_t partial_lines = 48;
    const size_t full_screen_bytes = (size_t)WATCH_LCD_H_RES * partial_lines * pixel_bytes;
    uint32_t chosen_buffer_pixels = WATCH_LCD_H_RES * partial_lines;
    bool chosen_double_buffer = false;
    bool chosen_buff_spiram = false;

    lvgl_cfg.task_affinity = 1;
    lvgl_cfg.task_priority = 6;
    lvgl_cfg.task_stack = 8192;
    lvgl_cfg.timer_period_ms = 16;
    lvgl_cfg.task_max_sleep_ms = 100;

    if (psram_free >= (full_screen_bytes + 32768)) {
        chosen_buffer_pixels = WATCH_LCD_H_RES * partial_lines;
        chosen_buff_spiram = true;
        chosen_double_buffer = (psram_free >= (full_screen_bytes * 2 + 65536));
        lvgl_cfg.timer_period_ms = chosen_double_buffer ? 8 : 12;
    } else {
        chosen_buffer_pixels = WATCH_LCD_H_RES * WATCH_LCD_V_RES / 10;
        chosen_double_buffer = false;
        chosen_buff_spiram = false;
    }

    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = chosen_buffer_pixels,
        .double_buffer = chosen_double_buffer,
        .trans_size = WATCH_LCD_H_RES * partial_lines,
        .hres = WATCH_LCD_H_RES,
        .vres = WATCH_LCD_V_RES,
        .monochrome = false,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = false,
            .buff_spiram = chosen_buff_spiram,
        },
    };
    lv_disp_t *disp = lvgl_port_add_disp(&disp_cfg);

    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = disp,
        .handle = touch_handle,
        .scale = {
            .x = 1.0f,
            .y = 1.05f, // Map reported coordinates (0-270/280) to screen height (0-284)
        }
    };
    lvgl_port_add_touch(&touch_cfg);
}

static void watch_bluetooth_notification_cb(const watch_bluetooth_notification_t *notif) {
    const watch_settings_t *settings = watch_settings_get();
    if (settings->vibration_enabled) {
        watch_vibration_notify(settings->vibration_strength);
    }
    ui_notification_popup_show(notif);
}

static void watch_bluetooth_telemetry_task(void *arg) {
    (void)arg;
    bool last_bt_connected = false;
    while (1) {
        uint32_t tel_interval_ms = 30000;
        bool connected = watch_bluetooth_is_connected();
        
        if (connected) {
            // Nếu vừa mới kết nối BLE, đợi 1s cho ổn định rồi gửi dữ liệu offline ngay
            if (!last_bt_connected) {
                vTaskDelay(pdMS_TO_TICKS(1000));
            }

            // Kiểm tra xem có danh sách bản ghi nhịp tim đo offline nào chưa đồng bộ không
            watch_health_record_t records[WATCH_HEALTH_MAX_RECORDS];
            uint8_t count = 0;
            if (watch_settings_get_health_records(records, &count) == ESP_OK && count > 0) {
                bool all_ok = true;
                for (int i = 0; i < count; i++) {
                    char msg[64];
                    // Định dạng gói tin: HTH|heart_rate|spo2|timestamp
                    snprintf(msg, sizeof(msg), "HTH|%u|%u|%lu",
                             records[i].heart_rate,
                             records[i].spo2,
                             (unsigned long)records[i].timestamp);
                    if (watch_bluetooth_send_command(msg) != ESP_OK) {
                        all_ok = false;
                        break;
                    }
                    vTaskDelay(pdMS_TO_TICKS(100)); // Tránh tràn bộ đệm truyền nhận BLE
                }
                if (all_ok) {
                    watch_settings_clear_health_records(); // Đồng bộ xong giải phóng hoàn toàn bộ đệm NVS
                    ESP_LOGI("BLE_TEL", "Sync %d offline health records successfully!", count);
                }
            }

            watch_battery_data_t bat = {0};
            watch_imu_data_t imu = {0};
            watch_heart_rate_data_t hr = {0};
            watch_gps_metrics_t gps = {0};
            
            bool bat_ok = hardware_i2c_sensor_get_battery(&bat);
            bool imu_ok = hardware_i2c_sensor_get_imu(&imu);
            bool hr_ok = hardware_i2c_sensor_get_heart_rate(&hr) && hr.valid;
            bool gps_ok = watch_gps_get_metrics(&gps);
            bool sport_active = watch_gps_is_active();
            
            tel_interval_ms = sport_active ? 2000 : 30000;

            uint32_t session_id = ui_activity_get_current_session_id();
            const char *sport_mode = sport_active ? ui_activity_get_current_sport_mode() : "none";
            char tel[256];
            
            snprintf(tel, sizeof(tel),
                     "TEL|%lu|%.3f|%.1f|%d|%.0f|%.0f|%s|%d|%.6f|%.6f|%lu|%u|%.1f|%.1f|%lu",
                     (unsigned long)(imu_ok ? imu.step_count : 0),
                     gps_ok ? gps.total_distance_km : 0.0f,
                     gps_ok ? gps.speed_kmh : 0.0f,
                     bat_ok ? (int)(bat.soc_percent + 0.5f) : -1,
                     hr_ok ? hr.heart_rate : 0.0f,
                     (hr_ok && hr.spo2_valid) ? hr.spo2 : 0.0f,
                     sport_mode,
                     gps_ok && gps.fix_valid ? 1 : 0,
                     gps_ok ? gps.latitude_deg : 0.0,
                     gps_ok ? gps.longitude_deg : 0.0,
                     (unsigned long)session_id,
                     gps_ok ? gps.satellites_in_use : 0U,
                     gps_ok ? (double)gps.hdop : 0.0,
                     gps_ok ? (double)gps.avg_cn0_dbhz : 0.0,
                     (unsigned long)(gps_ok ? gps.fix_age_ms : UINT32_MAX));
            watch_bluetooth_send_command(tel);
        }
        last_bt_connected = connected;
        vTaskDelay(pdMS_TO_TICKS(tel_interval_ms));
    }
}

#if CONFIG_WATCH_HEAP_MONITOR
static void watch_heap_monitor_task(void *arg) {
    (void)arg;
    uint32_t sample = 0;
    size_t min_internal_free = SIZE_MAX;
    size_t min_internal_largest = SIZE_MAX;
    size_t min_psram_free = SIZE_MAX;
    size_t min_psram_largest = SIZE_MAX;

    for (;;) {
        size_t total_free = esp_get_free_heap_size();
        size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        size_t internal_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
        size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        size_t psram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);

        if (internal_free < min_internal_free) min_internal_free = internal_free;
        if (internal_largest < min_internal_largest) min_internal_largest = internal_largest;
        if (psram_free < min_psram_free) min_psram_free = psram_free;
        if (psram_largest < min_psram_largest) min_psram_largest = psram_largest;

        ESP_LOGI(TAG,
                 "Heap free: %lu | Internal: %lu (min %lu) | Internal largest: %lu (min %lu) | PSRAM: %lu (min %lu) | PSRAM largest: %lu (min %lu)",
                 (unsigned long)total_free,
                 (unsigned long)internal_free,
                 (unsigned long)min_internal_free,
                 (unsigned long)internal_largest,
                 (unsigned long)min_internal_largest,
                 (unsigned long)psram_free,
                 (unsigned long)min_psram_free,
                 (unsigned long)psram_largest,
                 (unsigned long)min_psram_largest);

        if ((sample++ % 4U) == 0U) {
            bool heap_ok = heap_caps_check_integrity_all(true);
            if (!heap_ok) {
                ESP_LOGE(TAG, "Heap error!");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}
#endif

static bool watch_detect_triple_tap(void) {
    gpio_config_t touch_pin_cfg = {
        .pin_bit_mask = 1ULL << WATCH_PIN_TOUCH_INT,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&touch_pin_cfg);

    int tap_count = 1;
    uint32_t start_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    bool last_state = gpio_get_level(WATCH_PIN_TOUCH_INT);

    while ((uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS) - start_ms < 800) {
        bool current_state = gpio_get_level(WATCH_PIN_TOUCH_INT);
        if (last_state == 1 && current_state == 0) {
            tap_count++;
            if (tap_count >= 3) return true;
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        last_state = current_state;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return false;
}

void app_main(void) {
    esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();
    if (wakeup_cause == ESP_SLEEP_WAKEUP_EXT1) {
        gpio_config_t btn_pin_cfg = {
            .pin_bit_mask = (1ULL << WATCH_PIN_BUTTON_POWER),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&btn_pin_cfg);

        // Đè giữ nút nguồn ít nhất 1.5 giây mới cho phép khởi động
        bool held = true;
        for (int i = 0; i < 75; i++) {
            if (gpio_get_level(WATCH_PIN_BUTTON_POWER) != 0) {
                held = false;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(20));
        }

        if (!held) {
            esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
            esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
            esp_sleep_enable_ext1_wakeup((1ULL << WATCH_PIN_BUTTON_POWER), ESP_EXT1_WAKEUP_ANY_LOW);
            vTaskDelay(pdMS_TO_TICKS(100));
            esp_deep_sleep_start();
        }
    }

    esp_log_level_set("lcd_panel.io.i2c", ESP_LOG_INFO);
    esp_log_level_set("CST816S", ESP_LOG_INFO);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ui_time_set_timezone_offset(7 * 60);

    ESP_ERROR_CHECK_WITHOUT_ABORT(watch_settings_init());
    ESP_ERROR_CHECK_WITHOUT_ABORT(watch_pm_configure());
    ESP_ERROR_CHECK_WITHOUT_ABORT(watch_activity_log_init());
    const watch_settings_t *settings = watch_settings_get();

    if (wakeup_cause == ESP_SLEEP_WAKEUP_EXT0) {
        if (!settings->quick_wake_enabled || !watch_detect_triple_tap()) {
            gpio_config_t touch_pin_cfg = {
                .pin_bit_mask = 1ULL << WATCH_PIN_TOUCH_INT,
                .mode = GPIO_MODE_INPUT,
                .pull_up_en = GPIO_PULLUP_ENABLE,
                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                .intr_type = GPIO_INTR_DISABLE,
            };
            gpio_config(&touch_pin_cfg);

            gpio_config_t btn_pin_cfg = {
                .pin_bit_mask = (1ULL << WATCH_PIN_BUTTON_POWER),
                .mode = GPIO_MODE_INPUT,
                .pull_up_en = GPIO_PULLUP_ENABLE,
                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                .intr_type = GPIO_INTR_DISABLE,
            };
            gpio_config(&btn_pin_cfg);

            for (int i = 0; i < 25 && gpio_get_level(WATCH_PIN_TOUCH_INT) == 0; i++) {
                vTaskDelay(pdMS_TO_TICKS(20));
            }
            for (int i = 0; i < 25 && gpio_get_level(WATCH_PIN_BUTTON_POWER) == 0; i++) {
                vTaskDelay(pdMS_TO_TICKS(20));
            }

            esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
            esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
            esp_sleep_enable_ext1_wakeup((1ULL << WATCH_PIN_BUTTON_POWER), ESP_EXT1_WAKEUP_ANY_LOW);

            // Không cấu hình ngắt chạm để tránh tự động khởi động lại từ Deep Sleep khi chạm màn hình

            watch_gps_stop();
            hardware_i2c_sensor_set_heart_rate_enabled(false);
            vTaskDelay(pdMS_TO_TICKS(100));
            esp_deep_sleep_start();
        }
    }

    uint32_t backlight_duty = (uint32_t)settings->brightness_pct * 255U / 100U;
    ui_backlight_set_active_duty(backlight_duty);
    watch_backlight_init(backlight_duty);
    
    ESP_ERROR_CHECK_WITHOUT_ABORT(watch_vibration_init());

    esp_lcd_panel_handle_t panel_handle;
    esp_lcd_panel_io_handle_t io_handle;
    watch_lcd_init(&panel_handle, &io_handle);
    s_lcd_panel_handle = panel_handle;

    esp_lcd_touch_handle_t touch_handle;
    watch_touch_init(&touch_handle);

    watch_lvgl_port_init(panel_handle, io_handle, touch_handle);

    watch_network_init();
    ESP_ERROR_CHECK(watch_ota_init());
    ESP_ERROR_CHECK(watch_gps_control_init());
    ESP_ERROR_CHECK(watch_bluetooth_control_init());

    watch_bluetooth_set_new_callback(watch_bluetooth_notification_cb);

    hardware_i2c_sensor_set_screen_timeout(settings->screen_timeout_sec);
    hardware_i2c_sensor_set_quick_wake_enabled(settings->quick_wake_enabled);
    ESP_ERROR_CHECK(hardware_i2c_sensor_start());

    if (lvgl_port_lock(-1)) {
        ui_language_set((ui_language_t)settings->language);
        ui_watchface_set_style(settings->watchface_style);
        ui_menu_set_system_color(settings->icons_monochrome, watch_color_from_id(settings->icon_color));
        ui_init();
        ui_notification_popup_init();
        lvgl_port_unlock();
    }
    // Không tự động bật Bluetooth khi khởi động, chỉ bật khi người dùng kích hoạt trong màn hình
    // ESP_ERROR_CHECK_WITHOUT_ABORT(watch_bluetooth_request_enabled(watch_settings_get_bluetooth_enabled()));
    watch_ui_guard_start();

#if CONFIG_WATCH_HEAP_MONITOR
    xTaskCreatePinnedToCore(watch_heap_monitor_task, "heap_mon", 4096, NULL, 1, NULL, 0);
#endif
    xTaskCreatePinnedToCore(watch_bluetooth_telemetry_task, "ble_tel", 4096, NULL, 1, NULL, 0);
}
