/* Activity history metadata in NVS with route files stored in SPIFFS. */

#ifndef WATCH_ACTIVITY_LOG_H
#define WATCH_ACTIVITY_LOG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#define WATCH_ACTIVITY_LOG_MAX        3
#define WATCH_ACTIVITY_ROUTE_MAX      (768U * 1024U)
#define WATCH_ACTIVITY_ROUTE_PATH_MAX 48

typedef struct {
    uint32_t timestamp;
    uint8_t sport_id;
    uint32_t duration_sec;
    uint32_t steps;
    float distance_km;
    float avg_speed_kmh;
    char route_path[WATCH_ACTIVITY_ROUTE_PATH_MAX];
} watch_activity_record_t;

typedef esp_err_t (*watch_activity_route_chunk_cb_t)(const char *data, size_t len, void *ctx);

esp_err_t watch_activity_log_init(void);

/* Start a route capture file for the current workout. */
esp_err_t watch_activity_log_capture_start(void);

/* Append one GPS point to the current route capture.
   The extended route format is:
   latitude,longitude,hdop,satellites,reconnect */
esp_err_t watch_activity_log_capture_point(double latitude_deg,
                                           double longitude_deg,
                                           float hdop,
                                           uint8_t satellites,
                                           bool reconnect);

/* Promote the current capture file into activity history. */
esp_err_t watch_activity_log_capture_finish(const watch_activity_record_t *record);

/* Delete an unfinished current route capture. */
void watch_activity_log_capture_discard(void);

/* Save metadata to NVS and the route payload to a separate SPIFFS file. */
esp_err_t watch_activity_log_add(const watch_activity_record_t *record, const char *route);

size_t watch_activity_log_count(void);
size_t watch_activity_log_get(watch_activity_record_t *out, size_t max_records);

/* Stream a stable snapshot of the route currently being captured. */
esp_err_t watch_activity_log_stream_capture(watch_activity_route_chunk_cb_t cb, void *ctx);

/* Stream a historical route in chunks without loading the full file into RAM. */
esp_err_t watch_activity_log_stream_route(size_t index,
                                          watch_activity_route_chunk_cb_t cb,
                                          void *ctx);

#endif
