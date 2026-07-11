/* Kiểm soát và theo dõi trạng thái kết nối mạng Wi-Fi Station.
   Khởi chạy Wi-Fi ở chế độ Modem Sleep để tiết kiệm năng lượng,
   và xử lý sự kiện IP/Wi-Fi an toàn đa tác vụ bằng Spinlock. */

#include "network_state.h"
#include "watch_settings.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_err.h"
#include <stdint.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "NET_STATE";

static portMUX_TYPE s_state_lock = portMUX_INITIALIZER_UNLOCKED; // Spinlock bảo vệ các biến trạng thái hệ thống chạy trên đa nhân
static bool s_wifi_connected = false;                           // Cờ báo đã kết nối Wi-Fi thành công
static bool s_connect_attempt_active = false;                  // Cờ báo tiến trình kết nối đang diễn ra
static bool s_ignore_next_disconnect = false;                  // Cờ bỏ qua sự kiện ngắt kết nối tạm thời
static bool s_scanning = false;                                // Cờ chống chạy nhiều lần quét Wi-Fi song song
static watch_network_wifi_connect_state_t s_connect_state = WATCH_NETWORK_WIFI_CONNECT_IDLE;

// Các bộ đệm tĩnh lưu trữ tài khoản Wi-Fi
// SSID tối đa 32 ký tự theo chuẩn Wi-Fi, cộng 1 ký tự kết thúc chuỗi.
static char s_pending_ssid_storage[33];
// Mật khẩu WPA2 tối đa 64 ký tự, cộng 1 ký tự kết thúc chuỗi.
static char s_pending_password[65];

static char s_connected_ssid_storage[2][33];                    // Vùng đệm kép lưu SSID đã kết nối ổn định
static char s_last_ssid[33];                                    // Lưu SSID cuối cùng kết nối thành công
static char s_last_password[65];                                // Lưu mật khẩu cuối cùng kết nối thành công
static const char s_empty_ssid[33] = "";
static uint8_t s_connected_ssid_idx = 0;
static const char *s_connected_ssid = s_empty_ssid;
static bool s_loaded_saved_credentials = false;                 // Cờ báo đã nạp thành công cấu hình cũ từ Flash
static bool s_wifi_started = false;

static void watch_network_safe_copy(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0) return;
    strncpy(dst, src ? src : "", dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static void watch_network_set_ignore_next_disconnect(bool ignore) {
    taskENTER_CRITICAL(&s_state_lock);
    s_ignore_next_disconnect = ignore;
    taskEXIT_CRITICAL(&s_state_lock);
}

static void watch_network_mark_connect_failed(void) {
    taskENTER_CRITICAL(&s_state_lock);
    s_ignore_next_disconnect = false;
    s_connect_attempt_active = false;
    s_connect_state = WATCH_NETWORK_WIFI_CONNECT_FAILED;
    s_pending_password[0] = '\0';
    taskEXIT_CRITICAL(&s_state_lock);
}

void watch_network_begin_wifi_connect_attempt(const char *ssid, const char *password) {
    taskENTER_CRITICAL(&s_state_lock);
    watch_network_safe_copy(s_pending_ssid_storage, sizeof(s_pending_ssid_storage), ssid);
    watch_network_safe_copy(s_pending_password, sizeof(s_pending_password), password);
    s_connect_state = WATCH_NETWORK_WIFI_CONNECT_CONNECTING;
    s_connect_attempt_active = true;
    taskEXIT_CRITICAL(&s_state_lock);
}

/* Hàm callback trung tâm xử lý các sự kiện Wi-Fi và IP từ hệ điều hành */
static void watch_network_event_cb(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    (void)arg;
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            wifi_event_sta_disconnected_t *disc = (wifi_event_sta_disconnected_t *)event_data;
            bool ignore_disconnect = false;

            taskENTER_CRITICAL(&s_state_lock);
            if (s_ignore_next_disconnect) {
                s_ignore_next_disconnect = false;
                ignore_disconnect = true;
            }
            taskEXIT_CRITICAL(&s_state_lock);

            if (ignore_disconnect) {
                ESP_LOGI(TAG, "Bỏ qua sự kiện ngắt kết nối tạm thời trước khi thử lại");
                return;
            }

            ESP_LOGI(TAG, "Sự kiện ngắt kết nối Wi-Fi. Mã nguyên nhân: %d", disc ? disc->reason : -1);
            
            taskENTER_CRITICAL(&s_state_lock);
            s_wifi_connected = false;
            s_connected_ssid = s_empty_ssid;
            if (s_connect_attempt_active || s_connect_state == WATCH_NETWORK_WIFI_CONNECT_CONNECTING) {
                s_connect_attempt_active = false;
                s_connect_state = WATCH_NETWORK_WIFI_CONNECT_FAILED;
            } else {
                s_connect_state = WATCH_NETWORK_WIFI_CONNECT_IDLE;
            }
            taskEXIT_CRITICAL(&s_state_lock);
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            uint8_t next_idx;
            char current_ssid[33] = {0};
            wifi_ap_record_t ap_info = {0};
            if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
                memcpy(current_ssid, ap_info.ssid, sizeof(ap_info.ssid));
            }
            ESP_LOGI(TAG, "Đã nhận được cấu hình IP từ DHCP Router!");

            taskENTER_CRITICAL(&s_state_lock);
            if (!s_connect_attempt_active) {
                next_idx = (uint8_t)(s_connected_ssid_idx ^ 1U);
                watch_network_safe_copy(s_connected_ssid_storage[next_idx],
                                        sizeof(s_connected_ssid_storage[next_idx]),
                                        current_ssid[0] ? current_ssid : s_last_ssid);
                s_wifi_connected = true;
                s_connect_state = WATCH_NETWORK_WIFI_CONNECT_SUCCESS;
                s_connected_ssid_idx = next_idx;
                s_connected_ssid = s_connected_ssid_storage[s_connected_ssid_idx];
                watch_network_safe_copy(s_last_ssid, sizeof(s_last_ssid), s_connected_ssid);
                taskEXIT_CRITICAL(&s_state_lock);
                return;
            }
            
            /* Cập nhật thông tin điểm kết nối thành công */
            next_idx = (uint8_t)(s_connected_ssid_idx ^ 1U);
            watch_network_safe_copy(s_connected_ssid_storage[next_idx],
                                    sizeof(s_connected_ssid_storage[next_idx]),
                                    s_pending_ssid_storage);
            s_wifi_connected = true;
            s_connect_attempt_active = false;
            s_connect_state = WATCH_NETWORK_WIFI_CONNECT_SUCCESS;
            s_connected_ssid_idx = next_idx;
            s_connected_ssid = s_connected_ssid_storage[s_connected_ssid_idx];
            
            watch_network_safe_copy(s_last_ssid, sizeof(s_last_ssid), s_connected_ssid);
            watch_network_safe_copy(s_last_password, sizeof(s_last_password), s_pending_password);
            taskEXIT_CRITICAL(&s_state_lock);

            /* Ghi nhớ thông tin kết nối Wi-Fi vào bộ nhớ Flash NVS */
            ESP_ERROR_CHECK_WITHOUT_ABORT(watch_settings_set_wifi_credentials(s_connected_ssid, s_pending_password));
        }
    }
}

void watch_network_init(void) {
    static bool inited = false;
    if (inited) return;
    inited = true;

    /* Khởi tạo Lớp TCP/IP Stack phần mềm */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta(); // Tạo cấu hình Station mặc định

    /* Khởi tạo phần cứng sóng Wi-Fi */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Đăng ký lắng nghe sự kiện Wi-Fi rớt kết nối và IP cấp phát */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &watch_network_event_cb, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &watch_network_event_cb, NULL, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); // Cấu hình chế độ trạm thu (Station STA)
    
    /* Đọc tài khoản Wi-Fi lưu trữ từ bộ nhớ Flash */
    char saved_ssid[33] = {0};
    char saved_password[65] = {0};
    if (watch_settings_get_wifi_credentials(saved_ssid, sizeof(saved_ssid),
                                            saved_password, sizeof(saved_password)) == ESP_OK &&
        saved_ssid[0] != '\0') {
        taskENTER_CRITICAL(&s_state_lock);
        watch_network_safe_copy(s_last_ssid, sizeof(s_last_ssid), saved_ssid);
        watch_network_safe_copy(s_last_password, sizeof(s_last_password), saved_password);
        s_loaded_saved_credentials = true;
        taskEXIT_CRITICAL(&s_state_lock);
    }
    ESP_LOGI(TAG, "Ngăn xếp mạng Wi-Fi đã khởi tạo hoàn tất ở chế độ Station");
}

esp_err_t watch_network_wifi_start(void) {
    taskENTER_CRITICAL(&s_state_lock);
    bool started = s_wifi_started;
    taskEXIT_CRITICAL(&s_state_lock);
    if (started) return ESP_OK;

    esp_err_t err = esp_wifi_start();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STOPPED) return err;
    
    // Kích hoạt Modem Sleep tiết kiệm pin tối đa cho Wi-Fi
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_ps(WIFI_PS_MAX_MODEM));

    taskENTER_CRITICAL(&s_state_lock);
    s_wifi_started = true;
    taskEXIT_CRITICAL(&s_state_lock);
    return ESP_OK;
}

esp_err_t watch_network_wifi_stop(void) {
    taskENTER_CRITICAL(&s_state_lock);
    bool started = s_wifi_started;
    bool connected = s_wifi_connected;
    s_ignore_next_disconnect = started && connected;
    s_wifi_connected = false;
    s_connect_attempt_active = false;
    s_scanning = false;
    s_connected_ssid = s_empty_ssid;
    s_connect_state = WATCH_NETWORK_WIFI_CONNECT_IDLE;
    taskEXIT_CRITICAL(&s_state_lock);
    if (!started) return ESP_OK;

    esp_err_t err = esp_wifi_stop();
    if (err == ESP_OK || err == ESP_ERR_WIFI_NOT_STARTED) {
        taskENTER_CRITICAL(&s_state_lock);
        s_wifi_started = false;
        taskEXIT_CRITICAL(&s_state_lock);
        return ESP_OK;
    }
    return err;
}

bool watch_network_is_wifi_started(void) {
    taskENTER_CRITICAL(&s_state_lock);
    bool started = s_wifi_started;
    taskEXIT_CRITICAL(&s_state_lock);
    return started;
}

esp_err_t watch_network_wifi_scan(wifi_ap_record_t *out_records, uint16_t *out_num) {
    if (!out_records || !out_num) return ESP_ERR_INVALID_ARG;
    esp_err_t start_err = watch_network_wifi_start();
    if (start_err != ESP_OK) return start_err;

    /* Chỉ cho phép quét Wi-Fi nếu tác vụ gọi từ Task phụ có tên chỉ định (wifi_scan)
     * nhằm tránh block hoặc trễ luồng vẽ giao diện UI chính */
    const char *task_name = pcTaskGetName(NULL);
    if (!task_name || strcmp(task_name, WATCH_NETWORK_WIFI_SCAN_TASK_NAME) != 0) {
        ESP_LOGW(TAG, "Từ chối quét Wi-Fi từ tác vụ không hợp lệ: '%s'", task_name ? task_name : "Ẩn danh");
        return ESP_ERR_INVALID_STATE;
    }

    taskENTER_CRITICAL(&s_state_lock);
    bool busy = s_scanning || s_connect_state == WATCH_NETWORK_WIFI_CONNECT_CONNECTING;
    if (!busy) s_scanning = true;
    taskEXIT_CRITICAL(&s_state_lock);
    if (busy) {
        ESP_LOGW(TAG, "Bỏ qua quét Wi-Fi khi đang kết nối dở dang");
        return ESP_ERR_INVALID_STATE;
    }

    wifi_scan_config_t scan_cfg = {.scan_type = WIFI_SCAN_TYPE_ACTIVE};
    
    /* Bắt đầu quét Wi-Fi (Hàm chặn luồng tới khi quét xong khoảng 1-2 giây) */
    esp_err_t err = esp_wifi_scan_start(&scan_cfg, true);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Lỗi quét Wi-Fi: %d", err);
        taskENTER_CRITICAL(&s_state_lock);
        s_scanning = false;
        taskEXIT_CRITICAL(&s_state_lock);
        return err;
    }
    
    uint16_t capacity = *out_num;
    err = esp_wifi_scan_get_ap_records(&capacity, out_records); // Trích xuất kết quả quét được
    if (err == ESP_OK) {
        *out_num = capacity;
    }
    taskENTER_CRITICAL(&s_state_lock);
    s_scanning = false;
    taskEXIT_CRITICAL(&s_state_lock);
    return err;
}

esp_err_t watch_network_wifi_connect(const char *ssid, const char *password) {
    if (!ssid) return ESP_ERR_INVALID_ARG;
    esp_err_t start_err = watch_network_wifi_start();
    if (start_err != ESP_OK) return start_err;
  
    wifi_config_t wifi_conf = {0};
    strncpy((char *)wifi_conf.sta.ssid, ssid, sizeof(wifi_conf.sta.ssid) - 1);
    if (password) {
        strncpy((char *)wifi_conf.sta.password, password, sizeof(wifi_conf.sta.password) - 1);
    }

    wifi_conf.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    // Tự động kết nối lại tối đa 5 lần nếu mất kết nối Wi-Fi.
    wifi_conf.sta.failure_retry_cnt = 5;

    ESP_LOGI(TAG, "Bắt đầu kết nối Wi-Fi SSID: '%s'", ssid);

    watch_network_set_ignore_next_disconnect(true);
    esp_err_t err = esp_wifi_disconnect(); // Đứt liên kết cũ
    if (err == ESP_ERR_WIFI_NOT_CONNECT) {
        watch_network_set_ignore_next_disconnect(false);
    } else if (err != ESP_OK) {
        ESP_LOGW(TAG, "Ngắt kết nối cũ thất bại: %d", err);
        watch_network_mark_connect_failed();
        return err;
    }

    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_conf);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Lỗi nạp cấu hình Wi-Fi: %d", err);
        watch_network_mark_connect_failed();
        return err;
    }

    watch_network_begin_wifi_connect_attempt(ssid, password);
  
    err = esp_wifi_connect(); // Phát lệnh kết nối bất đồng bộ
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Gửi lệnh kết nối Wi-Fi lỗi: %d", err);
        watch_network_mark_connect_failed();
    }
    return err;
}

esp_err_t watch_network_wifi_connect_saved(void) {
    char ssid[33] = {0};
    char password[65] = {0};
    if (!watch_network_get_last_wifi_credentials(ssid, sizeof(ssid), password, sizeof(password))) {
        return ESP_ERR_NOT_FOUND;
    }
    if (watch_network_is_wifi_connected()) return ESP_OK;
    return watch_network_wifi_connect(ssid, password);
}

esp_err_t watch_network_wifi_disconnect(void) {
    taskENTER_CRITICAL(&s_state_lock);
    s_ignore_next_disconnect = false;
    s_connect_attempt_active = false;
    s_wifi_connected = false;
    s_connected_ssid = s_empty_ssid;
    s_connect_state = WATCH_NETWORK_WIFI_CONNECT_IDLE;
    taskEXIT_CRITICAL(&s_state_lock);

    esp_err_t err = esp_wifi_disconnect();
    if (err == ESP_ERR_WIFI_NOT_CONNECT) return ESP_OK;
    return err;
}

bool watch_network_get_last_wifi_credentials(char *ssid, size_t ssid_size,
                                             char *password, size_t password_size) {
    if (!ssid || ssid_size == 0) return false;
    taskENTER_CRITICAL(&s_state_lock);
    bool has = s_last_ssid[0] != '\0' || s_loaded_saved_credentials;
    if (has) {
        watch_network_safe_copy(ssid, ssid_size, s_last_ssid);
        watch_network_safe_copy(password, password_size, s_last_password);
    }
    taskEXIT_CRITICAL(&s_state_lock);
    return has;
}

bool watch_network_is_wifi_connected(void) {
    taskENTER_CRITICAL(&s_state_lock);
    bool connected = s_wifi_connected;
    taskEXIT_CRITICAL(&s_state_lock);
    return connected;
}

watch_network_wifi_connect_state_t watch_network_get_wifi_connect_state(void) {
    taskENTER_CRITICAL(&s_state_lock);
    watch_network_wifi_connect_state_t state = s_connect_state;
    taskEXIT_CRITICAL(&s_state_lock);
    return state;
}

void watch_network_clear_wifi_connect_state(void) {
    taskENTER_CRITICAL(&s_state_lock);
    if (s_connect_state != WATCH_NETWORK_WIFI_CONNECT_CONNECTING) {
        s_connect_state = s_wifi_connected ? WATCH_NETWORK_WIFI_CONNECT_SUCCESS : WATCH_NETWORK_WIFI_CONNECT_IDLE;
    }
    taskEXIT_CRITICAL(&s_state_lock);
}

const char *watch_network_get_connected_ssid(void) {
    return s_connected_ssid;
}

bool watch_network_is_connected_ssid(const char *ssid) {
    if (!ssid) return false;
    const char *connected_ssid;
    taskENTER_CRITICAL(&s_state_lock);
    connected_ssid = s_connected_ssid;
    taskEXIT_CRITICAL(&s_state_lock);
    return connected_ssid[0] && strcmp(connected_ssid, ssid) == 0;
}
