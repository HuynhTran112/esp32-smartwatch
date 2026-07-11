/* Dịch vụ nâng cấp phần mềm qua mạng HTTPS sử dụng API của Espressif */

#include "ota_service.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "network_state.h"
#include "watch_settings.h"

/* Forward declaration - tránh circular dependency với bluetooth component */
extern esp_err_t watch_bluetooth_send_command(const char *command);

#ifndef CONFIG_OTA_FIRMWARE_URL
#define CONFIG_OTA_FIRMWARE_URL ""
#endif

static const char *TAG = "OTA_SERVICE";
static SemaphoreHandle_t s_mutex = NULL; // Khóa đồng bộ trạng thái luồng
static portMUX_TYPE s_init_lock = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t s_task = NULL;        // Handle quản lý Task OTA FreeRTOS
static bool s_task_starting = false;
static watch_ota_status_t s_status = {
    .state = WATCH_OTA_IDLE,
    .progress_percent = -1,
};
static char s_ota_url[256];               // Bộ đệm lưu trữ địa chỉ URL tải firmware

static watch_ota_state_t s_last_notified_state = WATCH_OTA_IDLE;
static int s_last_notified_progress = -999;

// Thời gian chờ kết nối Wi-Fi tối đa cho tiến trình OTA: 20 giây (20000ms).
// Lý do: Quá trình bắt tay bảo mật WPA2 và cấp phát IP qua DHCP của router thường mất từ 3 đến 10 giây. 
// Ngưỡng 20 giây là thời gian chờ tối ưu giúp tránh việc ngắt giữa chừng trên các router chậm mà không duy trì 
// phát sóng RF gây tốn pin quá lâu nếu không tìm thấy điểm phát Wi-Fi.
#define WATCH_OTA_WIFI_WAIT_MS 20000U

static esp_err_t watch_ota_prepare_wifi(void) {
    esp_err_t err = watch_network_wifi_start();
    if (err != ESP_OK) return err;
    if (watch_network_is_wifi_connected()) return ESP_OK;

    err = watch_network_wifi_connect_saved();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    uint32_t waited_ms = 0;
    // Vòng lặp chờ kết nối Wi-Fi với chu kỳ quét trạng thái mỗi 250ms để tối ưu hóa CPU
    while (!watch_network_is_wifi_connected() && waited_ms < WATCH_OTA_WIFI_WAIT_MS) {
        vTaskDelay(pdMS_TO_TICKS(250));
        waited_ms += 250U;
    }
    return watch_network_is_wifi_connected() ? ESP_OK : ESP_ERR_TIMEOUT;
}

static const char *watch_ota_state_code(watch_ota_state_t state) {
    switch (state) {
        case WATCH_OTA_IDLE: return "I";
        case WATCH_OTA_RUNNING: return "R";
        case WATCH_OTA_SUCCEEDED: return "S";
        case WATCH_OTA_FAILED: return "F";
        default: return "U";
    }
}

static void watch_ota_notify_status(watch_ota_state_t state, int progress_percent) {
    if (state == s_last_notified_state && progress_percent == s_last_notified_progress) {
        return;
    }
    s_last_notified_state = state;
    s_last_notified_progress = progress_percent;

    char msg[24];
    snprintf(msg, sizeof(msg), "OTA_STAT|%s|%d", watch_ota_state_code(state), progress_percent);
    ESP_ERROR_CHECK_WITHOUT_ABORT(watch_bluetooth_send_command(msg));
}

esp_err_t watch_ota_init(void) {
    if (s_mutex) return ESP_OK;

    SemaphoreHandle_t new_mutex = xSemaphoreCreateMutex();
    if (!new_mutex) return ESP_ERR_NO_MEM;

    taskENTER_CRITICAL(&s_init_lock);
    if (!s_mutex) {
        s_mutex = new_mutex;
        new_mutex = NULL;
    }
    taskEXIT_CRITICAL(&s_init_lock);

    if (new_mutex) {
        vSemaphoreDelete(new_mutex);
    }
    return ESP_OK;
}

static void watch_ota_lock(void) {
    if (s_mutex) xSemaphoreTake(s_mutex, portMAX_DELAY);
}

static void watch_ota_unlock(void) {
    if (s_mutex) xSemaphoreGive(s_mutex);
}

static void watch_ota_write_status_locked(watch_ota_state_t state, int progress_percent,
                                          int bytes_read, int image_size, esp_err_t err,
                                          const char *message) {
    s_status.state = state;
    s_status.progress_percent = progress_percent;
    s_status.bytes_read = bytes_read;
    s_status.image_size = image_size;
    s_status.last_error = err;
    if (message) {
        snprintf(s_status.message, sizeof(s_status.message), "%s", message);
    }
}

static void watch_ota_set_status(watch_ota_state_t state, int progress_percent,
                                 int bytes_read, int image_size, esp_err_t err,
                                 const char *message) {
    watch_ota_lock();
    watch_ota_write_status_locked(state, progress_percent, bytes_read, image_size, err, message);
    watch_ota_unlock();
    watch_ota_notify_status(state, progress_percent);
}

bool watch_ota_url_configured(void) {
    char url[sizeof(s_ota_url)] = {0};
    if (watch_settings_get_ota_url(url, sizeof(url)) == ESP_OK && url[0] != '\0') {
        return true;
    }
    return CONFIG_OTA_FIRMWARE_URL[0] != '\0';
}

bool watch_ota_url_is_allowed(const char *url) {
    if (!url || strncmp(url, "https://", 8) != 0) return false;

    const char *authority = url + 8;
    const char *path = strchr(authority, '/');
    if (!path || path == authority) return false;

    for (const char *p = authority; p < path; ++p) {
        if (*p == '@' || *p == '?' || *p == '#') return false;
    }

    const char *path_end = strpbrk(path, "?#");
    if (!path_end) path_end = url + strlen(url);
    return path_end - path >= 4 && strncasecmp(path_end - 4, ".bin", 4) == 0;
}

void watch_ota_get_status(watch_ota_status_t *out) {
    if (!out) return;
    if (watch_ota_init() != ESP_OK) return;
    
    watch_ota_lock();
    *out = s_status;
    watch_ota_unlock();
}

/* Tác vụ FreeRTOS phụ chuyên trách xử lý giao thức HTTPS OTA tải phần mềm */
static void watch_ota_task(void *arg) {
    (void)arg;
    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
    bool wifi_was_started = watch_network_is_wifi_started();
    bool wifi_was_connected = watch_network_is_wifi_connected();
    bool restart_required = false;

    watch_ota_lock();
    s_task = current_task;
    s_task_starting = false;
    watch_ota_unlock();

    watch_ota_set_status(WATCH_OTA_RUNNING, 0, 0, 0, ESP_OK, "Đang kết nối Wi-Fi...");
    esp_err_t err = watch_ota_prepare_wifi();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Cannot prepare Wi-Fi for OTA: %s", esp_err_to_name(err));
        watch_ota_set_status(WATCH_OTA_FAILED, -1, 0, 0, err,
                             "Không thể kết nối Wi-Fi đã lưu");
        goto done;
    }

    /* Thiết lập cấu hình kết nối HTTP */
    esp_http_client_config_t http_config = {
        .url = s_ota_url,
        .crt_bundle_attach = esp_crt_bundle_attach, // Đính kèm bộ chứng chỉ số root bảo mật (CA Bundle)
        
        // Timeout tối đa của kết nối mạng HTTP: 15 giây (15000ms).
        // Lý do: Cho phép duy trì tải về qua mạng HTTPS không dây có tín hiệu yếu, 
        // hạn chế tối đa việc ngắt quãng tải giữa chừng khi gặp nhiễu sóng tạm thời.
        .timeout_ms = 15000,
        
        .keep_alive_enable = true,                  // Giữ kết nối socket liên tục tăng tốc độ truyền
    };
    
    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };
    esp_https_ota_handle_t handle = NULL;

    watch_ota_set_status(WATCH_OTA_RUNNING, 0, 0, 0, ESP_OK, "Đang kết nối máy chủ...");

    /* Bắt đầu quá trình nạp OTA */
    err = esp_https_ota_begin(&ota_config, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Giao tiếp HTTPS OTA lỗi khởi đầu: %s", esp_err_to_name(err));
        watch_ota_set_status(WATCH_OTA_FAILED, -1, 0, 0, err, "Không thể kết nối máy chủ");
        goto done;
    }

    /* Đọc app descriptor để xác nhận file tải về là firmware ESP-IDF hợp lệ. */
    esp_app_desc_t app_desc = {0};
    err = esp_https_ota_get_img_desc(handle, &app_desc);
    if (err != ESP_OK) {
        esp_https_ota_abort(handle);
        watch_ota_set_status(WATCH_OTA_FAILED, -1, 0, 0, err,
                             "File không phải firmware ESP-IDF hợp lệ");
        goto done;
    }
    watch_ota_lock();
    snprintf(s_status.new_version, sizeof(s_status.new_version), "%s", app_desc.version);
    watch_ota_unlock();

    int image_size = esp_https_ota_get_image_size(handle);
    watch_ota_set_status(WATCH_OTA_RUNNING, 0, 0, image_size, ESP_OK, "Đang tải firmware mới...");

    /* Vòng lặp tải và ghi dữ liệu nhị phân trực tiếp vào phân vùng Flash */
    do {
        err = esp_https_ota_perform(handle);
        int bytes_read = esp_https_ota_get_image_len_read(handle);
        image_size = esp_https_ota_get_image_size(handle);
        
        int progress = 0;
        if (image_size > 0) {
            progress = (bytes_read * 100) / image_size;
            if (progress > 100) progress = 100;
        }
        watch_ota_set_status(WATCH_OTA_RUNNING, progress, bytes_read, image_size, err,
                             "Đang ghi dữ liệu vào Flash...");
        
        // Trì hoãn ngắn 10ms để giải phóng bộ nhớ cache/Flash cho nhân xử lý Core 1 (Task vẽ UI LVGL chạy không bị giật)
        vTaskDelay(pdMS_TO_TICKS(10));
    } while (err == ESP_ERR_HTTPS_OTA_IN_PROGRESS);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Tải file phần mềm bị lỗi ngắt quãng: %s", esp_err_to_name(err));
        int bytes_read = esp_https_ota_get_image_len_read(handle);
        esp_https_ota_abort(handle); // Hủy bỏ và dọn dẹp phân vùng lỗi
        watch_ota_set_status(WATCH_OTA_FAILED, -1, bytes_read, image_size, err,
                             "Lỗi tải dữ liệu");
        goto done;
    }

    /* Xác thực gói dữ liệu nhận được có toàn vẹn không */
    if (!esp_https_ota_is_complete_data_received(handle)) {
        int bytes_read = esp_https_ota_get_image_len_read(handle);
        esp_https_ota_abort(handle);
        watch_ota_set_status(WATCH_OTA_FAILED, -1, bytes_read, image_size, ESP_FAIL,
                             "Lỗi: Thiếu gói tin dữ liệu!");
        goto done;
    }

    int final_bytes_read = esp_https_ota_get_image_len_read(handle);
    err = esp_https_ota_finish(handle); // Đóng tiến trình, hoán đổi phân vùng khởi động
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Firmware OTA không hợp lệ: %s", esp_err_to_name(err));
        watch_ota_set_status(WATCH_OTA_FAILED, -1, final_bytes_read, image_size, err,
                             "Firmware OTA không hợp lệ");
        goto done;
    }

    restart_required = true;
    watch_ota_set_status(WATCH_OTA_SUCCEEDED, 100, image_size, image_size, ESP_OK,
                         "Nâng cấp thành công! Đang tự động khởi động lại...");

done:
    if (!wifi_was_started) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(watch_network_wifi_stop());
    } else if (!wifi_was_connected) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(watch_network_wifi_disconnect());
    }
    watch_ota_lock();
    if (s_task == current_task) {
        s_task = NULL;
    }
    s_task_starting = false;
    watch_ota_unlock();
    if (restart_required) {
        vTaskDelay(pdMS_TO_TICKS(1500));
        esp_restart();
    }
    vTaskDelete(NULL); // Hủy tác vụ khi kết thúc
}

esp_err_t watch_ota_start(void) {
    esp_err_t init_err = watch_ota_init();
    if (init_err != ESP_OK) return init_err;

    char requested_url[sizeof(s_ota_url)] = {0};
    if (watch_settings_get_ota_url(requested_url, sizeof(requested_url)) != ESP_OK ||
        requested_url[0] == '\0') {
        snprintf(requested_url, sizeof(requested_url), "%s", CONFIG_OTA_FIRMWARE_URL);
    }

    if (!watch_ota_url_is_allowed(requested_url)) {
        watch_ota_set_status(WATCH_OTA_FAILED, -1, 0, 0, ESP_ERR_INVALID_ARG,
                             "Lỗi: Chưa thiết lập địa chỉ máy chủ tải!");
        return ESP_ERR_INVALID_ARG;
    }

    watch_ota_lock();
    if (s_task || s_task_starting) {
        watch_ota_unlock();
        return ESP_ERR_INVALID_STATE; // Lỗi: Tác vụ đang chạy rồi
    }
    snprintf(s_ota_url, sizeof(s_ota_url), "%s", requested_url);
    watch_ota_write_status_locked(WATCH_OTA_RUNNING, 0, 0, 0, ESP_OK, "Đang khởi chạy tiến trình OTA...");
    s_status.new_version[0] = '\0';
    s_task_starting = true;
    watch_ota_unlock();

    // Tạo task OTA nền với stack 8KB (yêu cầu bộ đệm cho HTTPS/TLS) ở mức ưu tiên 3 trên Core 0.
    BaseType_t ok = xTaskCreatePinnedToCore(watch_ota_task, "watch_ota_task", 8192, NULL, 3, NULL, 0);
    if (ok != pdPASS) {
        watch_ota_lock();
        s_task_starting = false;
        watch_ota_write_status_locked(WATCH_OTA_FAILED, -1, 0, 0, ESP_ERR_NO_MEM,
                                      "Không đủ RAM khởi chạy Task OTA");
        watch_ota_unlock();
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}
