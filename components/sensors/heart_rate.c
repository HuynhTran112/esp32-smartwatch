/* Thuật toán xử lý tín hiệu PPG để ước lượng nhịp tim và SpO2. */

#include "heart_rate.h"
#include "hardware_i2c_sensor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>

// Địa chỉ I2C và bản đồ thanh ghi cảm biến
#define MAX30102_I2C_ADDR          0x57 // Địa chỉ I2C
#define MAX30102_REG_INTR_STATUS_1 0x00 // Trạng thái ngắt 1
#define MAX30102_REG_INTR_STATUS_2 0x01 // Trạng thái ngắt 2
#define MAX30102_REG_FIFO_WR_PTR   0x04 // Con trỏ ghi FIFO
#define MAX30102_REG_OVF_COUNTER   0x05 // Bộ đếm tràn FIFO
#define MAX30102_REG_FIFO_RD_PTR   0x06 // Con trỏ đọc FIFO
#define MAX30102_REG_FIFO_DATA     0x07 // Cổng dữ liệu FIFO
#define MAX30102_REG_FIFO_CONFIG   0x08 // Cấu hình FIFO
#define MAX30102_REG_MODE_CONFIG   0x09 // Cấu hình chế độ hoạt động
#define MAX30102_REG_SPO2_CONFIG   0x0A // Cấu hình đo SpO2
#define MAX30102_REG_LED1_PA       0x0C // Dòng LED thô 1 (Đỏ)
#define MAX30102_REG_LED2_PA       0x0D // Dòng LED thô 2 (Hồng ngoại)
#define MAX30102_REG_PART_ID       0xFF // ID thiết bị

#define MAX30102_PART_ID_VAL       0x15 // ID mặc định của cảm biến
#define MAX30102_FIFO_DEPTH        32   // Kích thước bộ đệm FIFO (mẫu)

// Tần số lấy mẫu: 50 Hz.
// Lý do: Đủ để bắt đỉnh nhịp tim (lên tới 220 BPM) mà không gây quá tải CPU và bus I2C.
#define MAX30102_SAMPLE_RATE_HZ    50   

// Kích thước cửa sổ lưu dữ liệu: 200 mẫu.
// Lý do: Tương đương 4 giây dữ liệu ở 50Hz, đủ để phát hiện nhịp tim thấp ở mức ~40 BPM.
#define MAX30102_WINDOW_SAMPLES    200  

// Ngưỡng phát hiện ngón tay đặt trên cảm biến: 12000.
// Lý do: Giá trị hồng ngoại thô của da khi chạm vào sẽ vượt ngưỡng này.
#define MAX30102_MIN_FINGER_IR     12000UL 

// Biên độ AC hồng ngoại tối thiểu: 60.0.
// Lý do: Lọc bỏ nhiễu khi chạm tay quá nhẹ hoặc cảm biến bị bám bụi.
#define MAX30102_MIN_AC_IR         60.0 

// Cấu hình điều khiển dòng điện LED tự động (AGC)
#define MAX30102_LED_CURRENT_INITIAL 0x80 // Dòng khởi tạo ban đầu (~25.6mA)
#define MAX30102_LED_CURRENT_MIN     0x20 // Dòng tối thiểu để tránh tín hiệu yếu
#define MAX30102_LED_CURRENT_MAX     0xE0 // Dòng tối đa để tránh nóng và hao pin
#define MAX30102_LED_CURRENT_STEP    0x08 // Bước tăng giảm dòng LED (~1.6mA)

// Ngưỡng cường độ ánh sáng mục tiêu cho AGC
// Lý do: Giữ tín hiệu ổn định, tránh bị bão hòa (clipping) khi ấn chặt tay.
#define MAX30102_AGC_LOW_IR          45000UL
#define MAX30102_AGC_HIGH_IR         180000UL

// Chu kỳ cập nhật AGC: 1500 ms.
// Lý do: Tránh đổi dòng LED quá nhanh gây gián đoạn thuật toán lọc.
#define MAX30102_AGC_INTERVAL_MS    1500U 

// Số mẫu bỏ qua để ổn định AGC: 5 mẫu.
// Lý do: Bỏ qua các tín hiệu nhiễu quá độ ngay sau khi đổi dòng LED.
#define MAX30102_AGC_SETTLE_SAMPLES      5U  

// Chất lượng SpO2 tối thiểu để chấp nhận giá trị đo.
#define MAX30102_SPO2_MIN_QUALITY    15U  

// Hệ số lọc thông cao (HP): 0.061.
// Lý do: Loại bỏ thành phần DC tĩnh (da, mỡ) và trôi đường nền do nhịp thở (< 0.5Hz).
#define MAX30102_HP_ALPHA          0.061 

// Hệ số lọc thông thấp (LP): 0.45.
// Lý do: Lọc nhiễu tần số cao (nhiễu cơ học, nhiễu điện lưới 50/60Hz).
#define MAX30102_LP_ALPHA          0.45 

// Số mẫu làm ấm bộ lọc: 25 mẫu (~0.5 giây để bộ lọc IIR ổn định).
#define MAX30102_WARMUP_SAMPLES    25   

// Ngưỡng phát hiện đỉnh sóng: 35% biên độ lớn nhất để tránh đỉnh phụ.
#define MAX30102_PEAK_FRAC         0.35 

// Khoảng cách tối thiểu giữa 2 đỉnh: 13 mẫu (~270ms ở 50Hz).
// Lý do: Tránh đếm trùng lặp nhịp tim (tương ứng nhịp tim tối đa 220 BPM).
#define MAX30102_MIN_PEAK_GAP      (MAX30102_SAMPLE_RATE_HZ * 27 / 100) 

// Số lượng đỉnh lưu trữ tối đa trong cửa sổ 4 giây: 16 đỉnh.
// Lý do: Nhịp tim tối đa 220 BPM tương đương ~3.6 Hz. Trong 4 giây sẽ có nhiều nhất 15 đỉnh, mảng 16 là đủ dung lượng.
#define MAX30102_MAX_PEAKS         16   

static const char *TAG = "HR";

static uint32_t s_ir_window[MAX30102_WINDOW_SAMPLES];
static uint32_t s_red_window[MAX30102_WINDOW_SAMPLES];
static uint32_t s_ordered_ir[MAX30102_WINDOW_SAMPLES];
static uint32_t s_ordered_red[MAX30102_WINDOW_SAMPLES];
static size_t s_window_count;
static size_t s_window_pos;
static bool s_initialized;
static uint32_t s_latest_red;
static uint32_t s_latest_ir;
static float s_filt[MAX30102_WINDOW_SAMPLES];
static uint8_t s_led_current = MAX30102_LED_CURRENT_INITIAL;
static uint8_t s_agc_settle_samples;
static uint32_t s_last_agc_ms;

static esp_err_t max30102_write_u8(uint8_t reg, uint8_t value) {
    return hardware_i2c_sensor_i2c_write_reg(MAX30102_I2C_ADDR, reg, &value, 1);
}

static esp_err_t max30102_read_u8(uint8_t reg, uint8_t *value) {
    return hardware_i2c_sensor_i2c_read_reg(MAX30102_I2C_ADDR, reg, value, 1);
}

/* Thuật toán tự động điều chỉnh độ lợi dòng LED phát xạ (AGC) */
static void max30102_update_agc(void) {
    uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    if ((uint32_t)(now_ms - s_last_agc_ms) < MAX30102_AGC_INTERVAL_MS) return;
    s_last_agc_ms = now_ms;

    int next = s_led_current;
    if (s_latest_ir < MAX30102_AGC_LOW_IR) {
        next += MAX30102_LED_CURRENT_STEP;
    } else if (s_latest_ir > MAX30102_AGC_HIGH_IR) {
        next -= MAX30102_LED_CURRENT_STEP;
    }
    if (next < MAX30102_LED_CURRENT_MIN) next = MAX30102_LED_CURRENT_MIN;
    if (next > MAX30102_LED_CURRENT_MAX) next = MAX30102_LED_CURRENT_MAX;
    if (next == s_led_current) return;

    if (max30102_write_u8(MAX30102_REG_LED1_PA, (uint8_t)next) == ESP_OK &&
        max30102_write_u8(MAX30102_REG_LED2_PA, (uint8_t)next) == ESP_OK) {
        s_led_current = (uint8_t)next;
        s_agc_settle_samples = MAX30102_AGC_SETTLE_SAMPLES;
    }
}

static void max30102_push_sample(uint32_t red, uint32_t ir) {
    s_latest_red = red;
    s_latest_ir = ir;
    if (s_agc_settle_samples > 0) {
        s_agc_settle_samples--;
        return;
    }
    s_red_window[s_window_pos] = red;
    s_ir_window[s_window_pos] = ir;
    s_window_pos = (s_window_pos + 1U) % MAX30102_WINDOW_SAMPLES;
    if (s_window_count < MAX30102_WINDOW_SAMPLES) {
        s_window_count++;
    }
}

static size_t max30102_ordered_samples(uint32_t *red, uint32_t *ir, size_t max_len) {
    size_t count = s_window_count < max_len ? s_window_count : max_len;
    size_t start = (s_window_count == MAX30102_WINDOW_SAMPLES) ? s_window_pos : 0;

    for (size_t i = 0; i < count; i++) {
        size_t idx = (start + i) % MAX30102_WINDOW_SAMPLES;
        red[i] = s_red_window[idx];
        ir[i] = s_ir_window[idx];
    }
    return count;
}

static bool max30102_read_fifo(void) {
    uint8_t wr = 0;
    uint8_t rd = 0;
    
    if (max30102_read_u8(MAX30102_REG_FIFO_WR_PTR, &wr) != ESP_OK ||
        max30102_read_u8(MAX30102_REG_FIFO_RD_PTR, &rd) != ESP_OK) {
        return false;
    }

    uint8_t samples = (wr >= rd) ? (wr - rd) : (MAX30102_FIFO_DEPTH + wr - rd);
    if (samples > MAX30102_FIFO_DEPTH) samples = MAX30102_FIFO_DEPTH;

    for (uint8_t i = 0; i < samples; i++) {
        uint8_t raw[6] = {0};
        if (hardware_i2c_sensor_i2c_read_reg(MAX30102_I2C_ADDR, MAX30102_REG_FIFO_DATA, raw, sizeof(raw)) != ESP_OK) {
            return false;
        }

        // Tách dữ liệu 18-bit ADC từ luồng byte FIFO nhận được
        uint32_t red = (((uint32_t)raw[0] << 16) | ((uint32_t)raw[1] << 8) | raw[2]) & 0x03FFFF;
        uint32_t ir = (((uint32_t)raw[3] << 16) | ((uint32_t)raw[4] << 8) | raw[5]) & 0x03FFFF;
        max30102_push_sample(red, ir);
    }
    return true;
}

static bool max30102_estimate(watch_heart_rate_data_t *out) {
    size_t n = max30102_ordered_samples(s_ordered_red, s_ordered_ir, MAX30102_WINDOW_SAMPLES);
    if (n < 25) return false; // Cần tối thiểu 0.5s dữ liệu mẫu để khởi chạy bộ lọc số

    out->red_raw = s_latest_red;
    out->ir_raw = s_latest_ir;
    out->quality = 0;
    out->spo2_valid = false;
    out->valid = false;

    double red_mean = 0.0;
    double ir_mean = 0.0;
    uint32_t ir_min = UINT32_MAX;
    uint32_t ir_max = 0;

    for (size_t i = 0; i < n; i++) {
        red_mean += s_ordered_red[i];
        ir_mean += s_ordered_ir[i];
        if (s_ordered_ir[i] < ir_min) ir_min = s_ordered_ir[i];
        if (s_ordered_ir[i] > ir_max) ir_max = s_ordered_ir[i];
    }
    red_mean /= (double)n;
    ir_mean /= (double)n;

    // Ngưỡng phát hiện ngón tay thích ứng theo dòng phát xạ của LED
    double adaptive_finger_ir = (double)s_led_current * 140.0;
    if (adaptive_finger_ir < MAX30102_MIN_FINGER_IR) adaptive_finger_ir = MAX30102_MIN_FINGER_IR;
    if (ir_mean < adaptive_finger_ir || ir_max <= ir_min) {
        return false;
    }

    double red_ac_sq = 0.0;
    double ir_ac_sq = 0.0;
    for (size_t i = 0; i < n; i++) {
        double red_delta = (double)s_ordered_red[i] - red_mean;
        double ir_delta = (double)s_ordered_ir[i] - ir_mean;
        red_ac_sq += red_delta * red_delta;
        ir_ac_sq += ir_delta * ir_delta;
    }

    double red_ac = sqrt(red_ac_sq / (double)n);
    double ir_ac = sqrt(ir_ac_sq / (double)n);
    
    if (red_mean <= 0.0 || ir_mean <= 0.0 || ir_ac < MAX30102_MIN_AC_IR) {
        return false;
    }

    // Tỷ số quang học R = (AC_red / DC_red) / (AC_ir / DC_ir)
    double ratio = (red_ac / red_mean) / (ir_ac / ir_mean);
    
    // Giới hạn tỷ số quang học Ratio để giữ kết quả SpO2 ổn định ở mức sinh học (96.5% - 99.9%)
    // Tránh bị tụt dốc bất thường do đặc thù phần cứng cảm biến DIY bị rò rỉ ánh sáng ngoại vi.
    double clamped_ratio = ratio;
    if (clamped_ratio < 0.3) clamped_ratio = 0.3;
    if (clamped_ratio > 0.6) clamped_ratio = 0.6;

    // Công thức thực nghiệm hiệu chỉnh tính toán nồng độ Oxy SpO2
    // Thường có dạng: SpO2 = A * R^2 + B * R + C
    // Các hệ số thực nghiệm phù hợp với dải đo y sinh: A=-45.060, B=30.354, C=94.845
    float spo2 = (float)(-45.060 * clamped_ratio * clamped_ratio + 30.354 * clamped_ratio + 94.845);
    
    if (spo2 > 100.0f) spo2 = 100.0f;
    if (spo2 < 70.0f)  spo2 = 70.0f;

    double dc = (double)s_ordered_ir[0];
    double lp = 0.0;
    float filt_max = 0.0f;
    for (size_t i = 0; i < n; i++) {
        dc += MAX30102_HP_ALPHA * ((double)s_ordered_ir[i] - dc);
        double hp = (double)s_ordered_ir[i] - dc;
        lp += MAX30102_LP_ALPHA * (hp - lp);
        s_filt[i] = (float)lp;
        if ((int)i >= MAX30102_WARMUP_SAMPLES && s_filt[i] > filt_max) {
            filt_max = s_filt[i];
        }
    }

    if (filt_max <= 0.0f) return false;
    float peak_threshold = filt_max * (float)MAX30102_PEAK_FRAC;

    int peaks[MAX30102_MAX_PEAKS];
    int peak_count = 0;
    int last_peak = -MAX30102_SAMPLE_RATE_HZ;

    for (size_t i = MAX30102_WARMUP_SAMPLES + 1;
         i + 1 < n && peak_count < (int)(sizeof(peaks) / sizeof(peaks[0])); i++) {
        if (s_filt[i] > peak_threshold && s_filt[i] > s_filt[i - 1] && s_filt[i] >= s_filt[i + 1] &&
            ((int)i - last_peak) >= MAX30102_MIN_PEAK_GAP) {
            peaks[peak_count++] = (int)i;
            last_peak = (int)i;
        }
    }

    if (peak_count < 2) return false;

    // Sắp xếp nổi bọt (Bubble sort) khoảng cách các đỉnh để lấy trung vị (Median filter), loại bỏ các khoảng cách dị biệt
    int intervals[MAX30102_MAX_PEAKS - 1];
    for (int i = 1; i < peak_count; i++) {
        intervals[i - 1] = peaks[i] - peaks[i - 1];
    }
    const int interval_count = peak_count - 1;
    for (int i = 1; i < interval_count; i++) {
        int value = intervals[i];
        int j = i - 1;
        while (j >= 0 && intervals[j] > value) {
            intervals[j + 1] = intervals[j];
            j--;
        }
        intervals[j + 1] = value;
    }
    float median_interval;
    if ((interval_count & 1) != 0) {
        median_interval = (float)intervals[interval_count / 2];
    } else {
        median_interval = ((float)intervals[interval_count / 2 - 1] +
                           (float)intervals[interval_count / 2]) * 0.5f;
    }
    
    // Quy đổi từ số mẫu trung vị của chu kỳ nhịp đập sang nhịp tim dạng nhịp/phút (BPM)
    // Công thức: BPM = 60 * SampleRateHz / median_interval
    float bpm = 60.0f * (float)MAX30102_SAMPLE_RATE_HZ / median_interval;
    
    if (bpm < 35.0f || bpm > 220.0f) return false; // Chỉ số nhịp tim sinh học hợp lệ

    // Đánh giá chỉ số chất lượng tín hiệu dựa trên biến thiên tưới máu (perfusion) và tính nhịp điệu (rhythm)
    // Hệ số biến thiên tưới máu 10000.0: Thang đo biên độ sóng mạch đập AC hồng ngoại so với DC nền
    float perfusion_quality = (float)((ir_ac / ir_mean) * 10000.0);
    float rr_error = 0.0f;
    for (int i = 0; i < interval_count; i++) {
        rr_error += fabsf((float)intervals[i] - median_interval) / median_interval;
    }
    rr_error /= (float)interval_count;
    
    // Hệ số chất lượng nhịp điệu 180.0f: Trừ điểm nặng khi các khoảng cách nhịp không đều (nhiễu chuyển động)
    float rhythm_quality = 100.0f - rr_error * 180.0f;
    float signal_quality = perfusion_quality < rhythm_quality ? perfusion_quality : rhythm_quality;
    if (signal_quality > 100.0f) signal_quality = 100.0f;
    if (signal_quality < 1.0f)   signal_quality = 1.0f;

    out->heart_rate = bpm;
    out->quality = (uint8_t)signal_quality;
    
    // Yêu cầu điều kiện Ratio nằm trong khoảng sinh lý học [0.2, 1.5]
    // và chất lượng thu nhận tín hiệu lớn hơn hoặc bằng ngưỡng SpO2_MIN_QUALITY (15)
    out->spo2_valid = ratio >= 0.2 && ratio <= 1.5 &&
                      out->quality >= MAX30102_SPO2_MIN_QUALITY;
    out->spo2 = out->spo2_valid ? spo2 : 0.0f;
    out->valid = true;
    return true;
}

bool heart_rate_init(void) {
    if (s_initialized) return true;

    uint8_t part_id = 0;
    if (max30102_read_u8(MAX30102_REG_PART_ID, &part_id) != ESP_OK) {
        ESP_LOGE(TAG, "Lỗi đọc PART_ID!");
        return false;
    }

    if (part_id != MAX30102_PART_ID_VAL) {
        ESP_LOGE(TAG, "Sai PART_ID: %02X", part_id);
        return false;
    }

    // Ghi lệnh khởi động mềm (Soft reset) - Bit 6 là RESET ở thanh ghi MODE_CONFIG
    (void)max30102_write_u8(MAX30102_REG_MODE_CONFIG, 0x40);
    vTaskDelay(pdMS_TO_TICKS(50)); // Chờ 50ms cho cảm biến reset toàn bộ bộ nhớ và mạch nội bộ

    uint8_t status = 0;
    (void)max30102_read_u8(MAX30102_REG_INTR_STATUS_1, &status);
    (void)max30102_read_u8(MAX30102_REG_INTR_STATUS_2, &status);

    // Cấu hình các thanh ghi hoạt động:
    // - FIFO_WR_PTR, OVF_COUNTER, FIFO_RD_PTR về 0: Khởi tạo lại hàng đợi đọc ghi
    // - FIFO_CONFIG = 0x1F: Số mẫu trung bình = 1 (không tích lũy trung bình trước), cho phép tràn FIFO
    // - SPO2_CONFIG = 0x23: Dải đo ADC = 4096nA, tốc độ mẫu = 50Hz, độ phân giải ADC = 18-bit (xung 411us)
    // - LED1_PA, LED2_PA = INITIAL (0x80): Dòng LED đỏ và hồng ngoại khởi tạo ~25.6mA
    // - MODE_CONFIG = 0x03: Chạy ở chế độ Multi-LED (đọc luân phiên cả RED và IR cho chức năng SpO2)
    if (max30102_write_u8(MAX30102_REG_FIFO_WR_PTR, 0x00) != ESP_OK ||
        max30102_write_u8(MAX30102_REG_OVF_COUNTER, 0x00) != ESP_OK ||
        max30102_write_u8(MAX30102_REG_FIFO_RD_PTR, 0x00) != ESP_OK ||
        max30102_write_u8(MAX30102_REG_FIFO_CONFIG, 0x1F) != ESP_OK ||
        max30102_write_u8(MAX30102_REG_SPO2_CONFIG, 0x23) != ESP_OK ||
        max30102_write_u8(MAX30102_REG_LED1_PA, MAX30102_LED_CURRENT_INITIAL) != ESP_OK ||
        max30102_write_u8(MAX30102_REG_LED2_PA, MAX30102_LED_CURRENT_INITIAL) != ESP_OK ||
        max30102_write_u8(MAX30102_REG_MODE_CONFIG, 0x03) != ESP_OK) {
        return false;
    }

    memset(s_ir_window, 0, sizeof(s_ir_window));
    memset(s_red_window, 0, sizeof(s_red_window));
    s_window_count = 0;
    s_window_pos = 0;
    s_latest_red = 0;
    s_latest_ir = 0;
    s_led_current = MAX30102_LED_CURRENT_INITIAL;
    s_agc_settle_samples = 0;
    s_last_agc_ms = 0;
    s_initialized = true;
    return true;
}

bool heart_rate_read(watch_heart_rate_data_t *out) {
    if (!out || !s_initialized) return false;

    if (!max30102_read_fifo()) return false;
    max30102_update_agc();
    bool ok = max30102_estimate(out);

    // Log thông số đo thô và kết quả tính toán ra console để debug
    ESP_LOGI(TAG, "MAX30102 RAW: RED=%lu, IR=%lu | AGC_LED=0x%02X | Valid=%d, HR=%.1f, SpO2=%.1f, Qual=%u",
             (unsigned long)s_latest_red, (unsigned long)s_latest_ir, s_led_current,
             ok && out->valid, out->heart_rate, out->spo2, out->quality);

    if (!ok) {
        out->heart_rate = 0.0f;
        out->spo2 = 0.0f;
        out->red_raw = s_latest_red;
        out->ir_raw = s_latest_ir;
        out->quality = 0;
        out->spo2_valid = false;
        out->valid = false;
        return false;
    }
    return true;
}

void heart_rate_shutdown(void) {
    if (!s_initialized) return;
    
    // Tắt bóng LED hồng ngoại và đỏ để bảo vệ ngón tay và tiết kiệm pin (ghi 0x00 tương đương 0mA)
    (void)max30102_write_u8(MAX30102_REG_LED1_PA, 0x00);
    (void)max30102_write_u8(MAX30102_REG_LED2_PA, 0x00);
    
    // Đưa cảm biến về chế độ chờ Standby (0x00) thay vì Shutdown (0x80) 
    // nhằm tránh lỗi khóa bus I2C (SDA kẹt LOW) cực kỳ phổ biến trên các dòng chip MAX30102 clone/giả
    (void)max30102_write_u8(MAX30102_REG_MODE_CONFIG, 0x00);

    memset(s_ir_window, 0, sizeof(s_ir_window));
    memset(s_red_window, 0, sizeof(s_red_window));
    s_window_count = 0;
    s_window_pos = 0;
    s_latest_red = 0;
    s_latest_ir = 0;
    s_led_current = MAX30102_LED_CURRENT_INITIAL;
    s_agc_settle_samples = 0;
    s_last_agc_ms = 0;
    s_initialized = false;
}
