/* Stores small activity metadata in NVS and route data in SPIFFS. */

#include "watch_activity_log.h"

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "esp_log.h"
#include "esp_random.h"
#include "esp_spiffs.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"

#define WATCH_ACTIVITY_NS       "watch"       // Tên không gian tên NVS cho dữ liệu hoạt động thể thao
#define WATCH_ACTIVITY_KEY      "activities"  // Khóa NVS lưu dữ liệu danh sách hoạt động
// Mã xác thực Magic signature của dữ liệu hoạt động
#define WATCH_ACTIVITY_MAGIC    0x57414354U
// Phiên bản cấu trúc dữ liệu hoạt động
#define WATCH_ACTIVITY_VERSION  3U
#define WATCH_ACTIVITY_FS_BASE  "/spiffs"     // Phân vùng ảo SPIFFS
#define WATCH_ACTIVITY_FS_LABEL "storage"     // Nhãn phân vùng
#define WATCH_ACTIVITY_CAPTURE_PATH WATCH_ACTIVITY_FS_BASE "/current.tmp" // Tệp tạm thời lưu vết định vị hiện hành

// Ngưỡng giữ lại bộ nhớ trống tối thiểu của SPIFFS (128 KB) để tránh tràn bộ nhớ Flash
#define WATCH_ACTIVITY_CAPTURE_RESERVE (128U * 1024U)

// Dung lượng tối thiểu của tệp ghi nhận (4 KB)
#define WATCH_ACTIVITY_CAPTURE_MIN     (4U * 1024U)

static const char *TAG = "ACT_LOG";

typedef struct {
    uint32_t magic;
    uint8_t version;
    uint8_t count;
    watch_activity_record_t records[WATCH_ACTIVITY_LOG_MAX];
} watch_activity_store_t;

static watch_activity_store_t s_store;
static bool s_fs_mounted;
static SemaphoreHandle_t s_store_mutex;
static SemaphoreHandle_t s_capture_mutex;
static FILE *s_capture_file;
static bool s_capture_first_point = true;
static uint16_t s_capture_unflushed_points;
static bool s_capture_limit_reached;
static size_t s_capture_byte_limit;

static bool activity_log_lock(void) {
    return s_store_mutex && xSemaphoreTake(s_store_mutex, portMAX_DELAY) == pdTRUE;
}

static void activity_log_unlock(void) {
    xSemaphoreGive(s_store_mutex);
}

static void activity_log_reset_store(void) {
    memset(&s_store, 0, sizeof(s_store));
    s_store.magic = WATCH_ACTIVITY_MAGIC;
    s_store.version = WATCH_ACTIVITY_VERSION;
}

static esp_err_t activity_log_mount_fs(void) {
    if (s_fs_mounted) return ESP_OK;

    esp_vfs_spiffs_conf_t conf = {
        .base_path = WATCH_ACTIVITY_FS_BASE,
        .partition_label = WATCH_ACTIVITY_FS_LABEL,
        .max_files = WATCH_ACTIVITY_LOG_MAX + 2,
        .format_if_mount_failed = true,
    };
    esp_err_t err = esp_vfs_spiffs_register(&conf);
    if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
        s_fs_mounted = true;
        ESP_LOGI(TAG, "SPIFFS partition '%s' mounted successfully", WATCH_ACTIVITY_FS_LABEL);
        return ESP_OK;
    }
    ESP_LOGE(TAG, "Cannot mount activity SPIFFS: %s", esp_err_to_name(err));
    return err;
}

static esp_err_t activity_log_save(void) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(WATCH_ACTIVITY_NS, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;

    err = nvs_set_blob(nvs, WATCH_ACTIVITY_KEY, &s_store, sizeof(s_store));
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    return err;
}

static void activity_log_delete_route(const watch_activity_record_t *record) {
    if (!record || record->route_path[0] == '\0') return;
    if (remove(record->route_path) != 0) {
        ESP_LOGW(TAG, "Cannot delete old route: %s", record->route_path);
    }
}

static esp_err_t activity_log_set_capture_limit(void) {
    size_t total = 0;
    size_t used = 0;
    esp_err_t err = esp_spiffs_info(WATCH_ACTIVITY_FS_LABEL, &total, &used);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_spiffs_info failed: %s", esp_err_to_name(err));
        return err;
    }

    const size_t free_bytes = total > used ? total - used : 0;
    ESP_LOGI(TAG, "SPIFFS space info: total=%u, used=%u, free=%u bytes", 
             (unsigned int)total, (unsigned int)used, (unsigned int)free_bytes);
             
    if (free_bytes <= WATCH_ACTIVITY_CAPTURE_RESERVE + WATCH_ACTIVITY_CAPTURE_MIN) {
        ESP_LOGW(TAG, "Not enough SPIFFS space: free=%u, required=%u bytes", 
                 (unsigned int)free_bytes, 
                 (unsigned int)(WATCH_ACTIVITY_CAPTURE_RESERVE + WATCH_ACTIVITY_CAPTURE_MIN));
        s_capture_byte_limit = 0;
        return ESP_ERR_NO_MEM;
    }
    s_capture_byte_limit = free_bytes - WATCH_ACTIVITY_CAPTURE_RESERVE;
    if (s_capture_byte_limit > WATCH_ACTIVITY_ROUTE_MAX) {
        s_capture_byte_limit = WATCH_ACTIVITY_ROUTE_MAX;
    }
    ESP_LOGI(TAG, "Route capture limit: %u bytes", (unsigned int)s_capture_byte_limit);
    return ESP_OK;
}

static bool activity_log_route_is_referenced(const char *path) {
    for (size_t i = 0; i < s_store.count; i++) {
        if (strcmp(path, s_store.records[i].route_path) == 0) return true;
    }
    return false;
}

static bool activity_log_is_managed_route(const char *name) {
    if (!name) return false;
    if (strcmp(name, "current.tmp") == 0) return true;
    size_t len = strlen(name);
    return len > 5 && name[0] == 'a' && strcmp(name + len - 4, ".rte") == 0;
}

static void activity_log_remove_orphan_routes(void) {
    DIR *dir = opendir(WATCH_ACTIVITY_FS_BASE);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        if (!activity_log_is_managed_route(entry->d_name)) continue;

        char path[WATCH_ACTIVITY_ROUTE_PATH_MAX];
        int len = snprintf(path, sizeof(path), entry->d_name[0] == '/'
                           ? WATCH_ACTIVITY_FS_BASE "%s"
                           : WATCH_ACTIVITY_FS_BASE "/%s",
                           entry->d_name);
        if (len > 0 && len < (int)sizeof(path) && !activity_log_route_is_referenced(path)) {
            ESP_LOGW(TAG, "Deleting orphan route: %s", path);
            remove(path);
        }
    }
    closedir(dir);
}

static esp_err_t activity_log_write_route(const char *path, const char *route) {
    FILE *file = fopen(path, "wb");
    if (!file) return ESP_FAIL;

    size_t len = route ? strnlen(route, WATCH_ACTIVITY_ROUTE_MAX) : 0;
    size_t written = len > 0 ? fwrite(route, 1, len, file) : 0;
    int close_result = fclose(file);
    if (written != len || close_result != 0) {
        remove(path);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t watch_activity_log_init(void) {
    if (!s_store_mutex) {
        s_store_mutex = xSemaphoreCreateMutex();
        if (!s_store_mutex) return ESP_ERR_NO_MEM;
    }
    if (!s_capture_mutex) {
        s_capture_mutex = xSemaphoreCreateMutex();
        if (!s_capture_mutex) return ESP_ERR_NO_MEM;
    }

    esp_err_t err = activity_log_mount_fs();
    if (err != ESP_OK) return err;

    activity_log_reset_store();
    nvs_handle_t nvs;
    err = nvs_open(WATCH_ACTIVITY_NS, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;

    size_t len = sizeof(s_store);
    err = nvs_get_blob(nvs, WATCH_ACTIVITY_KEY, &s_store, &len);
    if (err == ESP_OK && len == sizeof(s_store) &&
        s_store.magic == WATCH_ACTIVITY_MAGIC &&
        s_store.version == WATCH_ACTIVITY_VERSION &&
        s_store.count <= WATCH_ACTIVITY_LOG_MAX) {
        nvs_close(nvs);
        activity_log_remove_orphan_routes();
        ESP_LOGI(TAG, "Loaded %u activity records", s_store.count);
        return ESP_OK;
    }

    if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Old or invalid activity metadata; creating a new store");
        ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_erase_key(nvs, WATCH_ACTIVITY_KEY));
        ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_commit(nvs));
    }
    nvs_close(nvs);
    activity_log_reset_store();
    err = activity_log_save();
    if (err == ESP_OK) activity_log_remove_orphan_routes();
    return err;
}

esp_err_t watch_activity_log_capture_start(void) {
    esp_err_t err = activity_log_mount_fs();
    if (err != ESP_OK) return err;
    if (!s_capture_mutex ||
        xSemaphoreTake(s_capture_mutex, pdMS_TO_TICKS(250)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    if (s_capture_file) {
        fclose(s_capture_file);
        s_capture_file = NULL;
    }
    remove(WATCH_ACTIVITY_CAPTURE_PATH);
    err = activity_log_set_capture_limit();
    if (err != ESP_OK) {
        xSemaphoreGive(s_capture_mutex);
        return err;
    }
    s_capture_file = fopen(WATCH_ACTIVITY_CAPTURE_PATH, "wb");
    if (!s_capture_file) {
        ESP_LOGE(TAG, "Failed to open capture file '%s' for writing: errno=%d", WATCH_ACTIVITY_CAPTURE_PATH, errno);
    }
    s_capture_first_point = true;
    s_capture_unflushed_points = 0;
    s_capture_limit_reached = false;
    err = s_capture_file ? ESP_OK : ESP_FAIL;
    xSemaphoreGive(s_capture_mutex);
    return err;
}

esp_err_t watch_activity_log_capture_point(double latitude_deg,
                                           double longitude_deg,
                                           float hdop,
                                           uint8_t satellites,
                                           bool reconnect) {
    if (!s_capture_mutex ||
        xSemaphoreTake(s_capture_mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    if (!s_capture_file) {
        xSemaphoreGive(s_capture_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    if (s_capture_limit_reached) {
        xSemaphoreGive(s_capture_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    char point[64];
    int point_len = snprintf(point, sizeof(point), "%s%.6f,%.6f,%.1f,%u,%u",
                             s_capture_first_point ? "" : ";",
                             latitude_deg, longitude_deg, (double)hdop,
                             (unsigned int)satellites, reconnect ? 1U : 0U);
    long current_size = ftell(s_capture_file);
    if (point_len <= 0 || current_size < 0 ||
        (size_t)current_size + (size_t)point_len > s_capture_byte_limit) {
        s_capture_limit_reached = true;
        ESP_LOGW(TAG, "Route capture reached %u-byte limit; keeping workout statistics",
                 (unsigned int)s_capture_byte_limit);
        xSemaphoreGive(s_capture_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    size_t written = fwrite(point, 1, (size_t)point_len, s_capture_file);
    esp_err_t err = written == (size_t)point_len ? ESP_OK : ESP_FAIL;
    if (err == ESP_OK) {
        s_capture_first_point = false;
        if (++s_capture_unflushed_points >= 16U) {
            if (fflush(s_capture_file) != 0) err = ESP_FAIL;
            s_capture_unflushed_points = 0;
        }
    }
    xSemaphoreGive(s_capture_mutex);
    return err;
}

void watch_activity_log_capture_discard(void) {
    if (!s_capture_mutex ||
        xSemaphoreTake(s_capture_mutex, pdMS_TO_TICKS(250)) != pdTRUE) {
        return;
    }
    if (s_capture_file) {
        fclose(s_capture_file);
        s_capture_file = NULL;
    }
    remove(WATCH_ACTIVITY_CAPTURE_PATH);
    s_capture_first_point = true;
    s_capture_unflushed_points = 0;
    s_capture_limit_reached = false;
    s_capture_byte_limit = 0;
    xSemaphoreGive(s_capture_mutex);
}

esp_err_t watch_activity_log_capture_finish(const watch_activity_record_t *record) {
    if (!record) return ESP_ERR_INVALID_ARG;
    esp_err_t err = activity_log_mount_fs();
    if (err != ESP_OK) return err;

    if (!s_capture_mutex ||
        xSemaphoreTake(s_capture_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    if (s_capture_file) {
        FILE *capture_file = s_capture_file;
        s_capture_file = NULL;
        int flush_result = fflush(capture_file);
        int close_result = fclose(capture_file);
        if (flush_result != 0 || close_result != 0) {
            xSemaphoreGive(s_capture_mutex);
            return ESP_FAIL;
        }
    }

    if (!activity_log_lock()) {
        xSemaphoreGive(s_capture_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    watch_activity_record_t new_record = *record;
    snprintf(new_record.route_path, sizeof(new_record.route_path),
             WATCH_ACTIVITY_FS_BASE "/a%08lx_%u_%04lx.rte",
             (unsigned long)new_record.timestamp,
             (unsigned int)new_record.sport_id,
             (unsigned long)(esp_random() & 0xFFFFU));

    if (rename(WATCH_ACTIVITY_CAPTURE_PATH, new_record.route_path) != 0) {
        ESP_LOGE(TAG, "Failed to rename route capture file from '%s' to '%s': errno=%d", 
                 WATCH_ACTIVITY_CAPTURE_PATH, new_record.route_path, errno);
        activity_log_unlock();
        xSemaphoreGive(s_capture_mutex);
        return ESP_FAIL;
    }

    watch_activity_store_t previous_store = s_store;
    for (int i = WATCH_ACTIVITY_LOG_MAX - 1; i > 0; i--) {
        s_store.records[i] = s_store.records[i - 1];
    }
    s_store.records[0] = new_record;
    if (s_store.count < WATCH_ACTIVITY_LOG_MAX) s_store.count++;

    err = activity_log_save();
    if (err != ESP_OK) {
        s_store = previous_store;
        if (rename(new_record.route_path, WATCH_ACTIVITY_CAPTURE_PATH) != 0) {
            remove(new_record.route_path);
        }
    } else if (previous_store.count == WATCH_ACTIVITY_LOG_MAX &&
               strcmp(previous_store.records[WATCH_ACTIVITY_LOG_MAX - 1].route_path,
                      new_record.route_path) != 0) {
        activity_log_delete_route(&previous_store.records[WATCH_ACTIVITY_LOG_MAX - 1]);
    }

    s_capture_first_point = err == ESP_OK;
    s_capture_unflushed_points = 0;
    s_capture_byte_limit = 0;
    activity_log_unlock();
    xSemaphoreGive(s_capture_mutex);
    return err;
}

esp_err_t watch_activity_log_add(const watch_activity_record_t *record, const char *route) {
    if (!record) return ESP_ERR_INVALID_ARG;

    esp_err_t err = activity_log_mount_fs();
    if (err != ESP_OK) return err;
    if (!activity_log_lock()) return ESP_ERR_INVALID_STATE;

    watch_activity_record_t new_record = *record;
    const bool has_route = route && route[0] != '\0';
    if (has_route) {
        snprintf(new_record.route_path, sizeof(new_record.route_path),
                 WATCH_ACTIVITY_FS_BASE "/a%08lx_%u_%04lx.rte",
                 (unsigned long)new_record.timestamp,
                 (unsigned int)new_record.sport_id,
                 (unsigned long)(esp_random() & 0xFFFFU));

        err = activity_log_write_route(new_record.route_path, route);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Cannot write route: %s", new_record.route_path);
            activity_log_unlock();
            return err;
        }
    } else {
        new_record.route_path[0] = '\0';
    }

    watch_activity_store_t previous_store = s_store;
    for (int i = WATCH_ACTIVITY_LOG_MAX - 1; i > 0; i--) {
        s_store.records[i] = s_store.records[i - 1];
    }
    s_store.records[0] = new_record;
    if (s_store.count < WATCH_ACTIVITY_LOG_MAX) s_store.count++;

    err = activity_log_save();
    if (err != ESP_OK) {
        s_store = previous_store;
        if (has_route) remove(new_record.route_path);
        activity_log_unlock();
        return err;
    }
    if (previous_store.count == WATCH_ACTIVITY_LOG_MAX &&
        strcmp(previous_store.records[WATCH_ACTIVITY_LOG_MAX - 1].route_path,
               new_record.route_path) != 0) {
        activity_log_delete_route(&previous_store.records[WATCH_ACTIVITY_LOG_MAX - 1]);
    }

    if (has_route) {
        ESP_LOGI(TAG, "Saved activity route: %s", new_record.route_path);
    } else {
        ESP_LOGW(TAG, "Saved activity statistics without route");
    }
    activity_log_unlock();
    return ESP_OK;
}

size_t watch_activity_log_count(void) {
    if (!activity_log_lock()) return 0;
    size_t count = s_store.count;
    activity_log_unlock();
    return count;
}

size_t watch_activity_log_get(watch_activity_record_t *out, size_t max_records) {
    if (!out || max_records == 0 || !activity_log_lock()) return 0;
    size_t count = s_store.count < max_records ? s_store.count : max_records;
    memcpy(out, s_store.records, count * sizeof(s_store.records[0]));
    activity_log_unlock();
    return count;
}

esp_err_t watch_activity_log_stream_capture(watch_activity_route_chunk_cb_t cb, void *ctx) {
    if (!cb) return ESP_ERR_INVALID_ARG;
    esp_err_t err = activity_log_mount_fs();
    if (err != ESP_OK) return err;
    if (!s_capture_mutex ||
        xSemaphoreTake(s_capture_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    if (s_capture_file && fflush(s_capture_file) != 0) {
        xSemaphoreGive(s_capture_mutex);
        return ESP_FAIL;
    }
    FILE *file = fopen(WATCH_ACTIVITY_CAPTURE_PATH, "rb");
    if (!file) {
        xSemaphoreGive(s_capture_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        xSemaphoreGive(s_capture_mutex);
        return ESP_FAIL;
    }
    long snapshot_size = ftell(file);
    if (snapshot_size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        xSemaphoreGive(s_capture_mutex);
        return ESP_FAIL;
    }
    xSemaphoreGive(s_capture_mutex);

    char chunk[256];
    long remaining = snapshot_size;
    while (remaining > 0) {
        size_t requested = remaining < (long)sizeof(chunk) ? (size_t)remaining : sizeof(chunk);
        size_t count = fread(chunk, 1, requested, file);
        if (count == 0) {
            err = ferror(file) ? ESP_FAIL : ESP_ERR_INVALID_SIZE;
            break;
        }
        err = cb(chunk, count, ctx);
        if (err != ESP_OK) break;
        remaining -= (long)count;
    }
    fclose(file);
    return err;
}

esp_err_t watch_activity_log_stream_route(size_t index,
                                          watch_activity_route_chunk_cb_t cb,
                                          void *ctx) {
    if (!cb) return ESP_ERR_INVALID_ARG;
    if (!activity_log_lock()) return ESP_ERR_INVALID_STATE;
    if (index >= s_store.count || s_store.records[index].route_path[0] == '\0') {
        activity_log_unlock();
        return ESP_ERR_NOT_FOUND;
    }

    char route_path[WATCH_ACTIVITY_ROUTE_PATH_MAX];
    snprintf(route_path, sizeof(route_path), "%s", s_store.records[index].route_path);
    activity_log_unlock();

    FILE *file = fopen(route_path, "rb");
    if (!file) {
        return ESP_ERR_NOT_FOUND;
    }

    char chunk[256];
    esp_err_t err = ESP_OK;
    size_t count;
    while ((count = fread(chunk, 1, sizeof(chunk), file)) > 0) {
        err = cb(chunk, count, ctx);
        if (err != ESP_OK) break;
    }
    if (ferror(file) && err == ESP_OK) err = ESP_FAIL;
    fclose(file);
    return err;
}
