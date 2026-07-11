# Tài liệu Giải thích các Lưu đồ Giải thuật Con (Sub-flowcharts)

Tài liệu này giải thích chi tiết nguyên lý hoạt động của các lưu đồ con (được thiết kế bằng Draw.io) của các hệ thống cảm biến và định vị trên Smartwatch, phục vụ cho việc lưu trữ và giới thiệu năng lực thiết kế phần mềm nhúng.

---

## 1. Hệ thống cảm biến chuyển động & Đếm bước (IMU BMI270)

### 📌 1.1. Lưu đồ hoạt động của IMU BMI270
![IMU Flowchart](./docs/images/1.8.2.IMU.drawio%20(6).png)

* **Mô tả:** Khởi tạo cảm biến gia tốc BMI270 thông qua đường truyền I2C Port 1. Cấu hình các thanh ghi ngắt (Interrupt Registers) để tự động phát hiện chuyển động (Any-motion) và cấu hình tính năng đếm bước phần cứng tích hợp trong chip để giảm tải cho CPU chính của ESP32-S3.

### 📌 1.2. Lưu đồ giải thuật đếm bước chân (Step Counter)
![Step Counter Flowchart](./docs/images/1.8.2.1.Step_counter.drawio%20(14).png)

* **Mô tả:** Khi có ngắt chuyển động từ chân `IMU_INT` (GPIO 20), tác vụ đếm bước sẽ được kích hoạt để đọc dữ liệu số bước từ thanh ghi của BMI270. Dữ liệu này sau đó sẽ được lọc, lưu vào bộ nhớ tạm thời và chuẩn bị đồng bộ qua Bluetooth LE.

### 📌 1.3. Lưu đồ tính quãng đường (Distance Calculation)
![Distance Flowchart](./docs/images/1.8.2.2.Distance_count.drawio%20(7).png)

* **Mô tả:** Tính toán quãng đường di chuyển ($S$) dựa trên số bước chân thực tế nhân với chiều dài bước chân trung bình của người dùng (được cấu hình trước trên ứng dụng điện thoại), hoặc kết hợp dữ liệu GPS ngoài trời để tăng độ chính xác của lộ trình.

---

## 2. Cảm biến Sức khỏe & Tính toán chỉ số sinh hiệu (MAX30102)

### 📌 2.1. Lưu đồ quản lý cảm biến MAX30102
![MAX30102 Flowchart](./docs/images/1.8.3.MAX30102.drawio%20(6).png)

* **Mô tả:** Khởi tạo cảm biến nhịp tim/SpO2 MAX30102 qua I2C Port 1, cấu hình dòng phát của hai bóng LED (Red và IR) cùng tần số lấy mẫu. Sử dụng chân ngắt `HR_INT` (GPIO 13) để báo hiệu cho ESP32-S3 đọc bộ đệm FIFO 32 mẫu của cảm biến mỗi khi đầy dữ liệu.

### 📌 2.2. Giải thuật đo nồng độ Oxy trong máu (SpO2 Algorithm)
![SpO2 Flowchart](./docs/images/1.8.3.1.SpO2.drawio%20(5).png)

* **Mô tả:** Thu thập mẫu ánh sáng phản xạ từ LED Red và IR. Tính toán thành phần một chiều (DC) và xoay chiều (AC) của cả hai kênh sáng, sau đó tính tỉ lệ hấp thụ $R$:
  $$R = \frac{AC_{Red} / DC_{Red}}{AC_{IR} / DC_{IR}}$$
  Từ tỉ lệ $R$, đối chiếu với bảng hiệu chuẩn thực nghiệm để xuất ra giá trị nồng độ Oxy trong máu $SpO_{2} (\%)$.

### 📌 2.3. Giải thuật đo nhịp tim (Heart Rate Algorithm)
![Heart Rate Flowchart](./docs/images/1.8.3.2.Hearate.drawio%20(11).png)

* **Mô tả:** Tín hiệu AC từ kênh ánh sáng phản xạ được đưa qua bộ lọc dải thông (Bandpass Filter) để loại bỏ nhiễu nhiễu tần số thấp (do rung tay) và tần số cao. Áp dụng giải thuật phát hiện đỉnh (Peak Detection) hoặc phân tích tần số để tính toán chu kỳ mạch đập và xuất ra chỉ số nhịp tim BPM (Beats-Per-Minute).

---

## 3. Hệ thống Định vị & Lưu trữ Hành trình (GPS GT-U8)

### 📌 3.1. Lưu đồ xử lý dữ liệu GPS
![GPS Flowchart](./docs/images/1.9.GPS_handle%20(2).drawio%20(7).png)

* **Mô tả:** Nhận các chuỗi dữ liệu thô NMEA 0183 qua bộ thu UART 1. Giải mã các bản tin `$GPRMC` và `$GPGGA` để trích xuất tọa độ kinh/vĩ độ, tốc độ di chuyển hiện tại và thời gian thực chuẩn UTC. Tọa độ được ghi trực tiếp vào bộ nhớ flash (phân vùng SPIFFS) dưới dạng tệp tin log để đồng bộ vẽ bản đồ đường đi trên ứng dụng di động khi kết nối lại.
