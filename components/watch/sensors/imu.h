/* Quản lý cảm biến IMU. */

#ifndef IMU_H
#define IMU_H

#include <stdbool.h>
#include <stdint.h>

/* Danh mục các trạng thái chuyển động của người đeo đồng hồ */
typedef enum {
    WATCH_MOTION_STATIONARY = 0, // Đứng yên / Không chuyển động
    WATCH_MOTION_WALKING,        // Đi bộ nhịp nhàng
    WATCH_MOTION_RUNNING,        // Chạy bộ nhịp nhàng
    WATCH_MOTION_ACTIVE,         // Hoạt động chung (vận động tự do không theo nhịp điệu đi/chạy)
} watch_motion_type_t;

/* Cấu trúc chứa dữ liệu góc quay và bước chân lấy từ IMU */
typedef struct {
    float accel_x;                   // Gia tốc trục X (đơn vị G)
    float accel_y;                   // Gia tốc trục Y (đơn vị G)
    float accel_z;                   // Gia tốc trục Z (đơn vị G)
    float gyro_x;                    // Vận tốc góc trục X (độ/giây, không sử dụng để tiết kiệm pin)
    float gyro_y;                    // Vận tốc góc trục Y (độ/giây)
    float gyro_z;                    // Vận tốc góc trục Z (độ/giây)
    uint32_t step_count;             // Tổng số bước chân tích lũy kể từ lúc khởi động hoặc reset
    uint16_t cadence_spm;            // Tần suất bước chân dạng số bước/phút (Steps Per Minute)
    watch_motion_type_t motion_type; // Kiểu chuyển động nhận diện thời gian thực
    bool valid;                      // Dữ liệu đọc ra có hợp lệ hay không
} watch_imu_data_t;

/* Khởi tạo IMU qua I2C
   Trả về: true nếu thành công và cảm biến sẵn sàng */
bool imu_init(void);

/* Đọc dữ liệu gia tốc thô và cập nhật thuật toán đếm bước chân
   - out: Con trỏ cấu trúc chứa dữ liệu IMU xuất ra
   Trả về: true nếu đọc cảm biến thành công */
bool imu_read(watch_imu_data_t *out);

/* Thiết lập lại các thông số và bộ đếm bước chân về 0 */
void imu_reset_steps(void);

/* Đưa IMU vào chế độ ngủ sâu tiết kiệm năng lượng */
void imu_sleep(void);

#endif /* IMU_H */
