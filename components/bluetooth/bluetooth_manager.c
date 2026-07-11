/* GATT Server quản lý truyền nhận dữ liệu thông báo và telemetry cảm biến qua BLE.
   Hỗ trợ chia nhỏ vết tọa độ GPS thành các gói tin nhỏ (chunks) để truyền tải an toàn. */

#include "bluetooth_manager.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "ota_service.h"
#include "gps_tracker.h"
#include "watch_settings.h"
#include "watch_activity_log.h"
#include "sdkconfig.h"
#include "esp_timer.h"

/* Forward declarations - tránh circular dep bluetooth <-> ui */
extern void ui_time_set_timezone_offset(int offset_minutes);
extern void ui_notification_popup_show(const watch_bluetooth_notification_t *notif);
extern void ui_quick_replies_clear_from_ble(void);
extern void ui_quick_replies_add_from_ble(const char *reply);

static const char *TAG = "BLE_NOTIFY";

#ifndef CONFIG_WATCH_BLE_PAIR_PASSKEY
// Mã pin mặc định kết nối BLE
#define CONFIG_WATCH_BLE_PAIR_PASSKEY 123456
#endif

// Connection ID mặc định
#define WATCH_BLUETOOTH_CONN_ID_NONE          0xFFFF

// Độ dài tối đa gói tin gửi qua BLE
#define WATCH_BLUETOOTH_NOTIFY_CMD_MAX_LEN    512

// Thời gian chờ khóa Mutex bảo vệ bộ đệm Bluetooth
#define WATCH_BLUETOOTH_MUTEX_TIMEOUT_MS      50

/* ===================================================================
 *  CẤU HÌNH UUID - Phục vụ định danh dịch vụ kết nối với ứng dụng Flutter
 * =================================================================== */
/* Service UUID: 12345678-1234-1234-1234-123456789abc (Little Endian) */
static const uint8_t service_uuid128[16] = {
    0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12, 0x34, 0x12,
    0x34, 0x12, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12
};

/* Characteristic UUID: abcd1234-ab12-cd34-ef56-123456789abc (Little Endian) */
static const uint8_t char_uuid128[16] = {
    0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12, 0x56, 0xef,
    0x34, 0xcd, 0x12, 0xab, 0x34, 0x12, 0xcd, 0xab
};

/* Các biến trạng thái kết nối và bảo vệ tương tranh */
static volatile bool s_inited = false;
static volatile bool s_enabled = false;
static bool s_connected = false;
static uint32_t s_connection_generation = 0; // Số thứ tự phiên kết nối
static SemaphoreHandle_t s_mutex = NULL;      // Mutex bảo vệ mảng thông báo nhận được
static SemaphoreHandle_t s_notify_mutex = NULL; // Serialize notify sends and BLE teardown
static QueueHandle_t s_track_send_queue;
static TaskHandle_t s_track_send_task;
static QueueHandle_t s_bt_control_queue;
static TaskHandle_t s_bt_control_task;
static volatile bool s_bt_transitioning;
static esp_timer_handle_t s_adv_timer = NULL;

typedef enum {
    BLE_TRACK_SEND_CURRENT = 0,
    BLE_TRACK_SEND_HISTORY,
    BLE_TRACK_SEND_BUFFER,
    BLE_TRACK_SEND_ACTIVITY_LIST,
} ble_track_send_type_t;

typedef struct {
    ble_track_send_type_t type;
    size_t history_index;
    char *track;
} ble_track_send_cmd_t;

/* Mảng bộ đệm thông báo, hoạt động theo cơ chế dịch chuyển phần tử */
static watch_bluetooth_notification_t s_notifications[WATCH_BLUETOOTH_NOTIF_MAX];
static int s_notif_count = 0;
static uint32_t s_notif_overflow_count = 0;
static uint32_t s_next_notif_seq = 1;
static watch_bluetooth_nav_data_t s_nav_data = {0};

static watch_bluetooth_notif_new_cb_t s_new_cb = NULL;

/* Các handle quản lý giao tiếp GATTS của ESP32-S3 */
static esp_gatt_if_t s_gatts_if = ESP_GATT_IF_NONE;
static uint16_t s_service_handle = 0;
static uint16_t s_char_handle = 0;
static uint16_t s_descr_handle = 0;
static uint16_t s_conn_id = WATCH_BLUETOOTH_CONN_ID_NONE;
static uint16_t s_mtu = 23;
static bool s_notify_enabled = false;
static char s_ota_url_rx[256];
static size_t s_ota_url_rx_len;
static bool s_ota_url_rx_active;

static void ble_send_track_chunks_blocking(const char *track);

static void ble_control_worker(void *arg) {
    (void)arg;
    bool enabled = false;
    for (;;) {
        if (xQueueReceive(s_bt_control_queue, &enabled, portMAX_DELAY) != pdTRUE) continue;
        s_bt_transitioning = true;
        esp_err_t err = watch_bluetooth_set_enabled(enabled);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "BLE control request failed: %s", esp_err_to_name(err));
        }
        s_bt_transitioning = false;
    }
}

static uint16_t ble_crc16_ccitt(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

static uint32_t ble_crc32_update(uint32_t crc, const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ ((crc & 1U) ? 0xEDB88320U : 0U);
        }
    }
    return crc;
}

static int ble_hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static bool ble_parse_crc_hex(const char *hex, uint16_t *out_crc) {
    uint16_t value = 0;
    for (int i = 0; i < 4; i++) {
        int n = ble_hex_nibble(hex[i]);
        if (n < 0) return false;
        value = (uint16_t)((value << 4) | (uint16_t)n);
    }
    if (out_crc) *out_crc = value;
    return true;
}

static bool ble_strip_crc_frame(const char *data, int len, char *out, size_t out_size, int *out_len) {
    if (!data || len <= 0 || !out || out_size == 0 || !out_len) return false;

    int star = -1;
    for (int i = len - 1; i >= 0; i--) {
        if (data[i] == '*') {
            star = i;
            break;
        }
    }

    if (star >= 0 && len - star == 5) {
        uint16_t rx_crc = 0;
        if (ble_parse_crc_hex(data + star + 1, &rx_crc)) {
            uint16_t calc_crc = ble_crc16_ccitt((const uint8_t *)data, (size_t)star);
            if (rx_crc == calc_crc) {
                if ((size_t)star >= out_size) return false;
                memcpy(out, data, (size_t)star);
                out[star] = '\0';
                *out_len = star;
                return true;
            } else {
                ESP_LOGW(TAG, "BLE CRC mismatch (using raw fallback): rx=0x%04X calc=0x%04X body=%.*s", rx_crc, calc_crc, star, data);
            }
        } else {
            ESP_LOGW(TAG, "BLE CRC hex invalid (using raw fallback)");
        }
    }

    if ((size_t)len >= out_size) len = (int)out_size - 1;
    memcpy(out, data, (size_t)len);
    out[len] = '\0';
    *out_len = len;
    return true;
}

/* Cập nhật trạng thái liên kết BLE */
static void ble_set_link_state(bool connected, uint16_t conn_id, bool notify_enabled) {
    if (s_mutex && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLUETOOTH_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        if (connected) {
            s_connection_generation++;
        }
        s_connected = connected;
        s_conn_id = conn_id;
        s_notify_enabled = notify_enabled;
        s_mtu = 23;
        xSemaphoreGive(s_mutex);
    } else {
        ESP_LOGE(TAG, "Lỗi: Không xin được Mutex để đổi trạng thái liên kết BLE!");
    }
}

static void ble_set_notify_enabled(bool enabled) {
    if (s_mutex && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLUETOOTH_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        s_notify_enabled = enabled;
        xSemaphoreGive(s_mutex);
    } else {
        ESP_LOGE(TAG, "Lỗi: Không xin được Mutex cấu hình thông báo BLE!");
    }
}

static bool ble_get_link_state(uint16_t *conn_id, bool *notify_enabled) {
    bool connected = false;
    if (s_mutex && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLUETOOTH_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        connected = s_connected;
        if (conn_id) *conn_id = s_conn_id;
        if (notify_enabled) *notify_enabled = s_notify_enabled;
        xSemaphoreGive(s_mutex);
    } else {
        if (conn_id) *conn_id = WATCH_BLUETOOTH_CONN_ID_NONE;
        if (notify_enabled) *notify_enabled = false;
    }
    return connected;
}

static void ble_set_mtu(uint16_t mtu) {
    if (mtu < 23) mtu = 23;
    if (s_mutex && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLUETOOTH_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        s_mtu = mtu;
        xSemaphoreGive(s_mutex);
    }
}

static size_t ble_get_notify_payload_max(void) {
    uint16_t mtu = 23;
    if (s_mutex && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLUETOOTH_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        mtu = s_mtu;
        xSemaphoreGive(s_mutex);
    }
    if (mtu < 23) mtu = 23;
    size_t payload_max = (size_t)mtu - 3U;
    if (payload_max > WATCH_BLUETOOTH_NOTIFY_CMD_MAX_LEN) payload_max = WATCH_BLUETOOTH_NOTIFY_CMD_MAX_LEN;
    return payload_max;
}

// Mã định danh ứng dụng GATT
#define GATTS_APP_ID        0

// Số lượng handles tối thiểu cho 1 Characteristic
#define GATTS_NUM_HANDLES   4

// Kích thước bộ đệm nhận gói tin qua BLE
#define WATCH_BLUETOOTH_RX_BUF_SIZE 512

/* ===================================================================
 *  CẤU HÌNH QUẢNG BÁ (BLE Advertising)
 * =================================================================== */
static esp_ble_adv_params_t s_adv_params = {
    .adv_int_min       = 0x20,   // Khoảng cách phát quảng bá tối thiểu 20ms
    .adv_int_max       = 0x40,   // Khoảng cách phát quảng bá tối đa 40ms
    .adv_type          = ADV_TYPE_IND,
    .own_addr_type     = BLE_ADDR_TYPE_PUBLIC,
    .channel_map       = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static esp_ble_adv_data_t s_adv_data = {
    .set_scan_rsp    = false,
    .include_name    = false,     // Tên thiết bị được đưa sang scan response để không vượt 31 byte ADV
    .include_txpower = true,      // Gửi kèm cường độ phát sóng TX Power
    .min_interval    = 0x0006,
    .max_interval    = 0x0010,
    .appearance      = 0x00C1,    // Mã định dạng thiết bị: Đồng hồ đeo tay (Generic Watch)
    .service_uuid_len = sizeof(service_uuid128),
    .p_service_uuid  = (uint8_t *)service_uuid128,
    .flag            = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_data_t s_scan_rsp_data = {
    .set_scan_rsp    = true,
    .include_name    = true,
    .include_txpower = false,
};

static bool s_adv_data_configured = false;
static bool s_scan_rsp_configured = false;

static void ble_configure_security(void) {
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;
    esp_ble_io_cap_t iocap = ESP_IO_CAP_OUT;
    uint8_t key_size = 16;
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint32_t passkey = CONFIG_WATCH_BLE_PAIR_PASSKEY;

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_ble_gap_set_security_param(
        ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(auth_req)));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_ble_gap_set_security_param(
        ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(iocap)));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_ble_gap_set_security_param(
        ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(key_size)));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_ble_gap_set_security_param(
        ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(init_key)));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_ble_gap_set_security_param(
        ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(rsp_key)));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_ble_gap_set_security_param(
        ESP_BLE_SM_SET_STATIC_PASSKEY, &passkey, sizeof(passkey)));
}

/* ===================================================================
 *  GIẢI MÃ DỮ LIỆU BLE NHẬN ĐƯỢC (BLE Data Parsing)
 * =================================================================== */

/* Tìm vị trí của ký tự phân tách phân vùng dữ liệu */
static int ble_find_char(const char *str, int len, char sep, int start) {
    for (int i = start; i < len; i++) {
        if (str[i] == sep) return i;
    }
    return -1;
}

/* Sao chép an toàn chuỗi con từ bộ đệm BLE thô */
static void ble_safe_copy(char *dst, int dst_size, const char *src, int start, int end) {
    int copy_len = end - start;
    if (copy_len <= 0) { dst[0] = '\0'; return; }
    if (copy_len >= dst_size) copy_len = dst_size - 1;
    memcpy(dst, src + start, copy_len);
    dst[copy_len] = '\0';
}

static void ble_copy_cstr(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0) return;
    strncpy(dst, src ? src : "", dst_size - 1);
    dst[dst_size - 1] = '\0';
}

/* Quy đổi tên hành động rẽ từ App thành mã enum tương ứng */
static watch_bluetooth_nav_turn_t ble_parse_nav_turn(const char *turn) {
    if (!turn || turn[0] == '\0') return WATCH_BLUETOOTH_NAV_TURN_UNKNOWN;
    if (strcmp(turn, "straight") == 0) return WATCH_BLUETOOTH_NAV_TURN_STRAIGHT;
    if (strcmp(turn, "left") == 0) return WATCH_BLUETOOTH_NAV_TURN_LEFT;
    if (strcmp(turn, "right") == 0) return WATCH_BLUETOOTH_NAV_TURN_RIGHT;
    if (strcmp(turn, "slight_left") == 0) return WATCH_BLUETOOTH_NAV_TURN_SLIGHT_LEFT;
    if (strcmp(turn, "slight_right") == 0) return WATCH_BLUETOOTH_NAV_TURN_SLIGHT_RIGHT;
    if (strcmp(turn, "keep_left") == 0) return WATCH_BLUETOOTH_NAV_TURN_KEEP_LEFT;
    if (strcmp(turn, "keep_right") == 0) return WATCH_BLUETOOTH_NAV_TURN_KEEP_RIGHT;
    if (strcmp(turn, "uturn") == 0) return WATCH_BLUETOOTH_NAV_TURN_UTURN;
    if (strcmp(turn, "roundabout") == 0) return WATCH_BLUETOOTH_NAV_TURN_ROUNDABOUT;
    if (strcmp(turn, "arrive") == 0) return WATCH_BLUETOOTH_NAV_TURN_ARRIVE;
    return WATCH_BLUETOOTH_NAV_TURN_UNKNOWN;
}

static bool ble_token_is_decimal(const char *token) {
    if (!token || token[0] == '\0') return false;
    for (const char *p = token; *p; ++p) {
        if (*p < '0' || *p > '9') return false;
    }
    return true;
}

static void ble_normalize_nav_distance(char *dst, size_t dst_size, const char *token) {
    if (!dst || dst_size == 0) return;
    if (ble_token_is_decimal(token)) {
        size_t token_len = strlen(token);
        size_t copy_len = token_len;
        if (copy_len > dst_size - 1) copy_len = dst_size - 1;
        if (dst_size > 3 && copy_len > dst_size - 3) copy_len = dst_size - 3;
        memcpy(dst, token, copy_len);
        dst[copy_len] = '\0';
        if (copy_len + 2 < dst_size) {
            dst[copy_len++] = ' ';
            dst[copy_len++] = 'm';
            dst[copy_len] = '\0';
        }
    } else {
        ble_copy_cstr(dst, dst_size, token);
    }
}

/* Giải mã gói tin chỉ dẫn đường đi bản đồ: NAV|turn|distance|road|nextTurn|nextRoad|total */
static void ble_parse_nav_data(const char *data, int len) {
    int sep[6];
    int start = 0;
    for (int i = 0; i < 6; i++) {
        sep[i] = ble_find_char(data, len, '|', start);
        if (sep[i] < 0) {
            ESP_LOGW(TAG, "Định dạng NAV không hợp lệ, thiếu token: %.*s", len, data);
            return;
        }
        start = sep[i] + 1;
    }
    if (ble_find_char(data, len, '|', start) >= 0) {
        ESP_LOGW(TAG, "Định dạng NAV không hợp lệ, dư token: %.*s", len, data);
        return;
    }

    char current_turn[24];
    char distance[48];
    char next_turn[24];
    char next_road[WATCH_BLUETOOTH_NAV_ROAD_LEN];
    char total_remaining[48];

    ble_safe_copy(current_turn, sizeof(current_turn), data, sep[0] + 1, sep[1]);
    ble_safe_copy(distance, sizeof(distance), data, sep[1] + 1, sep[2]);
    ble_safe_copy(next_turn, sizeof(next_turn), data, sep[3] + 1, sep[4]);
    ble_safe_copy(next_road, sizeof(next_road), data, sep[4] + 1, sep[5]);
    ble_safe_copy(total_remaining, sizeof(total_remaining), data, sep[5] + 1, len);

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLUETOOTH_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        s_nav_data.active = true;
        s_nav_data.current_turn = ble_parse_nav_turn(current_turn);
        ble_safe_copy(s_nav_data.primary_road, WATCH_BLUETOOTH_NAV_ROAD_LEN, data, sep[2] + 1, sep[3]);
        ble_normalize_nav_distance(s_nav_data.eta_to_turn, sizeof(s_nav_data.eta_to_turn), distance);
        s_nav_data.next_turn = ble_parse_nav_turn(next_turn);
        ble_copy_cstr(s_nav_data.next_road, sizeof(s_nav_data.next_road), next_road);
        ble_normalize_nav_distance(s_nav_data.total_remaining,
                                   sizeof(s_nav_data.total_remaining), total_remaining);
        xSemaphoreGive(s_mutex);
    } else {
        ESP_LOGE(TAG, "Lỗi Mutex khi phân tích dữ liệu NAV!");
    }

    ESP_LOGD(TAG, "Đã cập nhật dữ liệu chỉ đường");
}

/* Giải mã gói tin đồng bộ thời gian từ GPS điện thoại: TIME|HH:MM:SS|DD/MM/YYYY */
static void ble_parse_time_sync(const char *data, int len) {
    int sep1 = ble_find_char(data, len, '|', 0);
    if (sep1 < 0) return;

    int sep2 = ble_find_char(data, len, '|', sep1 + 1);
    if (sep2 < 0) return;

    int sep3 = ble_find_char(data, len, '|', sep2 + 1);

    char first_value[24];
    ble_safe_copy(first_value, sizeof(first_value), data, sep1 + 1, sep2);
    if (!strchr(first_value, ':')) {
        char offset_str[12];
        ble_safe_copy(offset_str, sizeof(offset_str), data, sep2 + 1, len);
        int o_len = strlen(offset_str);
        while (o_len > 0 && (offset_str[o_len - 1] == '\r' || offset_str[o_len - 1] == '\n' || offset_str[o_len - 1] == ' ')) {
            offset_str[o_len - 1] = '\0';
            o_len--;
        }
        char *epoch_end = NULL;
        char *offset_end = NULL;
        long long epoch = strtoll(first_value, &epoch_end, 10);
        long offset_minutes = strtol(offset_str, &offset_end, 10);
        if (epoch_end == first_value || *epoch_end != '\0' || offset_end == offset_str || *offset_end != '\0' ||
            epoch < 1704067200LL || offset_minutes < -840 || offset_minutes > 840) {
            ESP_LOGW(TAG, "Gói TIME UTC không hợp lệ: %.*s", len, data);
            return;
        }

        ui_time_set_timezone_offset((int)offset_minutes);
        struct timeval tv = { .tv_sec = (time_t)epoch, .tv_usec = 0 };
        settimeofday(&tv, NULL);
        ESP_LOGI(TAG, "Đồng bộ epoch UTC=%lld, offset=%ld phút", epoch, offset_minutes);
        return;
    }

    char time_str[16];
    ble_safe_copy(time_str, sizeof(time_str), data, sep1 + 1, sep2);

    int hour = 0, minute = 0, second = 0;
    if (sscanf(time_str, "%d:%d:%d", &hour, &minute, &second) != 3 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
        ESP_LOGW(TAG, "Định dạng thời gian nhận được lỗi: %s", time_str);
        return;
    }

    int date_end = (sep3 > 0) ? sep3 : len;
    char date_str[16];
    ble_safe_copy(date_str, sizeof(date_str), data, sep2 + 1, date_end);

    int day = 1, month = 1, year = 2026;
    if (sscanf(date_str, "%d/%d/%d", &day, &month, &year) != 3 ||
        year < 2020 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31) {
        ESP_LOGW(TAG, "Định dạng ngày tháng nhận được lỗi: %s", date_str);
        return;
    }

    /* Thiết lập đồng bộ thời gian vào RTC hệ thống của ESP32-S3 */
    struct tm tm_info = {0};
    tm_info.tm_hour = hour;
    tm_info.tm_min  = minute;
    tm_info.tm_sec  = second;
    tm_info.tm_mday = day;
    tm_info.tm_mon  = month - 1;   // Định dạng tm: Tháng từ 0-11
    tm_info.tm_year = year - 1900; // Định dạng tm: Năm tính từ mốc 1900

    time_t t = mktime(&tm_info);
    if (t < 0) {
        ESP_LOGW(TAG, "Chuyển đổi thời gian mktime thất bại!");
        return;
    }

    struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
    settimeofday(&tv, NULL); // Đặt thời gian cho hệ điều hành
    ESP_LOGI(TAG, "Đồng bộ thời gian thành công từ điện thoại: %02d:%02d:%02d  %02d/%02d/%04d",
             hour, minute, second, day, month, year);
}

/* Đẩy một thông báo mới vào đầu hàng đợi và xóa thông báo cũ nhất */
static void ble_add_notification(const char *app, const char *title, const char *content) {
    if (!s_mutex || xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLUETOOTH_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGW(TAG, "Bỏ qua thông báo: Không xin được Mutex!");
        return;
    }
    bool dropped_oldest = (s_notif_count >= WATCH_BLUETOOTH_NOTIF_MAX);

    /* Dịch toàn bộ thông báo hiện có xuống 1 chỉ mục để chừa vị trí 0 */
    for (int i = WATCH_BLUETOOTH_NOTIF_MAX - 1; i > 0; i--) {
        s_notifications[i] = s_notifications[i - 1];
    }

    /* Điền dữ liệu thông báo mới vào vị trí đầu tiên (0) */
    ble_copy_cstr(s_notifications[0].app_name, WATCH_BLUETOOTH_NOTIF_APP_LEN, app);
    ble_copy_cstr(s_notifications[0].title, WATCH_BLUETOOTH_NOTIF_TITLE_LEN, title);
    ble_copy_cstr(s_notifications[0].content, WATCH_BLUETOOTH_NOTIF_CONTENT_LEN, content);
    s_notifications[0].timestamp = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    s_notifications[0].seq_id = s_next_notif_seq++;
    if (s_next_notif_seq == 0) s_next_notif_seq = 1;
    s_notifications[0].is_new = true;

    if (s_notif_count < WATCH_BLUETOOTH_NOTIF_MAX) s_notif_count++;
    if (dropped_oldest) s_notif_overflow_count++;

    watch_bluetooth_notification_t copy = s_notifications[0];
    watch_bluetooth_notif_new_cb_t cb = s_new_cb;

    xSemaphoreGive(s_mutex);

    ESP_LOGD(TAG, "Nhận thông báo mới từ ứng dụng %s", copy.app_name);

    /* Gọi hàm callback hiển thị UI ngoài vùng Mutex để tránh treo hệ thống */
    if (cb) {
        cb(&copy);
    }
}

/* Đọc và gửi dữ liệu tọa độ hành trình GPS (vết đường đi) lên điện thoại */
static void ble_send_current_track(void) {
    char *track = heap_caps_malloc(WATCH_GPS_TRACK_TEXT_MAX, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!track) track = malloc(WATCH_GPS_TRACK_TEXT_MAX);
    if (!track) {
        ESP_LOGE(TAG, "Lỗi cấp phát RAM lớn để đóng gói hành trình tọa độ!");
        return;
    }

    int len = watch_gps_format_track(track, WATCH_GPS_TRACK_TEXT_MAX);
    if (len > 0) {
        ble_send_track_chunks_blocking(track);
    }
    free(track);
}

static void ble_apply_ota_url(const char *url) {
    if (!watch_ota_url_is_allowed(url)) {
        ESP_LOGW(TAG, "Từ chối OTA URL HTTPS không hợp lệ");
        return;
    }
    esp_err_t err = watch_settings_set_ota_url(url);
    ESP_LOGI(TAG, "Cập nhật OTA URL qua BLE: %s", err == ESP_OK ? "Thành công" : esp_err_to_name(err));
}

typedef struct {
    uint32_t seq;
    size_t offset;
    bool begin_sent;
    uint32_t crc32;
    char context_msg[32];
} ble_route_stream_ctx_t;

static void ble_route_stream_init(ble_route_stream_ctx_t *ctx, const char *context_msg) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->crc32 = 0xFFFFFFFFU;
    snprintf(ctx->context_msg, sizeof(ctx->context_msg), "%s", context_msg);
}

static esp_err_t ble_send_track_begin(ble_route_stream_ctx_t *ctx) {
    esp_err_t err = watch_bluetooth_send_command(ctx->context_msg);
    if (err == ESP_OK) err = watch_bluetooth_send_command("TRACK_BEGIN");
    if (err == ESP_OK) ctx->begin_sent = true;
    return err;
}

static esp_err_t ble_send_track_end(const ble_route_stream_ctx_t *ctx) {
    char msg[48];
    snprintf(msg, sizeof(msg), "TRACK_META|%lu|%08lX",
             (unsigned long)ctx->offset,
             (unsigned long)(ctx->crc32 ^ 0xFFFFFFFFU));
    esp_err_t err = watch_bluetooth_send_command(msg);
    if (err == ESP_OK) err = watch_bluetooth_send_command("TRACK_END");
    return err;
}

static esp_err_t ble_send_route_file_chunk(const char *data, size_t len, void *ctx_ptr) {
    ble_route_stream_ctx_t *ctx = (ble_route_stream_ctx_t *)ctx_ptr;
    char msg[512];
    size_t pos = 0;

    if (!ctx->begin_sent) {
        esp_err_t err = ble_send_track_begin(ctx);
        if (err != ESP_OK) return err;
    }

    while (pos < len) {
        size_t payload_max = ble_get_notify_payload_max();
        int prefix_len = snprintf(msg, sizeof(msg), "TRK|%lu|%lu|",
                                  (unsigned long)ctx->seq, (unsigned long)ctx->offset);
        if (prefix_len <= 0 || (size_t)prefix_len + 5U >= payload_max) {
            return ESP_ERR_INVALID_SIZE;
        }
        size_t n = payload_max - (size_t)prefix_len - 5U;
        if (n > len - pos) n = len - pos;
        memcpy(msg + prefix_len, data + pos, n);
        msg[prefix_len + n] = '\0';

        esp_err_t err = watch_bluetooth_send_command(msg);
        if (err != ESP_OK) return err;
        ctx->crc32 = ble_crc32_update(ctx->crc32, (const uint8_t *)data + pos, n);
        pos += n;
        ctx->offset += n;
        ctx->seq++;
        vTaskDelay(pdMS_TO_TICKS(30));
    }
    return ESP_OK;
}

static void ble_send_history_track(size_t index) {
    size_t count = watch_activity_log_count();
    if (index >= count) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(watch_bluetooth_send_command("TRACK_ERR|NOT_FOUND"));
        return;
    }

    ble_route_stream_ctx_t ctx;
    char context_msg[32];
    snprintf(context_msg, sizeof(context_msg), "TRACK_CONTEXT|H|%u", (unsigned int)index);
    ble_route_stream_init(&ctx, context_msg);
    esp_err_t err = watch_activity_log_stream_route(index, ble_send_route_file_chunk, &ctx);
    if (err == ESP_OK) {
        if (!ctx.begin_sent) {
            ESP_ERROR_CHECK_WITHOUT_ABORT(ble_send_track_begin(&ctx));
        }
        ESP_ERROR_CHECK_WITHOUT_ABORT(ble_send_track_end(&ctx));
    } else {
        ESP_LOGW(TAG, "Không thể gửi route lịch sử index=%u: %s",
                 (unsigned int)index, esp_err_to_name(err));
        if (err == ESP_ERR_NOT_FOUND) {
            ESP_ERROR_CHECK_WITHOUT_ABORT(watch_bluetooth_send_command("TRACK_ERR|NOT_FOUND"));
        } else {
            ESP_ERROR_CHECK_WITHOUT_ABORT(watch_bluetooth_send_command("TRACK_ERR|READ"));
        }
    }
}

static bool ble_send_current_capture(void) {
    ble_route_stream_ctx_t ctx;
    ble_route_stream_init(&ctx, "TRACK_CONTEXT|LIVE");
    esp_err_t err = watch_activity_log_stream_capture(ble_send_route_file_chunk, &ctx);
    if (!ctx.begin_sent) return false;
    if (err == ESP_OK) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(ble_send_track_end(&ctx));
    } else {
        ESP_ERROR_CHECK_WITHOUT_ABORT(watch_bluetooth_send_command("TRACK_ERR|READ"));
    }
    return true;
}

static void ble_send_activity_history(void) {
    watch_activity_record_t records[WATCH_ACTIVITY_LOG_MAX];
    size_t count = watch_activity_log_get(records, WATCH_ACTIVITY_LOG_MAX);
    char msg[160];

    snprintf(msg, sizeof(msg), "ACT_LIST_BEGIN|%u", (unsigned int)count);
    if (watch_bluetooth_send_command(msg) != ESP_OK) return;

    for (size_t i = 0; i < count; i++) {
        const watch_activity_record_t *record = &records[i];
        snprintf(msg, sizeof(msg), "ACT_META|%u|%lu|%u|%lu|%lu|%.3f|%.1f",
                 (unsigned int)i,
                 (unsigned long)record->timestamp,
                 (unsigned int)record->sport_id,
                 (unsigned long)record->duration_sec,
                 (unsigned long)record->steps,
                 record->distance_km,
                 record->avg_speed_kmh);
        if (watch_bluetooth_send_command(msg) != ESP_OK) return;
        vTaskDelay(pdMS_TO_TICKS(30));
    }
    ESP_ERROR_CHECK_WITHOUT_ABORT(watch_bluetooth_send_command("ACT_LIST_END"));
}

static void ble_track_send_worker(void *arg) {
    (void)arg;
    ble_track_send_cmd_t cmd;
    while (xQueueReceive(s_track_send_queue, &cmd, portMAX_DELAY) == pdTRUE) {
        if (cmd.type == BLE_TRACK_SEND_CURRENT) {
            if (!ble_send_current_capture()) {
                if (!watch_gps_is_active() && watch_activity_log_count() > 0) {
                    ble_send_history_track(0);
                } else {
                    ble_send_current_track();
                }
            }
        } else if (cmd.type == BLE_TRACK_SEND_HISTORY) {
            ble_send_history_track(cmd.history_index);
        } else if (cmd.type == BLE_TRACK_SEND_BUFFER) {
            ble_send_track_chunks_blocking(cmd.track);
            free(cmd.track);
        } else if (cmd.type == BLE_TRACK_SEND_ACTIVITY_LIST) {
            ble_send_activity_history();
        }
    }
}

static bool ble_queue_track_send(const ble_track_send_cmd_t *cmd) {
    if (!cmd || !s_track_send_queue) return false;
    if (xQueueSend(s_track_send_queue, cmd, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Route send queue is busy; rejecting duplicate request");
        return false;
    }
    return true;
}

void watch_bluetooth_send_history_track(size_t index) {
    ble_track_send_cmd_t cmd = {
        .type = BLE_TRACK_SEND_HISTORY,
        .history_index = index,
    };
    (void)ble_queue_track_send(&cmd);
}

/* Định tuyến bóc tách gói dữ liệu thô ghi xuống từ điện thoại */
static void ble_parse_rx_data(const char *data, int len) {
    if (len > 10 && strncmp(data, "HELLO_ACK|", 10) == 0) {
        ESP_LOGI(TAG, "Ứng dụng xác nhận giao thức BLE phiên bản %.*s", len - 10, data + 10);
        return;
    }
    if (len == 9 && strncmp(data, "OTA_BEGIN", 9) == 0) {
        s_ota_url_rx_len = 0;
        s_ota_url_rx[0] = '\0';
        s_ota_url_rx_active = true;
        return;
    }
    if (len > 10 && strncmp(data, "OTA_CHUNK|", 10) == 0) {
        size_t chunk_len = (size_t)(len - 10);
        if (!s_ota_url_rx_active ||
            s_ota_url_rx_len + chunk_len >= sizeof(s_ota_url_rx)) {
            s_ota_url_rx_active = false;
            s_ota_url_rx_len = 0;
            ESP_LOGW(TAG, "Bỏ ghép OTA URL vì dữ liệu quá dài hoặc sai thứ tự");
            return;
        }
        memcpy(s_ota_url_rx + s_ota_url_rx_len, data + 10, chunk_len);
        s_ota_url_rx_len += chunk_len;
        s_ota_url_rx[s_ota_url_rx_len] = '\0';
        return;
    }
    if (len == 7 && strncmp(data, "OTA_END", 7) == 0) {
        if (s_ota_url_rx_active && s_ota_url_rx_len > 0) {
            s_ota_url_rx_active = false;
            ble_apply_ota_url(s_ota_url_rx);
        }
        return;
    }
    if (len == 7 && strncmp(data, "ACT_REQ", 7) == 0) {
        ble_track_send_cmd_t cmd = {.type = BLE_TRACK_SEND_ACTIVITY_LIST};
        (void)ble_queue_track_send(&cmd);
        return;
    }
    if (len == 9 && strncmp(data, "TRACK_REQ", 9) == 0) {
        ble_track_send_cmd_t cmd = {.type = BLE_TRACK_SEND_CURRENT};
        if (!ble_queue_track_send(&cmd)) {
            ESP_ERROR_CHECK_WITHOUT_ABORT(watch_bluetooth_send_command("TRACK_ERR|BUSY"));
        }
        return;
    }
    if (len > 10 && strncmp(data, "TRACK_REQ|", 10) == 0) {
        char *end = NULL;
        unsigned long index = strtoul(data + 10, &end, 10);
        if (end && end == data + len) {
            ble_track_send_cmd_t cmd = {
                .type = BLE_TRACK_SEND_HISTORY,
                .history_index = (size_t)index,
            };
            if (!ble_queue_track_send(&cmd)) {
                ESP_ERROR_CHECK_WITHOUT_ABORT(watch_bluetooth_send_command("TRACK_ERR|BUSY"));
            }
        }
        return;
    }

    if (len > 10 && strncmp(data, "REPLY_ACK|", 10) == 0) {
        bool ok = len == 12 && strncmp(data + 10, "OK", 2) == 0;
        watch_bluetooth_notification_t status = {0};
        snprintf(status.app_name, sizeof(status.app_name), "Phone");
        snprintf(status.title, sizeof(status.title), "%s", ok ? "Reply sent" : "Reply failed");
        snprintf(status.content, sizeof(status.content), "%s",
                 ok ? "Message delivered to the phone app"
                    : "Notification no longer supports reply");
        ui_notification_popup_show(&status);
        return;
    }

    if (len == 8 && strncmp(data, "QR_CLEAR", 8) == 0) {
        ui_quick_replies_clear_from_ble();
        return;
    }

    if (len > 7 && strncmp(data, "QR_ADD|", 7) == 0) {
        char reply[96];
        int copy_len = len - 7;
        if (copy_len >= (int)sizeof(reply)) copy_len = sizeof(reply) - 1;
        memcpy(reply, data + 7, copy_len);
        reply[copy_len] = '\0';
        ui_quick_replies_add_from_ble(reply);
        return;
    }

    if (len == 7 && strncmp(data, "QR_DONE", 7) == 0) {
        ESP_LOGI(TAG, "Da dong bo quick replies tu app");
        return;
    }

    if (len > 8 && strncmp(data, "OTA_URL|", 8) == 0) {
        char url[256];
        int copy_len = len - 8;
        if (copy_len >= (int)sizeof(url)) copy_len = sizeof(url) - 1;
        memcpy(url, data + 8, copy_len);
        url[copy_len] = '\0';
        ble_apply_ota_url(url);
        return;
    }

    if (len == 7 && strncmp(data, "NAV_END", 7) == 0) {
        watch_bluetooth_clear_navigation_data(); // Điện thoại tắt bản đồ chỉ đường
        ESP_LOGI(TAG, "Phiên chỉ đường đã kết thúc");
        return;
    }

    int sep1 = ble_find_char(data, len, '|', 0);
    if (sep1 < 0) return;

    if (sep1 == 4 && strncmp(data, "TIME", 4) == 0) {
        ble_parse_time_sync(data, len); // Đồng bộ thời gian thực
        return;
    }

    if (sep1 == 3 && strncmp(data, "NAV", 3) == 0) {
        ble_parse_nav_data(data, len); // Chỉ đường bản đồ
        return;
    }

    if (sep1 == 5 && strncmp(data, "NOTIF", 5) == 0) {
        int sep2 = ble_find_char(data, len, '|', sep1 + 1);
        if (sep2 < 0) return;
        int sep3 = ble_find_char(data, len, '|', sep2 + 1);
        if (sep3 < 0) return;

        char app[WATCH_BLUETOOTH_NOTIF_APP_LEN];
        char title[WATCH_BLUETOOTH_NOTIF_TITLE_LEN];
        char content[WATCH_BLUETOOTH_NOTIF_CONTENT_LEN];

        ble_safe_copy(app, sizeof(app), data, sep1 + 1, sep2);
        ble_safe_copy(title, sizeof(title), data, sep2 + 1, sep3);
        ble_safe_copy(content, sizeof(content), data, sep3 + 1, len);

        ble_add_notification(app, title, content);
        return;
    }

    int sep2 = ble_find_char(data, len, '|', sep1 + 1);
    if (sep2 < 0) return;

    char app[WATCH_BLUETOOTH_NOTIF_APP_LEN];
    char title[WATCH_BLUETOOTH_NOTIF_TITLE_LEN];
    char content[WATCH_BLUETOOTH_NOTIF_CONTENT_LEN];

    ble_safe_copy(app, sizeof(app), data, 0, sep1);
    ble_safe_copy(title, sizeof(title), data, sep1 + 1, sep2);
    ble_safe_copy(content, sizeof(content), data, sep2 + 1, len);

    ble_add_notification(app, title, content); // Tin nhắn/thông báo thường
}

static void adv_timer_callback(void *arg) {
    (void)arg;
    if (s_inited && s_enabled && !s_connected) {
        ESP_LOGI(TAG, "GAP: Đang kích hoạt quảng bá từ timer trì hoãn...");
        esp_err_t err = esp_ble_gap_start_advertising(&s_adv_params);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "GAP: Phát sóng quảng bá thất bại từ timer: %s", esp_err_to_name(err));
        }
    }
}

/* ===================================================================
 *  TRÌNH XỬ LÝ SỰ KIỆN GAP (GAP Event Handler)
 * =================================================================== */
static void ble_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            s_adv_data_configured = true;
            if (s_scan_rsp_configured) {
                ESP_LOGI(TAG, "GAP: Cấu hình dữ liệu Adv/ScanRsp thành công, bắt đầu phát sóng quảng bá...");
                esp_ble_gap_start_advertising(&s_adv_params);
            }
            break;
        case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
            s_scan_rsp_configured = true;
            if (s_adv_data_configured) {
                ESP_LOGI(TAG, "GAP: Cấu hình dữ liệu Adv/ScanRsp thành công, bắt đầu phát sóng quảng bá...");
                esp_ble_gap_start_advertising(&s_adv_params);
            }
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "GAP: Phát sóng quảng bá BLE thành công");
            } else {
                ESP_LOGE(TAG, "GAP: Phát sóng quảng bá thất bại! Mã lỗi: %d", param->adv_start_cmpl.status);
            }
            break;
        case ESP_GAP_BLE_SEC_REQ_EVT:
            ESP_LOGI(TAG, "GAP: Yêu cầu bảo mật BLE, chấp nhận pairing/bonding");
            esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
            break;
        case ESP_GAP_BLE_AUTH_CMPL_EVT:
            ESP_LOGI(TAG, "GAP: Pairing %s, auth_mode=0x%x",
                     param->ble_security.auth_cmpl.success ? "thành công" : "thất bại",
                     param->ble_security.auth_cmpl.auth_mode);
            if (!param->ble_security.auth_cmpl.success) {
                ESP_LOGW(TAG, "GAP: Pairing thất bại, reason=0x%x",
                         param->ble_security.auth_cmpl.fail_reason);
            }
            break;
        case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:
            ESP_LOGI(TAG, "GAP: BLE pairing passkey: %06lu",
                     (unsigned long)param->ble_security.key_notif.passkey);
            {
                watch_bluetooth_notification_t pairing = {0};
                snprintf(pairing.app_name, sizeof(pairing.app_name), "Bluetooth");
                snprintf(pairing.title, sizeof(pairing.title), "Pairing code");
                snprintf(pairing.content, sizeof(pairing.content), "%06lu",
                         (unsigned long)param->ble_security.key_notif.passkey);
                ui_notification_popup_show(&pairing);
            }
            break;
        default:
            break;
    }
}

/* ===================================================================
 *  TRÌNH XỬ LÝ SỰ KIỆN GATTS (GATTS Event Handler)
 * =================================================================== */
static void ble_gatts_event_handler(esp_gatts_cb_event_t event,
                                    esp_gatt_if_t gatts_if,
                                    esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_REG_EVT:
            if (param->reg.status != ESP_GATT_OK) {
                ESP_LOGE(TAG, "GATTS: Đăng ký App thất bại, status=0x%x", param->reg.status);
                break;
            }
            ESP_LOGI(TAG, "GATTS: Đăng ký App thành công, app_id=%d, if=%d", param->reg.app_id, gatts_if);
            s_gatts_if = gatts_if;
            esp_ble_gap_set_device_name("Smartwatch_DATN"); // Đặt tên Bluetooth thiết bị hiển thị khi quét
            ble_configure_security();
            s_adv_data_configured = false;
            s_scan_rsp_configured = false;
            esp_ble_gap_config_adv_data(&s_adv_data);
            esp_ble_gap_config_adv_data(&s_scan_rsp_data);

            /* Khai báo Service UUID */
            static esp_gatt_srvc_id_t service_id;
            service_id.is_primary = true;
            service_id.id.uuid.len = ESP_UUID_LEN_128;
            memcpy(service_id.id.uuid.uuid.uuid128, service_uuid128, 16);
            esp_ble_gatts_create_service(gatts_if, &service_id, GATTS_NUM_HANDLES);
            break;

        case ESP_GATTS_CREATE_EVT:
            if (param->create.status != ESP_GATT_OK) {
                ESP_LOGE(TAG, "GATTS: Khởi tạo Service thất bại, status=0x%x", param->create.status);
                break;
            }
            ESP_LOGI(TAG, "GATTS: Khởi tạo Service thành công, handle=%d", param->create.service_handle);
            s_service_handle = param->create.service_handle;
            esp_ble_gatts_start_service(s_service_handle);

            /* Đăng ký Characteristic thuộc tính Đọc/Ghi/Báo */
            static esp_bt_uuid_t char_uuid;
            char_uuid.len = ESP_UUID_LEN_128;
            memcpy(char_uuid.uuid.uuid128, char_uuid128, 16);

            esp_gatt_char_prop_t prop = ESP_GATT_CHAR_PROP_BIT_READ |
                                        ESP_GATT_CHAR_PROP_BIT_WRITE |
                                        ESP_GATT_CHAR_PROP_BIT_NOTIFY;
     
            static uint8_t char_val_buffer[512] = {0};
            static esp_attr_value_t char_val = {
                .attr_max_len = sizeof(char_val_buffer),
                .attr_len     = 0,
                .attr_value   = char_val_buffer,
            };

            esp_ble_gatts_add_char(s_service_handle,
                                   &char_uuid,
                                   ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED,
                                   prop,
                                   &char_val,
                                   NULL);
            break;

        case ESP_GATTS_ADD_CHAR_EVT:
            if (param->add_char.status != ESP_GATT_OK) {
                ESP_LOGE(TAG, "GATTS: Thêm Characteristic thất bại, status=0x%x", param->add_char.status);
                break;
            }
            ESP_LOGI(TAG, "GATTS: Thêm đặc tính Characteristic thành công, handle=%d", param->add_char.attr_handle);
            s_char_handle = param->add_char.attr_handle;

            /* Cấu hình CCCD Descriptor cho phép Client bật cơ chế Notify */
            static esp_bt_uuid_t descr_uuid;
            descr_uuid.len = ESP_UUID_LEN_16;
            descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
            esp_ble_gatts_add_char_descr(s_service_handle,
                                          &descr_uuid,
                                          ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED,
                                          NULL, NULL);
            break;

        case ESP_GATTS_ADD_CHAR_DESCR_EVT:
            if (param->add_char_descr.status != ESP_GATT_OK) {
                ESP_LOGE(TAG, "GATTS: Thêm CCCD thất bại, status=0x%x", param->add_char_descr.status);
                break;
            }
            ESP_LOGI(TAG, "GATTS: Thêm thanh ghi CCCD thành công, handle=%d",
                     param->add_char_descr.attr_handle);
            s_descr_handle = param->add_char_descr.attr_handle;
            break;

        case ESP_GATTS_CONNECT_EVT:
            ESP_LOGI(TAG, "GATTS: Thiết bị điện thoại đã kết nối! conn_id=%d", param->connect.conn_id);
            ble_set_link_state(true, param->connect.conn_id, false);
            
            // Cấu hình tham số kết nối BLE tối ưu (Connection Parameters Update) để tránh rớt mạng
            esp_ble_conn_update_params_t conn_params = {0};
            memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
            conn_params.min_int = 16;       // 16 * 1.25ms = 20ms
            conn_params.max_int = 32;       // 32 * 1.25ms = 40ms
            conn_params.latency = 0;
            conn_params.timeout = 400;      // 400 * 10ms = 4000ms (4 giây chống rớt kết nối khi CPU bận)
            esp_ble_gap_update_conn_params(&conn_params);

            esp_ble_set_encryption(param->connect.remote_bda, ESP_BLE_SEC_ENCRYPT_MITM);
            break;

        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(TAG, "GATTS: Ngắt kết nối với điện thoại. Lý do: 0x%x", param->disconnect.reason);
            ble_set_link_state(false, WATCH_BLUETOOTH_CONN_ID_NONE, false);
            if (s_inited && s_adv_timer) {
                esp_timer_stop(s_adv_timer);
                // Trì hoãn việc phát quảng bá 500ms để tránh crash hệ thống/xung đột stack BLE điều khiển
                esp_timer_start_once(s_adv_timer, 500000ULL);
            }
            break;

        case ESP_GATTS_WRITE_EVT:
            if (param->write.handle == s_descr_handle && param->write.len == 2) {
                uint16_t cccd = param->write.value[1] << 8 | param->write.value[0];
                bool notify_enabled = (cccd & 0x0001) != 0;
                ble_set_notify_enabled(notify_enabled);
                ESP_LOGI(TAG, "Bật/Tắt Notify từ Client: 0x%04X, notify=%s", cccd, notify_enabled ? "BẬT" : "TẮT");
                if (notify_enabled) {
                    ESP_ERROR_CHECK_WITHOUT_ABORT(watch_bluetooth_send_command("HELLO|2|7"));
                    ESP_ERROR_CHECK_WITHOUT_ABORT(watch_bluetooth_send_command("TIME_REQ"));
                }
            }

            /* Tiếp nhận dữ liệu văn bản ghi xuống đặc tính Characteristic */
            if (param->write.handle == s_char_handle && param->write.len > 0) {
                char buf[WATCH_BLUETOOTH_RX_BUF_SIZE];
                int copy_len = param->write.len;
                if (copy_len >= (int)sizeof(buf)) copy_len = sizeof(buf) - 1;
                memcpy(buf, param->write.value, copy_len);
                buf[copy_len] = '\0';

                ESP_LOGD(TAG, "Đã nhận dữ liệu BLE (%d bytes)", param->write.len);
                char body[WATCH_BLUETOOTH_RX_BUF_SIZE];
                int body_len = 0;
                if (ble_strip_crc_frame(buf, copy_len, body, sizeof(body), &body_len)) {
                    ble_parse_rx_data(body, body_len);
                }
            }

            if (param->write.need_rsp) {
                esp_gatt_rsp_t rsp = {0};
                rsp.attr_value.handle = param->write.handle;
                rsp.attr_value.len = 0;
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id,
                                            param->write.trans_id,
                                            ESP_GATT_OK, &rsp);
            }
            break;

        case ESP_GATTS_MTU_EVT:
            ESP_LOGI(TAG, "GATTS: Kích thước gói dữ liệu MTU được cấu hình = %d bytes", param->mtu.mtu);
            ble_set_mtu(param->mtu.mtu);
            break;
        default:
            break;
    }
}

/* ===================================================================
 *  API CÔNG CỘNG (Public APIs)
 * =================================================================== */

esp_err_t watch_bluetooth_init(void) {
    if (s_inited) return ESP_OK;

    if (!s_notify_mutex) {
        s_notify_mutex = xSemaphoreCreateMutex();
        if (!s_notify_mutex) return ESP_ERR_NO_MEM;
    }
    if (!s_mutex) {
        s_mutex = xSemaphoreCreateMutex();
        if (!s_mutex) return ESP_ERR_NO_MEM;
    }
    if (!s_track_send_queue) {
        s_track_send_queue = xQueueCreate(3, sizeof(ble_track_send_cmd_t));
        if (!s_track_send_queue) return ESP_ERR_NO_MEM;
    }
    if (!s_adv_timer) {
        const esp_timer_create_args_t adv_timer_args = {
            .callback = &adv_timer_callback,
            .name = "ble_adv_defer"
        };
        esp_err_t timer_err = esp_timer_create(&adv_timer_args, &s_adv_timer);
        if (timer_err != ESP_OK) {
            ESP_LOGE(TAG, "Thất bại khi tạo timer quảng bá BLE: %s", esp_err_to_name(timer_err));
            return timer_err;
        }
    }
    if (!s_track_send_task) {
        if (xTaskCreate(ble_track_send_worker, "ble_track_send", 4096, NULL, 1,
                        &s_track_send_task) != pdPASS) {
            return ESP_ERR_NO_MEM;
        }
    }

    s_gatts_if = ESP_GATT_IF_NONE;
    s_service_handle = 0;
    s_char_handle = 0;
    s_descr_handle = 0;
    s_adv_data_configured = false;
    s_scan_rsp_configured = false;

    /* Khởi động Bluetooth Controller thô ở chế độ BLE */
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_bt_controller_init(&bt_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Khởi tạo BT Controller thất bại: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (err != ESP_OK) {
        esp_bt_controller_deinit();
        ESP_LOGE(TAG, "Kích hoạt BLE Mode thất bại: %s", esp_err_to_name(err));
        return err;
    }

    /* Khởi tạo thư viện giao thức Bluedroid */
    err = esp_bluedroid_init();
    if (err != ESP_OK) {
        esp_bt_controller_disable();
        esp_bt_controller_deinit();
        ESP_LOGE(TAG, "Khởi tạo Bluedroid thất bại: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_bluedroid_enable();
    if (err != ESP_OK) {
        esp_bluedroid_deinit();
        esp_bt_controller_disable();
        esp_bt_controller_deinit();
        ESP_LOGE(TAG, "Kích hoạt Bluedroid thất bại: %s", esp_err_to_name(err));
        return err;
    }

    /* Đăng ký các hàm sự kiện */
    err = esp_ble_gap_register_callback(ble_gap_event_handler);
    if (err == ESP_OK) err = esp_ble_gatts_register_callback(ble_gatts_event_handler);
    if (err == ESP_OK) err = esp_ble_gatts_app_register(GATTS_APP_ID);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Đăng ký BLE GAP/GATT thất bại: %s", esp_err_to_name(err));
        esp_bluedroid_disable();
        esp_bluedroid_deinit();
        esp_bt_controller_disable();
        esp_bt_controller_deinit();
        return err;
    }
 
    /* Đặt kích thước gói truyền tối đa 512 bytes */
    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(512);
    if (local_mtu_ret != ESP_OK) {
        ESP_LOGE(TAG, "Cấu hình local MTU thất bại: %x", local_mtu_ret);
    }

    s_inited = true;
    ESP_LOGI(TAG, "Giao tiếp BLE GATT Server đã trực tuyến.");
    return ESP_OK;
}

esp_err_t watch_bluetooth_deinit(void) {
    if (!s_inited) return ESP_OK;
    if (!s_notify_mutex ||
        xSemaphoreTake(s_notify_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Timed out waiting for active BLE notify before shutdown");
        return ESP_ERR_TIMEOUT;
    }
    s_inited = false;

    if (s_adv_timer) {
        esp_timer_stop(s_adv_timer);
        esp_timer_delete(s_adv_timer);
        s_adv_timer = NULL;
    }

    esp_ble_gap_stop_advertising();
    esp_bluedroid_disable();
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();

    if (s_mutex) {
        if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLUETOOTH_MUTEX_TIMEOUT_MS)) == pdTRUE) {
            memset(&s_nav_data, 0, sizeof(s_nav_data));
            s_connected = false;
            s_conn_id = WATCH_BLUETOOTH_CONN_ID_NONE;
            s_notify_enabled = false;
            s_mtu = 23;
            s_notif_count = 0;
            s_notif_overflow_count = 0;
            memset(s_notifications, 0, sizeof(s_notifications));
            xSemaphoreGive(s_mutex);
        } else {
            ESP_LOGW(TAG, "Giải phóng BLE quá hạn Mutex; ép buộc đặt lại các biến trạng thái.");
            memset(&s_nav_data, 0, sizeof(s_nav_data));
            s_connected = false;
            s_conn_id = WATCH_BLUETOOTH_CONN_ID_NONE;
            s_notify_enabled = false;
            s_mtu = 23;
            s_notif_count = 0;
            s_notif_overflow_count = 0;
            memset(s_notifications, 0, sizeof(s_notifications));
        }
    }

    ESP_LOGI(TAG, "Bộ phát sóng BLE đã ngừng hoạt động để tiết kiệm pin.");
    s_gatts_if = ESP_GATT_IF_NONE;
    s_service_handle = 0;
    s_char_handle = 0;
    s_descr_handle = 0;
    s_adv_data_configured = false;
    s_scan_rsp_configured = false;
    xSemaphoreGive(s_notify_mutex);
    return ESP_OK;
}

int watch_bluetooth_get_count(void) {
    if (!s_mutex) return 0;
    int count = 0;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLUETOOTH_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        count = s_notif_count;
        xSemaphoreGive(s_mutex);
    }
    return count;
}

uint32_t watch_bluetooth_get_overflow_count(void) {
    if (!s_mutex) return 0;
    uint32_t count = 0;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLUETOOTH_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        count = s_notif_overflow_count;
        xSemaphoreGive(s_mutex);
    }
    return count;
}

bool watch_bluetooth_get(int index, watch_bluetooth_notification_t *out_notif) {
    if (!out_notif || !s_mutex) return false;

    bool ok = false;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLUETOOTH_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        if (index >= 0 && index < s_notif_count) {
            *out_notif = s_notifications[index];
            ok = true;
        }
        xSemaphoreGive(s_mutex);
    }
    return ok;
}

bool watch_bluetooth_has_new(void) {
    if (!s_mutex) return false;
    bool has_new = false;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLUETOOTH_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        for (int i = 0; i < s_notif_count; i++) {
            if (s_notifications[i].is_new) {
                has_new = true;
                break;
            }
        }
        xSemaphoreGive(s_mutex);
    }
    return has_new;
}

void watch_bluetooth_mark_read(int index) {
    if (!s_mutex) return;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLUETOOTH_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        if (index >= 0 && index < s_notif_count) {
            s_notifications[index].is_new = false;
        }
        xSemaphoreGive(s_mutex);
    }
}

bool watch_bluetooth_delete_by_seq_id(uint32_t seq_id) {
    if (!s_mutex) return false;

    bool deleted = false;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLUETOOTH_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        for (int i = 0; i < s_notif_count; i++) {
            if (s_notifications[i].seq_id != seq_id) continue;

            /* Dồn các tin nhắn phía sau lên trên để lấp khoảng trống */
            for (int j = i; j < s_notif_count - 1; j++) {
                s_notifications[j] = s_notifications[j + 1];
            }
            s_notif_count--;
            memset(&s_notifications[s_notif_count], 0, sizeof(s_notifications[s_notif_count]));
            deleted = true;
            break;
        }
        xSemaphoreGive(s_mutex);
    }
    return deleted;
}

void watch_bluetooth_clear_all(void) {
    if (!s_mutex) return;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLUETOOTH_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        s_notif_count = 0;
        s_notif_overflow_count = 0;
        memset(s_notifications, 0, sizeof(s_notifications));
        xSemaphoreGive(s_mutex);
    }
}

bool watch_bluetooth_is_connected(void) {
    return s_enabled && ble_get_link_state(NULL, NULL);
}

esp_err_t watch_bluetooth_set_enabled(bool enabled) {
    if (enabled && !s_enabled) {
        ESP_LOGI(TAG, "Đang mở Bluetooth...");
        esp_err_t err = watch_bluetooth_init();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Mở Bluetooth thất bại: %s", esp_err_to_name(err));
            return err;
        }
        s_enabled = true;
        ESP_ERROR_CHECK_WITHOUT_ABORT(watch_settings_set_bluetooth_enabled(true));
        ESP_LOGI(TAG, "Bluetooth đã mở thành công");
    } else if (!enabled && s_enabled) {
        ESP_LOGI(TAG, "Đang tắt Bluetooth...");
        esp_err_t err = watch_bluetooth_deinit();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Tắt Bluetooth thất bại: %s", esp_err_to_name(err));
            return err;
        }
        s_enabled = false;
        ESP_ERROR_CHECK_WITHOUT_ABORT(watch_settings_set_bluetooth_enabled(false));
        ESP_LOGI(TAG, "Bluetooth đã tắt");
    }
    return ESP_OK;
}

esp_err_t watch_bluetooth_control_init(void) {
    if (s_bt_control_queue && s_bt_control_task) return ESP_OK;

    if (!s_bt_control_queue) {
        s_bt_control_queue = xQueueCreate(1, sizeof(bool));
        if (!s_bt_control_queue) return ESP_ERR_NO_MEM;
    }
    if (!s_bt_control_task) {
        if (xTaskCreatePinnedToCore(ble_control_worker, "ble_control", 4096, NULL, 2,
                                    &s_bt_control_task, 0) != pdPASS) {
            vQueueDelete(s_bt_control_queue);
            s_bt_control_queue = NULL;
            return ESP_ERR_NO_MEM;
        }
    }
    return ESP_OK;
}

esp_err_t watch_bluetooth_request_enabled(bool enabled) {
    esp_err_t err = watch_bluetooth_control_init();
    if (err != ESP_OK) return err;
    return xQueueOverwrite(s_bt_control_queue, &enabled) == pdTRUE ? ESP_OK : ESP_FAIL;
}

bool watch_bluetooth_is_transitioning(void) {
    return s_bt_transitioning;
}

bool watch_bluetooth_is_enabled(void) {
    return s_enabled;
}


uint32_t watch_bluetooth_get_connection_generation(void) {
    uint32_t generation = 0;
    if (s_mutex && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLUETOOTH_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        generation = s_connection_generation;
        xSemaphoreGive(s_mutex);
    }
    return generation;
}

void watch_bluetooth_set_new_callback(watch_bluetooth_notif_new_cb_t cb) {
    if (s_mutex) {
        if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLUETOOTH_MUTEX_TIMEOUT_MS)) == pdTRUE) {
            s_new_cb = cb;
            xSemaphoreGive(s_mutex);
        } else {
            s_new_cb = cb;
        }
        return;
    }
    s_new_cb = cb;
}

bool watch_bluetooth_get_navigation_data(watch_bluetooth_nav_data_t *out_nav) {
    if (!out_nav || !s_mutex) return false;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLUETOOTH_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        *out_nav = s_nav_data;
        xSemaphoreGive(s_mutex);
        return out_nav->active;
    }
    memset(out_nav, 0, sizeof(*out_nav));
    return false;
}

void watch_bluetooth_clear_navigation_data(void) {
    if (!s_mutex) return;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(WATCH_BLUETOOTH_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        memset(&s_nav_data, 0, sizeof(s_nav_data));
        xSemaphoreGive(s_mutex);
    }
}

esp_err_t watch_bluetooth_send_command(const char *cmd) {
    if (!cmd || cmd[0] == '\0') return ESP_ERR_INVALID_ARG;
    if (!s_notify_mutex ||
        xSemaphoreTake(s_notify_mutex, pdMS_TO_TICKS(WATCH_BLUETOOTH_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t result = ESP_OK;
    uint16_t conn_id = WATCH_BLUETOOTH_CONN_ID_NONE;
    bool notify_enabled = false;
    
    if (!ble_get_link_state(&conn_id, &notify_enabled) || conn_id == WATCH_BLUETOOTH_CONN_ID_NONE) {
        result = ESP_ERR_INVALID_STATE;
        goto done;
    }
    if (!notify_enabled) {
        ESP_LOGW(TAG, "Không thể truyền dữ liệu vì Client chưa kích hoạt cổng Notify!");
        result = ESP_ERR_INVALID_STATE;
        goto done;
    }
    if (s_gatts_if == ESP_GATT_IF_NONE || s_char_handle == 0) {
        result = ESP_ERR_INVALID_STATE;
        goto done;
    }

    size_t raw_len = strlen(cmd);
    if (raw_len > WATCH_BLUETOOTH_NOTIFY_CMD_MAX_LEN) {
        result = ESP_ERR_INVALID_ARG;
        goto done;
    }
    char framed[WATCH_BLUETOOTH_NOTIFY_CMD_MAX_LEN + 6];
    uint16_t crc = ble_crc16_ccitt((const uint8_t *)cmd, raw_len);
    int framed_len = snprintf(framed, sizeof(framed), "%s*%04X", cmd, crc);
    if (framed_len <= 0 || framed_len >= (int)sizeof(framed)) {
        result = ESP_ERR_INVALID_SIZE;
        goto done;
    }
    size_t payload_max = ble_get_notify_payload_max();
    if ((size_t)framed_len > payload_max) {
        ESP_LOGW(TAG, "Notify exceeds MTU: len=%u, max=%u",
                 (unsigned int)framed_len, (unsigned int)payload_max);
        result = ESP_ERR_INVALID_SIZE;
        goto done;
    }
    uint16_t len = (uint16_t)framed_len;
    
    /* Gửi dữ liệu không đồng bộ lên điện thoại qua cơ chế Notify */
    result = esp_ble_gatts_send_indicate(
        s_gatts_if, conn_id, s_char_handle, len, (uint8_t *)framed, false);
        
    if (result == ESP_OK) {
        ESP_LOGD(TAG, "BLE notify sent: %u bytes", (unsigned int)len);
    } else {
        ESP_LOGW(TAG, "Gửi dữ liệu BLE thất bại: %s", esp_err_to_name(result));
    }

done:
    xSemaphoreGive(s_notify_mutex);
    return result;
}

/* Đóng gói dữ liệu chuỗi tọa độ lớn và truyền tải bằng các gói nhỏ chặn đồng bộ */
static void ble_send_track_chunks_blocking(const char *track) {
    if (!track || track[0] == '\0') return;

    size_t len = strlen(track);
    size_t offset = 0;
    char *msg = malloc(512);
    if (!msg) {
        ESP_LOGE(TAG, "Lỗi cấp phát bộ nhớ đệm gửi gói hành trình!");
        return;
    }

    ble_route_stream_ctx_t ctx;
    ble_route_stream_init(&ctx, "TRACK_CONTEXT|LIVE");
    if (ble_send_track_begin(&ctx) != ESP_OK) {
        free(msg);
        return;
    }
    uint32_t seq = 0;
    bool complete = true;
    while (offset < len) {
        size_t payload_max = ble_get_notify_payload_max();
        int prefix_len = snprintf(msg, 512, "TRK|%lu|%lu|", (unsigned long)seq, (unsigned long)offset);
        if (prefix_len <= 0 || (size_t)prefix_len + 5U >= payload_max) {
            ESP_LOGW(TAG, "Không thể đóng gói TRACK_CHUNK với MTU hiện tại, offset=%lu, max=%u",
                     (unsigned long)offset, (unsigned int)payload_max);
            complete = false;
            break;
        }
        size_t chunk_size = payload_max - (size_t)prefix_len - 5U;
        size_t n = len - offset;
        if (n > chunk_size) n = chunk_size;
        
        /* Đóng gói mảnh dữ liệu hành trình: TRACK_CHUNK|offset|đoạn_chuỗi */
        snprintf(msg + prefix_len, 512 - (size_t)prefix_len, "%.*s", (int)n, track + offset);
        if (watch_bluetooth_send_command(msg) != ESP_OK) {
            complete = false;
            break;
        }
        
        ctx.crc32 = ble_crc32_update(ctx.crc32, (const uint8_t *)track + offset, n);
        offset += n;
        ctx.offset = offset;
        seq++;
        vTaskDelay(pdMS_TO_TICKS(30)); // Delay 30ms giữa các gói tránh nghẽn hàng đợi BLE
    }
    if (complete && offset == len) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(ble_send_track_end(&ctx));
    } else {
        ESP_ERROR_CHECK_WITHOUT_ABORT(watch_bluetooth_send_command("TRACK_ERR|SEND"));
    }
    free(msg);
}

void watch_bluetooth_send_track_chunks(const char *track) {
    if (!track || track[0] == '\0') return;

    size_t len = strlen(track) + 1;
    char *copy = heap_caps_malloc(len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!copy) copy = malloc(len);
    if (!copy) {
        ESP_LOGE(TAG, "Lỗi cấp phát RAM lưu bản sao hành trình!");
        return;
    }
    memcpy(copy, track, len);

    /* Khởi chạy luồng bất đồng bộ truyền dữ liệu lớn để tránh khóa cứng UI đồng hồ */
    ble_track_send_cmd_t cmd = {
        .type = BLE_TRACK_SEND_BUFFER,
        .track = copy,
    };
    if (!ble_queue_track_send(&cmd)) {
        ESP_LOGW(TAG, "Route send queue is full");
        free(copy);
    }
}
