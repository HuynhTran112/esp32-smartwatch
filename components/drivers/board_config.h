/* Cấu hình tài nguyên phần cứng đồng hồ thông minh (Smartwatch)
   Tài liệu mô tả chi tiết sơ đồ chân GPIO kết nối giữa ESP32-S3 và các ngoại vi:
   Màn hình TFT LCD ST7789, Cảm ứng CST816S, IMU BMI270, Cảm biến nhịp tim MAX30102,
   IC quản lý pin MAX17048, Động cơ rung PWM và Định vị GNSS GT-U8. */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/* ===================================================================
 *  1. CẤU HÌNH MÀN HÌNH TFT LCD (Giao tiếp SPI)
 *  Độ phân giải 240x284 pixels
 * =================================================================== */
// Các chân GPIO kết nối giữa ESP32-S3 và màn hình
#define WATCH_PIN_LCD_SCLK      40  // Chân xung Clock của bus SPI (TFT_SCL)
#define WATCH_PIN_LCD_MOSI      39  // Chân truyền dữ liệu (TFT_SDA)
#define WATCH_PIN_LCD_RST       38  // Chân Reset cứng màn hình (TFT_RST)
#define WATCH_PIN_LCD_DC        42  // Chân phân biệt Dữ liệu/Lệnh (TFT_DC)
#define WATCH_PIN_LCD_CS        41  // Chân chọn chip ngoại vi SPI (TFT_CS)
#define WATCH_PIN_LCD_BACKLIGHT 2   // Chân điều khiển đèn nền LCD

#define WATCH_LCD_H_RES         240 // Độ phân giải chiều ngang màn hình (pixels)
#define WATCH_LCD_V_RES         284 // Độ phân giải chiều dọc màn hình (pixels)
#define WATCH_LCD_SPI_HOST      SPI2_HOST // Bộ điều khiển SPI2 phần cứng

// Tần số xung nhịp SPI là 40 MHz.
// Lý do: Đảm bảo màn hình hiển thị mượt mà (>30 FPS) và tránh nhiễu tín hiệu trên đường truyền SPI.
#define WATCH_LCD_PIXEL_CLK_HZ  (40 * 1000 * 1000) 

/* ===================================================================
 *  2. CẤU HÌNH ĐÈN NỀN LCD (Sử dụng bộ phát xung PWM - LEDC)
 * =================================================================== */
#define WATCH_LCD_BACKLIGHT_LEDC_CH    LEDC_CHANNEL_0  // Kênh PWM điều khiển đèn nền
#define WATCH_LCD_BACKLIGHT_LEDC_TIMER LEDC_TIMER_0    // Timer cho kênh PWM

// Mức sáng mặc định: 200/255 (~78%).
// Lý do: Đủ sáng khi dùng trong nhà/ngoài trời dịu và tiết kiệm pin.
#define WATCH_LCD_BACKLIGHT_DUTY_DEFAULT 200          

/* ===================================================================
 *  3. CẤU HÌNH CẢM ỨNG (Giao tiếp I2C Port 0)
 *  Cảm ứng điện dung
 * =================================================================== */
#define WATCH_PIN_TOUCH_SDA     48  // Chân dữ liệu I2C cảm ứng (TP_SDA)
#define WATCH_PIN_TOUCH_SCL     47  // Chân nhịp clock I2C cảm ứng (TP_SCL)
#define WATCH_PIN_TOUCH_INT     14  // Chân ngắt: Kéo thấp khi phát hiện chạm (TP_INT)
#define WATCH_PIN_TOUCH_RST     21  // Chân Reset cứng cảm ứng (TP_RST)

#define WATCH_TOUCH_I2C_PORT    I2C_NUM_0           // Cổng I2C số 0

// Tần số I2C Fast Mode (400 kHz).
// Lý do: Tốc độ tối đa cảm ứng hỗ trợ để đọc tọa độ nhanh, tránh trễ khi vuốt chạm.
#define WATCH_TOUCH_I2C_CLK_HZ  (400 * 1000)        

// Khử log I2C rác khi debug.
// Lý do: Tránh tràn log hệ thống khi cảm ứng chưa phản hồi lúc chuyển trạng thái.
#define WATCH_TEMP_DISABLE_I2C_LOGS 1               

/* ===================================================================
 *  4. CẤU HÌNH BUS I2C DÀNH CHO CẢM BIẾN (I2C Port 1)
 *  Bus dùng chung cho IMU, nhịp tim và đo pin
 * =================================================================== */
#define WATCH_SENSOR_I2C_PORT   I2C_NUM_1           // Cổng I2C phần cứng số 1
#define WATCH_PIN_IMU_I2C_SDA   19                  // Chân truyền dữ liệu I2C
#define WATCH_PIN_IMU_I2C_SCL   8                   // Chân xung nhịp clock I2C

#define WATCH_PIN_IMU_INT       20                  // Chân nhận tín hiệu ngắt từ IMU
#define WATCH_PIN_HR_INT        13                  // Chân nhận tín hiệu ngắt từ cảm biến nhịp tim
#define WATCH_PIN_BATTERY_ALRT  11                  // Chân cảnh báo pin yếu từ chip đo pin

#define WATCH_IMU_I2C_ADDR      0x68                // Địa chỉ I2C của cảm biến IMU

// Tần số I2C cho cảm biến là 400 kHz (Fast Mode).
// Lý do: Tốc độ tối đa các cảm biến hỗ trợ để đọc dữ liệu nhịp tim/gia tốc nhanh và tiết kiệm điện.
#define WATCH_IMU_I2C_CLK_HZ    400000              

/* ===================================================================
 *  5. CẤU HÌNH ĐỘNG CƠ RUNG (PWM qua bộ phát xung LEDC)
 * =================================================================== */
#define WATCH_PIN_VIBRATOR           12              // Chân GPIO điều khiển rung
#define WATCH_VIBRATOR_LEDC_CH       LEDC_CHANNEL_1  // Kênh điều khiển PWM motor
#define WATCH_VIBRATOR_LEDC_TIMER    LEDC_TIMER_1    // Timer cho motor rung

// Tần số phát xung rung là 200 Hz.
// Lý do: Tần số hoạt động tốt nhất giúp mô-tơ rung đủ mạnh, chạy êm và không bị rít.
#define WATCH_VIBRATOR_LEDC_FREQ_HZ  200             

/* ===================================================================
 *  6. CẤU HÌNH PHÍM BẤM VẬT LÝ
 * =================================================================== */
#define WATCH_PIN_BUTTON_POWER       1               // Nút bấm vật lý (nút nguồn kiêm BOOT) kéo thấp khi nhấn

/* ===================================================================
 *  7. CẤU HÌNH MODULE GPS (UART)
 * =================================================================== */
#define WATCH_GPS_MODULE_NAME        "GT-U8"         // Tên định danh module GPS
#define WATCH_GPS_UART_PORT          UART_NUM_1      // Sử dụng cổng UART 1

// Tốc độ Baudrate mặc định là 9600 bps.
// Lý do: Tốc độ mặc định của hầu hết các module GPS khi khởi động nguội.
#define WATCH_GPS_UART_BAUD          9600            

// Tốc độ Baudrate dự phòng (38400 bps và 115200 bps).
// Lý do: Đề phòng module GPS lưu baudrate cao hơn trong bộ nhớ của nó.
#define WATCH_GPS_UART_BAUD_FALLBACK_1 38400         
#define WATCH_GPS_UART_BAUD_FALLBACK_2 115200        

#define WATCH_PIN_GPS_RST            -1              // Không dùng Reset cứng
#define WATCH_PIN_GPS_PPS            4               // Chân nhận xung nhịp thời gian PPS từ GPS
#define WATCH_PIN_GPS_UART_TX        5               // Chân TX của ESP32 kết nối đến RX GPS
#define WATCH_PIN_GPS_UART_RX        6               // Chân RX của ESP32 nhận dữ liệu từ TX GPS

#endif // BOARD_CONFIG_H
