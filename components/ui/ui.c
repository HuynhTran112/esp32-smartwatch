/* Định nghĩa điểm vào chính (Main Entry Point) và khởi tạo luồng giao diện người dùng (GUI)
   File này quản lý vòng đời khởi tạo giao diện của đồng hồ thông minh sử dụng thư viện
   đồ họa LVGL (Light and Versatile Graphics Library). Giao diện được bắt đầu bằng cách khởi tạo
   hệ thống quản lý ngăn xếp màn hình (Screen Stack Manager), thiết lập thanh trạng thái cố định
   (Status Bar) ở trên cùng của giao diện và kích hoạt màn hình mặt đồng hồ chính (Watchface). */

#include "ui.h"
#include "ui_navigation.h"
#include "ui_screens.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "UI";
static TaskHandle_t s_sleep_task;
static portMUX_TYPE s_sleep_lock = portMUX_INITIALIZER_UNLOCKED;
static volatile bool g_sleep_requested;
static volatile bool g_sleep_in_progress;
static volatile uint32_t s_ui_heartbeat_ms;

void ui_runtime_mark_alive(void)
{
    taskENTER_CRITICAL(&s_sleep_lock);
    s_ui_heartbeat_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    taskEXIT_CRITICAL(&s_sleep_lock);
}

uint32_t ui_runtime_last_heartbeat_ms(void)
{
    taskENTER_CRITICAL(&s_sleep_lock);
    uint32_t heartbeat_ms = s_ui_heartbeat_ms;
    taskEXIT_CRITICAL(&s_sleep_lock);
    return heartbeat_ms;
}

static void ui_sleep_task(void *arg)
{
    (void)arg;

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        taskENTER_CRITICAL(&s_sleep_lock);
        bool requested = g_sleep_requested;
        taskEXIT_CRITICAL(&s_sleep_lock);
        if (!requested) {
            continue;
        }

        /* Stop LVGL callbacks while locked, then sleep only after releasing the lock. */
        if (lvgl_port_lock(-1)) {
            lv_timer_enable(false);
            lvgl_port_unlock();
        }

        ESP_LOGI(TAG, "Processing deferred deep-sleep request");
        ui_navigation_enter_deep_sleep();
    }
}

/* Khởi tạo hệ thống giao diện đồ họa người dùng của Smartwatch
   Hàm này thực hiện các bước:
   1. Khởi tạo cấu trúc quản lý định tuyến màn hình (ui_navigation_init) để theo dõi các màn hình đang mở.
   2. Tạo thanh Status Bar chung hiển thị trên cùng để theo dõi Pin, kết nối BLE, Wi-Fi và giờ hệ thống.
   3. Tạo đối tượng màn hình Watchface và đẩy vào ngăn xếp làm màn hình gốc (Root Screen). */
void ui_init(void)
{
    ui_runtime_mark_alive();
    if (!s_sleep_task) {
        // Tạo task hoãn ngủ sâu (Deferred Deep Sleep) với stack 4096 ở mức ưu tiên 5 trên Core 1.
        BaseType_t ok = xTaskCreatePinnedToCore(ui_sleep_task, "ui_sleep", 4096, NULL, 5,
                                               &s_sleep_task, 1);
        if (ok != pdPASS) {
            ESP_LOGE(TAG, "Cannot create deferred deep-sleep task");
        } else {
            taskENTER_CRITICAL(&s_sleep_lock);
            bool pending_sleep = g_sleep_requested;
            taskEXIT_CRITICAL(&s_sleep_lock);
            if (pending_sleep) {
                xTaskNotifyGive(s_sleep_task);
            }
        }
    }

    /* Bước 1: Khởi tạo cơ chế điều hướng và quản lý ngăn xếp màn hình (Navigation Stack) */
    ui_navigation_init();

    /* Bước 2: Tạo thanh trạng thái (Status Bar) hiển thị toàn cục trên lớp trên cùng (Top Layer) */
    ui_statusbar_create();

    /* Bước 3: Khởi động bộ kiểm tra báo thức nền, không phụ thuộc vào màn hình Alarm */
    ui_alarm_engine_start();

    /* Bước 4: Khởi tạo và tải màn hình mặt đồng hồ chính (Watchface Screen) làm màn hình gốc */
    lv_obj_t *wf = ui_watchface_screen_create();
    ui_navigation_push(wf);
}

/* Gửi yêu cầu chuyển tiếp sang chế độ ngủ sâu (Deep Sleep) tắt màn hình
   Hàm này gọi API từ phân lớp điều hướng để lưu lại trạng thái màn hình cuối cùng
   trước khi tắt nguồn màn hình và cấu hình các nguồn đánh thức (nút vật lý / ngắt chạm). */
void ui_request_deep_sleep(void)
{
    taskENTER_CRITICAL(&s_sleep_lock);
    bool notify = !g_sleep_in_progress;
    g_sleep_requested = true;
    g_sleep_in_progress = true;
    TaskHandle_t sleep_task = s_sleep_task;
    taskEXIT_CRITICAL(&s_sleep_lock);

    if (sleep_task && notify) {
        xTaskNotifyGive(sleep_task);
    } else if (!sleep_task) {
        ESP_LOGE(TAG, "Deep-sleep requested before sleep task was ready");
    }
}

bool ui_is_sleep_in_progress(void)
{
    taskENTER_CRITICAL(&s_sleep_lock);
    bool in_progress = g_sleep_in_progress;
    taskEXIT_CRITICAL(&s_sleep_lock);
    return in_progress;
}
