/* Hiện thực hệ thống quản lý, đọc/ghi cấu hình cài đặt đồng hồ vào Flash NVS
   Chi tiết thiết kế:
   1. Lưu trữ an toàn: Mọi thiết lập được đóng gói cùng với mã nhận dạng kiểm tra (Magic Header)
   và phiên bản cấu trúc (Version) giúp phần mềm phát hiện và ngăn chặn việc đọc dữ liệu lỗi thời
   sau khi cập nhật firmware.
   2. Ràng buộc an toàn (Sanitization): Hàm `watch_settings_sanitize` kiểm tra chéo các giá trị cài đặt
   nhận về từ người dùng (ví dụ: giới hạn độ sáng từ 5-100%, cường độ rung từ 0-2) nhằm tránh ghi đè
   các giá trị rác gây lỗi hệ thống.
   3. Hỗ trợ lưu trữ chuỗi nhị phân Blob và văn bản (WiFi SSID, Password, OTA URL). */

#include "watch_settings.h"
#include "esp_log.h"
#include "nvs.h"
#include "sdkconfig.h"
#include "esp_pm.h"
#include <time.h>
#include <string.h>

static const char *TAG = "SETTINGS_STORE";

#define WATCH_SETTINGS_NS       "watch"       // Tên phân vùng không gian lưu trữ NVS (Namespace) cho đồng hồ
#define WATCH_SETTINGS_KEY      "settings"    // Khóa lưu cấu hình chính hệ thống
#define WATCH_ALARMS_KEY        "alarms"      // Khóa lưu blob danh sách báo thức
#define WATCH_WIFI_SSID_KEY     "wifi_ssid"   // Khóa lưu chuỗi SSID Wi-Fi
#define WATCH_WIFI_PASS_KEY     "wifi_pass"   // Khóa lưu chuỗi mật khẩu Wi-Fi
#define WATCH_OTA_URL_KEY       "ota_url"     // Khóa lưu chuỗi URL máy chủ nạp OTA
#define WATCH_BT_ENABLED_KEY    "bt_enabled"  // Ghi nhớ trạng thái bật/tắt Bluetooth khi khởi động lại

// Mã xác thực Magic signature của cấu hình
#define WATCH_SETTINGS_MAGIC    0x57435354U

// Phiên bản dữ liệu cấu hình
#define WATCH_SETTINGS_VERSION  2U

/* Cấu trúc đóng gói dữ liệu ghi Flash */
typedef struct {
    uint32_t magic;
    uint8_t version;
    watch_settings_t settings;
} watch_settings_record_t;

/* Cài đặt mặc định ban đầu của thiết bị */
static watch_settings_t s_settings = {
    .brightness_pct = 78,               // Độ sáng màn hình mặc định ~78%
    .screen_timeout_sec = 0,            // Màn hình mặc định không bao giờ tự tắt (0)
    .quick_wake_enabled = false,        // Mặc định tắt tính năng gõ màn hình thức dậy
    .vibration_enabled = true,          // Mặc định cho phép rung còi báo
    .vibration_strength = 1,            // Rung ở mức trung bình (1)
    .language = 0,                      // Ngôn ngữ mặc định: Tiếng Anh (0)
    .watchface_style = 1,               // Giao diện mặt đồng hồ số 1
    .icons_monochrome = false,          // Biểu tượng menu màu sắc đầy đủ
    .icon_color = WATCH_COLOR_DEFAULT,
    .auto_light_sleep_enabled = false,  // Tạm thời tắt để kiểm tra lỗi GPS
};

/* Ràng buộc và chuẩn hóa các giá trị cấu hình đầu vào để tránh giá trị lỗi */
static watch_settings_t watch_settings_sanitize(watch_settings_t in) {
    if (in.brightness_pct < 5)   in.brightness_pct = 5;
    if (in.brightness_pct > 100) in.brightness_pct = 100;
    switch (in.screen_timeout_sec) {
        case 0:
        case 5:
        case 10:
        case 15:
        case 20:
            break;
        default:
            in.screen_timeout_sec = 0;
            break;
    }
    if (in.vibration_strength > 2) in.vibration_strength = 1;
    if (in.language > 1)         in.language = 0;
    if (in.watchface_style > 2)  in.watchface_style = 1;
    if (in.icon_color > WATCH_COLOR_ORANGE) in.icon_color = WATCH_COLOR_DEFAULT;
    if (!in.icons_monochrome)    in.icon_color = WATCH_COLOR_DEFAULT;
    return in;
}

/* Ghi toàn bộ dữ liệu cấu hình s_settings hiện hành vào NVS Flash */
static esp_err_t watch_settings_save_all(void) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(WATCH_SETTINGS_NS, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;

    watch_settings_record_t rec = {
        .magic = WATCH_SETTINGS_MAGIC,
        .version = WATCH_SETTINGS_VERSION,
        .settings = watch_settings_sanitize(s_settings),
    };
    
    err = nvs_set_blob(nvs, WATCH_SETTINGS_KEY, &rec, sizeof(rec));
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err;
}

esp_err_t watch_settings_init(void) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(WATCH_SETTINGS_NS, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Lỗi mở không gian lưu trữ NVS: %s", esp_err_to_name(err));
        return err;
    }

    watch_settings_record_t rec;
    size_t len = sizeof(rec);
    err = nvs_get_blob(nvs, WATCH_SETTINGS_KEY, &rec, &len);
    nvs_close(nvs);

    /* Xác thực kiểm tra dữ liệu đọc được */
    if (err == ESP_OK && len == sizeof(rec) &&
        rec.magic == WATCH_SETTINGS_MAGIC &&
        rec.version == WATCH_SETTINGS_VERSION) {
        s_settings = watch_settings_sanitize(rec.settings);
        ESP_LOGI(TAG, "Nạp cấu hình cài đặt thành công từ Flash NVS!");
        return ESP_OK;
    }

    if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Lỗi nạp cấu hình cài đặt cũ: %s. Khởi tạo lại cấu hình mặc định...", esp_err_to_name(err));
    }
    return watch_settings_save_all(); // Ghi đè cấu hình mặc định nếu lỗi hoặc chưa có
}

const watch_settings_t *watch_settings_get(void) {
    return &s_settings;
}

esp_err_t watch_settings_set_brightness_pct(uint8_t pct) {
    watch_settings_t next = s_settings;
    next.brightness_pct = pct;
    next = watch_settings_sanitize(next);
    if (next.brightness_pct == s_settings.brightness_pct) return ESP_OK;
    s_settings = next;
    return watch_settings_save_all();
}

esp_err_t watch_settings_set_screen_timeout(uint16_t seconds) {
    if (s_settings.screen_timeout_sec == seconds) return ESP_OK;
    s_settings.screen_timeout_sec = seconds;
    s_settings = watch_settings_sanitize(s_settings);
    return watch_settings_save_all();
}

esp_err_t watch_settings_set_quick_wake(bool enabled) {
    if (s_settings.quick_wake_enabled == enabled) return ESP_OK;
    s_settings.quick_wake_enabled = enabled;
    return watch_settings_save_all();
}

esp_err_t watch_settings_set_vibration_enabled(bool enabled) {
    if (s_settings.vibration_enabled == enabled) return ESP_OK;
    s_settings.vibration_enabled = enabled;
    return watch_settings_save_all();
}

esp_err_t watch_settings_set_vibration_strength(uint8_t strength) {
    watch_settings_t next = s_settings;
    next.vibration_strength = strength;
    next = watch_settings_sanitize(next);
    if (next.vibration_strength == s_settings.vibration_strength) return ESP_OK;
    s_settings = next;
    return watch_settings_save_all();
}

esp_err_t watch_settings_set_language(uint8_t language) {
    watch_settings_t next = s_settings;
    next.language = language;
    next = watch_settings_sanitize(next);
    if (next.language == s_settings.language) return ESP_OK;
    s_settings = next;
    return watch_settings_save_all();
}

esp_err_t watch_settings_set_watchface_style(uint8_t style) {
    watch_settings_t next = s_settings;
    next.watchface_style = style;
    next = watch_settings_sanitize(next);
    if (next.watchface_style == s_settings.watchface_style) return ESP_OK;
    s_settings = next;
    return watch_settings_save_all();
}

esp_err_t watch_settings_set_icon_color(bool monochrome, watch_color_id_t color) {
    watch_settings_t next = s_settings;
    next.icons_monochrome = monochrome;
    next.icon_color = color;
    next = watch_settings_sanitize(next);
    if (next.icons_monochrome == s_settings.icons_monochrome &&
        next.icon_color == s_settings.icon_color) {
        return ESP_OK;
    }
    s_settings = next;
    return watch_settings_save_all();
}

bool watch_settings_get_bluetooth_enabled(void) {
    nvs_handle_t nvs;
    if (nvs_open(WATCH_SETTINGS_NS, NVS_READONLY, &nvs) != ESP_OK) return true;

    uint8_t enabled = 1;
    esp_err_t err = nvs_get_u8(nvs, WATCH_BT_ENABLED_KEY, &enabled);
    nvs_close(nvs);
    return err == ESP_OK ? enabled != 0 : true;
}

esp_err_t watch_settings_set_bluetooth_enabled(bool enabled) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(WATCH_SETTINGS_NS, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;

    uint8_t current = 1;
    esp_err_t read_err = nvs_get_u8(nvs, WATCH_BT_ENABLED_KEY, &current);
    if ((read_err == ESP_OK && (current != 0) == enabled) ||
        (read_err == ESP_ERR_NVS_NOT_FOUND && enabled)) {
        nvs_close(nvs);
        return ESP_OK;
    }

    err = nvs_set_u8(nvs, WATCH_BT_ENABLED_KEY, enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    return err;
}

esp_err_t watch_settings_load_alarm_blob(void *data, size_t len) {
    if (!data || len == 0) return ESP_ERR_INVALID_ARG;

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(WATCH_SETTINGS_NS, NVS_READONLY, &nvs);
    if (err != ESP_OK) return err;

    size_t stored_len = len;
    err = nvs_get_blob(nvs, WATCH_ALARMS_KEY, data, &stored_len);
    nvs_close(nvs);
    
    if (err == ESP_OK && stored_len != len) {
        return ESP_ERR_INVALID_SIZE;
    }
    return err;
}

esp_err_t watch_settings_save_alarm_blob(const void *data, size_t len) {
    if (!data || len == 0) return ESP_ERR_INVALID_ARG;

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(WATCH_SETTINGS_NS, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;

    err = nvs_set_blob(nvs, WATCH_ALARMS_KEY, data, len);
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err;
}

esp_err_t watch_settings_get_wifi_credentials(char *ssid, size_t ssid_size,
                                              char *password, size_t password_size) {
    if (!ssid || ssid_size == 0) return ESP_ERR_INVALID_ARG;
    ssid[0] = '\0';
    if (password && password_size > 0) password[0] = '\0';

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(WATCH_SETTINGS_NS, NVS_READONLY, &nvs);
    if (err != ESP_OK) return err;

    size_t len = ssid_size;
    err = nvs_get_str(nvs, WATCH_WIFI_SSID_KEY, ssid, &len);
    
    if (err == ESP_OK && password && password_size > 0) {
        len = password_size;
        esp_err_t pass_err = nvs_get_str(nvs, WATCH_WIFI_PASS_KEY, password, &len);
        if (pass_err == ESP_ERR_NVS_NOT_FOUND) {
            password[0] = '\0';
        } else if (pass_err != ESP_OK) {
            err = pass_err;
        }
    }
    nvs_close(nvs);
    return err;
}

esp_err_t watch_settings_set_wifi_credentials(const char *ssid, const char *password) {
    if (!ssid || ssid[0] == '\0') return ESP_ERR_INVALID_ARG;

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(WATCH_SETTINGS_NS, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;

    err = nvs_set_str(nvs, WATCH_WIFI_SSID_KEY, ssid);
    if (err == ESP_OK) {
        err = nvs_set_str(nvs, WATCH_WIFI_PASS_KEY, password ? password : "");
    }
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err;
}

esp_err_t watch_settings_clear_wifi_credentials(void) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(WATCH_SETTINGS_NS, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;

    esp_err_t ssid_err = nvs_erase_key(nvs, WATCH_WIFI_SSID_KEY);
    esp_err_t pass_err = nvs_erase_key(nvs, WATCH_WIFI_PASS_KEY);
    
    if (ssid_err == ESP_ERR_NVS_NOT_FOUND) ssid_err = ESP_OK;
    if (pass_err == ESP_ERR_NVS_NOT_FOUND) pass_err = ESP_OK;
    
    if (ssid_err == ESP_OK && pass_err == ESP_OK) {
        err = nvs_commit(nvs);
    } else {
        err = ssid_err != ESP_OK ? ssid_err : pass_err;
    }
    nvs_close(nvs);
    return err;
}

esp_err_t watch_settings_get_ota_url(char *url, size_t url_size) {
    if (!url || url_size == 0) return ESP_ERR_INVALID_ARG;
    url[0] = '\0';

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(WATCH_SETTINGS_NS, NVS_READONLY, &nvs);
    if (err != ESP_OK) return err;

    size_t len = url_size;
    err = nvs_get_str(nvs, WATCH_OTA_URL_KEY, url, &len);
    nvs_close(nvs);
    return err;
}

esp_err_t watch_settings_set_ota_url(const char *url) {
    if (!url || url[0] == '\0') return ESP_ERR_INVALID_ARG;

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(WATCH_SETTINGS_NS, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;

    err = nvs_set_str(nvs, WATCH_OTA_URL_KEY, url);
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err;
}

esp_err_t watch_settings_set_auto_light_sleep(bool enabled) {
    if (s_settings.auto_light_sleep_enabled == enabled) return ESP_OK;
    s_settings.auto_light_sleep_enabled = enabled;
    esp_err_t err = watch_settings_save_all();
    if (err == ESP_OK) {
        watch_pm_configure();
    }
    return err;
}

esp_err_t watch_pm_configure(void) {
#if CONFIG_PM_ENABLE
    esp_pm_config_t pm_config = {
        .max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
        .min_freq_mhz = CONFIG_XTAL_FREQ,
        .light_sleep_enable = false // Khóa cứng tắt ngủ tự động để debug lỗi GPS
    };
    esp_err_t err = esp_pm_configure(&pm_config);
    ESP_LOGI(TAG, "Cấu hình Power Management: Auto Light Sleep %s (err=%s)",
             s_settings.auto_light_sleep_enabled ? "BẬT" : "TẮT",
             esp_err_to_name(err));
    return err;
#else
    ESP_LOGW(TAG, "CONFIG_PM_ENABLE không được bật trong sdkconfig");
    return ESP_OK;
#endif
}

typedef struct {
    uint8_t count;
    watch_health_record_t records[WATCH_HEALTH_MAX_RECORDS];
} watch_health_store_t;

esp_err_t watch_settings_add_health_record(uint8_t heart_rate, uint8_t spo2) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("health_store", NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;

    watch_health_store_t store = {0};
    size_t len = sizeof(store);
    err = nvs_get_blob(nvs, "store", &store, &len);
    if (err != ESP_OK || len != sizeof(store)) {
        store.count = 0;
    }

    // Lấy timestamp từ hệ thống RTC
    time_t now = time(NULL);
    
    // Nếu đầy bộ đệm (20 phần tử), dịch trái để xóa phần tử cũ nhất (FIFO)
    if (store.count >= WATCH_HEALTH_MAX_RECORDS) {
        for (int i = 1; i < WATCH_HEALTH_MAX_RECORDS; i++) {
            store.records[i - 1] = store.records[i];
        }
        store.count = WATCH_HEALTH_MAX_RECORDS - 1;
    }

    store.records[store.count].timestamp = (uint32_t)now;
    store.records[store.count].heart_rate = heart_rate;
    store.records[store.count].spo2 = spo2;
    store.count++;

    err = nvs_set_blob(nvs, "store", &store, sizeof(store));
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err;
}

esp_err_t watch_settings_get_health_records(watch_health_record_t *out_records, uint8_t *out_count) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("health_store", NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        *out_count = 0;
        return err;
    }

    watch_health_store_t store = {0};
    size_t len = sizeof(store);
    err = nvs_get_blob(nvs, "store", &store, &len);
    nvs_close(nvs);

    if (err != ESP_OK || len != sizeof(store)) {
        *out_count = 0;
        return err;
    }

    *out_count = store.count;
    memcpy(out_records, store.records, store.count * sizeof(watch_health_record_t));
    return ESP_OK;
}

esp_err_t watch_settings_clear_health_records(void) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("health_store", NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;

    watch_health_store_t store = {0};
    err = nvs_set_blob(nvs, "store", &store, sizeof(store));
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err;
}
