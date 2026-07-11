#include "gps_tracker.h"
#include "gps_parser.h"
#include "board_config.h"
#include "watch_activity_log.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "GPS";

static const int s_gps_baud_candidates[] = {
    WATCH_GPS_UART_BAUD,
};

// --- HẰNG SỐ CẤU HÌNH THỜI GIAN & ĐỒNG BỘ LUỒNG ---
// Thời gian chờ khóa Mutex bảo vệ dữ liệu GPS: 100ms.
#define GPS_MUTEX_TIMEOUT_MS          100

// --- HẰNG SỐ LỌC NHIỄU VÀ TÍNH TOÁN QUÃNG ĐƯỜNG ---
// Khoảng cách tối thiểu giữa 2 điểm để tính toán: 0.5 mét.
#define GPS_MIN_SEGMENT_DISTANCE_M    0.5

// Tốc độ tối đa cho phép: 25.0 m/s (~90 km/h) để lọc nhiễu nhảy tọa độ.
#define GPS_MAX_SEGMENT_SPEED_MPS     25.0

// Số lượng vệ tinh tối thiểu để bắt đầu định vị: 4.
#define GPS_MIN_SATELLITES            4

// Sai số HDOP tối đa để tránh tích lũy quãng đường ảo.
#define GPS_MAX_ACCUMULATE_HDOP       5.0f

// Ngưỡng tốc độ để xác định bắt đầu di chuyển: 1.5 km/h.
#define GPS_STATIONARY_SPEED_KMH      1.5f

// Số lần xác nhận để lọc nhiễu trạng thái dừng/đi (Debounce)
#define GPS_MOVING_CONFIRM_FIXES      3
#define GPS_STATIONARY_CONFIRM_FIXES  5

// --- HẰNG SỐ TÁI LIÊN KẾT ĐƯỜNG ĐI ---
// Số mẫu định vị ổn định liên tiếp cần thiết sau khi lấy lại sóng.
#define GPS_REACQUIRE_STABLE_FIXES    3

// Tốc độ tối đa quy đổi để chấp nhận nối lại đường đi sau khi mất tín hiệu: 
// Đi bộ: 3.0 m/s (~10.8 km/h), Đạp xe: 15.0 m/s (~54 km/h). 
// hệ thống sẽ coi là không thực tế đối với chế độ thể thao đó, buộc phải tạo đoạn hành trình mới (Segment) thay vì nối thẳng.
#define GPS_WALK_REACQUIRE_MAX_SPEED_MPS   3.0
#define GPS_CYCLE_REACQUIRE_MAX_SPEED_MPS  15.0

// Thời gian ngắt quãng tối đa cho phép nối lại đường đi:
// Đi bộ: 5 phút (300000 ms), Đạp xe: 10 phút (600000 ms).
#define GPS_WALK_REACQUIRE_MAX_GAP_MS       (5U * 60U * 1000U)
#define GPS_CYCLE_REACQUIRE_MAX_GAP_MS      (10U * 60U * 1000U)

// Khoảng cách tối đa cho phép để nối lại đường đi: Đi bộ: 2000m, Đạp xe: 8000m.
#define GPS_WALK_REACQUIRE_MAX_DISTANCE_M   2000.0
#define GPS_CYCLE_REACQUIRE_MAX_DISTANCE_M  8000.0

// Ngưỡng dung sai khoảng cách tái liên kết (30.0 mét)
#define GPS_REACQUIRE_TOLERANCE_M          30.0

// Ngưỡng HDOP tối đa để ghi nhận điểm vào vết (Track Log)
#define GPS_MAX_TRACK_HDOP                 2.5f
#define GPS_MAX_TRACK_HDOP_MOVING          3.5f

// Khoảng cách tối thiểu để thêm một điểm mới vào danh sách Track Log
#define GPS_MIN_TRACK_POINT_DISTANCE_KM    0.008f
#define GPS_MIN_TRACK_POINT_MOVING_KM      0.005f

// Ngưỡng loại bỏ điểm nhọn (Spike filter) tức thời
#define GPS_TRACK_SPIKE_MAX_M              18.0
#define GPS_TRACK_SPIKE_MOVING_M           25.0

// Ngưỡng góc rẽ tối đa để kiểm tra điểm nhọn
#define GPS_TRACK_MAX_TURN_DEG             55.0
#define GPS_TRACK_ALLEY_TURN_DEG           120.0

// Tốc độ tối thiểu để kích hoạt bộ lọc rẽ trong hẻm: 2.0 km/h.
#define GPS_TRACK_ALLEY_MIN_SPEED_KMH      2.0f

// Hệ số lọc thông thấp EMA làm mượt toạ độ (0.15)
#define GPS_SMOOTH_ALPHA_WEAK              0.15

// Kích thước vòng đệm UART RX (4KB)
#define GPS_UART_RX_BUFFER_SIZE       4096

// Chu kỳ ghi log chẩn đoán
#define GPS_HEARTBEAT_INTERVAL_MS     5000

// Thời gian xác định mất tín hiệu GPS (3.5 giây)
#define GPS_FIX_STALE_TIMEOUT_MS      3500

static portMUX_TYPE s_gps_state_lock = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t s_gps_task = NULL;
static SemaphoreHandle_t s_gps_mutex = NULL;
static SemaphoreHandle_t s_gps_stopped_sem = NULL;
static QueueHandle_t s_sensor_control_queue = NULL;
static TaskHandle_t s_sensor_control_task = NULL;
static bool s_gps_running = false;
static bool s_gps_uart_installed = false;
static watch_activity_mode_t s_activity_mode = WATCH_ACTIVITY_MODE_NONE;


static watch_gps_metrics_t s_metrics = {0};
static uint32_t s_last_rx_seen_ms = 0;

static double s_prev_lat = 0.0;
static double s_prev_lon = 0.0;
static bool s_has_prev_coords = false;
static uint32_t s_last_gps_update_ms = 0;
static watch_gps_track_point_t *s_track;
static size_t s_track_count = 0;
static size_t s_track_head = 0;
static float s_last_track_point_km = 0.0f;
static uint32_t s_fix_lost_at_ms = 0;
static uint8_t s_quality_fix_streak = 0;
static uint8_t s_moving_fix_streak = 0;
static uint8_t s_stationary_fix_streak = 0;
static bool s_doppler_moving = false;
static float s_speed_ema_kmh;
static uint32_t s_last_published_fix_ms;
static double s_smooth_lat = 0.0;
static double s_smooth_lon = 0.0;
static bool s_smooth_init = false;
static double s_last_track_lat = 0.0;
static double s_last_track_lon = 0.0;
static double s_prev_track_lat = 0.0;
static double s_prev_track_lon = 0.0;
static bool s_has_track_heading = false;
static double s_hold_lat = 0.0;
static double s_hold_lon = 0.0;
static bool s_hold_position = false;
static int s_current_pcas11_mode = -1;

static uint32_t s_baud_set_at_ms = 0;
static uint32_t s_last_valid_nmea_ms = 0;
static bool s_uart_idle_logged = false;
static bool s_logged_first_nmea = false;
static int s_gps_baud_index = 0;
static bool s_nmea_loss_logged = false;
static uint32_t s_autobaud_timeout_ms = 10000U;
static uint32_t s_checksum_error_count;
static uint32_t s_last_checksum_log_ms;
static uint32_t s_overlong_line_count;
static uint32_t s_last_overlong_log_ms;

static void gps_reset_distance_baseline_locked(void) {
    s_has_prev_coords = false;
    s_prev_lat = 0.0;
    s_prev_lon = 0.0;
    s_last_gps_update_ms = 0;
    s_fix_lost_at_ms = 0;
    s_quality_fix_streak = 0;
    s_moving_fix_streak = 0;
    s_stationary_fix_streak = 0;
    s_doppler_moving = false;
    s_speed_ema_kmh = 0.0f;
    s_last_published_fix_ms = 0;
    s_smooth_init = false;
    s_has_track_heading = false;
    s_hold_position = false;
}

static uint32_t watch_uptime_ms(void) {
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static bool gps_lock(const char *context) {
    if (!s_gps_mutex) return false;
    if (xSemaphoreTake(s_gps_mutex, pdMS_TO_TICKS(GPS_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        return true;
    }
    ESP_LOGW(TAG, "Mutex timeout tại: %s", context ? context : "unknown");
    return false;
}

static void gps_unlock(void) {
    if (s_gps_mutex) xSemaphoreGive(s_gps_mutex);
}

static double gps_haversine_m(double lat1, double lon1, double lat2, double lon2) {
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
    double phi1 = lat1 * M_PI / 180.0;
    double phi2 = lat2 * M_PI / 180.0;
    double dphi = (lat2 - lat1) * M_PI / 180.0;
    double dlambda = (lon2 - lon1) * M_PI / 180.0;
    double a = sin(dphi / 2.0) * sin(dphi / 2.0) +
               cos(phi1) * cos(phi2) * sin(dlambda / 2.0) * sin(dlambda / 2.0);
    if (a > 1.0) a = 1.0;
    return 6371000.0 * 2.0 * asin(sqrt(a));
}

static double gps_bearing_deg(double lat1, double lon1, double lat2, double lon2) {
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
    double phi1 = lat1 * M_PI / 180.0;
    double phi2 = lat2 * M_PI / 180.0;
    double dlambda = (lon2 - lon1) * M_PI / 180.0;
    double y = sin(dlambda) * cos(phi2);
    double x = cos(phi1) * sin(phi2) - sin(phi1) * cos(phi2) * cos(dlambda);
    double bearing = atan2(y, x) * 180.0 / M_PI;
    if (bearing < 0.0) bearing += 360.0;
    return bearing;
}

static void gps_update_smoothed_position(double raw_lat, double raw_lon, float hdop,
                                         double *out_lat, double *out_lon) {
    double alpha = GPS_SMOOTH_ALPHA_WEAK;
    if (hdop > 0.0f && hdop <= 1.5f) {
        alpha = 0.35;
    } else if (hdop > 0.0f && hdop <= 2.5f) {
        alpha = 0.25;
    }
    if (!s_smooth_init) {
        s_smooth_lat = raw_lat;
        s_smooth_lon = raw_lon;
        s_smooth_init = true;
    } else {
        s_smooth_lat += alpha * (raw_lat - s_smooth_lat);
        s_smooth_lon += alpha * (raw_lon - s_smooth_lon);
    }
    *out_lat = s_smooth_lat;
    *out_lon = s_smooth_lon;
}

static bool gps_track_point_is_plausible(double lat, double lon) {
    if (!s_has_track_heading) return true;

    double d = gps_haversine_m(s_last_track_lat, s_last_track_lon, lat, lon);
    if (d < 1.0) return false;

    const float speed = s_metrics.speed_kmh;
    const bool alley_moving = s_doppler_moving && speed >= GPS_TRACK_ALLEY_MIN_SPEED_KMH;
    const double spike_max = alley_moving ? GPS_TRACK_SPIKE_MOVING_M : GPS_TRACK_SPIKE_MAX_M;
    if (d > spike_max && speed < 12.0f) return false;

    double b1 = gps_bearing_deg(s_prev_track_lat, s_prev_track_lon, s_last_track_lat, s_last_track_lon);
    double b2 = gps_bearing_deg(s_last_track_lat, s_last_track_lon, lat, lon);
    double turn = fabs(b2 - b1);
    if (turn > 180.0) turn = 360.0 - turn;

    if (alley_moving) {
        /* Hẻm / rẽ thật: chỉ bỏ góc cực đoan trong đoạn rất ngắn. */
        if (turn > GPS_TRACK_ALLEY_TURN_DEG && d < 8.0) return false;
    } else if (turn > GPS_TRACK_MAX_TURN_DEG && d < GPS_TRACK_SPIKE_MAX_M) {
        return false;
    }
    return true;
}

static float gps_track_hdop_limit(void) {
    if (s_doppler_moving && s_metrics.speed_kmh >= GPS_TRACK_ALLEY_MIN_SPEED_KMH) {
        return GPS_MAX_TRACK_HDOP_MOVING;
    }
    return GPS_MAX_TRACK_HDOP;
}

static float gps_track_min_point_km(void) {
    if (s_doppler_moving && s_metrics.speed_kmh >= GPS_TRACK_ALLEY_MIN_SPEED_KMH) {
        return GPS_MIN_TRACK_POINT_MOVING_KM;
    }
    return GPS_MIN_TRACK_POINT_DISTANCE_KM;
}

static void gps_note_track_point(double lat, double lon) {
    if (s_has_track_heading) {
        s_prev_track_lat = s_last_track_lat;
        s_prev_track_lon = s_last_track_lon;
    }
    s_last_track_lat = lat;
    s_last_track_lon = lon;
    s_has_track_heading = true;
}

static void gps_append_track_point_locked(double lat, double lon, float distance_km, bool reconnect_segment) {
    if (!s_track || fabs(lat) <= 0.0001 || fabs(lon) <= 0.0001) return;

    if (s_metrics.hdop > 0.0f && s_metrics.hdop > gps_track_hdop_limit()) return;
    if (s_track_count > 0 && (distance_km - s_last_track_point_km) < gps_track_min_point_km()) return;
    if (!reconnect_segment && !gps_track_point_is_plausible(lat, lon)) return;

    watch_gps_track_point_t p = {
        .latitude_deg = lat,
        .longitude_deg = lon,
        .hdop = s_metrics.hdop,
        .satellites_in_use = (uint8_t)s_metrics.satellites_in_use,
        .reconnect_segment = reconnect_segment,
    };
    
    size_t write_index;
    if (s_track_count < WATCH_GPS_TRACK_MAX_POINTS) {
        write_index = (s_track_head + s_track_count) % WATCH_GPS_TRACK_MAX_POINTS;
        s_track_count++;
    } else {
        write_index = s_track_head;
        s_track_head = (s_track_head + 1U) % WATCH_GPS_TRACK_MAX_POINTS;
    }
    s_track[write_index] = p;
    s_last_track_point_km = distance_km;
    gps_note_track_point(lat, lon);
    
    esp_err_t capture_err = watch_activity_log_capture_point(
        p.latitude_deg, p.longitude_deg, p.hdop, p.satellites_in_use, p.reconnect_segment);
    if (capture_err != ESP_OK && capture_err != ESP_ERR_INVALID_STATE && capture_err != ESP_ERR_TIMEOUT) {
        ESP_LOGW(TAG, "Lỗi ghi nhận điểm: %s", esp_err_to_name(capture_err));
    }
}

static float gps_parse_gsv_average_cn0(const char *line) {
    if (!line || !strstr(line, "GSV")) return 0.0f;

    float sum = 0.0f;
    unsigned int count = 0;
    unsigned int field = 0;
    const char *start = line;
    for (const char *p = line; ; p++) {
        if (*p == ',' || *p == '*' || *p == '\0') {
            size_t len = (size_t)(p - start);
            if (field >= 7 && ((field - 7) % 4) == 0 && len > 0 && len < 4) {
                char token[4] = {0};
                memcpy(token, start, len);
                int cn0 = atoi(token);
                if (cn0 > 0 && cn0 <= 99) {
                    sum += (float)cn0;
                    count++;
                }
            }
            field++;
            start = p + 1;
        }
        if (*p == '*' || *p == '\0') break;
    }
    return count > 0 ? sum / (float)count : 0.0f;
}

static bool gps_is_running(void) {
    taskENTER_CRITICAL(&s_gps_state_lock);
    bool running = s_gps_running;
    taskEXIT_CRITICAL(&s_gps_state_lock);
    return running;
}

static void gps_set_running(bool running) {
    taskENTER_CRITICAL(&s_gps_state_lock);
    s_gps_running = running;
    taskEXIT_CRITICAL(&s_gps_state_lock);
}

static watch_activity_mode_t gps_get_activity_mode(void) {
    taskENTER_CRITICAL(&s_gps_state_lock);
    watch_activity_mode_t mode = s_activity_mode;
    taskEXIT_CRITICAL(&s_gps_state_lock);
    return mode;
}

static TaskHandle_t gps_get_task(void) {
    taskENTER_CRITICAL(&s_gps_state_lock);
    TaskHandle_t task = s_gps_task;
    taskEXIT_CRITICAL(&s_gps_state_lock);
    return task;
}

static void gps_set_task(TaskHandle_t task) {
    taskENTER_CRITICAL(&s_gps_state_lock);
    s_gps_task = task;
    taskEXIT_CRITICAL(&s_gps_state_lock);
}

static int gps_current_baud(void) {
    return s_gps_baud_candidates[s_gps_baud_index];
}

static void gps_reset_autobaud_state(void) {
    s_gps_baud_index = 0;
    s_logged_first_nmea = false;
    s_baud_set_at_ms = 0;
    s_last_valid_nmea_ms = 0;
    s_uart_idle_logged = false;
    s_nmea_loss_logged = false;
    s_last_rx_seen_ms = 0;
    s_autobaud_timeout_ms = 10000U;
    s_checksum_error_count = 0;
    s_last_checksum_log_ms = 0;
    s_overlong_line_count = 0;
    s_last_overlong_log_ms = 0;
}

static const char *gps_quality_label(const watch_gps_metrics_t *m, uint32_t age_ms) {
    if (!m || !m->uart_seen) return "NO_UART";
    if (age_ms > 3000) return "NO_SIGNAL";
    if (!m->nmea_seen) return "NMEA_ERR";
    if (!m->fix_valid) return "SEARCHING";
    if (m->satellites_in_use >= 8 && m->hdop > 0.0f && m->hdop <= 1.5f) return "EXCELLENT";
    if (m->satellites_in_use >= 5 && m->hdop > 0.0f && m->hdop <= 3.0f) return "GOOD";
    return "WEAK";
}

static void gps_switch_to_next_baud(void) {
    s_gps_baud_index = (s_gps_baud_index + 1) % (sizeof(s_gps_baud_candidates) / sizeof(s_gps_baud_candidates[0]));
    if (s_gps_baud_index == 0 && s_autobaud_timeout_ms < 60000U) {
        s_autobaud_timeout_ms *= 2U;
        if (s_autobaud_timeout_ms > 60000U) s_autobaud_timeout_ms = 60000U;
    }
    uart_set_baudrate(WATCH_GPS_UART_PORT, gps_current_baud());
    uart_flush_input(WATCH_GPS_UART_PORT);

    if (gps_lock("switch_baud")) {
        s_metrics.current_baud = gps_current_baud();
        gps_unlock();
    }

    s_logged_first_nmea = false;
    s_baud_set_at_ms = watch_uptime_ms();
    ESP_LOGW(TAG, "Baudrate GPS -> %d bps", gps_current_baud());
}

static bool gps_send_pcas(const char *command) {
    if (!command || command[0] != '$') return false;

    uint8_t checksum = 0;
    for (const char *p = command + 1; *p && *p != '*'; p++) {
        checksum ^= (uint8_t)*p;
    }

    char message[96];
    int length = snprintf(message, sizeof(message), "%s*%02X\r\n", command, checksum);
    if (length <= 0 || length >= (int)sizeof(message)) {
        return false;
    }

    int written = uart_write_bytes(WATCH_GPS_UART_PORT, message, length);
    if (written != length) return false;
    if (uart_wait_tx_done(WATCH_GPS_UART_PORT, pdMS_TO_TICKS(200)) != ESP_OK) return false;
    vTaskDelay(pdMS_TO_TICKS(100));
    return true;
}

static void gps_update_pcas11_mode(int mode) {
    if (s_current_pcas11_mode == mode) {
        return;
    }
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "$PCAS11,%d", mode);
    if (gps_send_pcas(cmd)) {
        s_current_pcas11_mode = mode;
        ESP_LOGI(TAG, "Đã cập nhật Dynamic Model sang %s thành công", cmd);
    } else {
        ESP_LOGW(TAG, "Lỗi gửi lệnh cấu hình Dynamic Model %s", cmd);
    }
}

static void gps_check_and_update_dynamic_model(bool moving) {
    int target_mode = 0;
    watch_activity_mode_t act_mode = gps_get_activity_mode();

    if (!moving) {
        target_mode = 1; // Stationary mode
    } else {
        if (act_mode == WATCH_ACTIVITY_MODE_WALKING) {
            target_mode = 2; // Pedestrian mode
        } else if (act_mode == WATCH_ACTIVITY_MODE_CYCLING) {
            target_mode = 3; // Automotive mode
        } else {
            target_mode = 0; // Portable mode
        }
    }

    gps_update_pcas11_mode(target_mode);
}

static void gps_configure_module(void) {
    bool mode_ok = gps_send_pcas("$PCAS04,3"); // GPS + BeiDou mode
    bool rate_ok = gps_send_pcas("$PCAS02,1000"); // 1Hz update rate

    s_current_pcas11_mode = -1; // Ép cấu hình lại

    // Cấu hình chế độ tĩnh động dựa trên môn thể thao hiện tại
    watch_activity_mode_t act_mode = gps_get_activity_mode();
    int initial_mode = 0; // Mặc định: Portable
    if (act_mode == WATCH_ACTIVITY_MODE_WALKING) {
        initial_mode = 2; // Pedestrian mode (tối ưu cho đi bộ, chạy bộ)
    } else if (act_mode == WATCH_ACTIVITY_MODE_CYCLING) {
        initial_mode = 3; // Automotive mode (tối ưu cho đạp xe, di chuyển nhanh)
    } else {
        initial_mode = 1; // Mặc định đứng yên khi chưa bắt đầu
    }

    char pcas11_cmd[32];
    snprintf(pcas11_cmd, sizeof(pcas11_cmd), "$PCAS11,%d", initial_mode);
    bool dynamic_ok = gps_send_pcas(pcas11_cmd);
    if (dynamic_ok) {
        s_current_pcas11_mode = initial_mode;
    }

    bool save_ok = gps_send_pcas("$PCAS00"); // Save config to flash memory
    ESP_LOGI(TAG, "Config module %s: Mode=%s, Rate=%s, DynamicCmd=%s (%s), Save=%s",
             WATCH_GPS_MODULE_NAME,
             mode_ok ? "OK" : "FAIL",
             rate_ok ? "OK" : "FAIL",
             pcas11_cmd,
             dynamic_ok ? "OK" : "FAIL",
             save_ok ? "OK" : "FAIL");
}

static void gps_parse_nmea_line(char *line) {
    if (!line || line[0] != '$') return;
    if (!gps_checksum_valid(line)) {
        uint32_t now_ms = watch_uptime_ms();
        s_checksum_error_count++;
        if (s_last_checksum_log_ms == 0 || (uint32_t)(now_ms - s_last_checksum_log_ms) >= 5000U) {
            ESP_LOGW(TAG, "Bỏ %lu dòng lỗi checksum. Dòng lỗi: %s", (unsigned long)s_checksum_error_count, line);
            s_checksum_error_count = 0;
            s_last_checksum_log_ms = now_ms;
        }
        return;
    }
    s_last_valid_nmea_ms = watch_uptime_ms();
    s_autobaud_timeout_ms = 10000U;
    s_nmea_loss_logged = false;

    char first_nmea[128] = {0};
    if (!s_logged_first_nmea) {
        strncpy(first_nmea, line, sizeof(first_nmea) - 1);
    }

    int gsv_sats_in_view = 0;
    float gsv_avg_cn0 = gps_parse_gsv_average_cn0(line);
    if (strstr(line, "GSV")) {
        char *p = strchr(line, ',');
        if (p) p = strchr(p + 1, ',');
        if (p) p = strchr(p + 1, ',');
        if (p && *(p + 1) != '\0') {
            gsv_sats_in_view = atoi(p + 1);
        }
    }

    gps_data_t parsed;
    bool has_parsed = gps_parse_line(line, &parsed);

    bool need_model_update = false;
    bool target_moving = false;

    if (gps_lock("parse_line")) {
        s_metrics.nmea_seen = true;
        s_metrics.nmea_lines++;
        if (gsv_sats_in_view > 0) {
            s_metrics.satellites_in_view = (uint32_t)gsv_sats_in_view;
        }
        if (gsv_avg_cn0 > 0.0f) {
            s_metrics.avg_cn0_dbhz = gsv_avg_cn0;
        }
        if (has_parsed) {
            if (parsed.has_gga) s_metrics.satellites_in_use = parsed.satellites;
            if (parsed.has_gga) {
                s_metrics.fix_quality = parsed.fix_quality;
                s_metrics.hdop = parsed.hdop;
                if (!parsed.fix_valid) {
                    s_metrics.fix_valid = false;
                    s_quality_fix_streak = 0;
                    if (s_has_prev_coords && s_fix_lost_at_ms == 0) {
                        s_fix_lost_at_ms = watch_uptime_ms();
                    }
                }
            } else if (parsed.pdop > 0.0f || parsed.hdop > 0.0f || parsed.vdop > 0.0f) {
                s_metrics.pdop = parsed.pdop;
                s_metrics.hdop = parsed.hdop;
                s_metrics.vdop = parsed.vdop;
            }
            if (parsed.has_rmc) {
                if (parsed.fix_valid) {
                    double curr_lat = parsed.latitude_deg;
                    double curr_lon = parsed.longitude_deg;
                    bool quality_ok = true;
                    if (s_metrics.satellites_in_use > 0 && s_metrics.satellites_in_use < GPS_MIN_SATELLITES) {
                        quality_ok = false;
                    }
                    if (s_metrics.hdop > 0.0f && s_metrics.hdop > GPS_MAX_ACCUMULATE_HDOP) {
                        quality_ok = false;
                    }

                    s_speed_ema_kmh = s_speed_ema_kmh <= 0.0f
                                          ? parsed.speed_kmh
                                          : s_speed_ema_kmh + 0.35f * (parsed.speed_kmh - s_speed_ema_kmh);
                    s_metrics.speed_kmh = s_speed_ema_kmh;
                    
                    if (parsed.speed_kmh >= GPS_STATIONARY_SPEED_KMH) {
                        s_stationary_fix_streak = 0;
                        if (s_moving_fix_streak < GPS_MOVING_CONFIRM_FIXES) s_moving_fix_streak++;
                        if (s_moving_fix_streak >= GPS_MOVING_CONFIRM_FIXES) s_doppler_moving = true;
                    } else {
                        s_moving_fix_streak = 0;
                        if (s_stationary_fix_streak < GPS_STATIONARY_CONFIRM_FIXES) s_stationary_fix_streak++;
                        if (s_stationary_fix_streak >= GPS_STATIONARY_CONFIRM_FIXES) s_doppler_moving = false;
                    }
                    
                    need_model_update = true;
                    target_moving = s_doppler_moving;
                    
                    if (quality_ok) {
                        if (s_quality_fix_streak < GPS_REACQUIRE_STABLE_FIXES) {
                            s_quality_fix_streak++;
                        }
                    } else {
                        if (s_quality_fix_streak > 0) {
                            s_quality_fix_streak--;
                        }
                    }
                    
                    if (s_metrics.fix_valid) {
                        if (!parsed.fix_valid || s_quality_fix_streak == 0) {
                            s_metrics.fix_valid = false;
                        }
                    } else {
                        s_metrics.fix_valid = quality_ok && (s_quality_fix_streak >= GPS_REACQUIRE_STABLE_FIXES);
                    }
                    if (!s_metrics.fix_valid && s_has_prev_coords && s_fix_lost_at_ms == 0) {
                        s_fix_lost_at_ms = watch_uptime_ms();
                    }

                    // Tính quãng đường và ghi nhận vết (Haversine)
                    if (s_metrics.fix_valid && fabs(curr_lat) > 0.0001 && fabs(curr_lon) > 0.0001) {
                        double smooth_lat = curr_lat;
                        double smooth_lon = curr_lon;
                        const bool moving = s_doppler_moving;

                        if (moving) {
                            s_hold_position = false;
                            gps_update_smoothed_position(curr_lat, curr_lon, s_metrics.hdop,
                                                         &smooth_lat, &smooth_lon);
                            s_hold_lat = smooth_lat;
                            s_hold_lon = smooth_lon;
                        } else if (s_hold_position) {
                            /* Đứng yên: giữ tọa độ đã khóa — đồng hồ và BLE cùng một điểm, không nhảy. */
                            smooth_lat = s_hold_lat;
                            smooth_lon = s_hold_lon;
                        } else {
                            gps_update_smoothed_position(curr_lat, curr_lon, s_metrics.hdop,
                                                         &smooth_lat, &smooth_lon);
                            s_hold_lat = smooth_lat;
                            s_hold_lon = smooth_lon;
                            s_hold_position = true;
                        }

                        uint32_t ms = watch_uptime_ms();
                        bool publish_fix = false;
                        if (!s_has_prev_coords) {
                            s_prev_lat = smooth_lat;
                            s_prev_lon = smooth_lon;
                            s_has_prev_coords = true;
                            s_last_gps_update_ms = ms;
                            gps_append_track_point_locked(smooth_lat, smooth_lon, s_metrics.total_distance_km, false);
                            publish_fix = true;
                        } else {
                            #ifndef M_PI
                            #define M_PI 3.14159265358979323846
                            #endif
                            
                            double phi1 = s_prev_lat * M_PI / 180.0;
                            double phi2 = curr_lat * M_PI / 180.0;
                            double delta_phi = (curr_lat - s_prev_lat) * M_PI / 180.0;
                            double delta_lambda = (curr_lon - s_prev_lon) * M_PI / 180.0;

                            double a = sin(delta_phi / 2.0) * sin(delta_phi / 2.0) +
                                       cos(phi1) * cos(phi2) *
                                       sin(delta_lambda / 2.0) * sin(delta_lambda / 2.0);
                            if (a > 1.0) a = 1.0;
                            double c = 2.0 * asin(sqrt(a));
                            double d_i = 6371000.0 * c; // mét

                            double dt = (double)(ms - s_last_gps_update_ms) / 1000.0;
                            if (dt <= 0.0) dt = 1.0;
                            double v_eff = d_i / dt;

                            bool reconnecting = s_fix_lost_at_ms != 0;
                            uint32_t gap_ms = reconnecting ? (uint32_t)(ms - s_fix_lost_at_ms) : 0;
                            watch_activity_mode_t activity_mode = gps_get_activity_mode();
                            double max_reconnect_speed_mps =
                                activity_mode == WATCH_ACTIVITY_MODE_CYCLING
                                    ? GPS_CYCLE_REACQUIRE_MAX_SPEED_MPS
                                    : GPS_WALK_REACQUIRE_MAX_SPEED_MPS;
                            uint32_t max_gap_ms =
                                activity_mode == WATCH_ACTIVITY_MODE_CYCLING
                                    ? GPS_CYCLE_REACQUIRE_MAX_GAP_MS
                                    : GPS_WALK_REACQUIRE_MAX_GAP_MS;
                            double hard_max_distance_m =
                                activity_mode == WATCH_ACTIVITY_MODE_CYCLING
                                    ? GPS_CYCLE_REACQUIRE_MAX_DISTANCE_M
                                    : GPS_WALK_REACQUIRE_MAX_DISTANCE_M;
                            double max_distance_m = ((double)gap_ms / 1000.0) * max_reconnect_speed_mps + GPS_REACQUIRE_TOLERANCE_M;
                            if (max_distance_m > hard_max_distance_m) {
                                max_distance_m = hard_max_distance_m;
                            }
                            
                            bool compensation_ok = !reconnecting || (gap_ms <= max_gap_ms && d_i <= max_distance_m);
                            double min_seg = GPS_MIN_SEGMENT_DISTANCE_M;
                            if (s_metrics.hdop > 1.0f) min_seg *= (double)s_metrics.hdop;
                            if (s_metrics.avg_cn0_dbhz > 0.0f && s_metrics.avg_cn0_dbhz < 28.0f) {
                                min_seg *= 1.5;
                            }
                            
                            bool moving_seg = s_doppler_moving;
                            double segment_distance_m = d_i;
                            if (!reconnecting && moving_seg) {
                                double doppler_distance_m = ((double)parsed.speed_kmh / 3.6) * dt;
                                double position_weight = s_metrics.hdop <= 1.5f ? 0.70 :
                                                         s_metrics.hdop <= 3.0f ? 0.40 : 0.20;
                                segment_distance_m = d_i * position_weight + doppler_distance_m * (1.0 - position_weight);
                            }

                            if (compensation_ok && moving_seg && d_i >= min_seg && v_eff <= GPS_MAX_SEGMENT_SPEED_MPS) {
                                s_metrics.total_distance_km += (float)(segment_distance_m / 1000.0);
                                gps_append_track_point_locked(smooth_lat, smooth_lon, s_metrics.total_distance_km, reconnecting);
                                s_prev_lat = smooth_lat;
                                s_prev_lon = smooth_lon;
                                s_last_gps_update_ms = ms;
                                publish_fix = true;
                            } else if (reconnecting) {
                                if (!compensation_ok || v_eff > GPS_MAX_SEGMENT_SPEED_MPS) {
                                     ESP_LOGW(TAG, "Bỏ nối GPS: mất %lu ms, %.1f m > giới hạn %.1f m",
                                              (unsigned long)gap_ms, d_i, max_distance_m);
                                }
                                s_prev_lat = smooth_lat;
                                s_prev_lon = smooth_lon;
                                s_last_gps_update_ms = ms;
                                publish_fix = compensation_ok && v_eff <= GPS_MAX_SEGMENT_SPEED_MPS;
                            } else if (v_eff > GPS_MAX_SEGMENT_SPEED_MPS) {
                                s_prev_lat = smooth_lat;
                                s_prev_lon = smooth_lon;
                                s_last_gps_update_ms = ms;
                            } else {
                                publish_fix = true;
                            }
                        }
                        if (publish_fix) {
                            s_metrics.latitude_deg = smooth_lat;
                            s_metrics.longitude_deg = smooth_lon;
                            s_last_published_fix_ms = ms;
                            s_fix_lost_at_ms = 0;
                        } else {
                            s_metrics.fix_valid = false;
                            if (s_fix_lost_at_ms == 0) s_fix_lost_at_ms = ms;
                        }
                    }
                } else {
                    s_metrics.fix_valid = false;
                    s_quality_fix_streak = 0;
                    if (s_has_prev_coords && s_fix_lost_at_ms == 0) {
                        s_fix_lost_at_ms = watch_uptime_ms();
                    }
                }
            }
        }
        gps_unlock();
    }

    if (!s_logged_first_nmea) {
        ESP_LOGI(TAG, "First NMEA: %s", first_nmea);
        s_logged_first_nmea = true;
        gps_configure_module();
    } else if (need_model_update) {
        gps_check_and_update_dynamic_model(target_moving);
    }
}

static void gps_uart_reader_task(void *arg) {
    (void)arg;
    uint8_t buf[128];
    char line[256];
    size_t line_len = 0;
    bool discard_line = false;
    uint32_t last_log = 0;

    while (gps_is_running()) {
        const int n = uart_read_bytes(WATCH_GPS_UART_PORT, buf, sizeof(buf), pdMS_TO_TICKS(200));
        uint32_t ms = watch_uptime_ms();

        if (s_baud_set_at_ms == 0) s_baud_set_at_ms = ms;
        
        if (s_last_published_fix_ms != 0 && (uint32_t)(ms - s_last_published_fix_ms) > GPS_FIX_STALE_TIMEOUT_MS && gps_lock("stale_fix")) {
            if (s_metrics.fix_valid) {
                s_metrics.fix_valid = false;
                s_metrics.speed_kmh = 0.0f;
                s_quality_fix_streak = 0;
                if (s_has_prev_coords && s_fix_lost_at_ms == 0) {
                    s_fix_lost_at_ms = s_last_published_fix_ms;
                }
            }
            gps_unlock();
        }
        
        if (s_logged_first_nmea && s_last_valid_nmea_ms != 0 && (uint32_t)(ms - s_last_valid_nmea_ms) >= 10000) {
            if (!s_nmea_loss_logged) {
                ESP_LOGW(TAG, "Mất NMEA hợp lệ; giữ nguyên baud %d", gps_current_baud());
                s_nmea_loss_logged = true;
                if (gps_lock("nmea_loss")) {
                    s_metrics.fix_valid = false;
                    s_quality_fix_streak = 0;
                    if (s_has_prev_coords && s_fix_lost_at_ms == 0) s_fix_lost_at_ms = ms;
                    gps_unlock();
                }
            }
        }
        
        if (!s_logged_first_nmea && (uint32_t)(ms - s_baud_set_at_ms) >= s_autobaud_timeout_ms) {
            gps_switch_to_next_baud();
            line_len = 0;
            discard_line = false;
            continue;
        }

        if (ms - last_log >= GPS_HEARTBEAT_INTERVAL_MS) {
            if (gps_lock("log_status")) {
                const uint32_t age_ms = s_metrics.last_rx_ms ? (uint32_t)(ms - s_metrics.last_rx_ms) : UINT32_MAX;
                ESP_LOGI(TAG, "Status: %s | Lock: %s | Sats: %u/%u | HDOP: %.1f | Dist: %.3f km | Age: %lu ms | Baud: %d",
                         gps_quality_label(&s_metrics, age_ms),
                         s_metrics.fix_valid ? "LOCKED" : "SEARCHING",
                         s_metrics.satellites_in_use,
                         s_metrics.satellites_in_view,
                         (double)s_metrics.hdop,
                         (double)s_metrics.total_distance_km,
                         (unsigned long)age_ms,
                         gps_current_baud());
                gps_unlock();
            }
            last_log = ms;
        }

        if (n <= 0) {
            if (s_last_rx_seen_ms != 0 && (ms - s_last_rx_seen_ms) >= 5000 && !s_uart_idle_logged) {
                ESP_LOGW(TAG, "UART im lặng quá 5 giây...");
                s_uart_idle_logged = true;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        s_last_rx_seen_ms = ms;
        s_uart_idle_logged = false;
        if (gps_lock("raw_bytes")) {
            s_metrics.raw_bytes += n;
            s_metrics.uart_seen = true;
            s_metrics.last_rx_ms = ms;
            gps_unlock();
        }

        for (int i = 0; i < n; i++) {
            char c = (char)buf[i];
            if (c == '\r') continue;
            if (c == '\n') {
                if (!discard_line) {
                    line[line_len] = '\0';
                    if (line_len > 5) gps_parse_nmea_line(line);
                }
                line_len = 0;
                discard_line = false;
            } else if (discard_line) {
                continue;
            } else if (line_len < sizeof(line) - 1) {
                line[line_len++] = c;
            } else {
                s_overlong_line_count++;
                if (s_last_overlong_log_ms == 0 || (uint32_t)(ms - s_last_overlong_log_ms) >= 5000U) {
                    if (line_len > 40) line[40] = '\0';
                    else line[line_len] = '\0';
                    ESP_LOGW(TAG, "Bỏ %lu dòng NMEA quá dài. Đoạn đầu: %s", (unsigned long)s_overlong_line_count, line);
                    s_overlong_line_count = 0;
                    s_last_overlong_log_ms = ms;
                }
                line_len = 0;
                discard_line = true;
            }
        }
    }
    
    gps_set_task(NULL);
    if (s_gps_stopped_sem) xSemaphoreGive(s_gps_stopped_sem);
    vTaskDelete(NULL);
}

static void gps_delete_uart_driver(void) {
    if (!s_gps_uart_installed) return;
    esp_err_t err = uart_driver_delete(WATCH_GPS_UART_PORT);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Lỗi gỡ driver UART: %s", esp_err_to_name(err));
        return;
    }
    s_gps_uart_installed = false;
}

static void gps_reset_release(void) {
#if WATCH_PIN_GPS_RST >= 0
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << WATCH_PIN_GPS_RST,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    gpio_set_level(WATCH_PIN_GPS_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(WATCH_PIN_GPS_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
#else
    vTaskDelay(pdMS_TO_TICKS(100));
#endif
}

void watch_gps_start(void) {
    if (gps_is_running()) return;
    if (gps_get_task()) {
        if (!s_gps_stopped_sem || xSemaphoreTake(s_gps_stopped_sem, pdMS_TO_TICKS(1500)) != pdTRUE) {
            ESP_LOGW(TAG, "Bỏ qua yêu cầu start do luồng cũ đang tắt!");
            return;
        }
    }
    
    if (!s_gps_mutex) s_gps_mutex = xSemaphoreCreateMutex();
    if (s_gps_mutex && !s_track) {
        s_track = heap_caps_calloc(WATCH_GPS_TRACK_MAX_POINTS, sizeof(*s_track), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_track) {
            ESP_LOGE(TAG, "Lỗi cấp phát track buffer trên PSRAM");
            return;
        }
    }
    if (!s_gps_mutex) return;
    
    if (!s_gps_stopped_sem) s_gps_stopped_sem = xSemaphoreCreateBinary();
    if (!s_gps_stopped_sem) return;
    while (xSemaphoreTake(s_gps_stopped_sem, 0) == pdTRUE) {}

    gps_reset_autobaud_state();
    
    if (gps_lock("start_init")) {
        s_metrics.uart_seen = false;
        s_metrics.nmea_seen = false;
        s_metrics.fix_valid = false;
        s_metrics.last_rx_ms = 0;
        s_metrics.fix_age_ms = UINT32_MAX;
        s_metrics.raw_bytes = 0;
        s_metrics.nmea_lines = 0;
        s_metrics.satellites_in_use = 0;
        s_metrics.satellites_in_view = 0;
        s_metrics.avg_cn0_dbhz = 0.0f;
        s_metrics.current_baud = gps_current_baud();
        gps_reset_distance_baseline_locked();
        gps_unlock();
    }

    gps_reset_release();

#if defined(WATCH_PIN_GPS_PPS) && WATCH_PIN_GPS_PPS >= 0
    gpio_config_t pps_cfg = {
        .pin_bit_mask = 1ULL << WATCH_PIN_GPS_PPS,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&pps_cfg);
#endif
  
    uart_config_t cfg = {
        .baud_rate = gps_current_baud(),
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_XTAL,
    };

    esp_err_t err = uart_driver_install(WATCH_GPS_UART_PORT, GPS_UART_RX_BUFFER_SIZE, 0, 0, NULL, 0);
    if (err == ESP_ERR_INVALID_STATE) {
        s_gps_uart_installed = true;
    } else if (err != ESP_OK) {
        return;
    } else {
        s_gps_uart_installed = true;
    }

    if (uart_param_config(WATCH_GPS_UART_PORT, &cfg) != ESP_OK ||
        uart_set_pin(WATCH_GPS_UART_PORT, WATCH_PIN_GPS_UART_TX, WATCH_PIN_GPS_UART_RX, -1, -1) != ESP_OK) {
        gps_delete_uart_driver();
        return;
    }

    // Thiết lập pull-up phần cứng để làm sạch tín hiệu và chống nhiễu
    gpio_pullup_en(WATCH_PIN_GPS_UART_RX);
    gpio_pullup_en(WATCH_PIN_GPS_UART_TX);

    s_baud_set_at_ms = watch_uptime_ms();
    vTaskDelay(pdMS_TO_TICKS(200));

    // Gửi chuỗi ký tự đánh thức module GPS ra khỏi chế độ Standby
    ESP_LOGI(TAG, "Gửi tín hiệu kích hoạt đánh thức module GPS...");
    const char *wakeup_cmd = "\r\n";
    uart_write_bytes(WATCH_GPS_UART_PORT, wakeup_cmd, strlen(wakeup_cmd));
    (void)uart_wait_tx_done(WATCH_GPS_UART_PORT, pdMS_TO_TICKS(100));
    vTaskDelay(pdMS_TO_TICKS(50));

    // gps_configure_module();
  
    gps_set_running(true);
    TaskHandle_t task_handle = NULL;
    
    BaseType_t task_ok = xTaskCreatePinnedToCore(gps_uart_reader_task, "gps_task", 6144, NULL, 3, &task_handle, 0);
    if (task_ok != pdPASS) {
        gps_set_running(false);
        gps_delete_uart_driver();
        return;
    }
    gps_set_task(task_handle);
}

void watch_gps_stop(void) {
    watch_gps_set_activity_mode(WATCH_ACTIVITY_MODE_NONE);

    // Gửi lệnh Standby cho module GPS để giảm tiêu thụ điện trước khi ngắt UART
    if (gps_is_running()) {
        ESP_LOGI(TAG, "Gửi lệnh Standby tới module GPS...");
        (void)gps_send_pcas("$PCAS03,0");
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    gps_set_running(false);

    if (gps_get_task() && (!s_gps_stopped_sem || xSemaphoreTake(s_gps_stopped_sem, pdMS_TO_TICKS(1500)) != pdTRUE)) {
        ESP_LOGW(TAG, "Timeout đợi dừng task GPS; giữ nguyên driver.");
        return;
    }

    // Đặt chân TX của ESP32 (RX GPS) lên mức cao để tránh làm GPS reset mềm khi gỡ driver UART
    gpio_set_direction(WATCH_PIN_GPS_UART_TX, GPIO_MODE_OUTPUT);
    gpio_set_level(WATCH_PIN_GPS_UART_TX, 1);
    gpio_pullup_en(WATCH_PIN_GPS_UART_TX);
    
    gpio_set_direction(WATCH_PIN_GPS_UART_RX, GPIO_MODE_INPUT);
    gpio_pullup_en(WATCH_PIN_GPS_UART_RX);

    gps_delete_uart_driver();
}

typedef enum {
    GPS_CMD_START,
    GPS_CMD_STOP,
} gps_control_cmd_t;

static void watch_gps_control_task(void *arg) {
    (void)arg;
    gps_control_cmd_t cmd;
    for (;;) {
        if (xQueueReceive(s_sensor_control_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            if (cmd == GPS_CMD_START) {
                watch_gps_start();
            } else {
                watch_gps_stop();
            }
        }
    }
}

esp_err_t watch_gps_control_init(void) {
    if (s_sensor_control_queue && s_sensor_control_task) return ESP_OK;

    s_sensor_control_queue = xQueueCreate(1, sizeof(gps_control_cmd_t));
    if (!s_sensor_control_queue) return ESP_ERR_NO_MEM;

    if (xTaskCreatePinnedToCore(watch_gps_control_task, "gps_ctrl", 3072, NULL, 2, &s_sensor_control_task, 0) != pdPASS) {
        vQueueDelete(s_sensor_control_queue);
        s_sensor_control_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static void watch_gps_request_control(gps_control_cmd_t cmd) {
    if (!s_sensor_control_queue) return;
    xQueueOverwrite(s_sensor_control_queue, &cmd);
}

void watch_gps_request_start(void) {
    watch_gps_request_control(GPS_CMD_START);
}

void watch_gps_request_stop(void) {
    watch_gps_set_activity_mode(WATCH_ACTIVITY_MODE_NONE);
    watch_gps_request_control(GPS_CMD_STOP);
}

bool watch_gps_get_metrics(watch_gps_metrics_t *out) {
    if (!out || !s_gps_mutex) return false;
    if (!gps_lock("get_metrics")) return false;
    *out = s_metrics;
    uint32_t now_ms = watch_uptime_ms();
    out->fix_age_ms = s_last_published_fix_ms != 0 ? (uint32_t)(now_ms - s_last_published_fix_ms) : UINT32_MAX;
    if (out->fix_age_ms > GPS_FIX_STALE_TIMEOUT_MS) {
        out->fix_valid = false;
        out->speed_kmh = 0.0f;
    }
    gps_unlock();
    return true;
}

void watch_gps_reset_track(void) {
    if (!s_gps_mutex) return;
    if (!gps_lock("reset_track")) return;
    s_metrics.total_distance_km = 0.0f;
    s_track_count = 0;
    s_track_head = 0;
    s_last_track_point_km = 0.0f;
    s_smooth_init = false;
    s_has_track_heading = false;
    s_hold_position = false;
    gps_reset_distance_baseline_locked();
    gps_unlock();
}

bool watch_gps_is_active(void) {
    return gps_is_running();
}

void watch_gps_set_activity_mode(watch_activity_mode_t mode) {
    taskENTER_CRITICAL(&s_gps_state_lock);
    s_activity_mode = mode;
    taskEXIT_CRITICAL(&s_gps_state_lock);
}

size_t watch_gps_get_track(watch_gps_track_point_t *out, size_t max_points) {
    if (!out || max_points == 0 || !s_track || !s_gps_mutex) return 0;
    if (!gps_lock("get_track")) return 0;
    size_t n = s_track_count < max_points ? s_track_count : max_points;
    for (size_t i = 0; i < n; i++) {
        out[i] = s_track[(s_track_head + i) % WATCH_GPS_TRACK_MAX_POINTS];
    }
    gps_unlock();
    return n;
}

int watch_gps_format_track(char *out, size_t out_size) {
    if (!out || out_size == 0 || !s_track || !s_gps_mutex) return 0;
    out[0] = '\0';
    if (!gps_lock("format_track")) return 0;

    size_t used = 0;
    for (size_t i = 0; i < s_track_count; i++) {
        const watch_gps_track_point_t *point = &s_track[(s_track_head + i) % WATCH_GPS_TRACK_MAX_POINTS];
        int written = snprintf(out + used, out_size - used, "%s%.6f,%.6f,%.1f,%u,%u",
                               i == 0 ? "" : ";",
                               point->latitude_deg,
                               point->longitude_deg,
                               (double)point->hdop,
                               (unsigned int)point->satellites_in_use,
                               point->reconnect_segment ? 1U : 0U);
        if (written < 0) break;
        if ((size_t)written >= out_size - used) {
            used = out_size - 1;
            out[used] = '\0';
            break;
        }
        used += (size_t)written;
    }
    gps_unlock();
    return (int)used;
}
